// NetworkTypes.cpp
#include "NetworkTypes.h"
#include "FSyncLog.h"  // 로그 카테고리를 위해 추가
#include "HAL/PlatformTime.h"

void FNetworkLatencyStats::AddRTTSample(double RTT)
{
    // 이상치 감지 및 필터링
    if (SampleCount > 5 && bFilterOutliers)  // 최소 5개 샘플이 있을 때만 이상치 감지
    {
        // 이상치 임계값 계산 (Tukey의 방법: Q3 + 1.5 * IQR)
        if (RecentRTTs.Num() >= 4)  // 사분위수 계산에 필요한 최소 샘플 수
        {
            TArray<double> SortedRTTs = RecentRTTs;
            SortedRTTs.Sort();

            const int32 Q1Index = SortedRTTs.Num() / 4;
            const int32 Q3Index = SortedRTTs.Num() * 3 / 4;

            const double Q1 = SortedRTTs[Q1Index];
            const double Q3 = SortedRTTs[Q3Index];
            const double IQR = Q3 - Q1;  // 사분위 범위

            OutlierThreshold = Q3 + (1.5 * IQR);

            // 이상치 감지
            if (RTT > OutlierThreshold)
            {
                OutliersDetected++;

                // 이상치 필터링 (극단적인 이상치만 필터링)
                if (RTT > Q3 + (3.0 * IQR))
                {
                    // 로그에 이상치 기록
                    UE_LOG(LogMultiServerSync, Verbose, TEXT("Extreme outlier detected and filtered: %.2f ms (threshold: %.2f ms)"),
                        RTT, OutlierThreshold);

                    // 극단적 이상치는 평균값으로 대체
                    RTT = AvgRTT > 0.0 ? AvgRTT : RTT;
                }
                else
                {
                    // 일반적인 이상치는 로그만 남김
                    UE_LOG(LogMultiServerSync, Verbose, TEXT("Outlier detected: %.2f ms (threshold: %.2f ms)"),
                        RTT, OutlierThreshold);
                }
            }
        }
    }

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

    // 백분위수 계산
    if (RecentRTTs.Num() > 0)
    {
        // RTT 샘플을 정렬된 복사본 생성
        TArray<double> SortedRTTs = RecentRTTs;
        SortedRTTs.Sort();

        // 백분위수 계산하는 람다 함수
        auto CalculatePercentile = [&SortedRTTs](double Percentile) -> double {
            const double Index = (SortedRTTs.Num() - 1) * Percentile;
            const int32 LowerIndex = FMath::FloorToInt(Index);
            const int32 UpperIndex = FMath::CeilToInt(Index);

            if (LowerIndex == UpperIndex)
            {
                return SortedRTTs[LowerIndex];
            }

            const double Weight = Index - LowerIndex;
            return SortedRTTs[LowerIndex] * (1.0 - Weight) + SortedRTTs[UpperIndex] * Weight;
            };

        // 각 백분위수 계산
        Percentile50 = CalculatePercentile(0.50);
        Percentile95 = CalculatePercentile(0.95);
        Percentile99 = CalculatePercentile(0.99);
    }

    // 샘플 수 업데이트
    SampleCount++;

    // 마지막 업데이트 시간 기록
    LastUpdateTime = FPlatformTime::Seconds();

    // 시계열 샘플 추가 (일정 간격으로)
    double CurrentTime = FPlatformTime::Seconds();
    if (LastTimeSeriesSampleTime == 0.0 || (CurrentTime - LastTimeSeriesSampleTime) >= TimeSeriesSampleInterval)
    {
        // 새 시계열 샘플 추가
        FLatencyTimeSeriesSample NewSample(CurrentTime, RTT, Jitter);
        TimeSeries.Add(NewSample);
        LastTimeSeriesSampleTime = CurrentTime;

        // 최대 샘플 수 제한
        while (TimeSeries.Num() > MaxTimeSeriesSamples)
        {
            TimeSeries.RemoveAt(0);
        }

        // 충분한 샘플이 있을 때 추세 분석 수행
        if (TimeSeries.Num() >= 10)
        {
            AnalyzeTrend();
        }
    }
}

