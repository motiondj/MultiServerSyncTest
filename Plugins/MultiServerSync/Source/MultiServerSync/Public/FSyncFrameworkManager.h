// FSyncFrameworkManager.h
#pragma once

#include "CoreMinimal.h"
#include "ISyncFrameworkManager.h"
#include "FProjectSettings.h" // 전방 선언 대신 직접 헤더를 포함

/**
 * Implementation of the synchronization framework manager
 * Manages all subsystems of the Multi-Server Sync Framework
 */
class MULTISERVERSYNC_API FSyncFrameworkManager : public ISyncFrameworkManager
{
public:
    /** Constructor */
    FSyncFrameworkManager();

    /** Destructor */
    virtual ~FSyncFrameworkManager();

    /** Initialize the manager and all subsystems */
    bool Initialize();

    /** Shutdown the manager and all subsystems */
    void Shutdown();

    // Begin ISyncFrameworkManager interface
    virtual TSharedPtr<IEnvironmentDetector> GetEnvironmentDetector() const override;
    virtual TSharedPtr<INetworkManager> GetNetworkManager() const override;
    virtual TSharedPtr<ITimeSync> GetTimeSync() const override;
    virtual TSharedPtr<IFrameSyncController> GetFrameSyncController() const override;
    virtual TSharedPtr<FSettingsManager> GetSettingsManager() const override;
    // End ISyncFrameworkManager interface

private:
    /** Environment detector subsystem */
    TSharedPtr<IEnvironmentDetector> EnvironmentDetector;

    /** Network manager subsystem */
    TSharedPtr<INetworkManager> NetworkManager;

    /** Time synchronization subsystem */
    TSharedPtr<ITimeSync> TimeSync;

    /** Frame synchronization controller */
    TSharedPtr<IFrameSyncController> FrameSyncController;

    /** Settings manager subsystem */
    TSharedPtr<FSettingsManager> SettingsManager;

    /** Indicates if the manager has been initialized */
    bool bIsInitialized;

    /** 설정을 모든 모듈에 적용 */
    void ApplySettingsToModules(const FProjectSettings& Settings);

    /** 설정 변경 시 네트워크로 브로드캐스트 */
    void BroadcastSettingsToNetwork();

    /** 네트워크에서 받은 설정 처리 */
    void ProcessNetworkSettings(const TArray<uint8>& SettingsData);

    /** 설정 요청에 응답 */
    void RespondToSettingsRequest();

    /** 메시지 핸들러 설정 */
    void SetupMessageHandlers();
};