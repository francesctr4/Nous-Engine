#pragma once

#include "Engine/EngineExport.h"

#include <functional>
#include <string>

namespace nous::engine::multithreading
{
	///////////////////////////////////////////////////////////////////////////
	/// @brief Represents an executable task with a name and function.
	///////////////////////////////////////////////////////////////////////////
	class NOUS_Job
	{
	public:

		/// @brief NOUS_Job constructor.
		NOUS_ENGINE_API NOUS_Job(std::string  name, std::function<void()> func);

		/// @brief Executes the stored function inside the job.
		NOUS_ENGINE_API void Execute() const;

		/// @return std::string with the NOUS_Job name identifier.
		[[nodiscard]] NOUS_ENGINE_API const std::string& GetName() const;

	private:

		std::string				mName;
		std::function<void()>	mFunction;

	};
}