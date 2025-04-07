// Copyright Your Company. All Rights Reserved.

#include "Components/BP_NetworkLatencyTester.h"
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
    bDynamicSampling = false;
    MinSamplingInterval = 0.1f;
    MaxSamplingInterval = 5.0f;

    bMeasurementActive = false;

    // 통계 초기값
    MinRTT = 0.0f;
    MaxRTT = 0.0f;
    AvgRTT = 0.0f;
    Jitter = 0.0f;
    PacketLoss = 0.0f;
    Percentile50 = 0.0f;
    Percentile95 = 0.0f;
    Percentile99 = 0.0f;
    NetworkQuality = TEXT("Unknown");
    QualityLevel = 0;

    // 이상치 통계 초기값
    OutliersDetected = 0;
    OutlierThreshold = 0.0f;
    bEnableOutlierFiltering = true;

    // 품질 점수 초기값
    QualityScore = 0;
    LatencyScore = 0;
    JitterScore = 0;
    PacketLossScore = 0;
    StabilityScore = 0;
    QualityTrend = 0.0f;
    DetailedDescription = TEXT("No measurements yet.");

    // 이벤트 초기값
    LatestEventType = 0;
    LatestEventDescription = TEXT("No events.");

    // 품질 평가 설정 초기값
    bEnableStateMonitoring = true;
    QualityAssessmentInterval = 5.0f;
    StateChangeThreshold = 15.0f;
    LatencyThreshold = 150.0f;
    JitterThreshold = 50.0f;
    PacketLossThreshold = 0.05f;
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

    // 품질 평가 설정 적용
    ApplyQualitySettings();
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
// StartMeasurement 메서드 수정 (약 114줄 근처)
void UBP_NetworkLatencyTester::StartMeasurement()
{
    if (bMeasurementActive)
        return;

    UMultiServerSyncBPLibrary::StartNetworkLatencyMeasurement(
        ServerIP,
        ServerPort,
        MeasurementInterval,
        0, // 무제한 샘플
        bDynamicSampling,
        MinSamplingInterval,
        MaxSamplingInterval);

    bMeasurementActive = true;

    // 측정 시작 후 이상치 필터링 설정 적용
    UMultiServerSyncBPLibrary::SetOutlierFiltering(ServerIP, ServerPort, bEnableOutlierFiltering);

    // 품질 평가 설정 적용
    ApplyQualitySettings();

    // 로그 출력
    UE_LOG(LogTemp, Log, TEXT("Started network latency measurement to %s:%d (Interval: %.2f seconds, Dynamic: %s)"),
        *ServerIP, ServerPort, MeasurementInterval, bDynamicSampling ? TEXT("true") : TEXT("false"));

    // 화면에도 표시
    if (GEngine)
    {
        FString ModeText = bDynamicSampling ?
            FString::Printf(TEXT("Dynamic mode (%.2f-%.2f s)"), MinSamplingInterval, MaxSamplingInterval) :
            FString::Printf(TEXT("Fixed mode (%.2f s)"), MeasurementInterval);

        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
            FString::Printf(TEXT("Started latency measurement to %s:%d\n%s"),
                *ServerIP, ServerPort, *ModeText));
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
// UpdateStats 메서드 수정 (약 79줄 근처)
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
        PacketLoss,
        Percentile50,
        Percentile95,
        Percentile99);

    if (bSuccess)
    {
        // 네트워크 품질 평가
        QualityLevel = UMultiServerSyncBPLibrary::EvaluateNetworkQuality(ServerIP, ServerPort, NetworkQuality);

        // 이상치 통계 업데이트
        UMultiServerSyncBPLibrary::GetOutlierStats(ServerIP, ServerPort, OutliersDetected, OutlierThreshold);

        // 상세 품질 정보 업데이트
        UMultiServerSyncBPLibrary::EvaluateNetworkQualityDetailed(
            ServerIP,
            ServerPort,
            QualityScore,
            LatencyScore,
            JitterScore,
            PacketLossScore,
            StabilityScore,
            DetailedDescription,
            Recommendations,
            QualityTrend);

        // 최신 이벤트 가져오기
        UMultiServerSyncBPLibrary::GetLatestNetworkEvent(
            ServerIP,
            ServerPort,
            LatestEventType,
            LatestEventDescription);
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
    UE_LOG(LogTemp, Log, TEXT("Latency stats for %s:%d - Min: %.2f ms, Max: %.2f ms, Avg: %.2f ms, Jitter: %.2f ms, Loss: %.2f%%, P50/P95/P99: %.2f/%.2f/%.2f ms, Outliers: %d, Quality: %s"),
        *ServerIP, ServerPort, MinRTT, MaxRTT, AvgRTT, Jitter, PacketLoss * 100.0f,
        Percentile50, Percentile95, Percentile99, OutliersDetected, *NetworkQuality);

    // 화면에도 표시
    if (GEngine)
    {
        FString StatsMessage = FString::Printf(
            TEXT("Server %s:%d\nRTT: %.2f ms (%.2f-%.2f ms)\nJitter: %.2f ms\nLoss: %.2f%%\nP50/P95/P99: %.2f/%.2f/%.2f ms\nOutliers: %d (Threshold: %.2f ms)\nQuality: %s"),
            *ServerIP, ServerPort, AvgRTT, MinRTT, MaxRTT, Jitter, PacketLoss * 100.0f,
            Percentile50, Percentile95, Percentile99, OutliersDetected, OutlierThreshold, *NetworkQuality);

        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, StatsMessage);
    }
}

