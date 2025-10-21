#include <gtest/gtest.h>

// =====================================================
// 🧩 Minimal Test Fixture Template
// =====================================================

// This fixture provides a clean setup/teardown structure
// for any module test file (ModuleEditor, Renderer, etc.)
class ModuleEditorTest : public ::testing::Test {
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
TEST_F(ModuleEditorTest, FixtureInitializesCorrectly) {
    EXPECT_TRUE(initialized);
}

// Simple example logic test
TEST_F(ModuleEditorTest, ExampleLogic) {
    int a = 2;
    int b = 3;
    EXPECT_EQ(a + b, 5);
}