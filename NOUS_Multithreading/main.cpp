#include "NOUS_Multithreading.h"

void WorkerTask() 
{
	std::cout << NOUS_Multithreading::GetCurrentThreadID() << std::endl;
}

int main() 
{
	NOUS_Multithreading::NOUS_Thread* worker = NOUS_Multithreading::CreateThread("Worker", NOUS_Multithreading::ThreadState::READY);
	NOUS_Multithreading::StartThread(worker, WorkerTask);

	return 0;
}