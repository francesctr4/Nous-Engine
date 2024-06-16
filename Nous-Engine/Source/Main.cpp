#include <iostream>

#include "Logger.h"
#include "Asserts.h"

int main() {

	std::cout << "Holaaaaaaaaaaaa" << std::endl;

	NOUS_FATAL("A test message: %f", 3.14F);
	NOUS_ERROR("A test message: %f", 3.14F);
	NOUS_WARN("A test message: %f", 3.14F);
	NOUS_INFO("A test message: %f", 3.14F);
	NOUS_DEBUG("A test message: %f", 3.14F);
	NOUS_TRACE("A test message: %f", 3.14F);

	NOUS_ASSERT(1 == 0);

	return 0;
}