#include <gtest/gtest.h>

// =====================================================
// 🧩 Minimal Test Fixture Template
// =====================================================

// This fixture provides a clean setup/teardown structure
class t_ConsoleWindow : public ::testing::Test {
protected:
    // Called before each test
    void SetUp() override {
        // Initialize resources or objects here
        initialized = true;
    }

    // Called after each test
    void TearDown() override {
        // Clean up resources here
        initialized = false;
    }

    bool initialized = false;
};

// =====================================================
// 🧪 Example Tests
// =====================================================

// Sanity test to check fixture setup
TEST_F(t_ConsoleWindow, TEST) {
    EXPECT_TRUE(initialized);
}
