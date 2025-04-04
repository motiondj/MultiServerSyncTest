#include "FFrameSyncController.h"

FFrameSyncController::FFrameSyncController()
    : SyncedFrameNumber(0)
    , TargetFrameRate(60.0f)
    , FrameTimingAdjustmentMs(0.0f)
    , bIsMaster(false)
    , bIsInitialized(false)
    , bIsSynchronized(false)
{
}

FFrameSyncController::~FFrameSyncController()
{
    if (bIsInitialized)
    {
        Shutdown();
    }
}

bool FFrameSyncController::Initialize()
{
    bIsInitialized = true;
    return true;
}

void FFrameSyncController::Shutdown()
{
    UnregisterEngineCallbacks();
    bIsInitialized = false;
    bIsSynchronized = false;
}

int64 FFrameSyncController::GetSyncedFrameNumber()
{
    return SyncedFrameNumber;
}

bool FFrameSyncController::IsSynchronized()
{
    return bIsSynchronized;
}

void FFrameSyncController::SetTargetFrameRate(float FramesPerSecond)
{
    TargetFrameRate = FramesPerSecond;
}

void FFrameSyncController::SetMasterMode(bool bInIsMaster)
{
    bIsMaster = bInIsMaster;
}

bool FFrameSyncController::IsMasterMode() const
{
    return bIsMaster;
}

void FFrameSyncController::HandleEngineTick(float DeltaTime)
{
    // 기본 구현에서는 프레임 번호만 증가
    SyncedFrameNumber++;
}

void FFrameSyncController::UpdateFrameTiming()
{
    // 기본 구현에서는 아무 작업도 수행하지 않음
}

void FFrameSyncController::SendFrameSyncMessage()
{
    // 기본 구현에서는 아무 작업도 수행하지 않음
}

void FFrameSyncController::ProcessFrameSyncMessage(const TArray<uint8>& Message)
{
    // 기본 구현에서는 아무 작업도 수행하지 않음
}

float FFrameSyncController::GetFrameTimingAdjustmentMs() const
{
    return FrameTimingAdjustmentMs;
}

void FFrameSyncController::RegisterEngineCallbacks()
{
    // 실제 구현에서는 엔진 델리게이트 등록
}

void FFrameSyncController::UnregisterEngineCallbacks()
{
    // 실제 구현에서는 엔진 델리게이트 등록 해제
}

void FFrameSyncController::HandlePreRender()
{
    // 기본 구현에서는 아무 작업도 수행하지 않음
}