// Plugins/MultiServerSync/Source/MultiServerSync/Private/FSyncFrameworkManager.cpp
#include "FSyncFrameworkManager.h"
#include "FSyncLog.h"
#include "FEnvironmentDetector.h"
#include "FNetworkManager.h"
#include "FTimeSync.h"
#include "FSettingsManager.h"
#include "FFrameSyncController.h"

FSyncFrameworkManager::FSyncFrameworkManager()
    : bIsInitialized(false)
{
    MSYNC_LOG_INFO(TEXT("FSyncFrameworkManager created"));
}

FSyncFrameworkManager::~FSyncFrameworkManager()
{
    if (bIsInitialized)
    {
        Shutdown();
    }

    MSYNC_LOG_INFO(TEXT("FSyncFrameworkManager destroyed"));
}

bool FSyncFrameworkManager::Initialize()
{
    MSYNC_LOG_INFO(TEXT("Initializing FSyncFrameworkManager"));

    // Initialize logging system
    FSyncLog::Initialize();

    // Create environment detector
    EnvironmentDetector = MakeShared<FEnvironmentDetector>();
    if (!EnvironmentDetector->Initialize())
    {
        MSYNC_LOG_ERROR(TEXT("Failed to initialize EnvironmentDetector"));
        return false;
    }

    // Create network manager
    NetworkManager = MakeShared<FNetworkManager>();
    if (!NetworkManager->Initialize())
    {
        MSYNC_LOG_ERROR(TEXT("Failed to initialize NetworkManager"));
        return false;
    }

    // Create time sync
    TimeSync = MakeShared<FTimeSync>();
    if (!TimeSync->Initialize())
    {
        MSYNC_LOG_ERROR(TEXT("Failed to initialize TimeSync"));
        return false;
    }

    // Create frame sync controller
    FrameSyncController = MakeShared<FFrameSyncController>();
    if (!FrameSyncController->Initialize())
    {
        MSYNC_LOG_ERROR(TEXT("Failed to initialize FrameSyncController"));
        return false;
    }

    // Create settings manager
    SettingsManager = MakeShared<FSettingsManager>();
    if (!SettingsManager->Initialize())
    {
        MSYNC_LOG_ERROR(TEXT("Failed to initialize SettingsManager"));
        return false;
    }

    // 네트워크 관리자와 설정 관리자 간의 연결 구성
    if (NetworkManager.IsValid() && SettingsManager.IsValid())
    {
        // 네트워크 매니저 구현체 가져오기
        FNetworkManager* NetworkManagerImpl = static_cast<FNetworkManager*>(NetworkManager.Get());

        // 설정 메시지 핸들러 등록
        NetworkManagerImpl->RegisterSettingsMessageHandler(
            [this](const TArray<uint8>& Data, const FString& SenderId)
            {
                // 설정 데이터 처리
                if (SettingsManager.IsValid())
                {
                    // 설정 메시지 유형에 따라 다른 처리 가능
                    SettingsManager->DeserializeSettings(Data);
                }
            }
        );

        // 설정 변경 핸들러 등록
        SettingsManager->RegisterOnSettingsChangedCallback(
            [this](const FGlobalSettings& NewSettings)
            {
                // 설정이 변경되면 네트워크를 통해 다른 서버에 알림
                if (NetworkManager.IsValid())
                {
                    FNetworkManager* NetworkMgr = static_cast<FNetworkManager*>(NetworkManager.Get());
                    TArray<uint8> SettingsData = SettingsManager->SerializeSettings();
                    NetworkMgr->BroadcastSettingsMessage(SettingsData, ENetworkMessageType::SettingsUpdate);
                }
            }
        );
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

    bIsInitialized = true;
    MSYNC_LOG_INFO(TEXT("FSyncFrameworkManager initialized successfully"));

    return true;

    // 마스터 변경 핸들러 등록 - 마스터가 변경될 때 설정 관리 로직 실행
    if (NetworkManager.IsValid() && SettingsManager.IsValid())
    {
        FNetworkManager* NetworkManagerImpl = static_cast<FNetworkManager*>(NetworkManager.Get());

        // 마스터 변경 시 설정 동기화
        NetworkManagerImpl->RegisterMasterChangeHandler(
            [this](const FString& MasterId, bool bLocalIsMaster)
            {
                if (SettingsManager.IsValid())
                {
                    if (bLocalIsMaster)
                    {
                        // 로컬 서버가 마스터가 되면 설정 브로드캐스트
                        UE_LOG(LogMultiServerSync, Display, TEXT("Local server became master, broadcasting settings"));
                        SettingsManager->BroadcastSettings();
                    }
                    else if (!MasterId.IsEmpty())
                    {
                        // 다른 서버가 마스터가 되면 설정 요청
                        UE_LOG(LogMultiServerSync, Display, TEXT("New master detected (%s), requesting settings"), *MasterId);
                        SettingsManager->RequestSettingsFromMaster();
                    }
                }
            }
        );
    }

    // 틱 델리게이트 등록
    FTickerDelegate TickDelegate = FTickerDelegate::CreateRaw(this, &FSyncFrameworkManager::TickHandler);
    TickDelegateHandle = FTSTicker::GetCoreTicker().AddTicker(TickDelegate, 1.0f); // 1초마다 틱

    // Shutdown 메서드에 다음 코드 추가
    // 틱 델리게이트 등록 해제
    FTSTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);

    // TickHandler 구현
    bool FSyncFrameworkManager::TickHandler(float DeltaTime)
    {
        // 설정 동기화 상태 업데이트
        if (SettingsManager.IsValid())
        {
            SettingsManager->UpdateSettingsSyncStatus();
        }

        return true; // 계속 틱 수신
    }

    // 설정 변경 이벤트 핸들러를 등록하여 모듈 간 설정 동기화
    if (SettingsManager.IsValid())
    {
        SettingsManager->RegisterOnSettingsChangedCallback(
            [this](const FGlobalSettings& NewSettings)
            {
                // TimeSync 설정 업데이트
                if (TimeSync.IsValid())
                {
                    FTimeSync* TimeSyncImpl = static_cast<FTimeSync*>(TimeSync.Get());
                    TimeSyncImpl->SetSyncInterval(NewSettings.SyncIntervalMs);

                    // PLL 매개변수 설정 - TimeSync를 통해 SoftwarePLL에 접근
                    // TimeSyncImpl->ConfigurePLL(NewSettings.PGain, NewSettings.IGain, NewSettings.FilterWeight);
                }

                // FrameSyncController 설정 업데이트
                if (FrameSyncController.IsValid())
                {
                    FrameSyncController->SetTargetFrameRate(NewSettings.TargetFrameRate);
                }

                // NetworkManager 설정 업데이트
                if (NetworkManager.IsValid())
                {
                    FNetworkManager* NetworkManagerImpl = static_cast<FNetworkManager*>(NetworkManager.Get());
                    NetworkManagerImpl->SetMasterPriority(NewSettings.MasterPriority);

                    // 마스터 모드 강제 설정
                    if (NewSettings.bForceMaster && !NetworkManagerImpl->IsMaster())
                    {
                        NetworkManagerImpl->StartMasterElection();
                    }
                }
            }
        );

        // 초기 설정 적용
        const FGlobalSettings& InitialSettings = SettingsManager->GetSettings();
        if (TimeSync.IsValid())
        {
            FTimeSync* TimeSyncImpl = static_cast<FTimeSync*>(TimeSync.Get());
            TimeSyncImpl->SetSyncInterval(InitialSettings.SyncIntervalMs);
        }

        if (FrameSyncController.IsValid())
        {
            FrameSyncController->SetTargetFrameRate(InitialSettings.TargetFrameRate);
        }

        if (NetworkManager.IsValid())
        {
            FNetworkManager* NetworkManagerImpl = static_cast<FNetworkManager*>(NetworkManager.Get());
            NetworkManagerImpl->SetMasterPriority(InitialSettings.MasterPriority);
        }
    }
}

void FSyncFrameworkManager::Shutdown()
{
    if (!bIsInitialized)
    {
        return;
    }

    MSYNC_LOG_INFO(TEXT("Shutting down FSyncFrameworkManager"));

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

    // Shutdown settings manager
    if (SettingsManager.IsValid())
    {
        SettingsManager->Shutdown();
        SettingsManager.Reset();
    }

    // GetSettingsManager 메서드 구현 추가
    TSharedPtr<FSettingsManager> FSyncFrameworkManager::GetSettingsManager() const
    {
        return SettingsManager;
    }

    // Shutdown network manager
    if (NetworkManager.IsValid())
    {
        NetworkManager->Shutdown();
        NetworkManager.Reset();
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
    MSYNC_LOG_INFO(TEXT("FSyncFrameworkManager shutdown completed"));
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