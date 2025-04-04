// FPTPClient.h
#pragma once

#include "CoreMinimal.h"

/**
 * PTP (Precision Time Protocol) client implementation
 * Based on IEEE 1588 standard for precise time synchronization
 */
class FPTPClient
{
public:
    /** Constructor */
    FPTPClient();

    /** Destructor */
    ~FPTPClient();

    /** Initialize the PTP client */
    bool Initialize();

    /** Shutdown the PTP client */
    void Shutdown();

    /** Set master mode (true) or slave mode (false) */
    void SetMasterMode(bool bIsMaster);

    /** Check if operating in master mode */
    bool IsMasterMode() const;

    /** Send a sync message (only in master mode) */
    void SendSyncMessage();

    /** Process a received PTP message */
    void ProcessMessage(const TArray<uint8>& Message);

    /** Get the current time offset in microseconds */
    int64 GetTimeOffsetMicroseconds() const;

    /** Get the current path delay in microseconds */
    int64 GetPathDelayMicroseconds() const;

    /** Get the current estimated error in microseconds */
    int64 GetEstimatedErrorMicroseconds() const;

    /** Check if time is currently synchronized */
    bool IsSynchronized() const;

    /** Get the current sync interval in seconds */
    double GetSyncInterval() const;

    /** Set the sync interval in seconds */
    void SetSyncInterval(double IntervalSeconds);

    /** Update the PTP client state (call periodically) */
    void Update();

private:
    /** PTP message types */
    enum class EPTPMessageType : uint8
    {
        Sync = 0,
        DelayReq = 1,
        FollowUp = 2,
        DelayResp = 3,
        Unknown = 255
    };

    /** Is the PTP client operating in master mode */
    bool bIsMaster;

    /** Is the PTP client initialized */
    bool bIsInitialized;

    /** Is the time currently synchronized */
    bool bIsSynchronized;

    /** Current time offset in microseconds */
    int64 TimeOffsetMicroseconds;

    /** Current path delay in microseconds */
    int64 PathDelayMicroseconds;

    /** Estimated synchronization error in microseconds */
    int64 EstimatedErrorMicroseconds;

    /** Last sync time */
    int64 LastSyncTime;

    /** Sync sequence number */
    uint16 SyncSequenceNumber;

    /** Sync interval in seconds */
    double SyncInterval;

    /** Last sync message timestamp */
    int64 LastSyncMessageTimestamp;

    /** Last delay request timestamp */
    int64 LastDelayReqTimestamp;

    /** PTP timestamps for delay calculation */
    int64 T1; // Sync 발신 시간 (마스터)
    int64 T2; // Sync 수신 시간 (슬레이브)
    int64 T3; // DelayReq 발신 시간 (슬레이브)
    int64 T4; // DelayReq 수신 시간 (마스터)

    /** Create a PTP message of specified type */
    TArray<uint8> CreatePTPMessage(EPTPMessageType Type);

    /** Parse a PTP message and extract its type */
    EPTPMessageType ParsePTPMessageType(const TArray<uint8>& Message);

    /** Process a sync message */
    void ProcessSyncMessage(const TArray<uint8>& Message);

    /** Process a delay request message */
    void ProcessDelayReqMessage(const TArray<uint8>& Message);

    /** Process a delay response message */
    void ProcessDelayRespMessage(const TArray<uint8>& Message);

    /** Process a follow-up message */
    void ProcessFollowUpMessage(const TArray<uint8>& Message);

    /** Send a follow-up message (precise timestamp) */
    void SendFollowUpMessage(int64 OriginTimestampMicros);

    /** Send a delay request message */
    void SendDelayReqMessage();

    /** Send a delay response message */
    void SendDelayRespMessage(int64 RequestReceivedTimestamp, uint16 SequenceId);

    /** Get current timestamp in microseconds */
    int64 GetTimestampMicroseconds() const;
};