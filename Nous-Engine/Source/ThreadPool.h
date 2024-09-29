#pragma once

#include <vector>
#include <thread>
#include <functional>

#include "concurrentqueue.h"

// Adapt the thread pool size according to the hardware capabilities.
#define MAX_THREADS std::thread::hardware_concurrency()

// Lock-free thread-safe concurrent FIFO queue. Multiple producer, multiple consumer. Third Party.
template<typename T>
using TSQueue = moodycamel::ConcurrentQueue<T>;

class ThreadPool
{
public:

	// Constructor.
	ThreadPool() : done(false)
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
	~ThreadPool()
	{
		done = true;
	}

	// Submit a task to be handled by threads on the thread pool.
	template<typename T>
	void Submit(T task)
	{
		workQueue.enqueue(std::function<void()>(task));
	}

private:

	TSQueue<std::function<void()>> workQueue;

	std::vector<std::jthread> threads;
	std::atomic_bool done;

	void WorkerThread()
	{
		while (!done)
		{
			std::function<void()> task;

			if (workQueue.try_dequeue(task))
			{
				task();
			}
			else
			{
				std::this_thread::yield();
			}
		}
	}

};