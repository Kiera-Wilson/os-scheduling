#include <iostream>
#include <string>
#include <list>
#include <vector>
#include <chrono>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <unistd.h>
#include "configreader.h"
#include "process.h"

//check 

// Shared data for all cores
typedef struct SchedulerData {
    std::mutex mutex;
    std::condition_variable condition;
    ScheduleAlgorithm algorithm;
    uint32_t context_switch;
    uint32_t time_slice;
    std::list<Process*> ready_queue;
    bool all_terminated;
} SchedulerData;

void coreRunProcesses(uint8_t core_id, SchedulerData *data);
int printProcessOutput(std::vector<Process*>& processes, std::mutex& mutex);
void clearOutput(int num_lines);
uint64_t currentTime();
std::string processStateToString(Process::State state);

int main(int argc, char **argv)
{
    // Ensure user entered a command line parameter for configuration file name
    if (argc < 2)
    {
        std::cerr << "Error: must specify configuration file" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Declare variables used throughout main
    int i;
    SchedulerData *shared_data;
    std::vector<Process*> processes;

    // Read configuration file for scheduling simulation
    SchedulerConfig *config = readConfigFile(argv[1]); //read file and store

    // Store configuration parameters in shared data object
    uint8_t num_cores = config->cores;
    shared_data = new SchedulerData();
    shared_data->algorithm = config->algorithm;
    shared_data->context_switch = config->context_switch;
    shared_data->time_slice = config->time_slice;
    shared_data->all_terminated = false;

    uint8_t startWaitTime = 0;
    

    // Create processes
    uint64_t start = currentTime(); //get current time
    for (i = 0; i < config->num_processes; i++) //lop over processes
    {
        Process *p = new Process(config->processes[i], start); //new process class
        processes.push_back(p); //add them to the list of processes
        p->setStartTurnTime(currentTime());
        // If process should be launched immediately, add to ready queue
        if (p->getState() == Process::State::Ready)
        {
            p->setStartWaitTime(currentTime());
            shared_data->ready_queue.push_back(p);
        }
    }

    // Free configuration data from memory
    deleteConfig(config);

    // Launch 1 scheduling thread per cpu core
    std::thread *schedule_threads = new std::thread[num_cores];
    for (i = 0; i < num_cores; i++)
    {
        schedule_threads[i] = std::thread(coreRunProcesses, i, shared_data); //launch for CPU processor
    }

    // Main thread work goes here
    int num_lines = 0;
    while (!(shared_data->all_terminated))
    {
        // Clear output from previous iteration
        clearOutput(num_lines);


        // Do the following:


        //   - Get current time, based on current time check next thing (time since the program started
        uint64_t current_time = currentTime();
        //loop through processes
        for(i=0; i< processes.size(); i++){
            //lock the shared data while we use it
            

        // - *Check if any processes need to move from NotStarted to Ready (based on elapsed time), and if so put that process in the ready queue
            if(processes[i]->getStartTime() <= (currentTime() - start) && processes[i]->getState() == Process::State::NotStarted){
                std::lock_guard<std::mutex>lock(shared_data->mutex);
                //change state from not ready to ready
                processes[i]->setState(Process::State::Ready, currentTime());
                //add process to the ready queue
                processes[i]->setStartWaitTime(currentTime());
                shared_data->ready_queue.push_back(processes[i]);

            }

            //if it is in the cpu burst(the index of current burst is even number), put it back in the ready queue
            // - *Check if any processes have finished their I/O burst, and if so put that process back in the ready queue
            if(processes[i]->getState()== Process::State::IO && (currentTime() - processes[i]->getBurstStartTime())>= processes[i]->getSingleBurstTime(processes[i]->getCurrentBurst()))
            {
                
                //ready state
                processes[i]->setState(Process::State::Ready, currentTime());
                //put back into ready queue
                processes[i]->setCurrentBurst(processes[i]->getCurrentBurst()+1);
                processes[i]->setStartWaitTime(currentTime());
                {
                    std::lock_guard<std::mutex>lock(shared_data->mutex);
                    shared_data->ready_queue.push_back(processes[i]);
                }
                
            }

            //   - *Check if any running process need to be interrupted (RR time slice expires or newly ready process has higher priority)
            if(processes[i]->getState() == Process::State::Running){
                std::lock_guard<std::mutex>lock(shared_data->mutex);
                //if RR time slice expires or newly ready process has higher priority
                if(shared_data->time_slice >= (currentTime() - processes[i]->getBurstStartTime()) && shared_data->algorithm == RR)
                {
                    processes[i]->interrupt();

                } else if(shared_data->algorithm == PP){
                    //loop to check if something else has higher priority
                    for (int k =0; k<processes.size(); k++){
                        //if something does have a lower priority, stop it
                        if(processes[k]->getState() == Process::State::Ready){
                            if(processes[k]->getPriority() < processes[i]->getPriority()){
                            processes[i]->interrupt();
                        }
                        }
                    }
                }

            }

        }


        {//scope start
         //   - *Sort the ready queue (if needed - based on scheduling algorithm) (RR andd FCFS dont need to be sorted)
        std::lock_guard<std::mutex>lock(shared_data->mutex);

        if(shared_data->algorithm == PP){
            PpComparator pp;
            shared_data->ready_queue.sort(pp);
        
        }else if(shared_data->algorithm == SJF){
            SjfComparator sjf;
            shared_data->ready_queue.sort(sjf);
        }

        }// scope end

        int counter = 0;
        //   - Determine if all processes are in the terminated state
        for(int i=0; i<processes.size(); i++){
            if(processes[i]->getState() == Process::State::Terminated){
                counter++;
            }
        }
        if(counter == processes.size()){
            shared_data->all_terminated = true;
        }

        //   - * = accesses shared data (ready queue), so be sure to use proper synchronization
        // output process status table
        num_lines = printProcessOutput(processes, shared_data->mutex);

        // sleep 50 ms
        usleep(50000);
    }


    // wait for threads to finish
    for (i = 0; i < num_cores; i++)
    {
        schedule_threads[i].join();
    }

    // print final statistics
    //  - CPU utilization
    //  - Throughput
    //     - Average for first 50% of processes finished
    //     - Average for second 50% of processes finished
    //     - Overall average
    //  - Average turnaround time
    //  - Average waiting time

    
    // Clean up before quitting program
    processes.clear();

    return 0;
}
    

void coreRunProcesses(uint8_t core_id, SchedulerData *shared_data)
{
    // Work to be done by each core idependent of the other cores
    // Repeat until all processes in terminated state:
    while (!(shared_data->all_terminated))
    {
    //   - *Get process at front of ready queue

    //   - Simulate the processes running until one of the following:
    //     - CPU burst time has elapsed
    //     - Interrupted (RR time slice has elapsed or process preempted by higher priority process)
        
        Process* process = NULL;
        
        {
            std::lock_guard<std::mutex>lock(shared_data->mutex);
            
            if(shared_data->ready_queue.front() != NULL){
                process = shared_data->ready_queue.front();
                shared_data->ready_queue.pop_front(); //take the element off of the ready queue
                process->setCpuCore(core_id);
                process->setBurstStartTime(currentTime());
                process->setEndWaitTime(currentTime());
                process->setState(Process::State::Running, currentTime());
                process->setStartCPUTime(currentTime());
            }
        }
        

        while(process != NULL){ 
            if((currentTime() - process->getBurstStartTime()) >= process->getSingleBurstTime(process->getCurrentBurst()) && process->getCurrentBurst() < (process->getBurstnumber()-1)){
                process->setState(Process::State::IO, currentTime());
                process->setCurrentBurst(process->getCurrentBurst()+1);
                process->setBurstStartTime(currentTime());
                process->setCpuCore(-1);
                process->setEndCPUTime(currentTime());
                break;
            }else if(process->isInterrupted() == true && shared_data->algorithm == RR){
                process->updateBurstTime(process->getCurrentBurst(), process->getSingleBurstTime(process->getCurrentBurst()) - (currentTime() - process->getBurstStartTime()));
                process->setState(Process::State::Ready, currentTime());
                process->interruptHandled();
                process->setStartWaitTime(currentTime());
                process->setCpuCore(-1);
                {
                    std::lock_guard<std::mutex>lock(shared_data->mutex);
                    shared_data->ready_queue.push_back(process);
                }
                process->setEndCPUTime(currentTime());
                break;

            }else if(process->isInterrupted() == true && shared_data->algorithm == PP){
                process->updateBurstTime(process->getCurrentBurst(), process->getSingleBurstTime(process->getCurrentBurst()) - (currentTime() - process->getBurstStartTime()));
                process->setState(Process::State::Ready, currentTime());
                process->interruptHandled();
                process->setStartWaitTime(currentTime());
                process->setCpuCore(-1);
                {
                    std::lock_guard<std::mutex>lock(shared_data->mutex);
                    shared_data->ready_queue.push_back(process);
                }
                process->setEndCPUTime(currentTime());
                break;
                
            }else if(currentTime() - process->getBurstStartTime() >= process->getSingleBurstTime(process->getCurrentBurst()) && process->getCurrentBurst() == (process->getBurstnumber()-1)){
                process->setState(Process::State::Terminated, currentTime());
                //process->setCurrentBurst(process->getCurrentBurst()+1);
                process->setCpuCore(-1);
                process->setEndCPUTime(currentTime());
                process->setEndTurnTime(currentTime());
                break;
            }
            process->updateProcess(currentTime());

        }

    //  - Place the process back in the appropriate queue
    //     - I/O queue if CPU burst finished (and process not finished) -- no actual queue, simply set state to IO
    //     - Terminated if CPU burst finished and no more bursts remain -- no actual queue, simply set state to Terminated
    //     - *Ready queue if interrupted (be sure to modify the CPU burst time to now reflect the remaining time)
    //  - Wait context switching time

    /*
    {
        std::lock_guard<std::mutex>lock(shared_data->mutex);
        usleep(shared_data->context_switch);
    }*/

    
   

    usleep(50000);

    //  - * = accesses shared data (ready queue), so be sure to use proper synchronization
    }
}

int printProcessOutput(std::vector<Process*>& processes, std::mutex& mutex)
{
    int i;
    int num_lines = 2;
    std::lock_guard<std::mutex> lock(mutex);
    printf("|   PID | Priority |      State | Core | Turn Time | Wait Time | CPU Time | Remain Time |\n");
    printf("+-------+----------+------------+------+-----------+-----------+----------+-------------+\n");
    for (i = 0; i < processes.size(); i++)
    {
        if (processes[i]->getState() != Process::State::NotStarted)
        {
            uint16_t pid = processes[i]->getPid();
            uint8_t priority = processes[i]->getPriority();
            std::string process_state = processStateToString(processes[i]->getState());
            int8_t core = processes[i]->getCpuCore();
            std::string cpu_core = (core >= 0) ? std::to_string(core) : "--";
            double turn_time = processes[i]->getTurnaroundTime();
            double wait_time = processes[i]->getWaitTime();
            double cpu_time = processes[i]->getCpuTime();
            double remain_time = processes[i]->getRemainingTime();
            printf("| %5u | %8u | %10s | %4s | %9.1lf | %9.1lf | %8.1lf | %11.1lf |\n", 
                   pid, priority, process_state.c_str(), cpu_core.c_str(), turn_time, 
                   wait_time, cpu_time, remain_time);
            num_lines++;
        }
    }
    return num_lines;
}

void clearOutput(int num_lines)
{
    int i;
    for (i = 0; i < num_lines; i++)
    {
        fputs("\033[A\033[2K", stdout);
    }
    rewind(stdout);
    fflush(stdout);
}

uint64_t currentTime()
{
    uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::system_clock::now().time_since_epoch()).count();
    return ms;
}

std::string processStateToString(Process::State state)
{
    std::string str;
    switch (state)
    {
        case Process::State::NotStarted:
            str = "not started";
            break;
        case Process::State::Ready:
            str = "ready";
            break;
        case Process::State::Running:
            str = "running";
            break;
        case Process::State::IO:
            str = "i/o";
            break;
        case Process::State::Terminated:
            str = "terminated";
            break;
        default:
            str = "unknown";
            break;
    }
    return str;
}