// 추세 분석 수행
void FNetworkLatencyStats::AnalyzeTrend()
{
    // 최소 10개 이상의 샘플이 필요
    if (TimeSeries.Num() < 10)
    {
        return;
    }

    double CurrentTime = FPlatformTime::Seconds();

    // 단기 추세 계산 (최근 10개 샘플)
    int32 ShortTermCount = FMath::Min(10, TimeSeries.Num());
    double ShortTermSum = 0.0;
    double ShortTermWeightSum = 0.0;
    double ShortTermFirstAvg = 0.0;
    double ShortTermLastAvg = 0.0;

    // 단기 추세의 가중 기울기 계산
    for (int32 i = TimeSeries.Num() - ShortTermCount; i < TimeSeries.Num(); ++i)
    {
        double Weight = (i - (TimeSeries.Num() - ShortTermCount) + 1.0) / ShortTermCount;
        ShortTermSum += TimeSeries[i].RTT * Weight;
        ShortTermWeightSum += Weight;

        // 처음과 마지막 5개 샘플 비교를 위한 평균
        if (i < TimeSeries.Num() - ShortTermCount + 5)
        {
            ShortTermFirstAvg += TimeSeries[i].RTT / 5.0;
        }
        if (i >= TimeSeries.Num() - 5)
        {
            ShortTermLastAvg += TimeSeries[i].RTT / 5.0;
        }
    }

    // 단기 추세: 최근 5개 샘플 평균 - 이전 5개 샘플 평균
    TrendAnalysis.ShortTermTrend = ShortTermLastAvg - ShortTermFirstAvg;

    // 장기 추세 계산 (전체 시계열)
    double LongTermSum = 0.0;
    double LongTermWeightSum = 0.0;
    double LongTermFirstAvg = 0.0;
    double LongTermLastAvg = 0.0;
    int32 LongTermQuarterSize = TimeSeries.Num() / 4;

    for (int32 i = 0; i < TimeSeries.Num(); ++i)
    {
        double Weight = (i + 1.0) / TimeSeries.Num();
        LongTermSum += TimeSeries[i].RTT * Weight;
        LongTermWeightSum += Weight;

        // 첫 1/4과 마지막 1/4 샘플 비교
        if (i < LongTermQuarterSize)
        {
            LongTermFirstAvg += TimeSeries[i].RTT / LongTermQuarterSize;
        }
        if (i >= TimeSeries.Num() - LongTermQuarterSize)
        {
            LongTermLastAvg += TimeSeries[i].RTT / LongTermQuarterSize;
        }
    }

    // 장기 추세: 마지막 1/4 평균 - 첫 1/4 평균
    TrendAnalysis.LongTermTrend = LongTermLastAvg - LongTermFirstAvg;

    // 변동성 계산 (시계열의 표준 편차)
    double VarianceSum = 0.0;
    double TimeSeriesAvg = AvgRTT; // 이미 계산된 평균 사용

    for (const FLatencyTimeSeriesSample& Sample : TimeSeries)
    {
        double Diff = Sample.RTT - TimeSeriesAvg;
        VarianceSum += (Diff * Diff);
    }

    TrendAnalysis.Volatility = FMath::Sqrt(VarianceSum / TimeSeries.Num());

    // 최상/최악의 RTT 이후 경과 시간
    double WorstRTTTime = 0.0;
    double BestRTTTime = 0.0;
    double WorstRTT = 0.0;
    double BestRTT = FLT_MAX;

    for (const FLatencyTimeSeriesSample& Sample : TimeSeries)
    {
        if (Sample.RTT > WorstRTT)
        {
            WorstRTT = Sample.RTT;
            WorstRTTTime = Sample.Timestamp;
        }
        if (Sample.RTT < BestRTT)
        {
            BestRTT = Sample.RTT;
            BestRTTTime = Sample.Timestamp;
        }
    }

    TrendAnalysis.TimeSinceWorstRTT = CurrentTime - WorstRTTTime;
    TrendAnalysis.TimeSinceBestRTT = CurrentTime - BestRTTTime;

    // 분석 결과 로깅
    UE_LOG(LogMultiServerSync, Verbose, TEXT("Network trend analysis: Short-term: %.2f ms, Long-term: %.2f ms, Volatility: %.2f ms"),
        TrendAnalysis.ShortTermTrend, TrendAnalysis.LongTermTrend, TrendAnalysis.Volatility);
}

