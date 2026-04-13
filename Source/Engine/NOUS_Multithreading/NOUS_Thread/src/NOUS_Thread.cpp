#include "Engine/NOUS_Multithreading/NOUS_Thread/include/NOUS_Thread.h"

/// @brief NOUS_Thread constructor.
NOUS_Multithreading::NOUS_Thread::NOUS_Thread() :
	mThreadState(ThreadState::READY), mIsRunning(false),
    mCurrentJob(nullptr), mTimerRunning(false)
{

}

/// @brief NOUS_Thread destructor.
NOUS_Multithreading::NOUS_Thread::~NOUS_Thread() 
{ 
	if (mIsRunning) Join();
}

/// @brief Starts the thread with a given function.
/// @param func The function to execute in the thread.
void NOUS_Multithreading::NOUS_Thread::Start(const std::function<void()>& func)
{
	if (mIsRunning) return;

	mIsRunning = true;

	mThreadHandle = std::thread([this, func]
	{
		func();
		mIsRunning = false;
		});

	mThreadID = mThreadHandle.get_id();
	mThreadState.store(ThreadState::READY);
}

/// @brief Joins the thread if joinable.
void NOUS_Multithreading::NOUS_Thread::Join()
{
	if (mThreadHandle.joinable())
	{
		mThreadHandle.join();
		mIsRunning = false;
	}
}

/// @brief Setters and getters.

void NOUS_Multithreading::NOUS_Thread::SetName(const std::string& name) 
{ 
	mThreadName = name; 
}

const std::string& NOUS_Multithreading::NOUS_Thread::GetName() const 
{ 
	return mThreadName; 
}

void NOUS_Multithreading::NOUS_Thread::SetThreadState(const ThreadState state)
{ 
	mThreadState.store(state); 
}

NOUS_Multithreading::ThreadState NOUS_Multithreading::NOUS_Thread::GetThreadState() const
{ 
	return mThreadState.load(); 
}

void NOUS_Multithreading::NOUS_Thread::SetCurrentJob(NOUS_Job* job) 
{ 
	mCurrentJob = job; 
}

NOUS_Multithreading::NOUS_Job* NOUS_Multithreading::NOUS_Thread::GetCurrentJob() const 
{ 
	return mCurrentJob; 
}

bool NOUS_Multithreading::NOUS_Thread::IsRunning() const 
{ 
	return mIsRunning; 
}

std::thread::id NOUS_Multithreading::NOUS_Thread::GetID() const
{
	return mThreadID;
}

/// @brief Job execution time tracking.

void NOUS_Multithreading::NOUS_Thread::StartExecutionTimer() 
{
    mStartTime = std::chrono::steady_clock::now();
    mTimerRunning = true;
}

void NOUS_Multithreading::NOUS_Thread::StopExecutionTimer() 
{
    mEndTime = std::chrono::steady_clock::now();
    mTimerRunning = false;
}

double NOUS_Multithreading::NOUS_Thread::GetExecutionTimeMS() const 
{
    if (mTimerRunning)
    {
        return std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(std::chrono::steady_clock::now() - mStartTime).count();
    }
    return std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(mEndTime - mStartTime).count();
}

/// @brief Sets a std::thread::id to this NOUS_Thread.
/// @note Used mainly for registering main thread.
void NOUS_Multithreading::NOUS_Thread::SetThreadID(const std::thread::id id)
{
	mThreadID = id;
}

/// @brief Returns a uint32_t derived from the thread ID hash, for display and logging only.
uint32_t NOUS_Multithreading::NOUS_Thread::GetDisplayID(const std::thread::id id)
{
	return static_cast<uint32_t>(std::hash<std::thread::id>{}(id));
}

/// @return std::string representation of the passed thread state.
std::string NOUS_Multithreading::NOUS_Thread::GetStringFromState(const ThreadState& state)
{
	switch (state)
	{
		case ThreadState::READY:	return "READY";
		case ThreadState::RUNNING:	return "RUNNING";
		default:					return "UNKNOWN";
	}
}

/// @brief Sleep the current thread for an amount of time (ms).
void NOUS_Multithreading::NOUS_Thread::SleepMS(const uint32_t& ms)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
