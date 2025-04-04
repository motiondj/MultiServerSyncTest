// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

/**
 * Test framework for Multi-Server Sync Framework
 * Provides utilities for automated testing
 */
class FTestFramework
{
public:
    /** Initialize the test framework */
    static bool Initialize();

    /** Shutdown the test framework */
    static void Shutdown();

    /** Run all tests */
    static bool RunAllTests();

    /** Run a specific test */
    static bool RunTest(const FString& TestName);

    /** Get test results */
    static TArray<FString> GetTestResults();

    /** Log test progress */
    static void LogTestProgress(const FString& Message);

    /** Create a mock environment for testing */
    static void CreateMockEnvironment();

    /** Clean up the mock environment */
    static void CleanupMockEnvironment();

private:
    /** Test results */
    static TArray<FString> TestResults;

    /** Mock environment created flag */
    static bool bMockEnvironmentCreated;
};

/**
 * Base class for Multi-Server Sync Framework tests
 */
class FMultiServerSyncTest : public FAutomationTestBase
{
public:
    /** Constructor */
    FMultiServerSyncTest(const FString& InName, const bool bInComplexTask)
        : FAutomationTestBase(InName, bInComplexTask)
    {
    }

    /** Run the test */
    virtual bool RunTest(const FString& Parameters) override
    {
        FTestFramework::LogTestProgress(FString::Printf(TEXT("Running test: %s"), *GetTestName()));

        // Create mock environment
        FTestFramework::CreateMockEnvironment();

        // Run the actual test
        bool bResult = RunActualTest(Parameters);

        // Clean up mock environment
        FTestFramework::CleanupMockEnvironment();

        FTestFramework::LogTestProgress(FString::Printf(TEXT("Test %s: %s"),
            *GetTestName(), bResult ? TEXT("PASSED") : TEXT("FAILED")));

        return bResult;
    }

protected:
    /** The actual test implementation to be overridden by subclasses */
    virtual bool RunActualTest(const FString& Parameters) = 0;
};

/**
 * Macro to define a Multi-Server Sync Framework test
 */
#define IMPLEMENT_MULTISERVERSYNC_TEST(TestClass, TestName, bComplexTask) \
    class TestClass : public FMultiServerSyncTest \
    { \
    public: \
        TestClass() : FMultiServerSyncTest(TestName, bComplexTask) {} \
        virtual bool RunActualTest(const FString& Parameters) override; \
    }; \
    IMPLEMENT_CUSTOM_COMPLEX_AUTOMATION_TEST(TestClass, FMultiServerSyncTest, "MultiServerSync." TestName, bComplexTask) \
    bool TestClass::RunActualTest(const FString& Parameters)