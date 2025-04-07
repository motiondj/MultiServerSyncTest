// Copyright Your Company. All Rights Reserved.

#include "MultiServerSyncBPLibrary.h"
#include "MultiServerSync.h"
#include "FSyncLog.h" // 로그 카테고리를 위해 추가
#include "ISyncFrameworkManager.h"
#include "SyncFrameworkManager.h" // FSyncFrameworkManagerUtil을 가져오기 위해 추가

// 로그 카테고리 정의
DEFINE_LOG_CATEGORY_STATIC(LogMultiServerSyncEditor, Log, All);

// 플러그인이 초기화되었는지 확인
bool UMultiServerSyncBPLibrary::IsInitialized()
{
    ISyncFrameworkManager* FrameworkManager = FSyncFrameworkManagerUtil::Get();
    return FrameworkManager != nullptr && FrameworkManager->IsInitialized();
}

// 현재 마스터 노드인지 확인
bool UMultiServerSyncBPLibrary::IsMasterNode()
{
    ISyncFrameworkManager* FrameworkManager = FSyncFrameworkManagerUtil::Get();
    if (!FrameworkManager)
        return false;

    TSharedPtr<INetworkManager> NetworkManager = FrameworkManager->GetNetworkManager();
    if (!NetworkManager.IsValid())
        return false;

    return NetworkManager->IsMaster();
}

// 현재 마스터 ID 가져오기
FString UMultiServerSyncBPLibrary::GetMasterNodeId()
{
    ISyncFrameworkManager* FrameworkManager = FSyncFrameworkManagerUtil::Get();
    if (!FrameworkManager)
        return TEXT("");

    TSharedPtr<INetworkManager> NetworkManager = FrameworkManager->GetNetworkManager();
    if (!NetworkManager.IsValid())
        return TEXT("");

    return NetworkManager->GetMasterId();
}

// 마스터 선출 시작
bool UMultiServerSyncBPLibrary::StartMasterElection()
{
    ISyncFrameworkManager* FrameworkManager = FSyncFrameworkManagerUtil::Get();
    if (!FrameworkManager)
        return false;

    TSharedPtr<INetworkManager> NetworkManager = FrameworkManager->GetNetworkManager();
    if (!NetworkManager.IsValid())
        return false;

    return NetworkManager->StartMasterElection();
}

// 마스터 우선순위 설정
void UMultiServerSyncBPLibrary::SetMasterPriority(float Priority)
{
    ISyncFrameworkManager* FrameworkManager = FSyncFrameworkManagerUtil::Get();
    if (!FrameworkManager)
        return;

    TSharedPtr<INetworkManager> NetworkManager = FrameworkManager->GetNetworkManager();
    if (!NetworkManager.IsValid())
        return;

    NetworkManager->SetMasterPriority(Priority);
}

// 서버 탐색 시작
bool UMultiServerSyncBPLibrary::DiscoverServers()
{
    ISyncFrameworkManager* FrameworkManager = FSyncFrameworkManagerUtil::Get();
    if (!FrameworkManager)
        return false;

    TSharedPtr<INetworkManager> NetworkManager = FrameworkManager->GetNetworkManager();
    if (!NetworkManager.IsValid())
        return false;

    return NetworkManager->DiscoverServers();
}

// 발견된 서버 목록 가져오기
TArray<FString> UMultiServerSyncBPLibrary::GetDiscoveredServers()
{
    TArray<FString> Result;

    ISyncFrameworkManager* FrameworkManager = FSyncFrameworkManagerUtil::Get();
    if (!FrameworkManager)
        return Result;

    TSharedPtr<INetworkManager> NetworkManager = FrameworkManager->GetNetworkManager();
    if (!NetworkManager.IsValid())
        return Result;

    // FNetworkManager 사용을 피하고 INetworkManager에서 직접 정보 얻기
    // 대안: 인터페이스에 GetDiscoveredServers 메서드 추가 고려
    if (NetworkManager->DiscoverServers())
    {
        // 서버 탐색이 성공하면 더미 데이터 반환
        Result.Add(TEXT("Server discovery in progress..."));
    }

    return Result;
}

