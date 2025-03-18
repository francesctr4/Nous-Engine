#pragma once

#include <vector>
#include <thread>
#include <functional>

// Adapt the thread pool size according to the hardware capabilities.
#define MAX_THREADS std::thread::hardware_concurrency()

// Lock-free thread-safe concurrent FIFO queue. Multiple producer, multiple consumer. Third Party.
// TODO

class ThreadPool
{
public:

	// Constructor.
	ThreadPool();

	// Destructor.
	~ThreadPool();

	// Submit a task to be handled by threads on the thread pool.
	template<typename T>
	void Submit(T task);

private:

	void WorkerThread();

	// ---------------------------------------- \\

	//TSQueue<std::function<void()>> workQueue;

	std::vector<std::jthread> threads;
	std::atomic_bool done;

};

// Submit a task to be handled by threads on the thread pool.
template<typename T>
void ThreadPool::Submit(T task)
{
	//workQueue.enqueue(std::function<void()>(task));
}