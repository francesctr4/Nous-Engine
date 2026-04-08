#include "Engine/NOUS_Multithreading/NOUS_Job/include/NOUS_Job.h"

/// @brief NOUS_Job constructor.
NOUS_Multithreading::NOUS_Job::NOUS_Job(std::string name, std::function<void()> func) :
	mName(std::move(name)), mFunction(std::move(func))
{

}

/// @brief Executes the stored function inside the job.
void NOUS_Multithreading::NOUS_Job::Execute() const
{ 
	mFunction(); 
}

/// @return std::string with the NOUS_Job name identifier.
const std::string& NOUS_Multithreading::NOUS_Job::GetName() const 
{ 
	return mName; 
}
