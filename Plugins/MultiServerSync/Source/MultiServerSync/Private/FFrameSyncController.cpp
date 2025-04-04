#include "FFrameSyncController.h"
#include "FSyncLog.h"
#include "Engine/Engine.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"

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
    UE_LOG(LogMultiServerSync, Display, TEXT("Initializing Frame Sync Controller"));

    // 엔진 틱 및 렌더링 이벤트 후킹
    RegisterEngineCallbacks();

    bIsInitialized = true;
    UE_LOG(LogMultiServerSync, Display, TEXT("Frame Sync Controller initialized successfully"));

    return true;
}

void FFrameSyncController::Shutdown()
{
    UE_LOG(LogMultiServerSync, Display, TEXT("Shutting down Frame Sync Controller"));

    // 엔진 콜백 등록 해제
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
    TargetFrameRate = FMath::Max(1.0f, FramesPerSecond);
    UE_LOG(LogMultiServerSync, Display, TEXT("Target frame rate set to %.2f FPS"), TargetFrameRate);
}

void FFrameSyncController::SetMasterMode(bool bInIsMaster)
{
    bIsMaster = bInIsMaster;
    UE_LOG(LogMultiServerSync, Display, TEXT("Frame Sync Controller set to %s mode"),
        bIsMaster ? TEXT("master") : TEXT("slave"));
}

bool FFrameSyncController::IsMasterMode() const
{
    return bIsMaster;
}

void FFrameSyncController::HandleEngineTick(float DeltaTime)
{
    if (!bIsInitialized)
    {
        return;
    }

    // 마스터 모드
    if (bIsMaster)
    {
        // 마스터는 단순히 프레임 번호를 증가시키고 전파
        SyncedFrameNumber++;
        SendFrameSyncMessage();
    }
    else
    {
        // 슬레이브는 마스터의 프레임 번호를 따라 프레임 타이밍 조정
        UpdateFrameTiming();
    }

    // 초당 한 번 로깅 (로그 과도 생성 방지)
    static double LastLogTime = 0.0;
    double CurrentTime = FPlatformTime::Seconds();
    if (CurrentTime - LastLogTime >= 1.0)
    {
        UE_LOG(LogMultiServerSync, Verbose, TEXT("Frame sync status: frame=%lld, adjustment=%.2fms, sync=%s"),
            SyncedFrameNumber, FrameTimingAdjustmentMs, bIsSynchronized ? TEXT("true") : TEXT("false"));
        LastLogTime = CurrentTime;
    }
}

void FFrameSyncController::UpdateFrameTiming()
{
    // 프레임 타이밍 조정 계산
    // 실제 구현에서는 마스터의 프레임 번호와 로컬 프레임 번호 비교

    // 간단한 예시: 프레임 번호 차이 기반의 단순 조정
    static int64 LastFrameNumber = 0;
    static double LastFrameTime = 0.0;

    double CurrentTime = FPlatformTime::Seconds();
    double DeltaTime = CurrentTime - LastFrameTime;

    if (LastFrameTime > 0.0 && DeltaTime < 1.0) // 프레임 드롭 또는 큰 스파이크 방지
    {
        // 프레임 지연 조정
        // 주의: 실제 상황에서는 더 복잡한 알고리즘 필요
        const float MaxAdjustmentMs = 5.0f; // 최대 조정 (밀리초)
        const float AdjustmentRate = 0.1f;  // 조정률 (0.0-1.0)

        // 목표 프레임 간격 (밀리초)
        float TargetFrameIntervalMs = 1000.0f / TargetFrameRate;

        // 실제 프레임 간격 (밀리초)
        float ActualFrameIntervalMs = static_cast<float>(DeltaTime * 1000.0);

        // 차이 계산
        float DifferenceMs = ActualFrameIntervalMs - TargetFrameIntervalMs;

        // 점진적 조정 계산 (급격한 변화 방지)
        float NewAdjustment = FMath::Clamp(DifferenceMs * AdjustmentRate, -MaxAdjustmentMs, MaxAdjustmentMs);

        // 현재 조정과 새 조정의 가중 혼합
        FrameTimingAdjustmentMs = FrameTimingAdjustmentMs * 0.9f + NewAdjustment * 0.1f;
    }

    LastFrameNumber = SyncedFrameNumber;
    LastFrameTime = CurrentTime;
}

