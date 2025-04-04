#include "Misc/AutomationTest.h"
#include "ModuleInterfaces.h"
#include "FSyncFrameworkManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNetworkManagerBasicTest, "MultiServerSync.NetworkManager.Basic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FNetworkManagerBasicTest::RunTest(const FString& Parameters)
{
    // 프레임워크 매니저에서 네트워크 매니저 가져오기
    FSyncFrameworkManager& Manager = FSyncFrameworkManager::Get();
    TSharedPtr<INetworkManager> NetworkManager = Manager.GetNetworkManager();

    // 네트워크 매니저가 유효한지 확인
    TestTrue("Network manager is valid", NetworkManager.IsValid());

    // 메시지 핸들러 등록
    bool bMessageHandlerRegistered = true;
    try
    {
        NetworkManager->RegisterMessageHandler([](const FString& SenderId, const TArray<uint8>& Message)
            {
                UE_LOG(LogTemp, Display, TEXT("Test message handler registered"));
            });
    }
    catch (...)
    {
        bMessageHandlerRegistered = false;
    }
    TestTrue("Message handler registration", bMessageHandlerRegistered);

    // 브로드캐스트 테스트
    TArray<uint8> TestMessage;
    FString TestString = TEXT("Test Message");
    TestMessage.SetNum(TestString.Len() * sizeof(TCHAR));
    FMemory::Memcpy(TestMessage.GetData(), *TestString, TestString.Len() * sizeof(TCHAR));

    bool bBroadcastResult = NetworkManager->BroadcastMessage(TestMessage);
    TestTrue("Message broadcast", bBroadcastResult);

    // 서버 탐색 테스트
    bool bDiscoveryResult = NetworkManager->DiscoverServers();
    TestTrue("Server discovery initiated", bDiscoveryResult);

    return true;
}