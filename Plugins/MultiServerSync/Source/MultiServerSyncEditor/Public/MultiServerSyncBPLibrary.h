// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
// 여기에 필요한 추가 헤더들을 포함하세요

// 항상 generated.h 파일이 마지막 include가 되어야 합니다
#include "MultiServerSyncBPLibrary.generated.h"

/**
 * 멀티서버 동기화 블루프린트 함수 라이브러리
 * 블루프린트에서 멀티서버 동기화 기능에 접근할 수 있는 함수들을 제공합니다.
 */
UCLASS()
class MULTISERVERSYNCEDITOR_API UMultiServerSyncBPLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * 플러그인이 초기화되었는지 확인합니다.
     * @return 초기화된 경우 true
     */
    UFUNCTION(BlueprintPure, Category = "MultiServerSync")
    static bool IsInitialized();

    /**
     * 현재 마스터 노드인지 확인합니다.
     * @return 마스터인 경우 true
     */
    UFUNCTION(BlueprintPure, Category = "MultiServerSync|MasterSlave")
    static bool IsMasterNode();

    /**
     * 현재 마스터 ID를 가져옵니다.
     * @return 마스터 ID
     */
    UFUNCTION(BlueprintPure, Category = "MultiServerSync|MasterSlave")
    static FString GetMasterNodeId();

    /**
     * 마스터 선출을 시작합니다.
     * @return 선출이 시작된 경우 true
     */
    UFUNCTION(BlueprintCallable, Category = "MultiServerSync|MasterSlave")
    static bool StartMasterElection();

    /**
     * 현재 노드의 마스터 우선순위를 설정합니다.
     * 높을수록 마스터가 될 가능성이 높아집니다.
     * @param Priority 우선순위 값
     */
    UFUNCTION(BlueprintCallable, Category = "MultiServerSync|MasterSlave")
    static void SetMasterPriority(float Priority);

    /**
     * 서버 탐색을 시작합니다.
     * @return 탐색이 시작된 경우 true
     */
    UFUNCTION(BlueprintCallable, Category = "MultiServerSync|Network")
    static bool DiscoverServers();

    /**
     * 발견된 서버 목록을 가져옵니다.
     * @return 서버 ID 목록
     */
    UFUNCTION(BlueprintPure, Category = "MultiServerSync|Network")
    static TArray<FString> GetDiscoveredServers();

    /**
     * 특정 서버에 대한 네트워크 지연 측정을 시작합니다.
     * @param ServerIP 서버 IP 주소
     * @param ServerPort 서버 포트
     * @param IntervalSeconds 측정 간격 (초)
     * @param SampleCount 측정할 샘플 수 (0이면 무제한)
     * @param bDynamicSampling 동적 샘플링 활성화 여부
     * @param MinIntervalSeconds 최소 샘플링 간격 (초, 동적 샘플링 시)
     * @param MaxIntervalSeconds 최대 샘플링 간격 (초, 동적 샘플링 시)
     */
    UFUNCTION(BlueprintCallable, Category = "MultiServerSync|Network")
    static void StartNetworkLatencyMeasurement(
        const FString& ServerIP,
        int32 ServerPort,
        float IntervalSeconds = 1.0f,
        int32 SampleCount = 0,
        bool bDynamicSampling = false,
        float MinIntervalSeconds = 0.1f,
        float MaxIntervalSeconds = 5.0f);

    /**
     * 특정 서버에 대한 네트워크 지연 측정을 중지합니다.
     * @param ServerIP 서버 IP 주소
     * @param ServerPort 서버 포트
     */
    UFUNCTION(BlueprintCallable, Category = "MultiServerSync|Network")
    static void StopNetworkLatencyMeasurement(const FString& ServerIP, int32 ServerPort);

    /**
     * 특정 서버의 네트워크 지연 통계를 가져옵니다.
     * @param ServerIP 서버 IP 주소
     * @param ServerPort 서버 포트
     * @param MinRTT 최소 왕복 시간 (ms)
     * @param MaxRTT 최대 왕복 시간 (ms)
     * @param AvgRTT 평균 왕복 시간 (ms)
     * @param Jitter 지터 (ms)
     * @param PacketLoss 패킷 손실 비율 (0.0 ~ 1.0)
     * @param Percentile50 50번째 백분위수 (중앙값) (ms)
     * @param Percentile95 95번째 백분위수 (ms)
     * @param Percentile99 99번째 백분위수 (ms)
     * @return 통계가 유효한지 여부
     */
    UFUNCTION(BlueprintCallable, Category = "MultiServerSync|Network")
    static bool GetNetworkLatencyStats(const FString& ServerIP, int32 ServerPort,
        float& MinRTT, float& MaxRTT, float& AvgRTT,
        float& Jitter, float& PacketLoss,
        float& Percentile50, float& Percentile95, float& Percentile99);

    /**
     * 이상치 필터링 설정을 변경합니다.
     * @param ServerIP 서버 IP 주소
     * @param ServerPort 서버 포트
     * @param bEnableFiltering 필터링 활성화 여부
     */
    UFUNCTION(BlueprintCallable, Category = "MultiServerSync|Network")
    static void SetOutlierFiltering(const FString& ServerIP, int32 ServerPort, bool bEnableFiltering);

    /**
     * 이상치 통계를 가져옵니다.
     * @param ServerIP 서버 IP 주소
     * @param ServerPort 서버 포트
     * @param OutliersDetected 감지된 이상치 수
     * @param OutlierThreshold 이상치 임계값 (ms)
     * @return 통계가 유효한지 여부
     */
    UFUNCTION(BlueprintCallable, Category = "MultiServerSync|Network")
    static bool GetOutlierStats(const FString& ServerIP, int32 ServerPort, int32& OutliersDetected, float& OutlierThreshold);

    /**
 * 시계열 샘플링 간격을 설정합니다.
 * @param ServerIP 서버 IP 주소
 * @param ServerPort 서버 포트
 * @param IntervalSeconds 샘플링 간격 (초)
 */
    UFUNCTION(BlueprintCallable, Category = "MultiServerSync|Network")
    static void SetTimeSeriesSampleInterval(const FString& ServerIP, int32 ServerPort, float IntervalSeconds);

    /**
     * 시계열 데이터를 가져옵니다. (간소화된 버전)
     * @param ServerIP 서버 IP 주소
     * @param ServerPort 서버 포트
     * @param OutTimestamps 타임스탬프 배열 (초)
     * @param OutRTTValues RTT 값 배열 (ms)
     * @return 데이터가 유효한지 여부
     */
    UFUNCTION(BlueprintCallable, Category = "MultiServerSync|Network")
    static bool GetTimeSeriesData(const FString& ServerIP, int32 ServerPort,
        TArray<float>& OutTimestamps, TArray<float>& OutRTTValues);

    /**
     * 네트워크 추세 분석 결과를 가져옵니다.
     * @param ServerIP 서버 IP 주소
     * @param ServerPort 서버 포트
     * @param ShortTermTrend 단기 추세 (양수: 악화, 음수: 개선)
     * @param LongTermTrend 장기 추세 (양수: 악화, 음수: 개선)
     * @param Volatility 변동성 (값이 클수록 불안정)
     * @param TimeSinceWorstRTT 최악의 RTT 이후 경과 시간 (초)
     * @param TimeSinceBestRTT 최상의 RTT 이후 경과 시간 (초)
     * @return 데이터가 유효한지 여부
     */
    UFUNCTION(BlueprintCallable, Category = "MultiServerSync|Network")
    static bool GetNetworkTrendAnalysis(const FString& ServerIP, int32 ServerPort,
        float& ShortTermTrend, float& LongTermTrend, float& Volatility,
        float& TimeSinceWorstRTT, float& TimeSinceBestRTT);

    /**
     * 특정 서버의 네트워크 품질을 평가합니다.
     * @param ServerIP 서버 IP 주소
     * @param ServerPort 서버 포트
     * @param QualityString 품질 설명 (Poor/Fair/Good/Excellent)
     * @return 품질 점수 (0: 불량, 1: 보통, 2: 좋음, 3: 매우 좋음)
     */
    UFUNCTION(BlueprintCallable, Category = "MultiServerSync|Network")
    static int32 EvaluateNetworkQuality(const FString& ServerIP, int32 ServerPort, FString& QualityString);

    /**
     * PTP 타임스탬프를 생성합니다.
     * 테스트 및 디버깅용
     * @return 현재 PTP 타임스탬프 (마이크로초)
     */
    UFUNCTION(BlueprintPure, Category = "MultiServerSync|TimeSync")
    static int64 GeneratePTPTimestamp();

    /**
     * 마스터-슬레이브 간 시간 오프셋을 가져옵니다.
     * @return 시간 오프셋 (마이크로초)
     */
    UFUNCTION(BlueprintPure, Category = "MultiServerSync|TimeSync")
    static float GetTimeOffset();

    /**
     * 마스터와의 시간 동기화 상태를 확인합니다.
     * @return 동기화 상태 (0: 비동기화, 1: 동기화 중, 2: 동기화됨)
     */
    UFUNCTION(BlueprintPure, Category = "MultiServerSync|TimeSync")
    static int32 GetSyncStatus();
};