// 네트워크 지연 측정 시작
void UMultiServerSyncBPLibrary::StartNetworkLatencyMeasurement(
    const FString& ServerIP,
    int32 ServerPort,
    float IntervalSeconds,
    int32 SampleCount,
    bool bDynamicSampling,
    float MinIntervalSeconds,
    float MaxIntervalSeconds)
{
    ISyncFrameworkManager* FrameworkManager = FSyncFrameworkManagerUtil::Get();
    if (!FrameworkManager)
    {
        UE_LOG(LogMultiServerSyncEditor, Error, TEXT("Failed to start latency measurement: Framework manager is not available"));
        return;
    }

    TSharedPtr<INetworkManager> NetworkManager = FrameworkManager->GetNetworkManager();
    if (!NetworkManager.IsValid())
    {
        UE_LOG(LogMultiServerSyncEditor, Error, TEXT("Failed to start latency measurement: Network manager is not available"));
        return;
    }

    // IP 주소 파싱
    FIPv4Address IPAddress;
    if (!FIPv4Address::Parse(ServerIP, IPAddress))
    {
        UE_LOG(LogMultiServerSyncEditor, Error, TEXT("Failed to start latency measurement: Invalid IP address '%s'"), *ServerIP);
        return;
    }

    // 엔드포인트 생성
    FIPv4Endpoint ServerEndpoint(IPAddress, static_cast<uint16>(ServerPort));

    // 동적 샘플링 정보 로깅
    if (bDynamicSampling)
    {
        UE_LOG(LogMultiServerSyncEditor, Display, TEXT("Starting latency measurement with dynamic sampling (Min: %.2f s, Max: %.2f s)"),
            MinIntervalSeconds, MaxIntervalSeconds);
    }

    // FNetworkManager 구현체를 직접 사용할 수 없으므로 인터페이스를 통해 측정만 시작
    // 동적 샘플링 옵션은 지원하지 않음 - 이는 향후 인터페이스 확장 필요
    NetworkManager->StartLatencyMeasurement(ServerEndpoint, IntervalSeconds, SampleCount);
}

// 네트워크 지연 측정 중지
void UMultiServerSyncBPLibrary::StopNetworkLatencyMeasurement(const FString& ServerIP, int32 ServerPort)
{
    ISyncFrameworkManager* FrameworkManager = FSyncFrameworkManagerUtil::Get();
    if (!FrameworkManager)
    {
        UE_LOG(LogMultiServerSyncEditor, Error, TEXT("Failed to stop latency measurement: Framework manager is not available"));
        return;
    }

    TSharedPtr<INetworkManager> NetworkManager = FrameworkManager->GetNetworkManager();
    if (!NetworkManager.IsValid())
    {
        UE_LOG(LogMultiServerSyncEditor, Error, TEXT("Failed to stop latency measurement: Network manager is not available"));
        return;
    }

    // IP 주소 파싱
    FIPv4Address IPAddress;
    if (!FIPv4Address::Parse(ServerIP, IPAddress))
    {
        UE_LOG(LogMultiServerSyncEditor, Error, TEXT("Failed to stop latency measurement: Invalid IP address '%s'"), *ServerIP);
        return;
    }

    // 엔드포인트 생성
    FIPv4Endpoint ServerEndpoint(IPAddress, static_cast<uint16>(ServerPort));

    // 측정 중지
    NetworkManager->StopLatencyMeasurement(ServerEndpoint);
}

// 네트워크 지연 통계 가져오기
bool UMultiServerSyncBPLibrary::GetNetworkLatencyStats(const FString& ServerIP, int32 ServerPort,
    float& MinRTT, float& MaxRTT, float& AvgRTT,
    float& Jitter, float& PacketLoss,
    float& Percentile50, float& Percentile95, float& Percentile99)
{
    // 초기값 설정
    MinRTT = 0.0f;
    MaxRTT = 0.0f;
    AvgRTT = 0.0f;
    Jitter = 0.0f;
    PacketLoss = 0.0f;
    Percentile50 = 0.0f;
    Percentile95 = 0.0f;
    Percentile99 = 0.0f;

    ISyncFrameworkManager* FrameworkManager = FSyncFrameworkManagerUtil::Get();
    if (!FrameworkManager)
    {
        UE_LOG(LogMultiServerSyncEditor, Error, TEXT("Failed to get latency stats: Framework manager is not available"));
        return false;
    }

    TSharedPtr<INetworkManager> NetworkManager = FrameworkManager->GetNetworkManager();
    if (!NetworkManager.IsValid())
    {
        UE_LOG(LogMultiServerSyncEditor, Error, TEXT("Failed to get latency stats: Network manager is not available"));
        return false;
    }

    // IP 주소 파싱
    FIPv4Address IPAddress;
    if (!FIPv4Address::Parse(ServerIP, IPAddress))
    {
        UE_LOG(LogMultiServerSyncEditor, Error, TEXT("Failed to get latency stats: Invalid IP address '%s'"), *ServerIP);
        return false;
    }

    // 엔드포인트 생성
    FIPv4Endpoint ServerEndpoint(IPAddress, static_cast<uint16>(ServerPort));

    // 통계 가져오기
    FNetworkLatencyStats Stats = NetworkManager->GetLatencyStats(ServerEndpoint);

    // 통계가 유효한지 확인
    if (Stats.SampleCount <= 0)
    {
        UE_LOG(LogMultiServerSyncEditor, Warning, TEXT("No latency stats available for server %s:%d"), *ServerIP, ServerPort);
        return false;
    }

    // 출력 변수에 값 복사
    MinRTT = static_cast<float>(Stats.MinRTT);
    MaxRTT = static_cast<float>(Stats.MaxRTT);
    AvgRTT = static_cast<float>(Stats.AvgRTT);
    Jitter = static_cast<float>(Stats.Jitter);
    Percentile50 = static_cast<float>(Stats.Percentile50);
    Percentile95 = static_cast<float>(Stats.Percentile95);
    Percentile99 = static_cast<float>(Stats.Percentile99);

    // 패킷 손실율 계산
    if (Stats.SampleCount + Stats.LostPackets > 0)
    {
        PacketLoss = static_cast<float>(Stats.LostPackets) / (Stats.SampleCount + Stats.LostPackets);
    }

    return true;
}

