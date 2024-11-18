#pragma once

#include <functional>
#include <string>
#include <vector>

#define BYPASS 2

class TestManager {
public:

    using FunctionPtr = std::function<bool()>;

    TestManager();
    ~TestManager();

    void RegisterTest(const std::string& name, FunctionPtr func);
    void RunTests();

private:

    struct Test
    {
        std::string name;
        FunctionPtr func;
    };

    std::vector<Test> tests;
};