// 네트워크 품질 평가 수행
FNetworkQualityAssessment FNetworkLatencyStats::AssessNetworkQuality()
{
    // 이미 현재 품질이 계산되어 있으면 바로 반환
    if (CurrentQuality.QualityScore > 0 &&
        FPlatformTime::Seconds() - LastQualityAssessmentTime < QualityAssessmentInterval)
    {
        return CurrentQuality;
    }

    // 새 품질 평가 생성
    FNetworkQualityAssessment NewQuality;

    // 샘플 수가 너무 적으면 낮은 신뢰도 표시
    if (SampleCount < 10)
    {
        NewQuality.QualityLevel = 0;
        NewQuality.QualityString = TEXT("Insufficient Data");
        NewQuality.DetailedDescription = TEXT("Not enough samples to evaluate network quality reliably.");
        NewQuality.Recommendations.Add(TEXT("Continue latency measurement to gather more data."));
        return NewQuality;
    }

    // 패킷 손실율 계산
    double PacketLossRate = 0.0;
    if (SampleCount + LostPackets > 0)
    {
        PacketLossRate = static_cast<double>(LostPackets) / (SampleCount + LostPackets);
    }

    // 각 지표별 개별 품질 점수 계산 (간소화된 버전)
    int32 LatencyScore = 0;
    int32 JitterScore = 0;
    int32 PacketLossScore = 0;
    int32 StabilityScore = 0;

    // 지연 시간 점수
    if (AvgRTT <= 20.0)
        LatencyScore = 100;
    else if (AvgRTT >= HighLatencyThreshold)
        LatencyScore = 0;
    else
    {
        double NormalizedRTT = (AvgRTT - 20.0) / (HighLatencyThreshold - 20.0);
        LatencyScore = FMath::Clamp<int32>(FMath::RoundToInt(100.0 * (1.0 - NormalizedRTT * NormalizedRTT)), 0, 100);
    }

    // 지터 점수
    if (Jitter <= 5.0)
        JitterScore = 100;
    else if (Jitter >= HighJitterThreshold)
        JitterScore = 0;
    else
    {
        double NormalizedJitter = (Jitter - 5.0) / (HighJitterThreshold - 5.0);
        JitterScore = FMath::Clamp<int32>(FMath::RoundToInt(100.0 * (1.0 - NormalizedJitter)), 0, 100);
    }

    // 패킷 손실 점수
    const double LowLossThreshold = 0.001;  // 0.1%
    if (PacketLossRate <= LowLossThreshold)
        PacketLossScore = 100;
    else if (PacketLossRate >= HighPacketLossThreshold)
        PacketLossScore = 0;
    else
    {
        double NormalizedLoss = (PacketLossRate - LowLossThreshold) / (HighPacketLossThreshold - LowLossThreshold);
        PacketLossScore = FMath::Clamp<int32>(FMath::RoundToInt(100.0 * (1.0 - pow(NormalizedLoss, 0.7))), 0, 100);
    }

    // 안정성 점수
    double VolatilityScore = 100.0;
    if (TrendAnalysis.Volatility > 0.0)
    {
        VolatilityScore = FMath::Clamp(100.0 - (TrendAnalysis.Volatility / 50.0) * 100.0, 0.0, 100.0);
    }

    double TrendScore = 50.0;
    if (TrendAnalysis.LongTermTrend < 0.0)
    {
        TrendScore = 50.0 + FMath::Min(fabs(TrendAnalysis.LongTermTrend) / 10.0 * 50.0, 50.0);
    }
    else if (TrendAnalysis.LongTermTrend > 0.0)
    {
        TrendScore = 50.0 - FMath::Min(TrendAnalysis.LongTermTrend / 10.0 * 50.0, 50.0);
    }

    StabilityScore = FMath::Clamp(FMath::RoundToInt(VolatilityScore * 0.7 + TrendScore * 0.3), 0, 100);

    // 품질 평가 구성
    NewQuality.LatencyScore = LatencyScore;
    NewQuality.JitterScore = JitterScore;
    NewQuality.PacketLossScore = PacketLossScore;
    NewQuality.StabilityScore = StabilityScore;

    // 종합 품질 점수 계산
    NewQuality.QualityScore = static_cast<int32>(
        LatencyScore * 0.4f +     // 지연 시간 40% 비중
        JitterScore * 0.3f +      // 지터 30% 비중
        PacketLossScore * 0.2f +  // 패킷 손실 20% 비중
        StabilityScore * 0.1f     // 안정성 10% 비중
        );

    // 품질 레벨 결정
    if (NewQuality.QualityScore >= 80)
    {
        NewQuality.QualityLevel = 3;
        NewQuality.QualityString = TEXT("Excellent");
    }
    else if (NewQuality.QualityScore >= 60)
    {
        NewQuality.QualityLevel = 2;
        NewQuality.QualityString = TEXT("Good");
    }
    else if (NewQuality.QualityScore >= 40)
    {
        NewQuality.QualityLevel = 1;
        NewQuality.QualityString = TEXT("Fair");
    }
    else
    {
        NewQuality.QualityLevel = 0;
        NewQuality.QualityString = TEXT("Poor");
    }

    // 상세 설명 및 권장 사항 추가
    NewQuality.DetailedDescription = FString::Printf(
        TEXT("Network quality is %s (%d/100). RTT: %.2f ms, Jitter: %.2f ms, Packet Loss: %.2f%%."),
        *NewQuality.QualityString,
        NewQuality.QualityScore,
        AvgRTT,
        Jitter,
        PacketLossRate * 100.0
    );

    // 품질 변화 추세 계산
    if (QualityHistory.Num() > 0)
    {
        int32 PrevScore = QualityHistory.Last().QualityScore;
        NewQuality.QualityChangeTrend = static_cast<float>(NewQuality.QualityScore - PrevScore);
    }

    // 품질 평가 결과 저장
    CurrentQuality = NewQuality;
    LastQualityAssessmentTime = FPlatformTime::Seconds();

    return NewQuality;
}

