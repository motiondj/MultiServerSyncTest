// NetworkTypes.h
#pragma once

#include "CoreMinimal.h"

/**
 * 네트워크 지연 시간의 단일 시계열 샘플
 * 특정 시점의 지연 측정값과 시간 정보를 저장
 */
struct MULTISERVERSYNC_API FLatencyTimeSeriesSample
{
    double Timestamp;     // 샘플 시간 (초)
    double RTT;           // 측정된 RTT (ms)
    double Jitter;        // 측정 시점의 지터 (ms)

    // 기본 생성자
    FLatencyTimeSeriesSample()
        : Timestamp(0.0)
        , RTT(0.0)
        , Jitter(0.0)
    {
    }

    // 값으로 초기화하는 생성자
    FLatencyTimeSeriesSample(double InTimestamp, double InRTT, double InJitter)
        : Timestamp(InTimestamp)
        , RTT(InRTT)
        , Jitter(InJitter)
    {
    }
};

/**
 * 네트워크 추세 분석 결과
 * 측정된 지연 시간의 추세와 변화를 나타내는 지표
 */
struct MULTISERVERSYNC_API FNetworkTrendAnalysis
{
    double ShortTermTrend;    // 단기 추세 (양수: 악화, 음수: 개선)
    double LongTermTrend;     // 장기 추세 (양수: 악화, 음수: 개선)
    double Volatility;        // 변동성 (값이 클수록 불안정)
    double TimeSinceWorstRTT; // 최악의 RTT 이후 경과 시간 (초)
    double TimeSinceBestRTT;  // 최상의 RTT 이후 경과 시간 (초)

    // 기본 생성자
    FNetworkTrendAnalysis()
        : ShortTermTrend(0.0)
        , LongTermTrend(0.0)
        , Volatility(0.0)
        , TimeSinceWorstRTT(0.0)
        , TimeSinceBestRTT(0.0)
    {
    }
};

/**
 * 네트워크 이벤트 유형
 * 네트워크 상태 변화를 나타내는 이벤트
 */
enum class ENetworkEventType : uint8
{
    None,               // 이벤트 없음
    QualityImproved,    // 품질 개선
    QualityDegraded,    // 품질 저하
    ConnectionLost,     // 연결 끊김
    ConnectionRestored, // 연결 복구
    HighLatency,        // 높은 지연 시간
    HighJitter,         // 높은 지터
    HighPacketLoss,     // 높은 패킷 손실
    Stabilized          // 네트워크 안정화
};

/**
 * 네트워크 품질 평가 결과 구조체
 * 다양한 지표를 기반으로 네트워크 상태를 종합적으로 평가
 */
struct MULTISERVERSYNC_API FNetworkQualityAssessment
{
    int32 QualityScore;        // 종합 품질 점수 (0-100)
    int32 QualityLevel;        // 품질 레벨 (0: Poor, 1: Fair, 2: Good, 3: Excellent)
    FString QualityString;     // 품질 설명

    // 개별 지표 점수 (0-100)
    int32 LatencyScore;        // 지연 시간 점수
    int32 JitterScore;         // 지터 점수
    int32 PacketLossScore;     // 패킷 손실 점수
    int32 StabilityScore;      // 안정성 점수

    // 추가 정보
    FString DetailedDescription;   // 상세 설명
    TArray<FString> Recommendations; // 권장 사항

    // 이전 품질 평가와의 변화
    float QualityChangeTrend;  // 품질 변화 추세 (양수: 개선, 음수: 악화)

    // 네트워크 상태 이벤트
    ENetworkEventType LatestEvent;  // 최근 발생한 이벤트
    double EventTimestamp;           // 이벤트 발생 시간

    // 기본 생성자
    FNetworkQualityAssessment()
        : QualityScore(0)
        , QualityLevel(0)
        , QualityString(TEXT("Unknown"))
        , LatencyScore(0)
        , JitterScore(0)
        , PacketLossScore(0)
        , StabilityScore(0)
        , DetailedDescription(TEXT(""))
        , QualityChangeTrend(0.0f)
        , LatestEvent(ENetworkEventType::None)
        , EventTimestamp(0.0)
    {
        Recommendations.Empty();
    }

    // 품질 정보를 문자열로 변환
    FString ToString() const
    {
        return FString::Printf(TEXT("Quality: %s (%d/100) - Latency: %d%%, Jitter: %d%%, Loss: %d%%, Stability: %d%%"),
            *QualityString, QualityScore, LatencyScore, JitterScore, PacketLossScore, StabilityScore);
    }