// 이상치 필터링 설정
void UMultiServerSyncBPLibrary::SetOutlierFiltering(const FString& ServerIP, int32 ServerPort, bool bEnableFiltering)
{
    ISyncFrameworkManager* FrameworkManager = FSyncFrameworkManagerUtil::Get();
    if (!FrameworkManager)
    {
        UE_LOG(LogMultiServerSyncEditor, Error, TEXT("Failed to set outlier filtering: Framework manager is not available"));
        return;
    }

    TSharedPtr<INetworkManager> NetworkManager = FrameworkManager->GetNetworkManager();
    if (!NetworkManager.IsValid())
    {
        UE_LOG(LogMultiServerSyncEditor, Error, TEXT("Failed to set outlier filtering: Network manager is not available"));
        return;
    }

    // IP 주소 파싱
    FIPv4Address IPAddress;
    if (!FIPv4Address::Parse(ServerIP, IPAddress))
    {
        UE_LOG(LogMultiServerSyncEditor, Error, TEXT("Failed to set outlier filtering: Invalid IP address '%s'"), *ServerIP);
        return;
    }

    // 엔드포인트 생성
    FIPv4Endpoint ServerEndpoint(IPAddress, static_cast<uint16>(ServerPort));

    // 이상치 필터링 설정
    NetworkManager->SetOutlierFiltering(ServerEndpoint, bEnableFiltering);
}

// 이상치 통계 가져오기
bool UMultiServerSyncBPLibrary::GetOutlierStats(const FString& ServerIP, int32 ServerPort, int32& OutliersDetected, float& OutlierThreshold)
{
    // 초기값 설정
    OutliersDetected = 0;
    OutlierThreshold = 0.0f;

    ISyncFrameworkManager* FrameworkManager = FSyncFrameworkManagerUtil::Get();
    if (!FrameworkManager)
    {
        UE_LOG(LogMultiServerSyncEditor, Error, TEXT("Failed to get outlier stats: Framework manager is not available"));
        return false;
    }

    TSharedPtr<INetworkManager> NetworkManager = FrameworkManager->GetNetworkManager();
    if (!NetworkManager.IsValid())
    {
        UE_LOG(LogMultiServerSyncEditor, Error, TEXT("Failed to get outlier stats: Network manager is not available"));
        return false;
    }

    // IP 주소 파싱
    FIPv4Address IPAddress;
    if (!FIPv4Address::Parse(ServerIP, IPAddress))
    {
        UE_LOG(LogMultiServerSyncEditor, Error, TEXT("Failed to get outlier stats: Invalid IP address '%s'"), *ServerIP);
        return false;
    }

    // 엔드포인트 생성
    FIPv4Endpoint ServerEndpoint(IPAddress, static_cast<uint16>(ServerPort));

    // 이상치 통계 가져오기
    double ThresholdValue = 0.0;
    bool bSuccess = NetworkManager->GetOutlierStats(ServerEndpoint, OutliersDetected, ThresholdValue);

    // double에서 float로 변환
    OutlierThreshold = static_cast<float>(ThresholdValue);

    return bSuccess;
}