// 이상치 필터링 설정
void UBP_NetworkLatencyTester::SetOutlierFiltering(bool bEnable)
{
    bEnableOutlierFiltering = bEnable;

    if (bMeasurementActive)
    {
        UMultiServerSyncBPLibrary::SetOutlierFiltering(ServerIP, ServerPort, bEnable);

        // 로그에 설정 변경 출력
        UE_LOG(LogTemp, Log, TEXT("Outlier filtering for %s:%d: %s"),
            *ServerIP, ServerPort, bEnable ? TEXT("Enabled") : TEXT("Disabled"));

        // 화면에도 표시
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.0f,
                bEnable ? FColor::Green : FColor::Orange,
                FString::Printf(TEXT("Outlier filtering: %s"), bEnable ? TEXT("Enabled") : TEXT("Disabled")));
        }
    }
}

// 품질 평가 설정 적용
void UBP_NetworkLatencyTester::ApplyQualitySettings()
{
    if (!bMeasurementActive)
        return;

    // 상태 모니터링 설정
    UMultiServerSyncBPLibrary::SetNetworkStateMonitoring(ServerIP, ServerPort, bEnableStateMonitoring);

    // 품질 평가 간격 설정
    UMultiServerSyncBPLibrary::SetQualityAssessmentInterval(ServerIP, ServerPort, QualityAssessmentInterval);

    // 상태 변화 임계값 설정
    UMultiServerSyncBPLibrary::SetNetworkStateChangeThreshold(ServerIP, ServerPort, StateChangeThreshold);

    // 성능 임계값 설정
    UMultiServerSyncBPLibrary::SetNetworkPerformanceThresholds(
        ServerIP,
        ServerPort,
        LatencyThreshold,
        JitterThreshold,
        PacketLossThreshold);

    // 로그 출력
    UE_LOG(LogTemp, Log, TEXT("Applied quality settings for %s:%d (Monitoring: %s, Interval: %.2f s)"),
        *ServerIP, ServerPort, bEnableStateMonitoring ? TEXT("Enabled") : TEXT("Disabled"), QualityAssessmentInterval);
}

