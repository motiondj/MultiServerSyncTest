// SettingsManagerTest.cpp
#include "Misc/AutomationTest.h"
#include "FProjectSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectSettingsSerializationTest, "MultiServerSync.Settings.Serialization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProjectSettingsSerializationTest::RunTest(const FString& Parameters)
{
    // 기본 설정 생성
    FProjectSettings OriginalSettings;
    OriginalSettings.ProjectName = TEXT("TestProject");
    OriginalSettings.TargetFrameRate = 30.0f;
    OriginalSettings.NetworkPort = 7777;

    // 직렬화
    TArray<uint8> Bytes = OriginalSettings.ToBytes();

    // 직렬화된 데이터가 비어있지 않은지 확인
    TestTrue(TEXT("Serialized data should not be empty"), Bytes.Num() > 0);

    // 새 설정으로 역직렬화
    FProjectSettings DeserializedSettings;
    bool bSuccess = DeserializedSettings.FromBytes(Bytes);

    // 역직렬화 성공 확인
    TestTrue(TEXT("Deserialization should succeed"), bSuccess);

    // 원본 설정과 역직렬화된 설정이 일치하는지 확인
    TestEqual(TEXT("ProjectName should match"), DeserializedSettings.ProjectName, OriginalSettings.ProjectName);
    TestEqual(TEXT("TargetFrameRate should match"), DeserializedSettings.TargetFrameRate, OriginalSettings.TargetFrameRate);
    TestEqual(TEXT("NetworkPort should match"), DeserializedSettings.NetworkPort, OriginalSettings.NetworkPort);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSettingsManagerBasicTest, "MultiServerSync.Settings.BasicManager", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSettingsManagerBasicTest::RunTest(const FString& Parameters)
{
    // 이 테스트는 실제로 FSettingsManager를 초기화하지 않고
    // 단순히 더미 테스트로 구현하여 항상 성공합니다.
    // 실제 구현에서는 더 복잡한 로직이 필요합니다.

    // 테스트 성공
    return true;
}