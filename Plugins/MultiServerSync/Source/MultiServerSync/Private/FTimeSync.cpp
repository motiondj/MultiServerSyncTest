// FTimeSync.cpp
#include "FTimeSync.h"
#include "FPTPClient.h"
#include "FSoftwarePLL.h"
#include "FSyncLog.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformTime.h"

FTimeSync::FTimeSync()
    : bIsMaster(false)
    , bIsInitialized(false)
    , bIsSynchronized(false)
    , TimeOffsetMicroseconds(0)
    , EstimatedErrorMicroseconds(0)
    , LastSyncTime(0)
    , SyncIntervalMs(100)
    , LastUpdateTime(0)
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
    UE_LOG(LogMultiServerSync, Display, TEXT("Initializing Time Sync system"));

    // PTP 클라이언트 초기화
    if (!PTPClient->Initialize())
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to initialize PTP client"));
        return false;
    }

    // 소프트웨어 PLL 초기화
    if (!SoftwarePLL->Initialize())
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to initialize Software PLL"));
        return false;
    }

    // PLL 매개변수 설정 (P 게인, I 게인, 필터 가중치)
    SoftwarePLL->Configure(0.5, 0.01, 0.5);

    LastSyncTime = GetLocalTimeMicroseconds();
    LastUpdateTime = LastSyncTime;

    bIsInitialized = true;
    UE_LOG(LogMultiServerSync, Display, TEXT("Time Sync system initialized successfully"));

    return true;
}

void FTimeSync::Shutdown()
{
    UE_LOG(LogMultiServerSync, Display, TEXT("Shutting down Time Sync system"));

    if (PTPClient.IsValid())
    {
        PTPClient->Shutdown();
    }

    if (SoftwarePLL.IsValid())
    {
        SoftwarePLL->Shutdown();
    }

    bIsInitialized = false;
    bIsSynchronized = false;
}

int64 FTimeSync::GetSyncedTimeMicroseconds()
{
    if (!bIsInitialized)
    {
        return GetLocalTimeMicroseconds();
    }

    // 마스터 모드에서는 로컬 시간 반환
    if (bIsMaster)
    {
        return GetLocalTimeMicroseconds();
    }

    // 슬레이브 모드에서는 PTP로 계산된 오프셋 적용
    return GetLocalTimeMicroseconds() + TimeOffsetMicroseconds;
}

// FTimeSync.cpp (계속)
int64 FTimeSync::GetEstimatedErrorMicroseconds()
{
    if (!bIsInitialized || !bIsSynchronized)
    {
        return 1000000; // 1초 (동기화되지 않은 경우 큰 오차 반환)
    }

    return EstimatedErrorMicroseconds;
}

bool FTimeSync::IsSynchronized()
{
    if (!bIsInitialized)
    {
        return false;
    }

    // 마스터는 항상 동기화된 상태로 간주
    if (bIsMaster)
    {
        return true;
    }

    // PTP 클라이언트의 동기화 상태 반환
    return PTPClient->IsSynchronized();
}

void FTimeSync::SetMasterMode(bool bInIsMaster)
{
    if (bIsMaster == bInIsMaster)
    {
        return; // 이미 같은 모드
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Time Sync system changing to %s mode"),
        bInIsMaster ? TEXT("master") : TEXT("slave"));

    bIsMaster = bInIsMaster;

    // PTP 클라이언트에 모드 설정
    if (PTPClient.IsValid())
    {
        PTPClient->SetMasterMode(bIsMaster);
    }
}

bool FTimeSync::IsMasterMode() const
{
    return bIsMaster;
}

void FTimeSync::ProcessPTPMessage(const TArray<uint8>& Message)
{
    if (!bIsInitialized || !PTPClient.IsValid())
    {
        return;
    }

    // PTP 클라이언트에 메시지 전달
    PTPClient->ProcessMessage(Message);

    // PTP 상태 업데이트
    UpdateTimeSync();
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
    if (!bIsInitialized || !PTPClient.IsValid())
    {
        return;
    }

    // PTP 클라이언트의 Sync 메시지 생성 및 전송
    PTPClient->SendSyncMessage();

    // NetworkManager를 통해 Sync 메시지를 전송
    // 이 부분은 FSyncFrameworkManager를 통해 NetworkManager에 접근해야 함
    // 아직 구현되지 않은 부분
}

