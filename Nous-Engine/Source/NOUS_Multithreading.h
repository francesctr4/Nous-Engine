#pragma once

#include "NOUS_Thread.h"

namespace NOUS_Multithreading
{
	/// @brief Global pointer to the main thread instance.
	/// @note Initialized by RegisterMainThread() and cleaned up by UnregisterMainThread().
	static NOUS_Thread* sMainThread = nullptr;

	/// @brief Initializes the main thread tracking.
	/// @note Must be paired with UnregisterMainThread() to prevent memory leaks.
	void RegisterMainThread();

	/// @brief Deletes the main thread instance if it exists.
	/// @note Should be called during application shutdown.
	void UnregisterMainThread();

	/// @brief Retrieves the main thread instance.
	/// @return Pointer to the main thread object, or nullptr if not registered.
	NOUS_Thread* GetMainThread();
}