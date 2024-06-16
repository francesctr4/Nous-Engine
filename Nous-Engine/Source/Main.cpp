#include <iostream>

#include "Logger.h"
#include "Asserts.h"

int main() {

	NOUS_FATAL("A test message: %f", 3.14F);
	NOUS_ERROR("A test message: %f", 3.14F);
	NOUS_WARN("A test message: %f", 3.14F);
	NOUS_INFO("A test message: %f", 6.14F);
	NOUS_DEBUG("A test message: %f", 3.14F);
	NOUS_TRACE("A test message: %f", 3.14F);

	//NOUS_ASSERT_MSG(1 == 0, "message");

	return 0;
}