// 네트워크 품질 상세 정보 출력
void UBP_NetworkLatencyTester::PrintQualityDetails()
{
    if (!bMeasurementActive)
    {
        UE_LOG(LogTemp, Warning, TEXT("No active latency measurement for %s:%d"), *ServerIP, ServerPort);
        return;
    }

    // 로그에 품질 정보 출력
    UE_LOG(LogTemp, Log, TEXT("Network quality for %s:%d - Score: %d/100 (Latency: %d%%, Jitter: %d%%, Loss: %d%%, Stability: %d%%)"),
        *ServerIP, ServerPort, QualityScore, LatencyScore, JitterScore, PacketLossScore, StabilityScore);

    UE_LOG(LogTemp, Log, TEXT("Quality Description: %s"), *DetailedDescription);

    // 권장 사항 출력
    if (Recommendations.Num() > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("Recommendations:"));
        for (const FString& Recommendation : Recommendations)
        {
            UE_LOG(LogTemp, Log, TEXT("  - %s"), *Recommendation);
        }
    }

    // 최신 이벤트 출력
    if (LatestEventType > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("Latest event: %s"), *LatestEventDescription);
    }

    // 화면에도 표시
    if (GEngine)
    {
        FString QualityMessage = FString::Printf(
            TEXT("Network Quality for %s:%d\n")
            TEXT("Score: %d/100 (%s)\n")
            TEXT("Latency: %d%%, Jitter: %d%%\n")
            TEXT("PacketLoss: %d%%, Stability: %d%%\n")
            TEXT("Trend: %s %.1f\n")
            TEXT("Event: %s"),
            *ServerIP, ServerPort,
            QualityScore, *NetworkQuality,
            LatencyScore, JitterScore,
            PacketLossScore, StabilityScore,
            QualityTrend >= 0 ? TEXT("+") : TEXT(""),
            QualityTrend,
            *LatestEventDescription);

        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, QualityMessage);

        // 권장 사항이 있으면 추가 표시
        if (Recommendations.Num() > 0)
        {
            FString RecommendationsMessage = TEXT("Recommendations:\n");
            for (int32 i = 0; i < FMath::Min(3, Recommendations.Num()); ++i)
            {
                RecommendationsMessage += FString::Printf(TEXT("- %s\n"), *Recommendations[i]);
            }

            if (Recommendations.Num() > 3)
            {
                RecommendationsMessage += TEXT("(more...)");
            }

            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, RecommendationsMessage);
        }
    }
}

// 이벤트 모니터링 활성화/비활성화
void UBP_NetworkLatencyTester::SetEventMonitoring(bool bEnable)
{
    bEnableStateMonitoring = bEnable;

    if (bMeasurementActive)
    {
        UMultiServerSyncBPLibrary::SetNetworkStateMonitoring(ServerIP, ServerPort, bEnable);

        // 로그 출력
        UE_LOG(LogTemp, Log, TEXT("Network event monitoring for %s:%d: %s"),
            *ServerIP, ServerPort, bEnable ? TEXT("Enabled") : TEXT("Disabled"));

        // 화면에도 표시
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.0f,
                bEnable ? FColor::Green : FColor::Orange,
                FString::Printf(TEXT("Network event monitoring: %s"),
                    bEnable ? TEXT("Enabled") : TEXT("Disabled")));
        }
    }
}

// 품질 평가 간격 설정
void UBP_NetworkLatencyTester::SetQualityInterval(float IntervalSeconds)
{
    QualityAssessmentInterval = FMath::Clamp(IntervalSeconds, 1.0f, 60.0f);

    if (bMeasurementActive)
    {
        UMultiServerSyncBPLibrary::SetQualityAssessmentInterval(ServerIP, ServerPort, QualityAssessmentInterval);

        // 로그 출력
        UE_LOG(LogTemp, Log, TEXT("Quality assessment interval for %s:%d set to %.2f seconds"),
            *ServerIP, ServerPort, QualityAssessmentInterval);
    }
}

// 성능 임계값 설정
void UBP_NetworkLatencyTester::SetPerformanceThresholds(float NewLatencyThreshold, float NewJitterThreshold, float NewPacketLossThreshold)
{
    LatencyThreshold = FMath::Clamp(NewLatencyThreshold, 50.0f, 500.0f);
    JitterThreshold = FMath::Clamp(NewJitterThreshold, 10.0f, 200.0f);
    PacketLossThreshold = FMath::Clamp(NewPacketLossThreshold, 0.01f, 0.5f);

    if (bMeasurementActive)
    {
        UMultiServerSyncBPLibrary::SetNetworkPerformanceThresholds(
            ServerIP,
            ServerPort,
            LatencyThreshold,
            JitterThreshold,
            PacketLossThreshold);

        // 로그 출력
        UE_LOG(LogTemp, Log, TEXT("Performance thresholds for %s:%d set to: Latency=%.2f ms, Jitter=%.2f ms, Loss=%.2f%%"),
            *ServerIP, ServerPort, LatencyThreshold, JitterThreshold, PacketLossThreshold * 100.0f);
    }
}