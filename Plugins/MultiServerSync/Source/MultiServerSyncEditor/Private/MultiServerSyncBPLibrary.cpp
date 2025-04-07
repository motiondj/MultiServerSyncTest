// Copyright Your Company. All Rights Reserved.

#include "MultiServerSyncBPLibrary.h"
#include "MultiServerSync.h"
#include "FSyncLog.h" // 로그 카테고리를 위해 추가
#include "ISyncFrameworkManager.h"

// 플러그인이 초기화되었는지 확인
bool UMultiServerSyncBPLibrary::IsInitialized()
{
    TSharedPtr<ISyncFrameworkManager> FrameworkManager = FMultiServerSyncModule::GetFrameworkManager();
    return FrameworkManager != nullptr && FrameworkManager->IsInitialized();
}

// 현재 마스터 노드인지 확인
bool UMultiServerSyncBPLibrary::IsMasterNode()
{
    TSharedPtr<ISyncFrameworkManager> FrameworkManager = FMultiServerSyncModule::GetFrameworkManager();
    if (!FrameworkManager)
        return false;

    INetworkManager* NetworkManager = FrameworkManager->GetNetworkManager();
    if (!NetworkManager)
        return false;

    return NetworkManager->IsMaster();
}

// 현재 마스터 ID 가져오기
FString UMultiServerSyncBPLibrary::GetMasterNodeId()
{
    TSharedPtr<ISyncFrameworkManager> FrameworkManager = FMultiServerSyncModule::GetFrameworkManager();
    if (!FrameworkManager)
        return TEXT("");

    INetworkManager* NetworkManager = FrameworkManager->GetNetworkManager();
    if (!NetworkManager)
        return TEXT("");

    return NetworkManager->GetMasterId();
}

// 마스터 선출 시작
bool UMultiServerSyncBPLibrary::StartMasterElection()
{
    TSharedPtr<ISyncFrameworkManager> FrameworkManager = FMultiServerSyncModule::GetFrameworkManager();
    if (!FrameworkManager)
        return false;

    INetworkManager* NetworkManager = FrameworkManager->GetNetworkManager();
    if (!NetworkManager)
        return false;

    return NetworkManager->StartMasterElection();
}

// 마스터 우선순위 설정
void UMultiServerSyncBPLibrary::SetMasterPriority(float Priority)
{
    TSharedPtr<ISyncFrameworkManager> FrameworkManager = FMultiServerSyncModule::GetFrameworkManager();
    if (!FrameworkManager)
        return;

    INetworkManager* NetworkManager = FrameworkManager->GetNetworkManager();
    if (!NetworkManager)
        return;

    NetworkManager->SetMasterPriority(Priority);
}

// 서버 탐색 시작
bool UMultiServerSyncBPLibrary::DiscoverServers()
{
    TSharedPtr<ISyncFrameworkManager> FrameworkManager = FMultiServerSyncModule::GetFrameworkManager();
    if (!FrameworkManager)
        return false;

    INetworkManager* NetworkManager = FrameworkManager->GetNetworkManager();
    if (!NetworkManager)
        return false;

    return NetworkManager->DiscoverServers();
}

// 발견된 서버 목록 가져오기
TArray<FString> UMultiServerSyncBPLibrary::GetDiscoveredServers()
{
    TSharedPtr<ISyncFrameworkManager> FrameworkManager = FMultiServerSyncModule::GetFrameworkManager();
    if (!FrameworkManager)
        return TArray<FString>();

    INetworkManager* NetworkManager = FrameworkManager->GetNetworkManager();
    if (!NetworkManager)
        return TArray<FString>();

    FNetworkManager* NetworkManagerImpl = static_cast<FNetworkManager*>(NetworkManager);
    return NetworkManagerImpl->GetDiscoveredServers();
}

