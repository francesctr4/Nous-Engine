#ifndef NOUSJOB_H
#define NOUSJOB_H

#include <Engine/Core/Globals.h>
#include <Engine/Core/EngineExport.h>
#include <functional>

namespace NOUS_Multithreading
{
	///////////////////////////////////////////////////////////////////////////
	/// @brief Represents an executable task with a name and function.
	///////////////////////////////////////////////////////////////////////////
	class NOUS_Job
	{
	public:

		/// @brief NOUS_Job constructor.
		NOUS_ENGINE_API NOUS_Job(const std::string& name, std::function<void()> func);

		/// @brief Executes the stored function inside the job.
		NOUS_ENGINE_API void Execute();

		/// @return std::string with the NOUS_Job name identifier.
		NOUS_ENGINE_API const std::string& GetName() const;

	private:

		std::string				mName;
		std::function<void()>	mFunction;

	};
}

#endif // NOUSJOB_H