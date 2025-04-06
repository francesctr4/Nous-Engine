#include "NOUS_Thread.h"

/// @brief NOUS_Thread constructor.
NOUS_Multithreading::NOUS_Thread::NOUS_Thread() :
	mThreadID(0), mIsRunning(false), mCurrentJob(nullptr), mThreadState(ThreadState::READY) 
{

}

/// @brief NOUS_Thread destructor.
NOUS_Multithreading::NOUS_Thread::~NOUS_Thread() 
{ 
	if (mIsRunning) Join(); 
}

/// @brief Starts the thread with a given function.
/// @param func The function to execute in the thread.
void NOUS_Multithreading::NOUS_Thread::Start(std::function<void()> func)
{
	if (mIsRunning) return;

	mIsRunning = true;

	mThreadHandle = std::thread([this, func]() {
		func();
		mIsRunning = false;
		});

	mThreadID = GetThreadID(mThreadHandle.get_id());
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

void NOUS_Multithreading::NOUS_Thread::SetThreadState(ThreadState state) 
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

uint32 NOUS_Multithreading::NOUS_Thread::GetID() const 
{ 
	return mThreadID; 
}

/// @brief Job execution time tracking.

void NOUS_Multithreading::NOUS_Thread::StartExecutionTimer() 
{ 
	mExecutionTime.Start(); 
}

void NOUS_Multithreading::NOUS_Thread::StopExecutionTimer() 
{ 
	mExecutionTime.Stop(); 
}

double NOUS_Multithreading::NOUS_Thread::GetExecutionTimeMS() const 
{ 
	return mExecutionTime.ReadMS(); 
}

/// @brief Sets a std::thread::id to this NOUS_Thread.
/// @note Used mainly for registering main thread.
void NOUS_Multithreading::NOUS_Thread::SetThreadID(std::thread::id id)
{
	std::stringstream ss;
	ss << id;
	mThreadID = static_cast<uint32>(std::stoul(ss.str()));
}

/// @brief Converts a std::thread::id to a numeric uint32.
/// @note Relies on string conversion; platform-dependent.
uint32 NOUS_Multithreading::NOUS_Thread::GetThreadID(std::thread::id id)
{
	std::stringstream ss;
	ss << id;
	return static_cast<uint32>(std::stoul(ss.str()));
}

/// @return std::string representation of the passed thread state.
const std::string NOUS_Multithreading::NOUS_Thread::GetStringFromState(const ThreadState& state)
{
	switch (state)
	{
		case ThreadState::READY:	return "READY";
		case ThreadState::RUNNING:	return "RUNNING";
		default:					return "UNKNOWN";
	}
}

/// @brief Sleep the current thread for an amount of time (ms).
const void NOUS_Multithreading::NOUS_Thread::SleepMS(const uint32& ms)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
