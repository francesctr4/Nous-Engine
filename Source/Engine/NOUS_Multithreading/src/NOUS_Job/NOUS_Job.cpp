#include <NOUS_Multithreading/NOUS_Job.h>

/// @brief NOUS_Job constructor.
nous::engine::multithreading::NOUS_Job::NOUS_Job(std::string name, std::function<void()> func) :
	mName(std::move(name)), mFunction(std::move(func))
{

}

/// @brief Executes the stored function inside the job.
void nous::engine::multithreading::NOUS_Job::Execute() const
{ 
	mFunction(); 
}

/// @return std::string with the NOUS_Job name identifier.
const std::string& nous::engine::multithreading::NOUS_Job::GetName() const
{ 
	return mName; 
}
