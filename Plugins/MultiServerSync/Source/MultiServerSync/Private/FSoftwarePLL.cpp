// FSoftwarePLL.cpp
#include "FSoftwarePLL.h"
#include "FSyncLog.h"
#include "HAL/PlatformTime.h"
#include "Misc/DateTime.h"

FSoftwarePLL::FSoftwarePLL()
    : P_Gain(0.5)
    , I_Gain(0.01)
    , FilterWeight(0.5)
    , FrequencyAdjustment(1.0)
    , PhaseAdjustment(0)
    , IntegratedError(0.0)
    , FilteredOffset(0.0)
    , LastOffset(0)
    , LastUpdateTime(0)
    , bIsLocked(false)
    , StabilityCounter(0)
    , LockThresholdMicroseconds(1000)
    , bIsInitialized(false)
{
}

FSoftwarePLL::~FSoftwarePLL()
{
    if (bIsInitialized)
    {
        Shutdown();
    }
}

bool FSoftwarePLL::Initialize()
{
    UE_LOG(LogMultiServerSync, Display, TEXT("Initializing Software PLL"));

    // 초기 상태 설정
    FrequencyAdjustment = 1.0;
    PhaseAdjustment = 0;
    IntegratedError = 0.0;
    FilteredOffset = 0.0;
    LastOffset = 0;
    LastUpdateTime = FDateTime::Now().GetTicks() / 10; // 100나노초 -> 마이크로초
    bIsLocked = false;
    StabilityCounter = 0;

    bIsInitialized = true;
    return true;
}

void FSoftwarePLL::Shutdown()
{
    UE_LOG(LogMultiServerSync, Display, TEXT("Shutting down Software PLL"));
    bIsInitialized = false;
}

void FSoftwarePLL::UpdateWithMeasurement(int64 OffsetMicroseconds, int64 TimestampMicroseconds)
{
    if (!bIsInitialized)
    {
        return;
    }

    // 오프셋 값 로깅
    UE_LOG(LogMultiServerSync, Verbose, TEXT("PLL: Update with offset %lld us at timestamp %lld us"),
        OffsetMicroseconds, TimestampMicroseconds);

    // 초기 업데이트인 경우
    if (LastUpdateTime == 0)
    {
        LastUpdateTime = TimestampMicroseconds;
        LastOffset = OffsetMicroseconds;
        FilteredOffset = static_cast<double>(OffsetMicroseconds);

        // 초기 위상 조정을 오프셋 값의 반대로 설정
        PhaseAdjustment = -OffsetMicroseconds;

        UE_LOG(LogMultiServerSync, Display, TEXT("PLL: Initial phase adjustment set to %lld us"),
            PhaseAdjustment);
        return;
    }

    // 시간 간격 계산 (초 단위)
    double DeltaTimeSeconds = static_cast<double>(TimestampMicroseconds - LastUpdateTime) / 1000000.0;

    // 너무 오랜 시간이 지났거나 너무 짧은 경우 필터링
    if (DeltaTimeSeconds <= 0.001 || DeltaTimeSeconds > 5.0)
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("PLL: Invalid time delta: %.6f seconds"), DeltaTimeSeconds);
        LastUpdateTime = TimestampMicroseconds;
        return;
    }

    // 오프셋 필터링
    FilteredOffset = ApplyFilter(static_cast<double>(OffsetMicroseconds), FilteredOffset);

    // 주파수 및 위상 조정 계산
    CalculateFrequencyAdjustment(OffsetMicroseconds);
    CalculatePhaseAdjustment(OffsetMicroseconds);

    // 락 상태 업데이트
    UpdateLockState(OffsetMicroseconds);

    // 상태 저장
    LastOffset = OffsetMicroseconds;
    LastUpdateTime = TimestampMicroseconds;

    // 현재 PLL 상태 로깅
    UE_LOG(LogMultiServerSync, Verbose, TEXT("PLL: Status - freq_adj=%.9f, phase_adj=%lld us, locked=%s"),
        FrequencyAdjustment, PhaseAdjustment, bIsLocked ? TEXT("true") : TEXT("false"));
}

int64 FSoftwarePLL::GetAdjustedTimeMicroseconds() const
{
    if (!bIsInitialized)
    {
        // 현재 시스템 시간 반환 (마이크로초)
        return FDateTime::Now().GetTicks() / 10;
    }

    // 현재 시스템 시간 가져오기 (마이크로초)
    int64 CurrentTimeMicroseconds = FDateTime::Now().GetTicks() / 10;

    // 주파수 조정 적용 (주파수 조정은 시간의 흐름 속도를 변경)
    // 주파수 조정은 현재 시간에 바로 적용하기 어려우므로 여기서는 단순화

    // 위상 조정 적용 (위상 조정은 시간의 오프셋을 변경)
    return CurrentTimeMicroseconds + PhaseAdjustment;
}

