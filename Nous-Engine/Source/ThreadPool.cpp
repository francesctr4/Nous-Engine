#include "ThreadPool.h"

// Constructor.
ThreadPool::ThreadPool() : done(false)
{
	try
	{
		for (unsigned int i = 0; i < MAX_THREADS; ++i)
		{
			threads.push_back(std::jthread(&ThreadPool::WorkerThread, this));
		}
	}
	catch (...)
	{
		done = true;
		throw;
	}
}

// Destructor.
ThreadPool::~ThreadPool()
{
	done = true;
}

void ThreadPool::WorkerThread()
{
	while (!done)
	{
		std::function<void()> task;

		/*if (workQueue.try_dequeue(task))
		{
			task();
		}
		else
		{
			std::this_thread::yield();
		}*/
	}
}