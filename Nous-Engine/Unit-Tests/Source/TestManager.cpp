#include "TestManager.h"

#include "Logger.h"
#include "Timer.h"

TestManager::TestManager() 
{
    // Constructor, no need for memory management because std::vector handles it.
}

TestManager::~TestManager() 
{
    // Destructor, no need for manual cleanup as std::vector automatically cleans up.
}

void TestManager::RegisterTest(const std::string& name, FunctionPtr func) 
{
    tests.emplace_back(Test(name,func));
}

void TestManager::RunTests() 
{
    uint32 passed = 0;
    uint32 failed = 0;
    uint32 skipped = 0;
    uint32 count = static_cast<uint32>(tests.size());

    for (uint32 i = 0; i < count; ++i) 
    {
        bool result = tests[i].func();

        if (result == true) 
        {
            ++passed;
        }
        else if (result == BYPASS) 
        {
            NOUS_WARN("[SKIPPED]: %s", tests[i].name.c_str());
            ++skipped;
        }
        else 
        {
            NOUS_ERROR("[FAILED]: %s", tests[i].name.c_str());
            ++failed;
        }

        char status[20];
        //std::format(status, failed ? "*** %d FAILED ***" : "SUCCESS", failed);

        NOUS_INFO("Executed %d of %d (skipped %d) %s (%.6f sec / %.6f sec total)", i + 1, count, skipped, status);
    }

    NOUS_INFO("Results: %d passed, %d failed, %d skipped.", passed, failed, skipped);
}