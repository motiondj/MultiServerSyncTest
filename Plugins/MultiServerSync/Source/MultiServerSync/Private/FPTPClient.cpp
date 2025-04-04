// FPTPClient.cpp
#include "FPTPClient.h"

FPTPClient::FPTPClient()
    : bIsMaster(false)
    , bIsInitialized(false)
    , bIsSynchronized(false)
    , TimeOffsetMicroseconds(0)
    , PathDelayMicroseconds(0)
    , EstimatedErrorMicroseconds(0)
    , LastSyncTime(0)
    , SyncSequenceNumber(0)
{
}

FPTPClient::~FPTPClient()
{
    if (bIsInitialized)
    {
        Shutdown();
    }
}

bool FPTPClient::Initialize()
{
    bIsInitialized = true;
    return true;
}

void FPTPClient::Shutdown()
{
    bIsInitialized = false;
}

void FPTPClient::SetMasterMode(bool bInIsMaster)
{
    bIsMaster = bInIsMaster;
}

bool FPTPClient::IsMasterMode() const
{
    return bIsMaster;
}

void FPTPClient::SendSyncMessage()
{
    // 기본 구현에서는 아무 작업도 수행하지 않음
}

void FPTPClient::ProcessMessage(const TArray<uint8>& Message)
{
    // 기본 구현에서는 아무 작업도 수행하지 않음
}

int64 FPTPClient::GetTimeOffsetMicroseconds() const
{
    return TimeOffsetMicroseconds;
}

int64 FPTPClient::GetPathDelayMicroseconds() const
{
    return PathDelayMicroseconds;
}

int64 FPTPClient::GetEstimatedErrorMicroseconds() const
{
    return EstimatedErrorMicroseconds;
}

bool FPTPClient::IsSynchronized() const
{
    return bIsSynchronized;
}

TArray<uint8> FPTPClient::CreatePTPMessage(EPTPMessageType Type)
{
    return TArray<uint8>();
}

FPTPClient::EPTPMessageType FPTPClient::ParsePTPMessageType(const TArray<uint8>& Message)
{
    return EPTPMessageType::Unknown;
}

void FPTPClient::ProcessSyncMessage(const TArray<uint8>& Message)
{
    // 기본 구현에서는 아무 작업도 수행하지 않음
}

void FPTPClient::ProcessDelayReqMessage(const TArray<uint8>& Message)
{
    // 기본 구현에서는 아무 작업도 수행하지 않음
}

void FPTPClient::ProcessDelayRespMessage(const TArray<uint8>& Message)
{
    // 기본 구현에서는 아무 작업도 수행하지 않음
}

void FPTPClient::ProcessFollowUpMessage(const TArray<uint8>& Message)
{
    // 기본 구현에서는 아무 작업도 수행하지 않음
}

int64 FPTPClient::GetTimestampMicroseconds() const
{
    return FDateTime::Now().GetTicks() / 10; // 100나노초 -> 마이크로초
}