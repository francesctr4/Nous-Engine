#include <gtest/gtest.h>

#include "Engine/NOUS_Multithreading/NOUS_Job/include/NOUS_Job.h"

#include <stdexcept>

using namespace NOUS_Multithreading;

// =====================================================
// Name and construction
// =====================================================

TEST(t_NOUS_Job, StoresName)
{
    NOUS_Job job("MyJob", [](){});
    EXPECT_EQ(job.GetName(), "MyJob");
}

TEST(t_NOUS_Job, StoresEmptyName)
{
    NOUS_Job job("", [](){});
    EXPECT_EQ(job.GetName(), "");
}

// =====================================================
// Execute
// =====================================================

TEST(t_NOUS_Job, ExecuteCallsFunction)
{
    bool called = false;
    NOUS_Job job("TestJob", [&called](){ called = true; });
    job.Execute();
    EXPECT_TRUE(called);
}

TEST(t_NOUS_Job, ExecuteRunsFunctionExactlyOnce)
{
    int count = 0;
    NOUS_Job job("CountJob", [&count](){ ++count; });
    job.Execute();
    EXPECT_EQ(count, 1);
}

TEST(t_NOUS_Job, ExecuteCalledMultipleTimesRunsMultipleTimes)
{
    int count = 0;
    NOUS_Job job("CountJob", [&count](){ ++count; });
    job.Execute();
    job.Execute();
    job.Execute();
    EXPECT_EQ(count, 3);
}

TEST(t_NOUS_Job, ExecutePropagatesException)
{
    NOUS_Job job("ThrowJob", [](){ throw std::runtime_error("boom"); });
    EXPECT_THROW(job.Execute(), std::runtime_error);
}

TEST(t_NOUS_Job, ExecuteWithNoOpFunctionDoesNotCrash)
{
    NOUS_Job job("NoOp", [](){});
    EXPECT_NO_THROW(job.Execute());
}
