#include "UnitTests.h"

int main(int argc, char** argv)
{
	NOUS_Multithreading::RegisterMainThread();

#if ENABLE_UNIT_TESTS == 1
	::testing::InitGoogleTest(&argc, argv);
	RUN_ALL_TESTS();
#endif // ENABLE_UNIT_TESTS

	NOUS_Multithreading::UnregisterMainThread();

	return 0;
}