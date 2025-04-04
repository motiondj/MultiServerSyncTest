#include "Misc/AutomationTest.h"
#include "ModuleInterfaces.h"
#include "FSyncFrameworkManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnvironmentDetectorBasicTest, "MultiServerSync.EnvironmentDetector.Basic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnvironmentDetectorBasicTest::RunTest(const FString& Parameters)
{
    // 프레임워크 매니저에서 환경 감지기 가져오기
    FSyncFrameworkManager& Manager = FSyncFrameworkManager::Get();
    TSharedPtr<IEnvironmentDetector> EnvironmentDetector = Manager.GetEnvironmentDetector();

    // 환경 감지기가 유효한지 확인
    TestTrue("Environment detector is valid", EnvironmentDetector.IsValid());

    // 기능 확인 테스트
    bool bNetworkInterfacesAvailable = EnvironmentDetector->IsFeatureAvailable(TEXT("NetworkInterfaces"));
    TestTrue("Network interfaces feature available", bNetworkInterfacesAvailable);

    // 기능 정보 테스트
    TMap<FString, FString> NetworkInfo = EnvironmentDetector->GetFeatureInfo(TEXT("NetworkInterfaces"));
    TestTrue("Network info not empty", NetworkInfo.Num() > 0);

    // nDisplay 감지 테스트
    bool bHasNDisplay = EnvironmentDetector->IsFeatureAvailable(TEXT("nDisplay"));
    // 이건 환경에 따라 다를 수 있으므로 결과를 확인만 함
    UE_LOG(LogTemp, Display, TEXT("nDisplay available: %s"), bHasNDisplay ? TEXT("Yes") : TEXT("No"));

    // 젠락 하드웨어 감지 테스트
    bool bHasGenlockHardware = EnvironmentDetector->IsFeatureAvailable(TEXT("GenlockHardware"));
    // 이건 환경에 따라 다를 수 있으므로 결과를 확인만 함
    UE_LOG(LogTemp, Display, TEXT("Genlock hardware available: %s"), bHasGenlockHardware ? TEXT("Yes") : TEXT("No"));

    return true;
}