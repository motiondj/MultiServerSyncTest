#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IPv4/IPv4Endpoint.h" // 이 줄을 추가하세요

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
 * 메시지 확인 상태 열거형
 * 메시지의 전송/수신 상태를 표현합니다.
 */
enum class EMessageAckStatus : uint8
{
    None,       // 초기 상태
    Sent,       // 메시지가 전송됨
    Received,   // 메시지가 수신됨
    Acknowledged,// 메시지가 확인됨
    Failed,     // 메시지 전송 실패
    Timeout     // 메시지 확인 시간 초과
};

/**
 * 메시지 확인 데이터 구조체
 * 메시지 시퀀스와 확인 상태를 추적합니다.
 */
struct MULTISERVERSYNC_API FMessageAckData
{
    uint16 SequenceNumber;          // 메시지 시퀀스 번호
    EMessageAckStatus Status;       // 메시지 상태
    double SentTime;                // 전송 시간
    double LastAttemptTime;         // 마지막 시도 시간
    int32 AttemptCount;             // 시도 횟수
    FIPv4Endpoint TargetEndpoint;   // 대상 엔드포인트

    // 기본 생성자
    FMessageAckData()
        : SequenceNumber(0)
        , Status(EMessageAckStatus::None)
        , SentTime(0.0)
        , LastAttemptTime(0.0)
        , AttemptCount(0)
    {
    }

    // 시퀀스 번호 및 대상으로 초기화하는 생성자
    FMessageAckData(uint16 InSequenceNumber, const FIPv4Endpoint& InTargetEndpoint)
        : SequenceNumber(InSequenceNumber)
        , Status(EMessageAckStatus::None)
        , SentTime(0.0)
        , LastAttemptTime(0.0)
        , AttemptCount(0)
        , TargetEndpoint(InTargetEndpoint)
    {
    }

    // 현재 상태에 대한 문자열 반환
    FString GetStatusString() const
    {
        switch (Status)
        {
        case EMessageAckStatus::None: return TEXT("None");
        case EMessageAckStatus::Sent: return TEXT("Sent");
        case EMessageAckStatus::Received: return TEXT("Received");
        case EMessageAckStatus::Acknowledged: return TEXT("Acknowledged");
        case EMessageAckStatus::Failed: return TEXT("Failed");
        case EMessageAckStatus::Timeout: return TEXT("Timeout");
        default: return TEXT("Unknown");
        }
    }
};

/**
 * 메시지 시퀀스 관리 구조체
 * 각 엔드포인트별 메시지 시퀀스 관리를 위한 구조체
 */
struct MULTISERVERSYNC_API FMessageSequenceTracker
{
    uint16 LastProcessedSequence;    // 마지막으로 처리된 시퀀스 번호
    uint16 NextExpectedSequence;     // 다음에 기대되는 시퀀스 번호
    TArray<uint16> ReceivedSequences; // 수신된 시퀀스 번호 목록
    TSet<uint16> MissingSequences;    // 누락된 시퀀스 번호 목록
    uint16 SequenceWindowSize;        // 시퀀스 윈도우 크기
    bool bOrderGuaranteed;           // 순서 보장 여부
    int32 MaxOutOfOrderMessages;      // 최대 순서 어긋난 메시지 수

    // 기본 생성자
    FMessageSequenceTracker()
        : LastProcessedSequence(0)
        , NextExpectedSequence(1)
        , SequenceWindowSize(100)
        , bOrderGuaranteed(false)
        , MaxOutOfOrderMessages(10)
    {
    }

    // 시퀀스 번호가 윈도우 내에 있는지 확인
    bool IsSequenceInWindow(uint16 Sequence) const
    {
        // 빈 윈도우인 경우
        if (ReceivedSequences.Num() == 0)
            return true;

        // 윈도우 범위 계산
        uint16 WindowStart = LastProcessedSequence;
        uint16 WindowEnd = (uint16)(WindowStart + SequenceWindowSize);

        // 윈도우가 랩어라운드되는 경우 처리
        if (WindowEnd < WindowStart)
        {
            return (Sequence >= WindowStart) || (Sequence <= WindowEnd);
        }
        else
        {
            return (Sequence >= WindowStart) && (Sequence <= WindowEnd);
        }
    }