// 시계열 샘플링 간격 설정
void UMultiServerSyncBPLibrary::SetTimeSeriesSampleInterval(const FString& ServerIP, int32 ServerPort, float IntervalSeconds)
{
    ISyncFrameworkManager* FrameworkManager = FSyncFrameworkManagerUtil::Get();
    if (!FrameworkManager)
    {
        UE_LOG(LogMultiServerSyncEditor, Error, TEXT("Failed to set time series interval: Framework manager is not available"));
        return;
    }

    TSharedPtr<INetworkManager> NetworkManager = FrameworkManager->GetNetworkManager();
    if (!NetworkManager.IsValid())
    {
        UE_LOG(LogMultiServerSyncEditor, Error, TEXT("Failed to set time series interval: Network manager is not available"));
        return;
    }

    // IP 주소 파싱
    FIPv4Address IPAddress;
    if (!FIPv4Address::Parse(ServerIP, IPAddress))
    {
        UE_LOG(LogMultiServerSyncEditor, Error, TEXT("Failed to set time series interval: Invalid IP address '%s'"), *ServerIP);
        return;
    }

    // 엔드포인트 생성
    FIPv4Endpoint ServerEndpoint(IPAddress, static_cast<uint16>(ServerPort));

    // 샘플링 간격 설정
    NetworkManager->SetTimeSeriesSampleInterval(ServerEndpoint, static_cast<double>(IntervalSeconds));
}

// 시계열 데이터 가져오기 (간소화된 블루프린트용 버전)
bool UMultiServerSyncBPLibrary::GetTimeSeriesData(const FString& ServerIP, int32 ServerPort,
    TArray<float>& OutTimestamps, TArray<float>& OutRTTValues)
{
    // 초기화
    OutTimestamps.Empty();
    OutRTTValues.Empty();

    ISyncFrameworkManager* FrameworkManager = FSyncFrameworkManagerUtil::Get();
    if (!FrameworkManager)
    {
        UE_LOG(LogMultiServerSyncEditor, Error, TEXT("Failed to get time series data: Framework manager is not available"));
        return false;
    }

    TSharedPtr<INetworkManager> NetworkManager = FrameworkManager->GetNetworkManager();
    if (!NetworkManager.IsValid())
    {
        UE_LOG(LogMultiServerSyncEditor, Error, TEXT("Failed to get time series data: Network manager is not available"));
        return false;
    }

    // IP 주소 파싱
    FIPv4Address IPAddress;
    if (!FIPv4Address::Parse(ServerIP, IPAddress))
    {
        UE_LOG(LogMultiServerSyncEditor, Error, TEXT("Failed to get time series data: Invalid IP address '%s'"), *ServerIP);
        return false;
    }

    // 엔드포인트 생성
    FIPv4Endpoint ServerEndpoint(IPAddress, static_cast<uint16>(ServerPort));

    // 전체 시계열 데이터 가져오기
    TArray<FLatencyTimeSeriesSample> TimeSeries;
    bool bSuccess = NetworkManager->GetTimeSeriesData(ServerEndpoint, TimeSeries);

    if (!bSuccess || TimeSeries.Num() == 0)
    {
        return false;
    }

    // 마지막 타임스탬프를 기준으로 상대 시간으로 변환 (시각화 용이성)
    double LastTimestamp = TimeSeries.Last().Timestamp;

    // 블루프린트에서 사용 가능한 형태로 변환
    for (const FLatencyTimeSeriesSample& Sample : TimeSeries)
    {
        // 상대 시간으로 변환 (초, 마지막 샘플로부터 경과 시간)
        OutTimestamps.Add(static_cast<float>(Sample.Timestamp - LastTimestamp));
        OutRTTValues.Add(static_cast<float>(Sample.RTT));
    }

    return true;
}

