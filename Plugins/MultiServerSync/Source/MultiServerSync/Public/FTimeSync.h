#pragma once

#include "CoreMinimal.h"
#include "ModuleInterfaces.h"

// Forward declarations
class FPTPClient;
class FSoftwarePLL;

/**
 * Time synchronization class that implements the ITimeSync interface
 * Manages time synchronization between multiple servers
 */
class FTimeSync : public ITimeSync
{
public:
    /** Constructor */
    FTimeSync();

    /** Destructor */
    virtual ~FTimeSync();

    // Begin ITimeSync interface
    virtual bool Initialize() override;
    virtual void Shutdown() override;
    virtual int64 GetSyncedTimeMicroseconds() override;
    virtual int64 GetEstimatedErrorMicroseconds() override;
    virtual bool IsSynchronized() override;
    virtual int64 GetTimeOffset() const override { return TimeOffsetMicroseconds; }
    virtual int32 GetSyncStatus() const override;
    virtual int64 GeneratePTPTimestamp() const override;
    // End ITimeSync interface

    /** Set master mode (true) or slave mode (false) */
    void SetMasterMode(bool bIsMaster);

    /** Check if operating in master mode */
    bool IsMasterMode() const;

    /** Process a received PTP message */
    void ProcessPTPMessage(const TArray<uint8>& Message);

    /** Get local time in microseconds */
    int64 GetLocalTimeMicroseconds() const;

    /** Get the offset between local and master time in microseconds */
    int64 GetTimeOffsetMicroseconds() const;

    /** Set sync interval in milliseconds */
    void SetSyncInterval(int32 IntervalMs);

    /** Get sync interval in milliseconds */
    int32 GetSyncInterval() const;

private:
    /** PTP client implementation */
    TUniquePtr<FPTPClient> PTPClient;

    /** Software PLL implementation */
    TUniquePtr<FSoftwarePLL> SoftwarePLL;

    /** Is the time sync system operating in master mode */
    bool bIsMaster;

    /** Is the time sync system initialized */
    bool bIsInitialized;

    /** Is the time currently synchronized */
    bool bIsSynchronized;

    /** Current time offset in microseconds */
    int64 TimeOffsetMicroseconds;

    /** Estimated synchronization error in microseconds */
    int64 EstimatedErrorMicroseconds;

    /** Last sync time */
    int64 LastSyncTime;

    /** Sync interval in milliseconds */
    int32 SyncIntervalMs;

    /** Last update time */
    int64 LastUpdateTime;

    /** Send a sync message if in master mode */
    void SendSyncMessage();

    /** Update time synchronization */
    void UpdateTimeSync();
};