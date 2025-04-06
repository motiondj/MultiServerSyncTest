// FGlobalSettings.cpp
#include "FGlobalSettings.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformTime.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "FSyncLog.h"

FGlobalSettings::FGlobalSettings()
    : ProjectName(TEXT("UnrealProject"))
    , ProjectVersion(TEXT("1.0"))
    , ProjectId(FGuid::NewGuid())
    , SyncPort(7000)
    , BroadcastInterval(1.0f)
    , ConnectionTimeout(5)
    , SyncIntervalMs(100)
    , MaxTimeOffsetMs(50)
    , PGain(0.5f)
    , IGain(0.01f)
    , FilterWeight(0.5f)
    , TargetFrameRate(60.0f)
    , bForceFrameLock(false)
    , MaxFrameSkew(2)
    , MasterPriority(0.5f)
    , bCanBeMaster(true)
    , bForceMaster(false)
    , SettingsVersion(1)
    , LastUpdatedBy(TEXT(""))
    , LastUpdatedTimeMs(0)
{
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
    // 기본적인 유효성 검사
    if (ProjectName.IsEmpty() || !ProjectId.IsValid())
    {
        return false;
    }

    // 네트워크 설정 유효성 검사
    if (SyncPort <= 0 || SyncPort > 65535 || BroadcastInterval <= 0.0f)
    {
        return false;
    }

    // 시간 동기화 설정 유효성 검사
    if (SyncIntervalMs <= 0 || MaxTimeOffsetMs <= 0 ||
        PGain <= 0.0f || IGain <= 0.0f || FilterWeight <= 0.0f || FilterWeight >= 1.0f)
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
    // 핵심 설정 비교 (비교 성능을 위해 가장 자주 변경되는 설정부터 확인)
    if (SettingsVersion != Other.SettingsVersion ||
        ProjectId != Other.ProjectId ||
        SyncPort != Other.SyncPort ||
        SyncIntervalMs != Other.SyncIntervalMs ||
        TargetFrameRate != Other.TargetFrameRate ||
        bForceFrameLock != Other.bForceFrameLock ||
        MasterPriority != Other.MasterPriority ||
        bCanBeMaster != Other.bCanBeMaster ||
        bForceMaster != Other.bForceMaster)
    {
        return true;
    }

    // 덜 중요한 설정 비교
    if (ProjectName != Other.ProjectName ||
        ProjectVersion != Other.ProjectVersion ||
        BroadcastInterval != Other.BroadcastInterval ||
        ConnectionTimeout != Other.ConnectionTimeout ||
        MaxTimeOffsetMs != Other.MaxTimeOffsetMs ||
        PGain != Other.PGain ||
        IGain != Other.IGain ||
        FilterWeight != Other.FilterWeight ||
        MaxFrameSkew != Other.MaxFrameSkew)
    {
        return true;
    }

    return false;
}

FString FGlobalSettings::ToString() const
{
    FString Result = TEXT("--- Global Settings ---\n");
    Result += FString::Printf(TEXT("Project: %s (v%s)\n"), *ProjectName, *ProjectVersion);
    Result += FString::Printf(TEXT("ProjectId: %s\n"), *ProjectId.ToString());
    Result += FString::Printf(TEXT("Network: Port=%d, Interval=%.2fs, Timeout=%ds\n"),
        SyncPort, BroadcastInterval, ConnectionTimeout);
    Result += FString::Printf(TEXT("Time Sync: Interval=%dms, MaxOffset=%dms, PLL(P=%.2f, I=%.4f, F=%.2f)\n"),
        SyncIntervalMs, MaxTimeOffsetMs, PGain, IGain, FilterWeight);
    Result += FString::Printf(TEXT("Frame Sync: FPS=%.2f, ForceLock=%s, MaxSkew=%d\n"),
        TargetFrameRate, bForceFrameLock ? TEXT("true") : TEXT("false"), MaxFrameSkew);
    Result += FString::Printf(TEXT("Master-Slave: Priority=%.2f, CanBeMaster=%s, ForceMaster=%s\n"),
        MasterPriority, bCanBeMaster ? TEXT("true") : TEXT("false"), bForceMaster ? TEXT("true") : TEXT("false"));
    Result += FString::Printf(TEXT("Version: %d, LastUpdated: %s by %s\n"),
        SettingsVersion, *FDateTime::FromUnixTimestamp(LastUpdatedTimeMs / 1000).ToString(), *LastUpdatedBy);

    return Result;
}

uint32 FGlobalSettings::GetSettingsHash() const
{
    // 간단한 해시 계산 - 모든 중요 설정을 포함
    uint32 Hash = GetTypeHash(ProjectId);
    Hash = HashCombine(Hash, GetTypeHash(ProjectName));
    Hash = HashCombine(Hash, GetTypeHash(ProjectVersion));
    Hash = HashCombine(Hash, GetTypeHash(SyncPort));
    Hash = HashCombine(Hash, GetTypeHash(SyncIntervalMs));
    Hash = HashCombine(Hash, GetTypeHash(TargetFrameRate));
    Hash = HashCombine(Hash, GetTypeHash(bForceFrameLock));
    Hash = HashCombine(Hash, GetTypeHash(MasterPriority));
    Hash = HashCombine(Hash, GetTypeHash(bCanBeMaster));
    Hash = HashCombine(Hash, GetTypeHash(bForceMaster));
    Hash = HashCombine(Hash, GetTypeHash(SettingsVersion));

    return Hash;
}

FArchive& operator<<(FArchive& Ar, FGlobalSettings& Settings)
{
    // 비 const 참조로 변경
    Settings.Serialize(Ar);
    return Ar;
}