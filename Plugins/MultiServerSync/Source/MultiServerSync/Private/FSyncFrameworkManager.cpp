// FSyncFrameworkManager.cpp
#include "FSyncFrameworkManager.h"
#include "FSyncLog.h"
#include "FEnvironmentDetector.h"
#include "FNetworkManager.h"
#include "FTimeSync.h"
#include "FFrameSyncController.h"
#include "FSettingsManager.h"
#include "FProjectSettings.h"

FSyncFrameworkManager::FSyncFrameworkManager()
    : bIsInitialized(false)
{
    UE_LOG(LogMultiServerSync, Display, TEXT("FSyncFrameworkManager created"));
}

FSyncFrameworkManager::~FSyncFrameworkManager()
{
    if (bIsInitialized)
    {
        Shutdown();
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("FSyncFrameworkManager destroyed"));
}

bool FSyncFrameworkManager::Initialize()
{
    UE_LOG(LogMultiServerSync, Display, TEXT("Initializing FSyncFrameworkManager"));

    // Initialize logging system
    FSyncLog::Initialize();

    // Create environment detector
    EnvironmentDetector = MakeShared<FEnvironmentDetector>();
    if (!EnvironmentDetector->Initialize())
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to initialize EnvironmentDetector"));
        return false;
    }

    // Create settings manager - 새로 추가
    SettingsManager = MakeShared<FSettingsManager>();
    if (!SettingsManager->Initialize())
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to initialize SettingsManager"));
        return false;
    }

    // Create network manager
    NetworkManager = MakeShared<FNetworkManager>();
    if (!NetworkManager->Initialize())
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to initialize NetworkManager"));
        return false;
    }

    // Create time sync
    TimeSync = MakeShared<FTimeSync>();
    if (!TimeSync->Initialize())
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to initialize TimeSync"));
        return false;
    }

    // Create frame sync controller
    FrameSyncController = MakeShared<FFrameSyncController>();
    if (!FrameSyncController->Initialize())
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to initialize FrameSyncController"));
        return false;
    }

    // 모듈 간 연결 - 메시지 핸들러 설정
    // NetworkManager에 TimeSync 메시지 핸들러 등록
    if (NetworkManager.IsValid() && TimeSync.IsValid())
    {
        // 네트워크 매니저 구현체 가져오기
        FNetworkManager* NetworkManagerImpl = static_cast<FNetworkManager*>(NetworkManager.Get());
        // 시간 동기화 구현체 가져오기
        FTimeSync* TimeSyncImpl = static_cast<FTimeSync*>(TimeSync.Get());

        // 메시지 핸들러 등록
        NetworkManagerImpl->RegisterMessageHandler(
            [TimeSyncImpl](const FString& SenderId, const TArray<uint8>& Data)
            {
                // 시간 동기화 메시지만 처리
                TimeSyncImpl->ProcessPTPMessage(Data);
            }
        );
    }

    // 모듈 간 연결 - 메시지 핸들러 설정
    SetupMessageHandlers();

    bIsInitialized = true;
    UE_LOG(LogMultiServerSync, Display, TEXT("FSyncFrameworkManager initialized successfully"));

    return true;
}

