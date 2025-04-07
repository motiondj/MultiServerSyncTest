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
void UMultiServerSyncBPLibrary::StartNetworkLatencyMeasurement(const FString& ServerIP, int32 ServerPort, float IntervalSeconds, int32 SampleCount)
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

    // 측정 시작
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
    float& Jitter, float& PacketLoss)
{
    // 초기값 설정
    MinRTT = 0.0f;
    MaxRTT = 0.0f;
    AvgRTT = 0.0f;
    Jitter = 0.0f;
    PacketLoss = 0.0f;

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

    // 패킷 손실율 계산
    if (Stats.SampleCount + Stats.LostPackets > 0)
    {
        PacketLoss = static_cast<float>(Stats.LostPackets) / (Stats.SampleCount + Stats.LostPackets);
    }

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