#pragma once

#include <vector>
#include <thread>
#include <functional>

#include "concurrentqueue.h"

#define MAX_THREADS std::thread::hardware_concurrency()

template<typename T>
using TSQueue = moodycamel::ConcurrentQueue<T>;

// Lock-free FIFO queue

class ThreadPool
{
	std::atomic_bool done;
	TSQueue<std::function<void()>> workQueue;
	std::vector<std::jthread> threads;
	void worker_thread()
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
public:
	ThreadPool() : done(false)
	{
		try
		{
			for (unsigned int i = 0; i < MAX_THREADS; ++i)
			{
				threads.push_back(
					std::jthread(&ThreadPool::worker_thread, this));
			}
		}
		catch (...)
		{
			done = true;
			throw;
		}
	}
	~ThreadPool()
	{
		done = true;
	}
	template<typename FunctionType>
	void submit(FunctionType f)
	{
		workQueue.enqueue(std::function<void()>(f));
	}
};