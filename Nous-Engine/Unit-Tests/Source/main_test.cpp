#include "TestManager.h"
#include "LinearAllocator_Test.h"
#include "Logger.h"

int main() 
{
    // Always initalize the test manager first.
    TestManager testManager;

    // TODO: add test registrations here.
    LinearAllocatorRegisterTests(&testManager);

    NOUS_DEBUG("Starting tests...");

    // Run all tests
    testManager.RunTests();

    return 0;
}