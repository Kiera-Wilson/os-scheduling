#include "process.h"

// Process class methods
//warning, dont divide this up into RR anf FCFS
//the only thing different between algorithms is how to sort the processor list
//understand the code and the problem and then make a game plan
//recommends to start the coding in main
//lanch with make, then ./bin/osscheduler resrc/config_01.txt
//warning: do not make it sleep for the CPU burst time, sleep for small time and check again for interupts as well
Process::Process(ProcessDetails details, uint64_t current_time)
{
    int i;
    pid = details.pid;
    start_time = details.start_time;
    num_bursts = details.num_bursts;
    current_burst = 0;
    burst_times = new uint32_t[num_bursts];
    for (i = 0; i < num_bursts; i++)
    {
        burst_times[i] = details.burst_times[i];
    }
    priority = details.priority;
    state = (start_time == 0) ? State::Ready : State::NotStarted;
    if (state == State::Ready)
    {
        launch_time = current_time;
    }
    is_interrupted = false;
    core = -1;
    turn_time = 0;
    wait_time = 0;
    cpu_time = 0;
    remain_time = 0;
    for (i = 0; i < num_bursts; i+=2)
    {
        remain_time += burst_times[i];
    }
}

Process::~Process()
{
    delete[] burst_times;
}

uint32_t Process::getBurstTotalTime(int i)
{
    //i dont really understand what burst_times is holding so I dont know if this will work
    uint32_t burstTotalTime = 0;
    for (i = 0; i < num_bursts; i++)
    {
        burstTotalTime = burstTotalTime + burst_times[i];
    }

    return burstTotalTime;
}

uint32_t Process::getSingleBurstTime(int i)
{
    return burst_times[i];
}

int Process::getBurstnumber()
{
    return num_bursts;
}

uint16_t Process::getCurrentBurst() 
{
    return current_burst;
}

void Process::setCurrentBurst(int nextBurst)  
{
    current_burst = nextBurst;
}

void Process::setStartWaitTime(uint64_t current){
    start_waitTime = current;
}

void Process::setStartCPUTime(uint64_t current){
    start_CPUTime = current;
}
void Process::setStartTurnTime(uint64_t current){
    start_TurnTime = current;
}



void Process::setEndCPUTime(uint64_t current){
    cpu_time = cpu_time + (current - start_CPUTime);
    remain_time = remain_time - (current - start_CPUTime);
    if(state == State::Terminated){
        remain_time = 0.0;
    }
    
}

void Process::setEndWaitTime(uint64_t current){
    wait_time = wait_time + (current - start_waitTime);
}



void Process::setEndTurnTime(uint64_t current){
    int32_t temp = current - start_TurnTime;
    turn_time = turn_time + temp;
}


uint16_t Process::getPid() const
{
    return pid;
}

uint32_t Process::getStartTime() const
{
    return start_time;
}

uint8_t Process::getPriority() const
{
    return priority;
}

uint64_t Process::getBurstStartTime() const
{
    return burst_start_time;
}

Process::State Process::getState() const
{
    return state;
}

bool Process::isInterrupted() const
{
    return is_interrupted;
}

int8_t Process::getCpuCore() const
{
    return core;
}

double Process::getTurnaroundTime() const
{
    return (double)turn_time / 1000.0;
}

double Process::getWaitTime() const
{
    return (double)wait_time / 1000.0;
}

double Process::getCpuTime() const
{
    return (double)cpu_time / 1000.0;
}

double Process::getRemainingTime() const
{
    return (double)remain_time / 1000.0;
}

void Process::setBurstStartTime(uint64_t current_time)
{
    burst_start_time = current_time;
}

void Process::setState(State new_state, uint64_t current_time)
{
    if (state == State::NotStarted && new_state == State::Ready)
    {
        launch_time = current_time;
        
    }
    state = new_state;
}

void Process::setCpuCore(int8_t core_num)
{
    core = core_num;
}

void Process::interrupt()
{
    is_interrupted = true;
}

void Process::interruptHandled()
{
    is_interrupted = false;
}

void Process::updateProcess(uint64_t current_time)
{
    // use `current_time` to update turnaround time, wait time, burst times, 
    // cpu time, and remaining time
    //remain_time = remain_time - cpu_time;
    wait_time = wait_time + (current_time - start_waitTime);
    
}

void Process::updateBurstTime(int burst_idx, uint32_t new_time)
{
    burst_times[burst_idx] = new_time;
}


// Comparator methods: used in std::list sort() method
// No comparator needed for FCFS or RR (ready queue never sorted)

// SJF - comparator for sorting read queue based on shortest remaining CPU time
bool SjfComparator::operator ()(const Process *p1, const Process *p2)
{
    if(p1->getRemainingTime() <= p2->getRemainingTime()){
        return true; // dont switch
    }else{
        return false; //switch
    }
    
}

// PP - comparator for sorting read queue based on priority
///true means you should swap, flase means you shouldnt
bool PpComparator::operator ()(const Process *p1, const Process *p2)
{
    if(p1->getPriority() <= p2->getPriority()){
        return true; // dont switch
    }else{
        return false; //switch
    }
}
