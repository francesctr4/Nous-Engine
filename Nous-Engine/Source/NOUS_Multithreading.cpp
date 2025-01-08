#include "NOUS_Multithreading.h"

#include "MemoryManager.h"

const std::unordered_map<NOUS_Multithreading::ThreadState, std::string> stateToString
{
    {NOUS_Multithreading::ThreadState::READY, "READY"},
    {NOUS_Multithreading::ThreadState::RUNNING, "RUNNING"},
    {NOUS_Multithreading::ThreadState::WAITING, "WAITING"}
};

// Definition of the static variable
std::vector<NOUS_Multithreading::NOUS_Thread*> NOUS_Multithreading::registeredThreads;

// Implementation of GetCurrentThreadID
uint32_t NOUS_Multithreading::GetCurrentThreadID() 
{
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return static_cast<uint32>(std::stoul(ss.str()));
}

std::string NOUS_Multithreading::GetStringFromState(const ThreadState& state)
{
    return stateToString.at(state);
}

void NOUS_Multithreading::ChangeState(NOUS_Thread* thread, const ThreadState& state)
{
    switch (state)
    {
        case NOUS_Multithreading::ThreadState::READY: 
        {
            thread->state = ThreadState::READY;
            thread->executionTime.Stop();
            break;
        }
        case NOUS_Multithreading::ThreadState::RUNNING: 
        {
            thread->state = ThreadState::RUNNING;
            thread->executionTime.Start();
            break;
        }
        case NOUS_Multithreading::ThreadState::WAITING: 
        {
            thread->state = ThreadState::WAITING;
            thread->executionTime.Pause();
            break;
        }
    }
}

NOUS_Multithreading::NOUS_Thread* NOUS_Multithreading::CreateThread(const std::string& name, NOUS_Multithreading::ThreadState initialState)
{
    NOUS_Multithreading::NOUS_Thread* newThread = NOUS_NEW<NOUS_Multithreading::NOUS_Thread>(MemoryManager::MemoryTag::APPLICATION);

    newThread->name = name;
    newThread->ID = NOUS_Multithreading::GetCurrentThreadID();
    newThread->state = ThreadState::READY;
    newThread->executionTime.Start();

    NOUS_Multithreading::registeredThreads.push_back(newThread);

    return newThread;
}

void NOUS_Multithreading::RegisterMainThread()
{
    NOUS_Thread* mainThread = NOUS_NEW<NOUS_Thread>(MemoryManager::MemoryTag::THREAD);

    mainThread->name = "Main Thread";
    mainThread->ID = NOUS_Multithreading::GetThreadID(std::this_thread::get_id());
    mainThread->state = ThreadState::RUNNING;
    mainThread->executionTime.Start();

    NOUS_Multithreading::registeredThreads.push_back(mainThread);
}

uint32 NOUS_Multithreading::GetThreadID(std::thread::id id)
{
    std::stringstream ss;
    ss << id;
    return static_cast<uint32>(std::stoul(ss.str()));
}

void NOUS_Multithreading::Initialize()
{
    // Create Thread Pool
}

void NOUS_Multithreading::Shutdown()
{
    for (auto& thread : registeredThreads)
    {
        NOUS_DELETE(thread, MemoryManager::MemoryTag::THREAD);
    }

    registeredThreads.clear();
    registeredThreads.shrink_to_fit();
}