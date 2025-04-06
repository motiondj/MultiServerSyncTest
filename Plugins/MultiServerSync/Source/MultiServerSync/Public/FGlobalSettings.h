// FGlobalSettings.h
#pragma once

#include "CoreMinimal.h"
#include "Serialization/Archive.h"

/**
 * 프로젝트 전역 설정 구조체
 * 다중 서버 환경에서 공유되는 모든 설정 정보를 포함
 */
struct MULTISERVERSYNC_API FGlobalSettings
{
    // 프로젝트 식별 정보
    FString ProjectName;
    FString ProjectVersion;
    FGuid ProjectId;

    // 네트워크 설정
    int32 SyncPort;
    float BroadcastInterval;
    int32 ConnectionTimeout;

    // 시간 동기화 설정
    int32 SyncIntervalMs;
    int32 MaxTimeOffsetMs;
    float PGain;
    float IGain;
    float FilterWeight;

    // 프레임 동기화 설정
    float TargetFrameRate;
    bool bForceFrameLock;
    int32 MaxFrameSkew;

    // 마스터-슬레이브 설정
    float MasterPriority;
    bool bCanBeMaster;
    bool bForceMaster;

    // 설정 버전 (업데이트 감지용)
    int32 SettingsVersion;

    // 마지막 업데이트 시간 및 업데이트한 서버
    FString LastUpdatedBy;
    int64 LastUpdatedTimeMs;

    // 직렬화/역직렬화 메서드
    void Serialize(FArchive& Ar);

    // 기본 생성자
    FGlobalSettings();

    // 설정 유효성 검사
    bool IsValid() const;

    // 두 설정 간 차이점 확인
    bool IsDifferentFrom(const FGlobalSettings& Other) const;

    // 설정을 문자열로 변환 (디버깅용)
    FString ToString() const;

    // 설정 해시 생성 (빠른 비교용)
    uint32 GetSettingsHash() const;

    // 직렬화 연산자 오버로드를 친구 함수로 선언
    friend FArchive& operator<<(FArchive& Ar, FGlobalSettings& Settings);
};