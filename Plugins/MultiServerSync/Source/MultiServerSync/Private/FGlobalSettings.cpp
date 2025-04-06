// FGlobalSettings.cpp
#include "FGlobalSettings.h"
#include "Serialization/Archive.h"
#include "Misc/Guid.h"

FGlobalSettings::FGlobalSettings()
    : SyncPort(7000)
    , BroadcastInterval(2.0f)
    , ConnectionTimeout(10)
    , SyncIntervalMs(100)
    , MaxTimeOffsetMs(5000)
    , PGain(0.5f)
    , IGain(0.01f)
    , FilterWeight(0.5f)
    , TargetFrameRate(60.0f)
    , bForceFrameLock(false)
    , MaxFrameSkew(1)
    , MasterPriority(0.5f)
    , bCanBeMaster(true)
    , bForceMaster(false)
    , SettingsVersion(1)
    , LastUpdatedTimeMs(0)
{
    ProjectId = FGuid::NewGuid();
    ProjectName = TEXT("DefaultProject");
    ProjectVersion = TEXT("1.0");
    LastUpdatedBy = TEXT("System");
}

void FGlobalSettings::Serialize(FArchive& Ar)
{
    Ar << ProjectName;
    Ar << ProjectVersion;
    Ar << ProjectId;

    Ar << SyncPort;
    Ar << BroadcastInterval;
    Ar << ConnectionTimeout;

    Ar << SyncIntervalMs;
    Ar << MaxTimeOffsetMs;
    Ar << PGain;
    Ar << IGain;
    Ar << FilterWeight;

    Ar << TargetFrameRate;
    Ar << bForceFrameLock;
    Ar << MaxFrameSkew;

    Ar << MasterPriority;
    Ar << bCanBeMaster;
    Ar << bForceMaster;

    Ar << SettingsVersion;
    Ar << LastUpdatedBy;
    Ar << LastUpdatedTimeMs;
}

bool FGlobalSettings::IsValid() const
{
    // 기본 유효성 검사
    if (ProjectName.IsEmpty() || ProjectVersion.IsEmpty() || !ProjectId.IsValid())
    {
        return false;
    }

    // 네트워크 설정 유효성 검사
    if (SyncPort < 1024 || SyncPort > 65535 || BroadcastInterval <= 0.0f || ConnectionTimeout <= 0)
    {
        return false;
    }

    // 시간 동기화 설정 유효성 검사
    if (SyncIntervalMs <= 0 || MaxTimeOffsetMs <= 0 || PGain <= 0.0f || IGain <= 0.0f || FilterWeight <= 0.0f || FilterWeight >= 1.0f)
    {
        return false;
    }

    // 프레임 동기화 설정 유효성 검사
    if (TargetFrameRate <= 0.0f || MaxFrameSkew < 0)
    {
        return false;
    }

    // 마스터-슬레이브 설정 유효성 검사
    if (MasterPriority < 0.0f || MasterPriority > 1.0f)
    {
        return false;
    }

    return true;
}

bool FGlobalSettings::IsDifferentFrom(const FGlobalSettings& Other) const
{
    // 핵심 설정 비교
    if (SyncPort != Other.SyncPort ||
        BroadcastInterval != Other.BroadcastInterval ||
        ConnectionTimeout != Other.ConnectionTimeout ||
        SyncIntervalMs != Other.SyncIntervalMs ||
        MaxTimeOffsetMs != Other.MaxTimeOffsetMs ||
        PGain != Other.PGain ||
        IGain != Other.IGain ||
        FilterWeight != Other.FilterWeight ||
        TargetFrameRate != Other.TargetFrameRate ||
        bForceFrameLock != Other.bForceFrameLock ||
        MaxFrameSkew != Other.MaxFrameSkew ||
        MasterPriority != Other.MasterPriority ||
        bCanBeMaster != Other.bCanBeMaster ||
        bForceMaster != Other.bForceMaster)
    {
        return true;
    }

    // 프로젝트 식별 정보 비교 - 프로젝트 ID는 동일해야 함
    if (ProjectId != Other.ProjectId)
    {
        return true;
    }

    // 버전이나 이름이 다른 경우는 무시함 (핵심 설정이 아님)

    return false;
}

FString FGlobalSettings::ToString() const
{
    return FString::Printf(TEXT("Project: %s (v%s), SyncPort: %d, TargetFPS: %.1f, MasterPriority: %.2f, SettingsVersion: %d"),
        *ProjectName, *ProjectVersion, SyncPort, TargetFrameRate, MasterPriority, SettingsVersion);
}

uint32 FGlobalSettings::GetSettingsHash() const
{
    // 간단한 해시 계산 - 실제 구현에서는 더 복잡한 해싱 알고리즘 사용 가능
    uint32 Hash = FCrc::MemCrc32(&SyncPort, sizeof(SyncPort));
    Hash = FCrc::StrCrc32(*ProjectName, Hash);
    Hash = FCrc::StrCrc32(*ProjectVersion, Hash);
    Hash = FCrc::MemCrc32(&SyncIntervalMs, sizeof(SyncIntervalMs), Hash);
    Hash = FCrc::MemCrc32(&TargetFrameRate, sizeof(TargetFrameRate), Hash);
    Hash = FCrc::MemCrc32(&MasterPriority, sizeof(MasterPriority), Hash);
    Hash = FCrc::MemCrc32(&SettingsVersion, sizeof(SettingsVersion), Hash);

    return Hash;
}

FArchive& operator<<(FArchive& Ar, FGlobalSettings& Settings)
{
    Settings.Serialize(Ar);
    return Ar;
}