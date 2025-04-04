// FSoftwarePLL.cpp
#include "FSoftwarePLL.h"

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
    bIsInitialized = true;
    return true;
}

void FSoftwarePLL::Shutdown()
{
    bIsInitialized = false;
}

void FSoftwarePLL::UpdateWithMeasurement(int64 OffsetMicroseconds, int64 TimestampMicroseconds)
{
    // 기본 구현에서는 아무 작업도 수행하지 않음
}

int64 FSoftwarePLL::GetAdjustedTimeMicroseconds() const
{
    return 0;
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
    P_Gain = InProportionalGain;
    I_Gain = InIntegralGain;
    FilterWeight = InFilterWeight;
}

double FSoftwarePLL::ApplyFilter(double NewValue, double OldValue)
{
    return (FilterWeight * NewValue) + ((1.0 - FilterWeight) * OldValue);
}

void FSoftwarePLL::CalculateFrequencyAdjustment(int64 OffsetMicroseconds)
{
    // 기본 구현에서는 아무 작업도 수행하지 않음
}

void FSoftwarePLL::CalculatePhaseAdjustment(int64 OffsetMicroseconds)
{
    // 기본 구현에서는 아무 작업도 수행하지 않음
}

void FSoftwarePLL::UpdateLockState(int64 OffsetMicroseconds)
{
    // 기본 구현에서는 아무 작업도 수행하지 않음
}