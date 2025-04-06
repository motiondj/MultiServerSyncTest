// Copyright Your Company. All Rights Reserved.

#include "BP_NetworkLatencyTester.h"
#include "MultiServerSyncBPLibrary.h"
#include "TimerManager.h"
#include "Engine/Engine.h"

// 생성자
UBP_NetworkLatencyTester::UBP_NetworkLatencyTester()
{
    // 컴포넌트 틱 활성화
    PrimaryComponentTick.bCanEverTick = true;

    // 기본값 설정
    ServerIP = TEXT("127.0.0.1");
    ServerPort = 7000;
    MeasurementInterval = 1.0f;
    StatsUpdateInterval = 1.0f;
    bAutoStart = true;

    bMeasurementActive = false;

    // 통계 초기값
    MinRTT = 0.0f;
    MaxRTT = 0.0f;
    AvgRTT = 0.0f;
    Jitter = 0.0f;
    PacketLoss = 0.0f;
    NetworkQuality = TEXT("Unknown");
    QualityLevel = 0;
}

// 컴포넌트 초기화
void UBP_NetworkLatencyTester::BeginPlay()
{
    Super::BeginPlay();

    // 자동 시작이 활성화되어 있으면 측정 시작
    if (bAutoStart)
    {
        StartMeasurement();
    }

    // 통계 업데이트 타이머 설정
    GetWorld()->GetTimerManager().SetTimer(
        StatsUpdateTimerHandle,
        this,
        &UBP_NetworkLatencyTester::UpdateStats,
        StatsUpdateInterval,
        true);
}

// 컴포넌트 종료
void UBP_NetworkLatencyTester::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 측정 중지
    if (bMeasurementActive)
    {
        StopMeasurement();
    }

    // 타이머 정리
    GetWorld()->GetTimerManager().ClearTimer(StatsUpdateTimerHandle);

    Super::EndPlay(EndPlayReason);
}

// 컴포넌트 틱
void UBP_NetworkLatencyTester::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // 필요한 경우 여기에 추가 로직 구현
}

// 측정 시작
void UBP_NetworkLatencyTester::StartMeasurement()
{
    if (bMeasurementActive)
        return;

    UMultiServerSyncBPLibrary::StartNetworkLatencyMeasurement(
        ServerIP,
        ServerPort,
        MeasurementInterval,
        0); // 무제한 샘플

    bMeasurementActive = true;

    // 로그 출력
    UE_LOG(LogTemp, Log, TEXT("Started network latency measurement to %s:%d (Interval: %.2f seconds)"),
        *ServerIP, ServerPort, MeasurementInterval);

    // 화면에도 표시
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
            FString::Printf(TEXT("Started latency measurement to %s:%d"), *ServerIP, ServerPort));
    }
}

// 측정 중지
void UBP_NetworkLatencyTester::StopMeasurement()
{
    if (!bMeasurementActive)
        return;

    UMultiServerSyncBPLibrary::StopNetworkLatencyMeasurement(ServerIP, ServerPort);

    bMeasurementActive = false;

    // 로그 출력
    UE_LOG(LogTemp, Log, TEXT("Stopped network latency measurement to %s:%d"),
        *ServerIP, ServerPort);

    // 화면에도 표시
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow,
            FString::Printf(TEXT("Stopped latency measurement to %s:%d"), *ServerIP, ServerPort));
    }
}

// 통계 업데이트
void UBP_NetworkLatencyTester::UpdateStats()
{
    if (!bMeasurementActive)
        return;

    // 통계 가져오기
    bool bSuccess = UMultiServerSyncBPLibrary::GetNetworkLatencyStats(
        ServerIP,
        ServerPort,
        MinRTT,
        MaxRTT,
        AvgRTT,
        Jitter,
        PacketLoss);

    if (bSuccess)
    {
        // 네트워크 품질 평가
        QualityLevel = UMultiServerSyncBPLibrary::EvaluateNetworkQuality(ServerIP, ServerPort, NetworkQuality);
    }
}

// 현재 통계 출력
void UBP_NetworkLatencyTester::PrintCurrentStats()
{
    if (!bMeasurementActive)
    {
        UE_LOG(LogTemp, Warning, TEXT("No active latency measurement for %s:%d"), *ServerIP, ServerPort);
        return;
    }

    // 로그에 통계 출력
    UE_LOG(LogTemp, Log, TEXT("Latency stats for %s:%d - Min: %.2f ms, Max: %.2f ms, Avg: %.2f ms, Jitter: %.2f ms, Loss: %.2f%%, Quality: %s"),
        *ServerIP, ServerPort, MinRTT, MaxRTT, AvgRTT, Jitter, PacketLoss * 100.0f, *NetworkQuality);

    // 화면에도 표시
    if (GEngine)
    {
        FString StatsMessage = FString::Printf(
            TEXT("Server %s:%d\nRTT: %.2f ms (%.2f-%.2f ms)\nJitter: %.2f ms\nLoss: %.2f%%\nQuality: %s"),
            *ServerIP, ServerPort, AvgRTT, MinRTT, MaxRTT, Jitter, PacketLoss * 100.0f, *NetworkQuality);

        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, StatsMessage);
    }
}