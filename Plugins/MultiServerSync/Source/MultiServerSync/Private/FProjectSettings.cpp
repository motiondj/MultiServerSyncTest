// Copyright Your Company. All Rights Reserved.

#include "FProjectSettings.h"
#include "FSyncLog.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"

FProjectSettings::FProjectSettings()
    : SettingsVersion(1)
    , ProjectName(TEXT("DefaultProject"))
    , bEnableMasterSlaveProtocol(true)
    , MasterElectionInterval(5.0f)
    , MasterAnnouncementInterval(2.0f)
    , bEnableTimeSync(true)
    , TimeSyncIntervalMs(100)
    , MaxTimeOffsetToleranceMs(10.0)
    , bEnableFrameSync(true)
    , TargetFrameRate(60.0f)
    , MaxFrameDelayTolerance(2)
    , NetworkPort(7000)
    , bEnableBroadcast(true)
    , PreferredNetworkInterface(TEXT("Default"))
{
}

void FProjectSettings::Serialize(FArchive& Ar)
{
    Ar << SettingsVersion;
    Ar << ProjectName;

    Ar << bEnableMasterSlaveProtocol;
    Ar << MasterElectionInterval;
    Ar << MasterAnnouncementInterval;

    Ar << bEnableTimeSync;
    Ar << TimeSyncIntervalMs;
    Ar << MaxTimeOffsetToleranceMs;

    Ar << bEnableFrameSync;
    Ar << TargetFrameRate;
    Ar << MaxFrameDelayTolerance;

    Ar << NetworkPort;
    Ar << bEnableBroadcast;
    Ar << PreferredNetworkInterface;
}

void FProjectSettings::Serialize(FStructuredArchive::FRecord Record)
{
    Record << SA_VALUE(TEXT("SettingsVersion"), SettingsVersion);
    Record << SA_VALUE(TEXT("ProjectName"), ProjectName);

    Record << SA_VALUE(TEXT("EnableMasterSlaveProtocol"), bEnableMasterSlaveProtocol);
    Record << SA_VALUE(TEXT("MasterElectionInterval"), MasterElectionInterval);
    Record << SA_VALUE(TEXT("MasterAnnouncementInterval"), MasterAnnouncementInterval);

    Record << SA_VALUE(TEXT("EnableTimeSync"), bEnableTimeSync);
    Record << SA_VALUE(TEXT("TimeSyncIntervalMs"), TimeSyncIntervalMs);
    Record << SA_VALUE(TEXT("MaxTimeOffsetToleranceMs"), MaxTimeOffsetToleranceMs);

    Record << SA_VALUE(TEXT("EnableFrameSync"), bEnableFrameSync);
    Record << SA_VALUE(TEXT("TargetFrameRate"), TargetFrameRate);
    Record << SA_VALUE(TEXT("MaxFrameDelayTolerance"), MaxFrameDelayTolerance);

    Record << SA_VALUE(TEXT("NetworkPort"), NetworkPort);
    Record << SA_VALUE(TEXT("EnableBroadcast"), bEnableBroadcast);
    Record << SA_VALUE(TEXT("PreferredNetworkInterface"), PreferredNetworkInterface);
}

TArray<uint8> FProjectSettings::ToBytes() const
{
    TArray<uint8> Result;
    FMemoryWriter Writer(Result);

    // 현재 객체의 복사본 생성 (const 메서드이므로)
    FProjectSettings Copy = *this;
    Copy.Serialize(Writer);

    return Result;
}

bool FProjectSettings::FromBytes(const TArray<uint8>& Bytes)
{
    if (Bytes.Num() == 0)
    {
        return false;
    }

    FMemoryReader Reader(Bytes);
    Serialize(Reader);

    return true;
}

bool FProjectSettings::operator==(const FProjectSettings& Other) const
{
    return SettingsVersion == Other.SettingsVersion
        && ProjectName == Other.ProjectName
        && bEnableMasterSlaveProtocol == Other.bEnableMasterSlaveProtocol
        && FMath::IsNearlyEqual(MasterElectionInterval, Other.MasterElectionInterval)
        && FMath::IsNearlyEqual(MasterAnnouncementInterval, Other.MasterAnnouncementInterval)
        && bEnableTimeSync == Other.bEnableTimeSync
        && TimeSyncIntervalMs == Other.TimeSyncIntervalMs
        && FMath::IsNearlyEqual(MaxTimeOffsetToleranceMs, Other.MaxTimeOffsetToleranceMs)
        && bEnableFrameSync == Other.bEnableFrameSync
        && FMath::IsNearlyEqual(TargetFrameRate, Other.TargetFrameRate)
        && MaxFrameDelayTolerance == Other.MaxFrameDelayTolerance
        && NetworkPort == Other.NetworkPort
        && bEnableBroadcast == Other.bEnableBroadcast
        && PreferredNetworkInterface == Other.PreferredNetworkInterface;
}

bool FProjectSettings::operator!=(const FProjectSettings& Other) const
{
    return !(*this == Other);
}

FString FProjectSettings::ToString() const
{
    return FString::Printf(TEXT("ProjectSettings [Version=%d, Project=%s, TimeSync=%s, FrameSync=%s, Port=%d]"),
        SettingsVersion,
        *ProjectName,
        bEnableTimeSync ? TEXT("Enabled") : TEXT("Disabled"),
        bEnableFrameSync ? TEXT("Enabled") : TEXT("Disabled"),
        NetworkPort);
}