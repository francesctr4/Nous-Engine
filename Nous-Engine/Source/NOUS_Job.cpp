#include "NOUS_Job.h"

#ifdef TRACY_ENABLE
#include "Tracy.h"
#endif

/// @brief NOUS_Job constructor.
NOUS_Multithreading::NOUS_Job::NOUS_Job(const std::string& name, std::function<void()> func) :
	mName(name), mFunction(func) 
{

}

/// @brief Executes the stored function inside the job.
void NOUS_Multithreading::NOUS_Job::Execute() 
{ 
	mFunction(); 
}

/// @return std::string with the NOUS_Job name identifier.
const std::string& NOUS_Multithreading::NOUS_Job::GetName() const 
{ 
	return mName; 
}
