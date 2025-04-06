// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FProjectSettings.h"
#include "Templates/SharedPointer.h"

/**
 * 프로젝트 설정 변경 콜백 델리게이트
 * 설정이 변경될 때마다 호출됨
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSettingsChanged, const FProjectSettings&);

/**
 * 프로젝트 설정 관리 클래스
 * 여러 서버 간에 설정을 동기화
 */
class MULTISERVERSYNC_API FSettingsManager
{
public:
    /** 생성자 */
    FSettingsManager();

    /** 소멸자 */
    ~FSettingsManager();

    /** 설정 관리자 초기화 */
    bool Initialize();

    /** 설정 관리자 종료 */
    void Shutdown();

    /** 현재 설정 가져오기 */
    const FProjectSettings& GetSettings() const;

    /** 새 설정 적용 (로컬에서만) */
    bool UpdateSettings(const FProjectSettings& NewSettings);

    /** 설정 변경 사항을 네트워크로 브로드캐스트 */
    bool BroadcastSettings();

    /** 네트워크에서 수신한 설정 처리 */
    bool ProcessReceivedSettings(const TArray<uint8>& SettingsData);

    /** 설정을 파일로 저장 */
    bool SaveSettingsToFile(const FString& FilePath);

    /** 파일에서 설정 로드 */
    bool LoadSettingsFromFile(const FString& FilePath);

    /** 설정 변경 이벤트에 콜백 등록 */
    FDelegateHandle RegisterOnSettingsChanged(const FOnSettingsChanged::FDelegate& Delegate);

    /** 설정 변경 콜백 등록 해제 */
    void UnregisterOnSettingsChanged(FDelegateHandle Handle);

private:
    /** 현재 프로젝트 설정 */
    FProjectSettings CurrentSettings;

    /** 설정 변경 이벤트 */
    FOnSettingsChanged OnSettingsChangedEvent;

    /** 마지막 설정 변경 시간 */
    double LastSettingsUpdateTime;

    /** 초기화 완료 여부 */
    bool bIsInitialized;

    /** 설정 변경 알림 */
    void NotifySettingsChanged();

    /** 설정 유효성 검사 */
    bool ValidateSettings(const FProjectSettings& Settings);
};