// 네트워크 상태 변화 감지 (계속)
ENetworkEventType FNetworkLatencyStats::DetectStateChange(const FNetworkQualityAssessment& NewQuality, const FNetworkQualityAssessment& PreviousQuality)
{
    // 품질 점수 변화 감지
    int32 ScoreDifference = NewQuality.QualityScore - PreviousQuality.QualityScore;

    // 변화가 임계값보다 작으면 이벤트 없음
    if (FMath::Abs(ScoreDifference) < StateChangeThreshold)
    {
        // 특별한 지표 확인 (이상값 감지)
        if (NewQuality.LatencyScore < 30 && PreviousQuality.LatencyScore >= 30)
        {
            return ENetworkEventType::HighLatency;
        }

        if (NewQuality.JitterScore < 30 && PreviousQuality.JitterScore >= 30)
        {
            return ENetworkEventType::HighJitter;
        }

        if (NewQuality.PacketLossScore < 30 && PreviousQuality.PacketLossScore >= 30)
        {
            return ENetworkEventType::HighPacketLoss;
        }

        return ENetworkEventType::None;
    }

    // 품질 개선
    if (ScoreDifference > 0)
    {
        // 매우 큰 개선이고 품질이 좋으면 안정화로 판단
        if (ScoreDifference > StateChangeThreshold * 2 && NewQuality.QualityScore >= 60)
        {
            return ENetworkEventType::Stabilized;
        }

        return ENetworkEventType::QualityImproved;
    }
    // 품질 저하
    else
    {
        // 심각한 품질 저하인지 확인
        if (ScoreDifference < -StateChangeThreshold * 2)
        {
            // 품질이 매우 나쁘면 연결 문제로 판단
            if (NewQuality.QualityScore < 20)
            {
                return ENetworkEventType::ConnectionLost;
            }

            return ENetworkEventType::QualityDegraded;
        }

        return ENetworkEventType::QualityDegraded;
    }
}

// 이벤트 추가 및 관리
void FNetworkLatencyStats::AddNetworkEvent(ENetworkEventType EventType, double Timestamp)
{
    // 이벤트가 없으면 추가하지 않음
    if (EventType == ENetworkEventType::None)
        return;

    // 이벤트 기록에 추가
    RecentEvents.Add(EventType);

    // 최대 이벤트 기록 크기 유지
    while (RecentEvents.Num() > MaxEventHistory)
    {
        RecentEvents.RemoveAt(0);
    }

    // 현재 품질 이벤트 정보 업데이트
    CurrentQuality.LatestEvent = EventType;
    CurrentQuality.EventTimestamp = Timestamp;
}

// 가장 최근 이벤트 얻기
ENetworkEventType FNetworkLatencyStats::GetLatestEvent() const
{
    if (RecentEvents.Num() > 0)
    {
        return RecentEvents.Last();
    }

    return ENetworkEventType::None;
}