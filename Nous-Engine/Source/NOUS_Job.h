#pragma once

#include "Globals.h"

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
		NOUS_Job(const std::string& name, std::function<void()> func);

		/// @brief Executes the stored function inside the job.
		void Execute();

		/// @return std::string with the NOUS_Job name identifier.
		const std::string& GetName() const;

	private:

		std::string				mName;
		std::function<void()>	mFunction;

	};
}