// NetworkTypes.cpp
#include "NetworkTypes.h"
#include "HAL/PlatformTime.h"

void FNetworkLatencyStats::AddRTTSample(double RTT)
{
    // 새 샘플 추가
    RecentRTTs.Add(RTT);
    CurrentRTT = RTT;

    // 최대 샘플 수 제한
    while (RecentRTTs.Num() > 100)
    {
        RecentRTTs.RemoveAt(0);
    }

    // 최소/최대 RTT 업데이트
    MinRTT = FMath::Min(MinRTT, RTT);
    MaxRTT = FMath::Max(MaxRTT, RTT);

    // 평균 계산
    double Sum = 0.0;
    for (double Sample : RecentRTTs)
    {
        Sum += Sample;
    }

    AvgRTT = Sum / RecentRTTs.Num();

    // 표준 편차 계산
    double VarianceSum = 0.0;
    for (double Sample : RecentRTTs)
    {
        double Diff = Sample - AvgRTT;
        VarianceSum += (Diff * Diff);
    }

    StandardDeviation = FMath::Sqrt(VarianceSum / RecentRTTs.Num());

    // 지터 계산 (연속된 샘플 간의 변화량의 평균)
    if (RecentRTTs.Num() > 1)
    {
        double JitterSum = 0.0;
        for (int32 i = 1; i < RecentRTTs.Num(); ++i)
        {
            JitterSum += FMath::Abs(RecentRTTs[i] - RecentRTTs[i - 1]);
        }

        Jitter = JitterSum / (RecentRTTs.Num() - 1);
    }

    // 샘플 수 업데이트
    SampleCount++;

    // 마지막 업데이트 시간 기록
    LastUpdateTime = FPlatformTime::Seconds();
}