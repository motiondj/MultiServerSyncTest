// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BP_NetworkLatencyTester.generated.h"

/**
 * 네트워크 지연 측정을 위한 액터 컴포넌트
 * 특정 서버의 네트워크 지연 시간을 측정하고 결과를 표시합니다.
 */
UCLASS(Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MULTISERVERSYNCEDITOR_API UBP_NetworkLatencyTester : public UActorComponent
{
    GENERATED_BODY()

public:
    // 기본 생성자
    UBP_NetworkLatencyTester();

    // 서버 IP 주소
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network Latency")
    FString ServerIP;

    // 서버 포트
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network Latency", meta = (ClampMin = "1", ClampMax = "65535"))
    int32 ServerPort;

    // 측정 간격 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network Latency", meta = (ClampMin = "0.1", ClampMax = "10.0"))
    float MeasurementInterval;

    // 자동 시작 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network Latency")
    bool bAutoStart;

    // 동적 샘플링 활성화 여부
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network Latency")
    bool bDynamicSampling;

    // 최소 샘플링 간격 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network Latency", meta = (ClampMin = "0.1", ClampMax = "1.0", EditCondition = "bDynamicSampling"))
    float MinSamplingInterval;

    // 최대 샘플링 간격 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network Latency", meta = (ClampMin = "1.0", ClampMax = "10.0", EditCondition = "bDynamicSampling"))
    float MaxSamplingInterval;

    // 통계 업데이트 간격 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network Latency", meta = (ClampMin = "0.1", ClampMax = "10.0"))
    float StatsUpdateInterval;

    // 지연 시간 통계
    UPROPERTY(BlueprintReadOnly, Category = "Network Latency")
    float MinRTT;

    UPROPERTY(BlueprintReadOnly, Category = "Network Latency")
    float MaxRTT;

    UPROPERTY(BlueprintReadOnly, Category = "Network Latency")
    float AvgRTT;

    UPROPERTY(BlueprintReadOnly, Category = "Network Latency")
    float Jitter;

    UPROPERTY(BlueprintReadOnly, Category = "Network Latency")
    float PacketLoss;

    // 백분위수 통계
    UPROPERTY(BlueprintReadOnly, Category = "Network Latency")
    float Percentile50;

    UPROPERTY(BlueprintReadOnly, Category = "Network Latency")
    float Percentile95;

    UPROPERTY(BlueprintReadOnly, Category = "Network Latency")
    float Percentile99;

    UPROPERTY(BlueprintReadOnly, Category = "Network Latency")
    FString NetworkQuality;

    UPROPERTY(BlueprintReadOnly, Category = "Network Latency")
    int32 QualityLevel;

    // 이상치 통계
    UPROPERTY(BlueprintReadOnly, Category = "Network Latency|Outliers")
    int32 OutliersDetected;

    UPROPERTY(BlueprintReadOnly, Category = "Network Latency|Outliers")
    float OutlierThreshold;

    // 이상치 필터링 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network Latency|Outliers")
    bool bEnableOutlierFiltering;

    // 이상치 필터링 설정 변경
    UFUNCTION(BlueprintCallable, Category = "Network Latency|Outliers")
    void SetOutlierFiltering(bool bEnable);

    // 상세 품질 점수
    UPROPERTY(BlueprintReadOnly, Category = "Network Latency|Quality")
    int32 QualityScore;

    // 개별 지표 점수
    UPROPERTY(BlueprintReadOnly, Category = "Network Latency|Quality")
    int32 LatencyScore;

    UPROPERTY(BlueprintReadOnly, Category = "Network Latency|Quality")
    int32 JitterScore;

    UPROPERTY(BlueprintReadOnly, Category = "Network Latency|Quality")
    int32 PacketLossScore;

    UPROPERTY(BlueprintReadOnly, Category = "Network Latency|Quality")
    int32 StabilityScore;

    // 품질 추세 (-100 ~ +100, 양수: 개선, 음수: 악화)
    UPROPERTY(BlueprintReadOnly, Category = "Network Latency|Quality")
    float QualityTrend;

    // 상세 설명
    UPROPERTY(BlueprintReadOnly, Category = "Network Latency|Quality")
    FString DetailedDescription;

    // 권장 사항
    UPROPERTY(BlueprintReadOnly, Category = "Network Latency|Quality")
    TArray<FString> Recommendations;

    // 네트워크 이벤트
    UPROPERTY(BlueprintReadOnly, Category = "Network Latency|Events")
    int32 LatestEventType;

    UPROPERTY(BlueprintReadOnly, Category = "Network Latency|Events")
    FString LatestEventDescription;

    // 품질 평가 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network Latency|Settings")
    bool bEnableStateMonitoring;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network Latency|Settings", meta = (ClampMin = "1.0", ClampMax = "60.0"))
    float QualityAssessmentInterval;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network Latency|Settings", meta = (ClampMin = "5.0", ClampMax = "50.0"))
    float StateChangeThreshold;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network Latency|Settings", meta = (ClampMin = "50.0", ClampMax = "500.0"))
    float LatencyThreshold;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network Latency|Settings", meta = (ClampMin = "10.0", ClampMax = "200.0"))
    float JitterThreshold;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network Latency|Settings", meta = (ClampMin = "0.01", ClampMax = "0.5", Units = "%"))
    float PacketLossThreshold;

protected:
    // 컴포넌트 초기화 시 호출
    virtual void BeginPlay() override;

    // 컴포넌트 종료 시 호출
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // 틱 함수
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // 통계 업데이트 타이머
    FTimerHandle StatsUpdateTimerHandle;

    // 통계 업데이트 함수
    UFUNCTION()
    void UpdateStats();

    // 측정 활성화 상태
    bool bMeasurementActive;

public:
    // 측정 시작
    UFUNCTION(BlueprintCallable, Category = "Network Latency")
    void StartMeasurement();

    // 측정 중지
    UFUNCTION(BlueprintCallable, Category = "Network Latency")
    void StopMeasurement();

    // 현재 통계 출력
    UFUNCTION(BlueprintCallable, Category = "Network Latency")
    void PrintCurrentStats();

    // 측정 상태 확인
    UFUNCTION(BlueprintPure, Category = "Network Latency")
    bool IsMeasurementActive() const { return bMeasurementActive; }

    // 네트워크 품질 텍스트 가져오기
    UFUNCTION(BlueprintPure, Category = "Network Latency")
    FString GetNetworkQualityText() const { return NetworkQuality; }

    // 네트워크 품질 레벨 가져오기 (0-3)
    UFUNCTION(BlueprintPure, Category = "Network Latency")
    int32 GetNetworkQualityLevel() const { return QualityLevel; }

    // 품질 평가 설정 적용
    UFUNCTION(BlueprintCallable, Category = "Network Latency|Quality")
    void ApplyQualitySettings();

    // 네트워크 품질 상세 정보 출력
    UFUNCTION(BlueprintCallable, Category = "Network Latency|Quality")
    void PrintQualityDetails();

    // 이벤트 모니터링 활성화/비활성화
    UFUNCTION(BlueprintCallable, Category = "Network Latency|Events")
    void SetEventMonitoring(bool bEnable);

    // 품질 평가 간격 설정
    UFUNCTION(BlueprintCallable, Category = "Network Latency|Settings")
    void SetQualityInterval(float IntervalSeconds);

    // 성능 임계값 설정
    UFUNCTION(BlueprintCallable, Category = "Network Latency|Settings")
    void SetPerformanceThresholds(float NewLatencyThreshold, float NewJitterThreshold, float NewPacketLossThreshold);
};