// 네트워크 추세 분석 결과 가져오기
bool UMultiServerSyncBPLibrary::GetNetworkTrendAnalysis(const FString& ServerIP, int32 ServerPort,
    float& ShortTermTrend, float& LongTermTrend, float& Volatility,
    float& TimeSinceWorstRTT, float& TimeSinceBestRTT)
{
    // 초기화
    ShortTermTrend = 0.0f;
    LongTermTrend = 0.0f;
    Volatility = 0.0f;
    TimeSinceWorstRTT = 0.0f;
    TimeSinceBestRTT = 0.0f;

    ISyncFrameworkManager* FrameworkManager = FSyncFrameworkManagerUtil::Get();
    if (!FrameworkManager)
    {
        UE_LOG(LogMultiServerSyncEditor, Error, TEXT("Failed to get trend analysis: Framework manager is not available"));
        return false;
    }

    TSharedPtr<INetworkManager> NetworkManager = FrameworkManager->GetNetworkManager();
    if (!NetworkManager.IsValid())
    {
        UE_LOG(LogMultiServerSyncEditor, Error, TEXT("Failed to get trend analysis: Network manager is not available"));
        return false;
    }

    // IP 주소 파싱
    FIPv4Address IPAddress;
    if (!FIPv4Address::Parse(ServerIP, IPAddress))
    {
        UE_LOG(LogMultiServerSyncEditor, Error, TEXT("Failed to get trend analysis: Invalid IP address '%s'"), *ServerIP);
        return false;
    }

    // 엔드포인트 생성
    FIPv4Endpoint ServerEndpoint(IPAddress, static_cast<uint16>(ServerPort));

    // 추세 분석 결과 가져오기
    FNetworkTrendAnalysis TrendAnalysis;
    bool bSuccess = NetworkManager->GetNetworkTrendAnalysis(ServerEndpoint, TrendAnalysis);

    if (!bSuccess)
    {
        return false;
    }

    // 결과를 블루프린트 변수에 복사
    ShortTermTrend = static_cast<float>(TrendAnalysis.ShortTermTrend);
    LongTermTrend = static_cast<float>(TrendAnalysis.LongTermTrend);
    Volatility = static_cast<float>(TrendAnalysis.Volatility);
    TimeSinceWorstRTT = static_cast<float>(TrendAnalysis.TimeSinceWorstRTT);
    TimeSinceBestRTT = static_cast<float>(TrendAnalysis.TimeSinceBestRTT);

    return true;
}

// 네트워크 품질 평가
int32 UMultiServerSyncBPLibrary::EvaluateNetworkQuality(const FString& ServerIP, int32 ServerPort, FString& QualityString)
{
    ISyncFrameworkManager* FrameworkManager = FSyncFrameworkManagerUtil::Get();
    if (!FrameworkManager)
    {
        QualityString = TEXT("Unknown");
        UE_LOG(LogMultiServerSyncEditor, Error, TEXT("Failed to evaluate network quality: Framework manager is not available"));
        return 0;
    }

    TSharedPtr<INetworkManager> NetworkManager = FrameworkManager->GetNetworkManager();
    if (!NetworkManager.IsValid())
    {
        QualityString = TEXT("Unknown");
        UE_LOG(LogMultiServerSyncEditor, Error, TEXT("Failed to evaluate network quality: Network manager is not available"));
        return 0;
    }

    // IP 주소 파싱
    FIPv4Address IPAddress;
    if (!FIPv4Address::Parse(ServerIP, IPAddress))
    {
        QualityString = TEXT("Unknown");
        UE_LOG(LogMultiServerSyncEditor, Error, TEXT("Failed to evaluate network quality: Invalid IP address '%s'"), *ServerIP);
        return 0;
    }

    // 엔드포인트 생성
    FIPv4Endpoint ServerEndpoint(IPAddress, static_cast<uint16>(ServerPort));

    // 품질 평가
    int32 Quality = NetworkManager->EvaluateNetworkQuality(ServerEndpoint);
    QualityString = NetworkManager->GetNetworkQualityString(ServerEndpoint);

    return Quality;
}

// PTP 타임스탬프 생성
int64 UMultiServerSyncBPLibrary::GeneratePTPTimestamp()
{
    ISyncFrameworkManager* FrameworkManager = FSyncFrameworkManagerUtil::Get();
    if (!FrameworkManager)
        return 0;

    TSharedPtr<ITimeSync> TimeSync = FrameworkManager->GetTimeSync();
    if (!TimeSync.IsValid())
        return 0;

    // 현재 시간 반환
    return TimeSync->GeneratePTPTimestamp();
}

// 시간 오프셋 가져오기
float UMultiServerSyncBPLibrary::GetTimeOffset()
{
    ISyncFrameworkManager* FrameworkManager = FSyncFrameworkManagerUtil::Get();
    if (!FrameworkManager)
        return 0.0f;

    TSharedPtr<ITimeSync> TimeSync = FrameworkManager->GetTimeSync();
    if (!TimeSync.IsValid())
        return 0.0f;

    // 오프셋을 밀리초 단위로 반환
    return static_cast<float>(TimeSync->GetTimeOffset()) / 1000.0f;
}

// 동기화 상태 가져오기
int32 UMultiServerSyncBPLibrary::GetSyncStatus()
{
    ISyncFrameworkManager* FrameworkManager = FSyncFrameworkManagerUtil::Get();
    if (!FrameworkManager)
        return 0;

    TSharedPtr<ITimeSync> TimeSync = FrameworkManager->GetTimeSync();
    if (!TimeSync.IsValid())
        return 0;

    return TimeSync->GetSyncStatus();
}