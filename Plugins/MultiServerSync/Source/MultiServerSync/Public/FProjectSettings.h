// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/StructuredArchive.h"

/**
 * 다중 서버 간에 공유되는 프로젝트 설정 구조체
 * 모든 서버가 일관된 설정으로 작동하도록 함
 */
struct MULTISERVERSYNC_API FProjectSettings
{
    /** 고유 설정 ID (변경 시마다 증가) */
    int32 SettingsVersion;

    /** 프로젝트 이름 */
    FString ProjectName;

    /** 마스터-슬레이브 설정 */
    bool bEnableMasterSlaveProtocol;
    float MasterElectionInterval;
    float MasterAnnouncementInterval;

    /** 시간 동기화 설정 */
    bool bEnableTimeSync;
    int32 TimeSyncIntervalMs;
    double MaxTimeOffsetToleranceMs;

    /** 프레임 동기화 설정 */
    bool bEnableFrameSync;
    float TargetFrameRate;
    int32 MaxFrameDelayTolerance;

    /** 네트워크 설정 */
    int32 NetworkPort;
    bool bEnableBroadcast;
    FString PreferredNetworkInterface;

    /** 기본 생성자 - 기본값 설정 */
    FProjectSettings();

    /** 직렬화 함수 (설정을 저장/전송 가능한 형태로 변환) */
    void Serialize(FArchive& Ar);

    /** 직렬화 함수 (구조화된 아카이브 방식) */
    void Serialize(FStructuredArchive::FRecord Record);

    /** 바이트 배열로 직렬화 */
    TArray<uint8> ToBytes() const;

    /** 바이트 배열에서 역직렬화 */
    bool FromBytes(const TArray<uint8>& Bytes);

    /** 두 설정이 동일한지 비교 */
    bool operator==(const FProjectSettings& Other) const;

    /** 두 설정이 다른지 비교 */
    bool operator!=(const FProjectSettings& Other) const;

    /** 설정의 문자열 표현 생성 */
    FString ToString() const;
};