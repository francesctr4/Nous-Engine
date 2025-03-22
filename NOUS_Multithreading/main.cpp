#include "UnitTests.h"

int main(int argc, char** argv)
{
	NOUS_Multithreading::RegisterMainThread();

	::testing::InitGoogleTest(&argc, argv);
	RUN_ALL_TESTS();

	NOUS_Multithreading::UnregisterMainThread();

	return 0;
}