// FTimeSync.cpp - UpdateTimeSync 함수 수정
void FTimeSync::UpdateTimeSync()
{
    if (!bIsInitialized || !PTPClient.IsValid())
    {
        return;
    }

    int64 CurrentTime = GetLocalTimeMicroseconds();

    // 마지막 업데이트 이후 충분한 시간이 지났는지 확인 (10ms)
    if (CurrentTime - LastUpdateTime < 10000)
    {
        return;
    }

    LastUpdateTime = CurrentTime;

    // 마스터 모드
    if (bIsMaster)
    {
        // 마스터에서는 주기적으로 Sync 메시지 전송
        if (CurrentTime - LastSyncTime >= SyncIntervalMs * 1000)
        {
            SendSyncMessage();
            LastSyncTime = CurrentTime;
        }
    }
    // 슬레이브 모드
    else
    {
        // PTP 클라이언트에서 시간 오프셋 및 오차 정보 가져오기
        TimeOffsetMicroseconds = PTPClient->GetTimeOffsetMicroseconds();
        EstimatedErrorMicroseconds = PTPClient->GetEstimatedErrorMicroseconds();
        bIsSynchronized = PTPClient->IsSynchronized();

        // PLL에 측정값 업데이트 (PTP에서 계산된 시간 오프셋 사용)
        if (SoftwarePLL.IsValid() && bIsSynchronized)
        {
            SoftwarePLL->UpdateWithMeasurement(TimeOffsetMicroseconds, CurrentTime);

            // PLL의 위상 조정값을 시간 오프셋에 반영
            if (SoftwarePLL->IsLocked())
            {
                // PLL이 안정화되면 PLL의 오프셋을 사용
                TimeOffsetMicroseconds = SoftwarePLL->GetPhaseAdjustment();

                // 오차 추정값도 업데이트
                EstimatedErrorMicroseconds = FMath::Min(
                    EstimatedErrorMicroseconds,
                    FMath::Abs(SoftwarePLL->GetEstimatedErrorMicroseconds()));
            }
        }

        // 디버깅 정보 주기적 로깅 (1초마다)
        if (CurrentTime - LastSyncTime >= 1000000)
        {
            UE_LOG(LogMultiServerSync, Verbose, TEXT("Time Sync Status: offset=%lld us, error=%lld us, sync=%s, pll_locked=%s"),
                TimeOffsetMicroseconds, EstimatedErrorMicroseconds,
                bIsSynchronized ? TEXT("true") : TEXT("false"),
                (SoftwarePLL.IsValid() && SoftwarePLL->IsLocked()) ? TEXT("true") : TEXT("false"));
            LastSyncTime = CurrentTime;
        }
    }

    // PTP 클라이언트 업데이트
    PTPClient->Update();
}

void FTimeSync::SetSyncInterval(int32 IntervalMs)
{
    SyncIntervalMs = FMath::Max(10, IntervalMs); // 최소 10ms

    if (PTPClient.IsValid())
    {
        PTPClient->SetSyncInterval(SyncIntervalMs / 1000.0);
    }
}

int32 FTimeSync::GetSyncInterval() const
{
    return SyncIntervalMs;
}

int32 FTimeSync::GetSyncStatus() const
{
    if (!bIsInitialized)
        return 0; // 비동기화

    if (bIsMaster)
        return 2; // 마스터는 항상 동기화됨

    if (bIsSynchronized) {
        // 오프셋 크기에 따라 상태 판단
        if (FMath::Abs(TimeOffsetMicroseconds) < 10000) // 10ms 이내
            return 2; // 동기화됨
        else
            return 1; // 동기화 중
    }

    return 0; // 비동기화
}

// GeneratePTPTimestamp 함수 추가
int64 FTimeSync::GeneratePTPTimestamp() const
{
    return GetLocalTimeMicroseconds();
}