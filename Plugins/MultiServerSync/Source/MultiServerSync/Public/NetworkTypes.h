// NetworkTypes.h
#pragma once

#include "CoreMinimal.h"

// 네트워크 지연 통계 구조체
struct MULTISERVERSYNC_API FNetworkLatencyStats
{
    double MinRTT;            // 최소 RTT (ms)
    double MaxRTT;            // 최대 RTT (ms)
    double AvgRTT;            // 평균 RTT (ms)
    double CurrentRTT;        // 현재 RTT (ms)
    double StandardDeviation; // 표준 편차 (ms)
    double Jitter;            // 지터 (ms)
    int32 SampleCount;        // 샘플 수
    int32 LostPackets;        // 손실된 패킷 수
    TArray<double> RecentRTTs;// 최근 RTT 기록 (통계 계산용)
    double LastUpdateTime;    // 마지막 업데이트 시간

    // 기본 생성자
    FNetworkLatencyStats()
        : MinRTT(FLT_MAX)
        , MaxRTT(0.0)
        , AvgRTT(0.0)
        , CurrentRTT(0.0)
        , StandardDeviation(0.0)
        , Jitter(0.0)
        , SampleCount(0)
        , LostPackets(0)
        , LastUpdateTime(0.0)
    {
        // 최근 RTT 기록을 위한 공간 예약
        RecentRTTs.Reserve(100);
    }

    // 최근 RTT 샘플 추가 및 통계 업데이트
    void AddRTTSample(double RTT);
};