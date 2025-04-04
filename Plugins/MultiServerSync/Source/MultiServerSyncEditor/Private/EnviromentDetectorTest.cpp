// EnviromentDetectorTest.cpp
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnvironmentDetectorDummyTest, "MultiServerSync.EnvironmentDetector.Dummy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnvironmentDetectorDummyTest::RunTest(const FString& Parameters)
{
    // 더미 테스트는 항상 성공
    return true;
}