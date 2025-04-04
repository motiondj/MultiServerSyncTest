﻿// Plugins/MultiServerSync/Source/MultiServerSync/Private/FSyncFrameworkManager.cpp
#include "FSyncFrameworkManager.h"
#include "FSyncLog.h"
#include "FEnvironmentDetector.h"
#include "FNetworkManager.h"
#include "FTimeSync.h"
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