    // 시퀀스 번호가 이미 처리되었는지 확인
    bool IsSequenceAlreadyProcessed(uint16 Sequence) const
    {
        // 처음 받는 메시지인 경우
        if (ReceivedSequences.Num() == 0)
            return false;

        // 마지막 처리 시퀀스보다 작은 경우
        // 랩어라운드 상황을 고려 (uint16 범위: 0-65535)
        if (LastProcessedSequence > 0xF000 && Sequence < 0x1000)
        {
            // 랩어라운드 케이스 (65530 -> 5)
            return false;
        }
        else if (LastProcessedSequence < 0x1000 && Sequence > 0xF000)
        {
            // 랩어라운드 케이스 (5 <- 65530)
            return true;
        }
        else
        {
            return Sequence <= LastProcessedSequence;
        }
    }

    // 새 메시지 추가 및 누락 메시지 감지
    bool AddSequence(uint16 Sequence)
    {
        // 이미 처리된 메시지는 무시
        if (IsSequenceAlreadyProcessed(Sequence))
            return false;

        // 윈도우 범위 밖이면 거부
        if (!IsSequenceInWindow(Sequence))
            return false;

        // 이미 수신된 시퀀스면 중복으로 처리
        if (ReceivedSequences.Contains(Sequence))
            return false;

        // 시퀀스 추가
        ReceivedSequences.Add(Sequence);

        // 순서가 맞지 않는 메시지 처리
        if (Sequence != NextExpectedSequence)
        {
            // 누락된 시퀀스 감지
            if (Sequence > NextExpectedSequence)
            {
                for (uint16 Missing = NextExpectedSequence; Missing < Sequence; Missing++)
                {
                    if (!ReceivedSequences.Contains(Missing))
                    {
                        MissingSequences.Add(Missing);
                    }
                }
            }

            // 순서 보장이 필요하면 false 반환
            if (bOrderGuaranteed)
            {
                // 순서 어긋난 메시지가 너무 많으면 이전 메시지를 포기
                if (ReceivedSequences.Num() > MaxOutOfOrderMessages)
                {
                    ProcessNextSequentialMessages();
                }
                return false;
            }
        }

        // 연속된 메시지 처리
        ProcessNextSequentialMessages();

        return true;
    }

    // 연속된 메시지 처리
    void ProcessNextSequentialMessages()
    {
        // 수신된 메시지를 오름차순으로 정렬
        ReceivedSequences.Sort();

        uint16 LastSequential = LastProcessedSequence;

        // 연속된 메시지 찾기
        for (int32 i = 0; i < ReceivedSequences.Num(); i++)
        {
            uint16 CurrentSeq = ReceivedSequences[i];

            // 연속되지 않은 메시지 발견
            if (CurrentSeq != (uint16)(LastSequential + 1))
                break;

            // 연속된 메시지 업데이트
            LastSequential = CurrentSeq;

            // 누락 목록에서 제거
            MissingSequences.Remove(CurrentSeq);
        }

        // 마지막 처리 시퀀스 업데이트
        if (LastSequential != LastProcessedSequence)
        {
            LastProcessedSequence = LastSequential;
            NextExpectedSequence = (uint16)(LastProcessedSequence + 1);

            // 처리된 메시지 제거
            ReceivedSequences.RemoveAll([this](uint16 Seq) {
                return Seq <= LastProcessedSequence;
                });
        }
    }

    // 누락된 메시지 조회
    TArray<uint16> GetMissingSequences() const
    {
        TArray<uint16> Result;
        for (const auto& Seq : MissingSequences)
        {
            Result.Add(Seq);
        }
        return Result;
    }

    // 누락된 메시지 요청 필요 여부
    bool NeedsRetransmissionRequest() const
    {
        return MissingSequences.Num() > 0;
    }
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

/**
 * 네트워크 메시지 타입
 * 각 메시지의 용도를 구분하는 열거형
 */
enum class EMessageType : uint8
{
    Invalid = 0,      // 유효하지 않은 메시지
    Data,             // 일반 데이터 메시지
    Ping_Request,     // 핑 요청 메시지
    Ping_Response,    // 핑 응답 메시지
    ACK,              // 메시지 수신 확인
    Reliable_Data,    // 신뢰성 있는 데이터 메시지
    // 필요에 따라 더 많은 메시지 타입 추가 가능
    Max               // 메시지 타입의 최대 값 (유효성 검사용)
};

/**
 * 메시지 전송 상태
 * 신뢰성 있는 메시지의 현재 상태를 나타냄
 */
enum class EMessageStatus : uint8
{
    Unsent,           // 전송되지 않음
    Sent,             // 전송됨
    Acknowledged,     // 확인됨
    Failed,           // 전송 실패
    Timeout           // 타임아웃
};

/**
 * 메시지 확인(ACK) 구조체
 * 수신된 메시지를 확인하기 위한 구조체
 */
struct MULTISERVERSYNC_API FACKMessage
{
    uint32 SequenceNumber;    // 확인하는 메시지의 시퀀스 번호
    EMessageType AckedType;   // 확인하는 메시지의 타입

