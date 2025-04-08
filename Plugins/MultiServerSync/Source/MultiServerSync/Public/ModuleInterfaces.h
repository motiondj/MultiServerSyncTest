// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interfaces/IPv4/IPv4Endpoint.h" // FIPv4Endpoint 타입을 위해 추가
#include "NetworkTypes.h" // FNetworkLatencyStats 정의가 포함된 헤더

/**
 * Common interface for all environment detection systems
 */
class IEnvironmentDetector
{
public:
    virtual ~IEnvironmentDetector() {}

    // Initialize the detector
    virtual bool Initialize() = 0;

    // Shutdown the detector
    virtual void Shutdown() = 0;

    // Check if a specific feature is available in the environment
    virtual bool IsFeatureAvailable(const FString& FeatureName) = 0;

    // Get information about a specific feature
    virtual TMap<FString, FString> GetFeatureInfo(const FString& FeatureName) = 0;
};

/**
 * Common interface for network communication systems
 */
class INetworkManager
{
public:
    virtual ~INetworkManager() {}

    // Initialize the network system
    virtual bool Initialize() = 0;

    // Shutdown the network system
    virtual void Shutdown() = 0;

    // Send a message to a specific endpoint
    virtual bool SendMessage(const FString& EndpointId, const TArray<uint8>& Message) = 0;

    // Broadcast a message to all endpoints
    virtual bool BroadcastMessage(const TArray<uint8>& Message) = 0;

    // 신뢰성 있는 메시지 전송(ACK 확인 대기)
    virtual bool SendReliableMessage(const FIPv4Endpoint& Endpoint, ENetworkMessageType MessageType, const TArray<uint8>& MessageData) = 0;

    // 신뢰성 있는 브로드캐스트 (모든 서버에 신뢰성 있는 메시지 전송)
    virtual bool BroadcastReliableMessage(ENetworkMessageType MessageType, const TArray<uint8>& MessageData) = 0;

    // Register a message handler callback
    virtual void RegisterMessageHandler(TFunction<void(const FString&, const TArray<uint8>&)> Handler) = 0;

    // 서버 탐색 메서드
    virtual bool DiscoverServers() = 0;

    // 마스터-슬레이브 관련 메서드
    virtual bool IsMaster() const = 0;
    virtual FString GetMasterId() const = 0;
    virtual bool StartMasterElection() = 0;
    virtual void AnnounceMaster() = 0;
    virtual void ResignMaster() = 0;
    virtual struct FMasterInfo GetMasterInfo() const = 0;
    virtual void SetMasterPriority(float Priority) = 0;
    virtual void RegisterMasterChangeHandler(TFunction<void(const FString&, bool)> Handler) = 0;

    // 설정 관리 관련 메서드 - 새로 추가
    virtual uint16 GetPort() const = 0;
    virtual bool SendSettingsMessage(const TArray<uint8>& SettingsData) = 0;
    virtual bool RequestSettings() = 0;

    // 네트워크 지연 측정 관련 메서드
    virtual void StartLatencyMeasurement(const FIPv4Endpoint& ServerEndpoint, float IntervalSeconds = 1.0f, int32 SampleCount = 0) = 0;
    virtual void StopLatencyMeasurement(const FIPv4Endpoint& ServerEndpoint) = 0;
    virtual FNetworkLatencyStats GetLatencyStats(const FIPv4Endpoint& ServerEndpoint) const = 0;
    virtual int32 EvaluateNetworkQuality(const FIPv4Endpoint& ServerEndpoint) const = 0;
    virtual FString GetNetworkQualityString(const FIPv4Endpoint& ServerEndpoint) const = 0;

    // 고급 네트워크 품질 평가 메서드
    virtual FNetworkQualityAssessment EvaluateNetworkQualityDetailed(const FIPv4Endpoint& ServerEndpoint) const = 0;

    // 네트워크 상태 변화 이벤트 구독 메서드 
    virtual void RegisterNetworkStateChangeHandler(TFunction<void(const FIPv4Endpoint&, ENetworkEventType, const FNetworkQualityAssessment&)> Handler) = 0;

    // 네트워크 상태 변화 임계값 설정
    virtual void SetNetworkStateChangeThreshold(const FIPv4Endpoint& ServerEndpoint, double Threshold) = 0;

    // 네트워크 성능 임계값 설정
    virtual void SetNetworkPerformanceThresholds(const FIPv4Endpoint& ServerEndpoint,
        double LatencyThreshold,
        double JitterThreshold,
        double PacketLossThreshold) = 0;

    // 품질 평가 간격 설정
    virtual void SetQualityAssessmentInterval(const FIPv4Endpoint& ServerEndpoint, double IntervalSeconds) = 0;

    // 네트워크 상태 변화 모니터링 활성화/비활성화
    virtual void SetNetworkStateMonitoring(const FIPv4Endpoint& ServerEndpoint, bool bEnable) = 0;

    // 네트워크 이벤트 기록 가져오기
    virtual bool GetNetworkEventHistory(const FIPv4Endpoint& ServerEndpoint, TArray<ENetworkEventType>& OutEvents) const = 0;

    // 이상치 필터링 관련 메서드 추가
    virtual void SetOutlierFiltering(const FIPv4Endpoint& ServerEndpoint, bool bEnableFiltering) = 0;
    virtual bool GetOutlierStats(const FIPv4Endpoint& ServerEndpoint, int32& OutliersDetected, double& OutlierThreshold) const = 0;

    // 시계열 관련 메서드
    virtual void SetTimeSeriesSampleInterval(const FIPv4Endpoint& ServerEndpoint, double IntervalSeconds) = 0;
    virtual bool GetTimeSeriesData(const FIPv4Endpoint& ServerEndpoint, TArray<FLatencyTimeSeriesSample>& OutTimeSeries) const = 0;
    virtual bool GetNetworkTrendAnalysis(const FIPv4Endpoint& ServerEndpoint, FNetworkTrendAnalysis& OutTrendAnalysis) const = 0;
};

/**
 * Common interface for time synchronization systems
 */
class ITimeSync
{
public:
    virtual ~ITimeSync() {}

    // Initialize the time synchronization
    virtual bool Initialize() = 0;

    // Shutdown the time synchronization
    virtual void Shutdown() = 0;

    // Get current synchronized time in microseconds
    virtual int64 GetSyncedTimeMicroseconds() = 0;

    // Get estimated synchronization error in microseconds
    virtual int64 GetEstimatedErrorMicroseconds() = 0;

    // Check if time synchronization is currently active
    virtual bool IsSynchronized() = 0;

    // Get time offset in microseconds (for BP interface)
    virtual int64 GetTimeOffset() const = 0;

    // Get sync status (0: not synced, 1: syncing, 2: synced) 
    virtual int32 GetSyncStatus() const = 0;

    // Generate PTP timestamp for testing
    virtual int64 GeneratePTPTimestamp() const = 0;
};

/**
 * Common interface for frame synchronization controllers
 */
class IFrameSyncController
{
public:
    virtual ~IFrameSyncController() {}

    // Initialize the frame synchronization
    virtual bool Initialize() = 0;

    // Shutdown the frame synchronization
    virtual void Shutdown() = 0;

    // Get current frame number in the synchronized sequence
    virtual int64 GetSyncedFrameNumber() = 0;

    // Check if frame synchronization is currently active
    virtual bool IsSynchronized() = 0;

    // Set the target frame rate for synchronization
    virtual void SetTargetFrameRate(float FramesPerSecond) = 0;
};