double FSoftwarePLL::GetFrequencyAdjustment() const
{
    return FrequencyAdjustment;
}

int64 FSoftwarePLL::GetPhaseAdjustment() const
{
    return PhaseAdjustment;
}

bool FSoftwarePLL::IsLocked() const
{
    return bIsLocked;
}

int64 FSoftwarePLL::GetEstimatedErrorMicroseconds() const
{
    return LastOffset;
}

void FSoftwarePLL::Configure(double InProportionalGain, double InIntegralGain, double InFilterWeight)
{
    // 값 범위 제한
    P_Gain = FMath::Clamp(InProportionalGain, 0.001, 5.0);
    I_Gain = FMath::Clamp(InIntegralGain, 0.0001, 1.0);
    FilterWeight = FMath::Clamp(InFilterWeight, 0.001, 0.999);

    UE_LOG(LogMultiServerSync, Display, TEXT("PLL: Configured with P=%.3f, I=%.5f, Filter=%.3f"),
        P_Gain, I_Gain, FilterWeight);
}

double FSoftwarePLL::ApplyFilter(double NewValue, double OldValue)
{
    // 지수 가중 이동 평균 필터
    return FilterWeight * NewValue + (1.0 - FilterWeight) * OldValue;
}

void FSoftwarePLL::CalculateFrequencyAdjustment(int64 OffsetMicroseconds)
{
    // P항: 비례 제어 (현재 오프셋에 비례하여 조정)
    double P_Term = static_cast<double>(OffsetMicroseconds) * P_Gain * 0.0000001; // 스케일링 팩터 적용

    // I항: 적분 제어 (누적 오차에 기반하여 조정)
    IntegratedError += static_cast<double>(OffsetMicroseconds) * I_Gain * 0.0000001; // 스케일링 팩터 적용

    // 적분항 제한 (적분 와인드업 방지)
    IntegratedError = FMath::Clamp(IntegratedError, -0.1, 0.1);

    // 최종 주파수 조정값 계산
    double NewFrequencyAdjustment = 1.0 - (P_Term + IntegratedError);

    // 주파수 조정값 제한 (너무 급격한 변화 방지)
    NewFrequencyAdjustment = FMath::Clamp(NewFrequencyAdjustment, 0.9, 1.1);

    // 필터링된 조정값 적용
    FrequencyAdjustment = ApplyFilter(NewFrequencyAdjustment, FrequencyAdjustment);

    if (FMath::Abs(FrequencyAdjustment - 1.0) > 0.01)
    {
        UE_LOG(LogMultiServerSync, Verbose, TEXT("PLL: Frequency adjustment: %.9f (P=%.9f, I=%.9f)"),
            FrequencyAdjustment, P_Term, IntegratedError);
    }
}

void FSoftwarePLL::CalculatePhaseAdjustment(int64 OffsetMicroseconds)
{
    // 위상 조정은 오프셋의 반대 방향으로 설정
    int64 TargetPhaseAdjustment = -OffsetMicroseconds;

    // 현재 위상 조정에서 목표 위상 조정으로 점진적으로 이동
    // 급격한 변화를 방지하기 위해 현재 값의 90%와 목표 값의 10%를 혼합
    double NewPhaseAdjustment = PhaseAdjustment * 0.9 + TargetPhaseAdjustment * 0.1;

    // 정수로 변환
    PhaseAdjustment = static_cast<int64>(NewPhaseAdjustment);

    // 큰 변화가 있을 경우 로깅
    if (FMath::Abs(PhaseAdjustment - TargetPhaseAdjustment) > 1000)
    {
        UE_LOG(LogMultiServerSync, Verbose, TEXT("PLL: Phase adjustment: current=%lld, target=%lld"),
            PhaseAdjustment, TargetPhaseAdjustment);
    }
}

void FSoftwarePLL::UpdateLockState(int64 OffsetMicroseconds)
{
    // 오프셋의 절대값이 임계값보다 작으면 안정적인 것으로 간주
    if (FMath::Abs(OffsetMicroseconds) < LockThresholdMicroseconds)
    {
        StabilityCounter++;

        // 연속으로 10번 이상 안정적이면 락 상태로 전환
        if (StabilityCounter >= 10 && !bIsLocked)
        {
            bIsLocked = true;
            UE_LOG(LogMultiServerSync, Display, TEXT("PLL: Lock achieved (offset=%lld us)"),
                OffsetMicroseconds);
        }
    }
    else
    {
        StabilityCounter = 0;

        // 락 상태였다면 락 해제
        if (bIsLocked)
        {
            bIsLocked = false;
            UE_LOG(LogMultiServerSync, Display, TEXT("PLL: Lock lost (offset=%lld us)"),
                OffsetMicroseconds);
        }
    }
}