    // 생성자
    FACKMessage()
        : SequenceNumber(0)
        , AckedType(EMessageType::Invalid)
    {
    }

    FACKMessage(uint32 InSequenceNumber, EMessageType InAckedType)
        : SequenceNumber(InSequenceNumber)
        , AckedType(InAckedType)
    {
    }

    // 직렬화 함수
    void Serialize(FMemoryWriter& Writer) const;
    void Deserialize(FMemoryReader& Reader);
};

/**
 * 신뢰성 있는 데이터 메시지 구조체
 * 신뢰성 있는 전송을 위한 메시지 구조체
 */
struct MULTISERVERSYNC_API FReliableDataMessage
{
    uint32 SequenceNumber;    // 메시지 시퀀스 번호
    TArray<uint8> Data;       // 실제 데이터
    bool RequireACK;          // ACK 필요 여부
    float Timeout;            // 타임아웃 시간(초)

    // 생성자
    FReliableDataMessage()
        : SequenceNumber(0)
        , RequireACK(true)
        , Timeout(1.0f)
    {
    }

    FReliableDataMessage(uint32 InSequenceNumber, const TArray<uint8>& InData, bool InRequireACK = true, float InTimeout = 1.0f)
        : SequenceNumber(InSequenceNumber)
        , Data(InData)
        , RequireACK(InRequireACK)
        , Timeout(InTimeout)
    {
    }

    // 직렬화 함수
    void Serialize(FMemoryWriter& Writer) const;
    void Deserialize(FMemoryReader& Reader);
};

/**
 * 전송 중인 메시지 추적 구조체
 * 재전송을 위해 전송된 메시지의 상태를 추적
 */
struct MULTISERVERSYNC_API FPendingMessage
{
    uint32 SequenceNumber;         // 메시지 시퀀스 번호
    EMessageType Type;             // 메시지 타입
    TArray<uint8> SerializedData;  // 직렬화된 메시지 데이터
    FIPv4Endpoint Destination;     // 목적지 엔드포인트
    float Timeout;                 // 타임아웃 시간(초)
    float ElapsedTime;             // 전송 후 경과 시간(초)
    int32 RetryCount;              // 재시도 횟수
    int32 MaxRetries;              // 최대 재시도 횟수
    EMessageStatus Status;         // 메시지 상태

    // 생성자
    FPendingMessage()
        : SequenceNumber(0)
        , Type(EMessageType::Invalid)
        , Timeout(1.0f)
        , ElapsedTime(0.0f)
        , RetryCount(0)
        , MaxRetries(3)
        , Status(EMessageStatus::Unsent)
    {
    }

    // 패러미터가 있는 생성자
    FPendingMessage(
        uint32 InSequenceNumber,
        EMessageType InType,
        const TArray<uint8>& InSerializedData,
        const FIPv4Endpoint& InDestination,
        float InTimeout = 1.0f,
        int32 InMaxRetries = 3)
        : SequenceNumber(InSequenceNumber)
        , Type(InType)
        , SerializedData(InSerializedData)
        , Destination(InDestination)
        , Timeout(InTimeout)
        , ElapsedTime(0.0f)
        , RetryCount(0)
        , MaxRetries(InMaxRetries)
        , Status(EMessageStatus::Unsent)
    {
    }

    // 타임아웃 여부 확인
    bool IsTimedOut() const
    {
        return ElapsedTime >= Timeout;
    }

    // 최대 재시도 횟수 초과 여부 확인
    bool HasExceededMaxRetries() const
    {
        return RetryCount >= MaxRetries;
    }

    // 재시도 가능 여부 확인
    bool CanRetry() const
    {
        return IsTimedOut() && !HasExceededMaxRetries();
    }

    // 시간 업데이트
    void UpdateTime(float DeltaTime)
    {
        ElapsedTime += DeltaTime;
    }

    // 재시도 수행
    void Retry()
    {
        RetryCount++;
        ElapsedTime = 0.0f;
        Status = EMessageStatus::Sent;
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