// 네트워크 지연 측정 시작
void UMultiServerSyncBPLibrary::StartNetworkLatencyMeasurement(const FString& ServerIP, int32 ServerPort, float IntervalSeconds, int32 SampleCount)
{
    TSharedPtr<ISyncFrameworkManager> FrameworkManager = FMultiServerSyncModule::GetFrameworkManager();
    if (!FrameworkManager)
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to start latency measurement: Framework manager is not available"));
        return;
    }

    INetworkManager* NetworkManager = FrameworkManager->GetNetworkManager();
    if (!NetworkManager)
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to start latency measurement: Network manager is not available"));
        return;
    }

    // IP 주소 파싱
    FIPv4Address IPAddress;
    if (!FIPv4Address::Parse(ServerIP, IPAddress))
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to start latency measurement: Invalid IP address '%s'"), *ServerIP);
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
    TSharedPtr<ISyncFrameworkManager> FrameworkManager = FMultiServerSyncModule::GetFrameworkManager();
    if (!FrameworkManager)
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to stop latency measurement: Framework manager is not available"));
        return;
    }

    INetworkManager* NetworkManager = FrameworkManager->GetNetworkManager();
    if (!NetworkManager)
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to stop latency measurement: Network manager is not available"));
        return;
    }

    // IP 주소 파싱
    FIPv4Address IPAddress;
    if (!FIPv4Address::Parse(ServerIP, IPAddress))
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to stop latency measurement: Invalid IP address '%s'"), *ServerIP);
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

    TSharedPtr<ISyncFrameworkManager> FrameworkManager = FMultiServerSyncModule::GetFrameworkManager();
    if (!FrameworkManager)
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to get latency stats: Framework manager is not available"));
        return false;
    }

    INetworkManager* NetworkManager = FrameworkManager->GetNetworkManager();
    if (!NetworkManager)
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to get latency stats: Network manager is not available"));
        return false;
    }

    // IP 주소 파싱
    FIPv4Address IPAddress;
    if (!FIPv4Address::Parse(ServerIP, IPAddress))
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to get latency stats: Invalid IP address '%s'"), *ServerIP);
        return false;
    }

    // 엔드포인트 생성
    FIPv4Endpoint ServerEndpoint(IPAddress, static_cast<uint16>(ServerPort));

    // 통계 가져오기
    FNetworkLatencyStats Stats = NetworkManager->GetLatencyStats(ServerEndpoint);

    // 통계가 유효한지 확인
    if (Stats.SampleCount <= 0)
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("No latency stats available for server %s:%d"), *ServerIP, ServerPort);
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
    TSharedPtr<ISyncFrameworkManager> FrameworkManager = FMultiServerSyncModule::GetFrameworkManager();
    if (!FrameworkManager)
    {
        QualityString = TEXT("Unknown");
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to evaluate network quality: Framework manager is not available"));
        return 0;
    }

    INetworkManager* NetworkManager = FrameworkManager->GetNetworkManager();
    if (!NetworkManager)
    {
        QualityString = TEXT("Unknown");
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to evaluate network quality: Network manager is not available"));
        return 0;
    }

    // IP 주소 파싱
    FIPv4Address IPAddress;
    if (!FIPv4Address::Parse(ServerIP, IPAddress))
    {
        QualityString = TEXT("Unknown");
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to evaluate network quality: Invalid IP address '%s'"), *ServerIP);
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
    TSharedPtr<ISyncFrameworkManager> FrameworkManager = FMultiServerSyncModule::GetFrameworkManager();
    if (!FrameworkManager)
        return 0;

    ITimeSync* TimeSync = FrameworkManager->GetTimeSync();
    if (!TimeSync)
        return 0;

    // PTP 타임스탬프 생성 메서드가 구현되어 있다고 가정
    // 실제 구현에 맞게 조정 필요
    return TimeSync->GeneratePTPTimestamp();
}

// 시간 오프셋 가져오기
float UMultiServerSyncBPLibrary::GetTimeOffset()
{
    TSharedPtr<ISyncFrameworkManager> FrameworkManager = FMultiServerSyncModule::GetFrameworkManager();
    if (!FrameworkManager)
        return 0.0f;

    ITimeSync* TimeSync = FrameworkManager->GetTimeSync();
    if (!TimeSync)
        return 0.0f;

    // 오프셋을 밀리초 단위로 반환
    return TimeSync->GetTimeOffset() * 1000.0f;
}

// 동기화 상태 가져오기
int32 UMultiServerSyncBPLibrary::GetSyncStatus()
{
    TSharedPtr<ISyncFrameworkManager> FrameworkManager = FMultiServerSyncModule::GetFrameworkManager();
    if (!FrameworkManager)
        return 0;

    ITimeSync* TimeSync = FrameworkManager->GetTimeSync();
    if (!TimeSync)
        return 0;

    return static_cast<int32>(TimeSync->GetSyncStatus());
}