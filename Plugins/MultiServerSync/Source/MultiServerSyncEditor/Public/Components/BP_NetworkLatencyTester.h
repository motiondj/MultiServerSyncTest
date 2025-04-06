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
class MULTISERVERSYNC_API UBP_NetworkLatencyTester : public UActorComponent
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

    UPROPERTY(BlueprintReadOnly, Category = "Network Latency")
    FString NetworkQuality;

    UPROPERTY(BlueprintReadOnly, Category = "Network Latency")
    int32 QualityLevel;

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
};