void FSyncFrameworkManager::Shutdown()
{
    if (!bIsInitialized)
    {
        return;
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Shutting down FSyncFrameworkManager"));

    // Shutdown frame sync controller
    if (FrameSyncController.IsValid())
    {
        FrameSyncController->Shutdown();
        FrameSyncController.Reset();
    }

    // Shutdown time sync
    if (TimeSync.IsValid())
    {
        TimeSync->Shutdown();
        TimeSync.Reset();
    }

    // Shutdown network manager
    if (NetworkManager.IsValid())
    {
        NetworkManager->Shutdown();
        NetworkManager.Reset();
    }

    // Shutdown settings manager - 새로 추가
    if (SettingsManager.IsValid())
    {
        SettingsManager->Shutdown();
        SettingsManager.Reset();
    }

    // Shutdown environment detector
    if (EnvironmentDetector.IsValid())
    {
        EnvironmentDetector->Shutdown();
        EnvironmentDetector.Reset();
    }

    // Shutdown logging system
    FSyncLog::Shutdown();

    bIsInitialized = false;
    UE_LOG(LogMultiServerSync, Display, TEXT("FSyncFrameworkManager shutdown completed"));
}

TSharedPtr<IEnvironmentDetector> FSyncFrameworkManager::GetEnvironmentDetector() const
{
    return EnvironmentDetector;
}

TSharedPtr<INetworkManager> FSyncFrameworkManager::GetNetworkManager() const
{
    return NetworkManager;
}

TSharedPtr<ITimeSync> FSyncFrameworkManager::GetTimeSync() const
{
    return TimeSync;
}

TSharedPtr<IFrameSyncController> FSyncFrameworkManager::GetFrameSyncController() const
{
    return FrameSyncController;
}

// 새로 추가된 함수
TSharedPtr<FSettingsManager> FSyncFrameworkManager::GetSettingsManager() const
{
    return SettingsManager;
}

void FSyncFrameworkManager::SetupMessageHandlers()
{
    if (!NetworkManager.IsValid())
    {
        return;
    }

    // 네트워크 매니저 구현체 가져오기
    FNetworkManager* NetworkManagerImpl = static_cast<FNetworkManager*>(NetworkManager.Get());

    // 메시지 핸들러 등록
    NetworkManagerImpl->RegisterMessageHandler(
        [this](const FString& SenderId, const TArray<uint8>& Data)
        {
            // 데이터가 비어있으면 무시
            if (Data.Num() == 0)
            {
                return;
            }

            // 메시지 유형에 따라 처리
            uint8 FirstByte = Data[0];

            // 시간 동기화 메시지 처리
            if (TimeSync.IsValid() && FirstByte == static_cast<uint8>(ENetworkMessageType::TimeSync))
            {
                // 시간 동기화 모듈에 데이터 전달
                FTimeSync* TimeSyncImpl = static_cast<FTimeSync*>(TimeSync.Get());
                TimeSyncImpl->ProcessPTPMessage(Data);
            }
            // 설정 동기화 메시지 처리
            else if (SettingsManager.IsValid() && FirstByte == static_cast<uint8>(ENetworkMessageType::SettingsSync))
            {
                // 설정 데이터를 첫 바이트를 제외하고 전달
                TArray<uint8> SettingsData;
                if (Data.Num() > 1)
                {
                    SettingsData.Append(Data.GetData() + 1, Data.Num() - 1);
                }
                ProcessNetworkSettings(SettingsData);
            }
            // 설정 요청 메시지 처리
            else if (SettingsManager.IsValid() && FirstByte == static_cast<uint8>(ENetworkMessageType::SettingsRequest))
            {
                RespondToSettingsRequest();
            }
        }
    );

    // 설정 변경 이벤트 등록
    if (SettingsManager.IsValid())
    {
        SettingsManager->RegisterOnSettingsChanged(FOnSettingsChanged::FDelegate::CreateLambda(
            [this](const FProjectSettings& NewSettings)
            {
                // 설정 변경 시 다른 모듈에 적용
                ApplySettingsToModules(NewSettings);

                // 네트워크로 브로드캐스트 (마스터 노드일 때)
                if (NetworkManager.IsValid() && static_cast<FNetworkManager*>(NetworkManager.Get())->IsMaster())
                {
                    BroadcastSettingsToNetwork();
                }
            }
        ));
    }
}

// 설정을 모든 모듈에 적용
void FSyncFrameworkManager::ApplySettingsToModules(const FProjectSettings& Settings)
{
    if (!bIsInitialized)
    {
        return;
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Applying settings to all modules"));

    // NetworkManager 설정 적용
    if (NetworkManager.IsValid())
    {
        FNetworkManager* NetworkManagerImpl = static_cast<FNetworkManager*>(NetworkManager.Get());

        // 마스터 우선순위 설정
        if (Settings.bEnableMasterSlaveProtocol)
        {
            // 포트 설정은 재시작 필요 (여기서는 로그만 출력)
            if (NetworkManagerImpl->GetPort() != Settings.NetworkPort)
            {
                UE_LOG(LogMultiServerSync, Warning, TEXT("Network port change requires restart"));
            }
        }
    }

    // TimeSync 설정 적용
    if (TimeSync.IsValid())
    {
        FTimeSync* TimeSyncImpl = static_cast<FTimeSync*>(TimeSync.Get());

        if (Settings.bEnableTimeSync)
        {
            TimeSyncImpl->SetSyncInterval(Settings.TimeSyncIntervalMs);
        }
    }

    // FrameSyncController 설정 적용
    if (FrameSyncController.IsValid())
    {
        if (Settings.bEnableFrameSync)
        {
            FrameSyncController->SetTargetFrameRate(Settings.TargetFrameRate);
        }
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Settings applied to all modules"));
}

// 설정을 네트워크로 브로드캐스트하는 메서드 구현
void FSyncFrameworkManager::BroadcastSettingsToNetwork()
{
    if (!NetworkManager.IsValid() || !SettingsManager.IsValid())
    {
        return;
    }

    FNetworkManager* NetworkManagerImpl = static_cast<FNetworkManager*>(NetworkManager.Get());

    // 마스터 노드만 설정을 브로드캐스트
    if (!NetworkManagerImpl->IsMaster())
    {
        return;
    }

    // 설정을 바이트 배열로 직렬화
    TArray<uint8> SettingsData = SettingsManager->GetSettings().ToBytes();

    if (SettingsData.Num() > 0)
    {
        UE_LOG(LogMultiServerSync, Display, TEXT("Broadcasting settings to network (%d bytes)"), SettingsData.Num());
        NetworkManagerImpl->SendSettingsMessage(SettingsData);
    }
}

// 네트워크에서 받은 설정 처리 메서드 구현
void FSyncFrameworkManager::ProcessNetworkSettings(const TArray<uint8>& SettingsData)
{
    if (!SettingsManager.IsValid() || SettingsData.Num() == 0)
    {
        return;
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Processing received settings (%d bytes)"), SettingsData.Num());

    // 설정 처리
    SettingsManager->ProcessReceivedSettings(SettingsData);
}

// 설정 요청에 응답하는 메서드 구현
void FSyncFrameworkManager::RespondToSettingsRequest()
{
    if (!NetworkManager.IsValid() || !SettingsManager.IsValid())
    {
        return;
    }

    FNetworkManager* NetworkManagerImpl = static_cast<FNetworkManager*>(NetworkManager.Get());

    // 마스터 노드만 설정 요청에 응답
    if (!NetworkManagerImpl->IsMaster())
    {
        return;
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Responding to settings request"));

    // 설정을 바이트 배열로 직렬화
    TArray<uint8> SettingsData = SettingsManager->GetSettings().ToBytes();

    if (SettingsData.Num() > 0)
    {
        // 설정 데이터 전송
        NetworkManagerImpl->SendSettingsMessage(SettingsData);
    }
}