    // 이벤트 유형을 문자열로 변환
    static FString EventTypeToString(ENetworkEventType EventType)
    {
        switch (EventType)
        {
        case ENetworkEventType::QualityImproved:
            return TEXT("Quality Improved");
        case ENetworkEventType::QualityDegraded:
            return TEXT("Quality Degraded");
        case ENetworkEventType::ConnectionLost:
            return TEXT("Connection Lost");
        case ENetworkEventType::ConnectionRestored:
            return TEXT("Connection Restored");
        case ENetworkEventType::HighLatency:
            return TEXT("High Latency");
        case ENetworkEventType::HighJitter:
            return TEXT("High Jitter");
        case ENetworkEventType::HighPacketLoss:
            return TEXT("High Packet Loss");
        case ENetworkEventType::Stabilized:
            return TEXT("Network Stabilized");
        default:
            return TEXT("None");
        }
    }
};

// 네트워크 지연 통계 구조체
struct MULTISERVERSYNC_API FNetworkLatencyStats
{
    double MinRTT;            // 최소 RTT (ms)
    double MaxRTT;            // 최대 RTT (ms)
    double AvgRTT;            // 평균 RTT (ms)
    double CurrentRTT;        // 현재 RTT (ms)
    double StandardDeviation; // 표준 편차 (ms)
    double Jitter;            // 지터 (ms)
    double Percentile50;      // 50번째 백분위수 (중앙값) (ms)
    double Percentile95;      // 95번째 백분위수 (ms)
    double Percentile99;      // 99번째 백분위수 (ms)
    int32 SampleCount;        // 샘플 수
    int32 LostPackets;        // 손실된 패킷 수
    TArray<double> RecentRTTs;// 최근 RTT 기록 (통계 계산용)
    double LastUpdateTime;    // 마지막 업데이트 시간

    // 이상치 관련 필드 (순서 변경)
    int32 OutliersDetected;       // 감지된 이상치 수
    double OutlierThreshold;      // 이상치 임계값 (ms)
    bool bFilterOutliers;         // 이상치 필터링 활성화 여부

    // 시계열 및 추세 분석 관련 필드
    TArray<FLatencyTimeSeriesSample> TimeSeries;   // 시계열 샘플 데이터
    int32 MaxTimeSeriesSamples;                    // 최대 시계열 샘플 수
    double TimeSeriesSampleInterval;               // 시계열 샘플 간격 (초)
    double LastTimeSeriesSampleTime;               // 마지막 시계열 샘플 시간
    FNetworkTrendAnalysis TrendAnalysis;           // 추세 분석 결과

    // 네트워크 상태 평가 관련 필드 (새로 추가)
    FNetworkQualityAssessment CurrentQuality;      // 현재 네트워크 품질 평가
    TArray<FNetworkQualityAssessment> QualityHistory; // 품질 평가 히스토리
    int32 MaxQualityHistoryCount;                  // 최대 품질 평가 기록 수
    double QualityAssessmentInterval;              // 품질 평가 간격 (초)
    double LastQualityAssessmentTime;              // 마지막 품질 평가 시간

    // 상태 변화 감지 관련 필드 (새로 추가)
    bool bMonitorStateChanges;                     // 상태 변화 모니터링 활성화 여부
    double StateChangeThreshold;                   // 상태 변화 감지 임계값
    TArray<ENetworkEventType> RecentEvents;        // 최근 이벤트 기록
    int32 MaxEventHistory;                         // 최대 이벤트 기록 수

    // 성능 지표 임계값 (새로 추가)
    double HighLatencyThreshold;                   // 높은 지연 시간 임계값 (ms)
    double HighJitterThreshold;                    // 높은 지터 임계값 (ms)
    double HighPacketLossThreshold;                // 높은 패킷 손실 임계값 (비율)

