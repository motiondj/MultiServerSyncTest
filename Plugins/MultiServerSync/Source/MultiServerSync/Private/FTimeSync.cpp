#include "FTimeSync.h"
#include "FPTPClient.h"
#include "FSoftwarePLL.h"

FTimeSync::FTimeSync()
    : bIsMaster(false)
    , bIsInitialized(false)
    , bIsSynchronized(false)
    , TimeOffsetMicroseconds(0)
    , EstimatedErrorMicroseconds(0)
    , LastSyncTime(0)
    , SyncIntervalMs(100)
{
    // TUniquePtr 생성을 생성자 내에서 할당하도록 수정
    PTPClient = MakeUnique<FPTPClient>();
    SoftwarePLL = MakeUnique<FSoftwarePLL>();
}

FTimeSync::~FTimeSync()
{
    if (bIsInitialized)
    {
        Shutdown();
    }
}

bool FTimeSync::Initialize()
{
    bIsInitialized = true;
    return true;
}

void FTimeSync::Shutdown()
{
    bIsInitialized = false;
    bIsSynchronized = false;
}

int64 FTimeSync::GetSyncedTimeMicroseconds()
{
    return GetLocalTimeMicroseconds() + TimeOffsetMicroseconds;
}

int64 FTimeSync::GetEstimatedErrorMicroseconds()
{
    return EstimatedErrorMicroseconds;
}

bool FTimeSync::IsSynchronized()
{
    return bIsSynchronized;
}

void FTimeSync::SetMasterMode(bool bInIsMaster)
{
    bIsMaster = bInIsMaster;
}

bool FTimeSync::IsMasterMode() const
{
    return bIsMaster;
}

void FTimeSync::ProcessPTPMessage(const TArray<uint8>& Message)
{
    // 기본 구현에서는 아무 작업도 수행하지 않음
}

int64 FTimeSync::GetLocalTimeMicroseconds() const
{
    // 현재 시스템 시간을 마이크로초 단위로 반환
    return FDateTime::Now().GetTicks() / 10; // 100나노초 -> 마이크로초
}

int64 FTimeSync::GetTimeOffsetMicroseconds() const
{
    return TimeOffsetMicroseconds;
}

void FTimeSync::SendSyncMessage()
{
    // 기본 구현에서는 아무 작업도 수행하지 않음
}

void FTimeSync::UpdateTimeSync()
{
    // 기본 구현에서는 아무 작업도 수행하지 않음
}