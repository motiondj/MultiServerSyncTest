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
    {
        // 최근 RTT 기록을 위한 공간 예약
        RecentRTTs.Reserve(100);

        // 시계열 데이터를 위한 공간 예약
        TimeSeries.Reserve(MaxTimeSeriesSamples);
    }

    // 최근 RTT 샘플 추가 및 통계 업데이트
    void AddRTTSample(double RTT);

    // 추세 분석 수행
    void AnalyzeTrend();

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
};