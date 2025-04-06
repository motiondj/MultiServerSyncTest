// FSettingsManager.h
#pragma once

#include "CoreMinimal.h"
#include "FGlobalSettings.h"

/**
 * 설정 관리자 클래스
 * 프로젝트 전역 설정을 관리하고 네트워크를 통해 공유
 */
class MULTISERVERSYNC_API FSettingsManager
{
public:
    /** 생성자 */
    FSettingsManager();

    /** 소멸자 */
    ~FSettingsManager();

    /** 초기화 */
    bool Initialize();

    /** 종료 */
    void Shutdown();

    /** 현재 설정 가져오기 */
    const FGlobalSettings& GetSettings() const;

    /** 설정 업데이트 */
    bool UpdateSettings(const FGlobalSettings& NewSettings);

    /** 특정 설정 값 업데이트 */
    template<typename T>
    bool UpdateSettingValue(const FString& SettingName, const T& Value);

    /** 설정을 파일로 저장 */
    bool SaveSettingsToFile(const FString& FilePath = TEXT(""));

    /** 파일에서 설정 로드 */
    bool LoadSettingsFromFile(const FString& FilePath = TEXT(""));

    /** 설정을 네트워크 메시지로 직렬화 */
    TArray<uint8> SerializeSettings() const;

    /** 네트워크 메시지에서 설정 역직렬화 */
    bool DeserializeSettings(const TArray<uint8>& Data);

    /** 마스터 서버에 설정 요청 */
    void RequestSettingsFromMaster();

    /** 모든 서버에 설정 브로드캐스트 */
    void BroadcastSettings();

    /** 설정 변경 이벤트에 대한 콜백 등록 */
    void RegisterOnSettingsChangedCallback(TFunction<void(const FGlobalSettings&)> Callback);

    /** 원격 설정 수신 이벤트에 대한 콜백 등록 */
    void RegisterOnRemoteSettingsReceivedCallback(TFunction<void(const FGlobalSettings&, bool)> Callback);

    /** 설정이 마스터 서버에서 업데이트되었을 때 처리 */
    bool ProcessMasterSettingsUpdate(const TArray<uint8>& SettingsData, const FString& MasterServerId);

    /** 설정 업데이트 요청 처리 */
    bool ProcessSettingsUpdateRequest(const TArray<uint8>& SettingsData, const FString& RequesterId);

    /** 설정 요청에 대한 응답 처리 */
    bool ProcessSettingsResponse(const TArray<uint8>& SettingsData, const FString& ResponderId);

    /** 설정 동기화 상태 업데이트 - 주기적으로 호출됨 */
    void UpdateSettingsSyncStatus();

private:
    /** 현재 전역 설정 */
    FGlobalSettings CurrentSettings;

    /** 설정이 초기화되었는지 여부 */
    bool bIsInitialized;

    /** 마지막으로 저장된 설정 파일 경로 */
    FString LastSavedFilePath;

    /** 설정 변경 콜백 */
    TArray<TFunction<void(const FGlobalSettings&)>> OnSettingsChangedCallbacks;

    /** 원격 설정 수신 콜백 */
    TArray<TFunction<void(const FGlobalSettings&, bool)>> OnRemoteSettingsReceivedCallbacks;

    /** 설정 변경 이벤트 실행 */
    void TriggerSettingsChangedEvent();

    /** 현재 설정의 유효성 검사 */
    bool ValidateSettings(const FGlobalSettings& Settings) const;

    /** 설정 충돌 해결 */
    FGlobalSettings ResolveSettingsConflict(const FGlobalSettings& LocalSettings, const FGlobalSettings& RemoteSettings) const;

    /** 기본 설정 파일 경로 가져오기 */
    FString GetDefaultSettingsFilePath() const;

    /** 마지막 설정 업데이트 시퀀스 번호 */
    uint32 LastSettingsUpdateSequence;

    /** 마지막 설정 업데이트 시간 */
    double LastSettingsUpdateTime;

    /** 설정 업데이트 시도 횟수 */
    int32 SettingsUpdateAttempts;

    /** 설정 업데이트 상태 */
    bool bSettingsUpdatePending;

    /** 마지막 설정 브로드캐스트 시간 */
    double LastSettingsBroadcastTime;

    /** 설정 브로드캐스트 간격 (초) */
    double SettingsBroadcastInterval;
};