void FFrameSyncController::SendFrameSyncMessage()
{
    if (!bIsInitialized || !bIsMaster)
    {
        return;
    }

    // 프레임 동기화 메시지 생성 (프레임 번호 포함)
    TArray<uint8> FrameSyncData;
    FrameSyncData.SetNum(sizeof(int64));
    FMemory::Memcpy(FrameSyncData.GetData(), &SyncedFrameNumber, sizeof(int64));

    // 실제 구현에서는 NetworkManager를 통해 전송
    // if (NetworkManager.IsValid())
    // {
    //     NetworkManager->BroadcastMessage(FrameSyncData);
    // }

    UE_LOG(LogMultiServerSync, Verbose, TEXT("Sent frame sync message: frame=%lld"), SyncedFrameNumber);
}

void FFrameSyncController::ProcessFrameSyncMessage(const TArray<uint8>& Message)
{
    if (!bIsInitialized || bIsMaster) // 마스터는 동기화 메시지를 처리하지 않음
    {
        return;
    }

    // 메시지에서 프레임 번호 추출
    if (Message.Num() >= sizeof(int64))
    {
        int64 ReceivedFrameNumber = 0;
        FMemory::Memcpy(&ReceivedFrameNumber, Message.GetData(), sizeof(int64));

        // 수신된 프레임 번호가 현재보다 크면 업데이트
        if (ReceivedFrameNumber > SyncedFrameNumber)
        {
            // 프레임 점프가 너무 크면 동기화 문제 로깅
            if (ReceivedFrameNumber > SyncedFrameNumber + 10)
            {
                UE_LOG(LogMultiServerSync, Warning, TEXT("Large frame number jump: local=%lld, received=%lld"),
                    SyncedFrameNumber, ReceivedFrameNumber);
            }

            SyncedFrameNumber = ReceivedFrameNumber;
            bIsSynchronized = true;

            UE_LOG(LogMultiServerSync, Verbose, TEXT("Updated frame number from sync message: frame=%lld"),
                SyncedFrameNumber);
        }
    }
    else
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Received invalid frame sync message size: %d"), Message.Num());
    }
}

float FFrameSyncController::GetFrameTimingAdjustmentMs() const
{
    return FrameTimingAdjustmentMs;
}

void FFrameSyncController::RegisterEngineCallbacks()
{
    // 엔진 틱 델리게이트 등록 - FTSTicker 대신 FTicker 사용
    TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateRaw(this, &FFrameSyncController::HandleEngineTick));

    // PreRender 델리게이트는 GameEngine 또는 UEngine에서 직접 연결하는 대신
    // GEngine에서 OnBeginFrame 또는 유사한 델리게이트를 사용하거나,
    // 여기서는 단순히 별도 틱으로 처리

    UE_LOG(LogMultiServerSync, Display, TEXT("Engine callbacks registered"));
}

void FFrameSyncController::UnregisterEngineCallbacks()
{
    // 틱 델리게이트 등록 해제
    FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);

    // PreRender 델리게이트 사용하지 않음

    UE_LOG(LogMultiServerSync, Display, TEXT("Engine callbacks unregistered"));
}

void FFrameSyncController::HandlePreRender()
{
    if (!bIsInitialized)
    {
        return;
    }

    // 프레임 타이밍 조정이 필요한 경우
    if (!bIsMaster && FMath::Abs(FrameTimingAdjustmentMs) > 0.1f)
    {
        // 여기서 렌더링 타이밍을 조정할 수 있음
        // 실제 구현에서는 렌더링 파이프라인과 더 깊게 통합

        UE_LOG(LogMultiServerSync, VeryVerbose, TEXT("Pre-render timing adjustment: %.2f ms"), FrameTimingAdjustmentMs);

        // 조정이 필요한 경우 FPS 제한 조정 또는 스레드 슬립 사용 가능
        if (FrameTimingAdjustmentMs > 0.0f)
        {
            // 프레임을 지연시키는 경우 - 스레드 슬립 적용
            float SleepTimeMs = FMath::Min(FrameTimingAdjustmentMs, 5.0f); // 최대 5ms 슬립
            FPlatformProcess::Sleep(SleepTimeMs / 1000.0f);
        }
        else
        {
            // 프레임을 가속하는 경우 - 다음 프레임에서 처리
            // (실제로는 가속이 어렵고, 일반적으로 다음 프레임에서 덜 지연시키는 방식 사용)
        }
    }
}