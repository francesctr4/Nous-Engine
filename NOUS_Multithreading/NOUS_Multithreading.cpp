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

void NOUS_Multithreading::RegisterThread(NOUS_Thread* thread)
{
    std::lock_guard<std::mutex> lock(sThreadsMutex);
    registeredThreads.emplace_back(thread);
}

void NOUS_Multithreading::UnregisterThread(uint32 threadID)
{
    std::lock_guard<std::mutex> lock(sThreadsMutex);

    // Find the thread with the matching threadID
    auto it = std::find_if(registeredThreads.begin(), registeredThreads.end(),
        [threadID](NOUS_Thread* thread) {
            return thread->ID == threadID; // Assuming NOUS_Thread has a GetID() function
        });

    // If found, erase it from the vector
    if (it != registeredThreads.end()) {
        registeredThreads.erase(it);
    }
}

NOUS_Multithreading::NOUS_Thread* NOUS_Multithreading::CreateThread(const std::string& name, ThreadState initialState)
{
    NOUS_Thread* newThread = NOUS_NEW<NOUS_Thread>(MemoryManager::MemoryTag::THREAD);

    newThread->name = name;
    newThread->ID = GetThreadID(newThread->handle.get_id());
    newThread->state = initialState;
    newThread->executionTime.Start();

    RegisterThread(newThread);

    return newThread;
}

void NOUS_Multithreading::StartThread(NOUS_Thread* thread, std::function<void()> task)
{
    if (!thread || thread->state != ThreadState::READY) 
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(sThreadsMutex);
        thread->task = task;
        thread->state = ThreadState::RUNNING;
    }

    thread->handle = std::thread([thread]() 
    {
        thread->executionTime.Start();

        if (thread->task) 
        {
            thread->task();
        }
            
        ChangeState(thread, ThreadState::READY);

        thread->executionTime.Stop();
    });
}

void NOUS_Multithreading::DestroyThread(NOUS_Thread*& thread)
{
    if (thread)
    {
        UnregisterThread(thread->ID);  // Remove from registry
        NOUS_DELETE<NOUS_Thread>(thread, MemoryManager::MemoryTag::THREAD); // Delete
        thread = nullptr; // Invalidate pointer
    }
}

void NOUS_Multithreading::RegisterMainThread()
{
    NOUS_Thread* mainThread = NOUS_NEW<NOUS_Thread>(MemoryManager::MemoryTag::THREAD);

    mainThread->name = "Main Thread";
    mainThread->ID = GetThreadID(std::this_thread::get_id());
    mainThread->state = ThreadState::RUNNING;
    mainThread->executionTime.Start();

    RegisterThread(mainThread);
}

uint32 NOUS_Multithreading::GetThreadID(std::thread::id id)
{
    std::stringstream ss;
    ss << id;
    return static_cast<uint32>(std::stoul(ss.str()));
}

NOUS_Multithreading::NOUS_Thread* NOUS_Multithreading::GetThreadHandle(uint32 threadID)
{
    return registeredThreads.at(threadID);
}

void NOUS_Multithreading::Initialize()
{
    RegisterMainThread();

    // Create Thread Pool
}

void NOUS_Multithreading::Shutdown()
{
    for (auto& thread : registeredThreads)
    {
        NOUS_DELETE(thread, MemoryManager::MemoryTag::THREAD);
    }

    registeredThreads.clear();
}