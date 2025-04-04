// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Software Phase-Locked Loop (PLL) implementation
 * Provides continuous fine-grained timing adjustments based on time measurements
 */
class FSoftwarePLL
{
public:
    /** Constructor */
    FSoftwarePLL();

    /** Destructor */
    ~FSoftwarePLL();

    /** Initialize the software PLL */
    bool Initialize();

    /** Shutdown the software PLL */
    void Shutdown();

    /** Update the PLL with a new time offset measurement */
    void UpdateWithMeasurement(int64 OffsetMicroseconds, int64 TimestampMicroseconds);

    /** Get the current adjusted time in microseconds */
    int64 GetAdjustedTimeMicroseconds() const;

    /** Get the current frequency adjustment ratio (1.0 means no adjustment) */
    double GetFrequencyAdjustment() const;

    /** Get the current phase adjustment in microseconds */
    int64 GetPhaseAdjustment() const;

    /** Check if the PLL is locked (stable) */
    bool IsLocked() const;

    /** Get the estimated error in microseconds */
    int64 GetEstimatedErrorMicroseconds() const;

    /** Configure the PLL parameters */
    void Configure(double ProportionalGain, double IntegralGain, double FilterWeight);

private:
    /** Proportional gain for PLL */
    double P_Gain;

    /** Integral gain for PLL */
    double I_Gain;

    /** Exponential filter weight */
    double FilterWeight;

    /** Current frequency adjustment ratio */
    double FrequencyAdjustment;

    /** Current phase adjustment in microseconds */
    int64 PhaseAdjustment;

    /** Integrated error value for I term */
    double IntegratedError;

    /** Filtered offset value */
    double FilteredOffset;

    /** Last measured offset */
    int64 LastOffset;

    /** Last update timestamp */
    int64 LastUpdateTime;

    /** PLL lock state */
    bool bIsLocked;

    /** Lock stability counter */
    int32 StabilityCounter;

    /** Lock threshold in microseconds */
    int64 LockThresholdMicroseconds;

    /** PLL initialization state */
    bool bIsInitialized;

    /** Apply filtering to measurements */
    double ApplyFilter(double NewValue, double OldValue);

    /** Calculate frequency adjustment based on current measurements */
    void CalculateFrequencyAdjustment(int64 OffsetMicroseconds);

    /** Calculate phase adjustment based on current measurements */
    void CalculatePhaseAdjustment(int64 OffsetMicroseconds);

    /** Update lock state based on current error */
    void UpdateLockState(int64 OffsetMicroseconds);
};