    // 기본 생성자
    FNetworkLatencyStats()
        : MinRTT(FLT_MAX)
        , MaxRTT(0.0)
        , AvgRTT(0.0)
        , CurrentRTT(0.0)
        , StandardDeviation(0.0)
        , Jitter(0.0)
        , Percentile50(0.0)
        , Percentile95(0.0)
        , Percentile99(0.0)
        , SampleCount(0)
        , LostPackets(0)
        , RecentRTTs()
        , LastUpdateTime(0.0)
        , OutliersDetected(0)
        , OutlierThreshold(0.0)
        , bFilterOutliers(true)
        , TimeSeries()
        , MaxTimeSeriesSamples(300)            // 기본값: 5분(300초)치 데이터 저장
        , TimeSeriesSampleInterval(1.0)        // 기본값: 1초마다 샘플링
        , LastTimeSeriesSampleTime(0.0)
        , TrendAnalysis()
        , CurrentQuality()
        , MaxQualityHistoryCount(20)           // 기본값: 최근 20개 품질 평가 기록
        , QualityAssessmentInterval(5.0)       // 기본값: 5초마다 품질 평가
        , LastQualityAssessmentTime(0.0)
        , bMonitorStateChanges(true)           // 기본값: 상태 변화 모니터링 활성화
        , StateChangeThreshold(15.0)           // 기본값: 품질 점수 15점 이상 변화 시 이벤트 발생
        , MaxEventHistory(10)                  // 기본값: 최근 10개 이벤트 기록
        , HighLatencyThreshold(150.0)          // 기본값: 150ms 이상을 높은 지연으로 간주
        , HighJitterThreshold(50.0)            // 기본값: 50ms 이상을 높은 지터로 간주
        , HighPacketLossThreshold(0.05)        // 기본값: 5% 이상을 높은 패킷 손실로 간주
    {
        // 최근 RTT 기록을 위한 공간 예약
        RecentRTTs.Reserve(100);

        // 시계열 데이터를 위한 공간 예약
        TimeSeries.Reserve(MaxTimeSeriesSamples);

        // 품질 평가 히스토리를 위한 공간 예약
        QualityHistory.Reserve(MaxQualityHistoryCount);

        // 이벤트 기록을 위한 공간 예약
        RecentEvents.Reserve(MaxEventHistory);
    }

    // 최근 RTT 샘플 추가 및 통계 업데이트
    void AddRTTSample(double RTT);

    // 추세 분석 수행
    void AnalyzeTrend();

    // 네트워크 품질 평가 수행 (새로 추가)
    FNetworkQualityAssessment AssessNetworkQuality();

    // 네트워크 상태 변화 감지 (새로 추가)
    ENetworkEventType DetectStateChange(const FNetworkQualityAssessment& NewQuality, const FNetworkQualityAssessment& PreviousQuality);

    // 이벤트 추가 및 관리 (새로 추가)
    void AddNetworkEvent(ENetworkEventType EventType, double Timestamp);

    // 가장 최근 이벤트 얻기 (새로 추가)
    ENetworkEventType GetLatestEvent() const;

    // 시계열 샘플 간격 설정
    void SetTimeSeriesSampleInterval(double IntervalSeconds)
    {
        TimeSeriesSampleInterval = FMath::Max(0.1, IntervalSeconds);
    }

    // 최대 시계열 샘플 수 설정
    void SetMaxTimeSeriesSamples(int32 MaxSamples)
    {
        MaxTimeSeriesSamples = FMath::Max(10, MaxSamples);
        TimeSeries.Reserve(MaxTimeSeriesSamples);
    }

    // 시계열 데이터 가져오기
    const TArray<FLatencyTimeSeriesSample>& GetTimeSeries() const
    {
        return TimeSeries;
    }

    // 추세 분석 결과 가져오기
    const FNetworkTrendAnalysis& GetTrendAnalysis() const
    {
        return TrendAnalysis;
    }

    // 품질 평가 간격 설정 (새로 추가)
    void SetQualityAssessmentInterval(double IntervalSeconds)
    {
        QualityAssessmentInterval = FMath::Max(1.0, IntervalSeconds);
    }

    // 품질 평가 히스토리 가져오기 (새로 추가)
    const TArray<FNetworkQualityAssessment>& GetQualityHistory() const
    {
        return QualityHistory;
    }

    // 성능 지표 임계값 설정 (새로 추가)
    void SetPerformanceThresholds(double LatencyThreshold, double JitterThreshold, double PacketLossThreshold)
    {
        HighLatencyThreshold = FMath::Max(50.0, LatencyThreshold);      // 최소 50ms
        HighJitterThreshold = FMath::Max(10.0, JitterThreshold);        // 최소 10ms
        HighPacketLossThreshold = FMath::Clamp(PacketLossThreshold, 0.01, 0.5); // 1%~50%
    }
};