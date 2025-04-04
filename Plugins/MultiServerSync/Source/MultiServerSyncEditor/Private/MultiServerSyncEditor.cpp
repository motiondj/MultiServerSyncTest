#include "MultiServerSyncEditor.h"
#include "MultiServerSync.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "LevelEditor.h"
#include "FSyncFrameworkManager.h"
#include "ModuleInterfaces.h"

#define LOCTEXT_NAMESPACE "FMultiServerSyncEditorModule"

void FMultiServerSyncEditorModule::StartupModule()
{
    // Register UI elements when the editor module loads
    RegisterMenus();

    UE_LOG(LogTemp, Log, TEXT("MultiServerSyncEditor: Module started"));
}

void FMultiServerSyncEditorModule::ShutdownModule()
{
    UE_LOG(LogTemp, Log, TEXT("MultiServerSyncEditor: Module stopped"));
}

void FMultiServerSyncEditorModule::RegisterMenus()
{
    // Register editor extensions when ready
    FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

    // Create a menu extender
    TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());

    // Add a menu extension
    MenuExtender->AddMenuExtension(
        "WindowLayout",
        EExtensionHook::After,
        nullptr,
        FMenuExtensionDelegate::CreateLambda([this](FMenuBuilder& Builder)
            {
                Builder.AddSubMenu(
                    LOCTEXT("MultiServerSyncMenu", "Multi-Server Sync"),
                    LOCTEXT("MultiServerSyncMenuTooltip", "Multi-Server Synchronization Framework utilities"),
                    FNewMenuDelegate::CreateLambda([this](FMenuBuilder& SubMenuBuilder)
                        {
                            SubMenuBuilder.AddMenuEntry(
                                LOCTEXT("ShowEnvironmentInfo", "Show Environment Info"),
                                LOCTEXT("ShowEnvironmentInfoTooltip", "Display detected environment information"),
                                FSlateIcon(),
                                FUIAction(FExecuteAction::CreateRaw(this, &FMultiServerSyncEditorModule::ShowEnvironmentInfo))
                            );

                            SubMenuBuilder.AddMenuEntry(
                                LOCTEXT("RunNetworkTest", "Run Network Test"),
                                LOCTEXT("RunNetworkTestTooltip", "Run a basic network connectivity test"),
                                FSlateIcon(),
                                FUIAction(FExecuteAction::CreateRaw(this, &FMultiServerSyncEditorModule::RunNetworkTest))
                            );
                        })
                );
            })
    );

    // Add our extender to the menu
    LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
}

void FMultiServerSyncEditorModule::ShowEnvironmentInfo()
{
    UE_LOG(LogTemp, Display, TEXT("=== Multi-Server Sync Environment Information ==="));

    // 프레임워크 매니저 가져오기
    FSyncFrameworkManager& Manager = FSyncFrameworkManager::Get();

    // 환경 감지기 가져오기
    TSharedPtr<IEnvironmentDetector> EnvironmentDetector = Manager.GetEnvironmentDetector();
    if (!EnvironmentDetector.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Environment detector not available"));
        return;
    }

    // 네트워크 인터페이스 정보 표시
    if (EnvironmentDetector->IsFeatureAvailable(TEXT("NetworkInterfaces")))
    {
        TMap<FString, FString> NetworkInfo = EnvironmentDetector->GetFeatureInfo(TEXT("NetworkInterfaces"));
        UE_LOG(LogTemp, Display, TEXT("=== Network Interfaces ==="));
        for (const auto& Pair : NetworkInfo)
        {
            UE_LOG(LogTemp, Display, TEXT("%s: %s"), *Pair.Key, *Pair.Value);
        }
    }

    // nDisplay 정보 표시
    if (EnvironmentDetector->IsFeatureAvailable(TEXT("nDisplay")))
    {
        TMap<FString, FString> NDisplayInfo = EnvironmentDetector->GetFeatureInfo(TEXT("nDisplay"));
        UE_LOG(LogTemp, Display, TEXT("=== nDisplay ==="));
        for (const auto& Pair : NDisplayInfo)
        {
            UE_LOG(LogTemp, Display, TEXT("%s: %s"), *Pair.Key, *Pair.Value);
        }
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("nDisplay: Not available"));
    }

    // 젠락 하드웨어 정보 표시
    if (EnvironmentDetector->IsFeatureAvailable(TEXT("GenlockHardware")))
    {
        TMap<FString, FString> GenlockInfo = EnvironmentDetector->GetFeatureInfo(TEXT("GenlockHardware"));
        UE_LOG(LogTemp, Display, TEXT("=== Genlock Hardware ==="));
        for (const auto& Pair : GenlockInfo)
        {
            UE_LOG(LogTemp, Display, TEXT("%s: %s"), *Pair.Key, *Pair.Value);
        }
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("Genlock Hardware: Not available"));
    }

    UE_LOG(LogTemp, Display, TEXT("=== End of Environment Information ==="));
}

void FMultiServerSyncEditorModule::RunNetworkTest()
{
    UE_LOG(LogTemp, Display, TEXT("=== Running Network Connectivity Test ==="));

    // 프레임워크 매니저 가져오기
    FSyncFrameworkManager& Manager = FSyncFrameworkManager::Get();

    // 네트워크 매니저 가져오기
    TSharedPtr<INetworkManager> NetworkManager = Manager.GetNetworkManager();
    if (!NetworkManager.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Network manager not available"));
        return;
    }

    // 메시지 핸들러 등록
    NetworkManager->RegisterMessageHandler([](const FString& SenderId, const TArray<uint8>& Message)
        {
            FString MessageStr;
            if (Message.Num() > 0)
            {
                MessageStr = FString((TCHAR*)Message.GetData(), Message.Num() / sizeof(TCHAR));
            }

            UE_LOG(LogTemp, Display, TEXT("Received message from %s: %s"), *SenderId, *MessageStr);
        });

    // 서버 탐색
    NetworkManager->DiscoverServers();

    // 테스트 메시지 송신
    FString TestMessage = TEXT("Hello from network test!");
    TArray<uint8> MessageData;
    MessageData.SetNum(TestMessage.Len() * sizeof(TCHAR));
    FMemory::Memcpy(MessageData.GetData(), *TestMessage, TestMessage.Len() * sizeof(TCHAR));

    // 5초 후에 메시지 브로드캐스트
    FTimerHandle BroadcastTimerHandle;
    GEditor->GetTimerManager()->SetTimer(BroadcastTimerHandle, [NetworkManager, MessageData]()
        {
            UE_LOG(LogTemp, Display, TEXT("Broadcasting test message..."));
            NetworkManager->BroadcastMessage(MessageData);
        }, 5.0f, false);

    UE_LOG(LogTemp, Display, TEXT("Network test started. Check log for results in a few seconds."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMultiServerSyncEditorModule, MultiServerSyncEditor)