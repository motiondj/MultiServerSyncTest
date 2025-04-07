// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModuleInterfaces.h"
#include "Common/UdpSocketBuilder.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "HAL/Runnable.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Containers/Ticker.h"
#include "Serialization/ArrayReader.h" // FArrayReaderPtr 정의를 위해 추가

// 메시지 유형 정의
enum class ENetworkMessageType : uint8
{
    Discovery = 0,    // 서버 탐색 메시지
    DiscoveryResponse = 1, // 탐색 응답 메시지
    TimeSync = 2,     // 시간 동기화 메시지
    FrameSync = 3,    // 프레임 동기화 메시지
    Command = 4,      // 일반 명령 메시지
    Data = 5,         // 데이터 전송 메시지

    // 마스터-슬레이브 프로토콜 관련 메시지
    MasterAnnouncement = 10,  // 마스터가 자신의 상태를 알림
    MasterQuery = 11,         // 마스터 정보 요청
    MasterResponse = 12,      // 마스터 정보 응답
    MasterElection = 13,      // 마스터 선출 시작
    MasterVote = 14,          // 마스터 선출 투표
    MasterResign = 15,        // 마스터 사임 알림
    RoleChange = 16,          // 역할 변경 알림

    // 설정 공유 관련 메시지
    SettingsSync = 20,        // 설정 동기화 메시지
    SettingsRequest = 21,     // 설정 요청 메시지
    SettingsResponse = 22,    // 설정 응답 메시지

    // 핑 관련 메시지
    PingRequest = 30,         // 핑 요청 메시지
    PingResponse = 31,        // 핑 응답 메시지

    Custom = 255      // 사용자 정의 메시지
};

// 핑 메시지 유형 열거형
enum class EPingMessageType : uint8
{
    Request = 0,  // 핑 요청
    Response = 1  // 핑 응답
};

// 메시지 헤더 구조체
#pragma pack(push, 1)
struct FNetworkMessageHeader
{
    uint32 MagicNumber;        // 패킷 식별 번호 (0x4D53594E : "MSYN")
    ENetworkMessageType Type;   // 메시지 유형
    uint16 Size;               // 메시지 크기 (헤더 포함)
    uint16 SequenceNumber;     // 시퀀스 번호
    FGuid ProjectId;           // 프로젝트 ID
    uint8 Version;             // 프로토콜 버전
    uint8 Flags;               // 플래그
};
#pragma pack(pop)

// 핑 메시지 구조체
struct FPingMessage
{
    EPingMessageType Type;      // 메시지 타입
    uint64 Timestamp;           // 발신 타임스탬프
    uint32 SequenceNumber;      // 시퀀스 번호

    // 직렬화 함수
    void Serialize(FMemoryWriter& Writer) const;

    // 역직렬화 함수
    void Deserialize(FMemoryReader& Reader);
};

/**
 * 네트워크 메시지 클래스
 * 네트워크를 통해 전송되는 메시지를 표현
 */
class FNetworkMessage
{
public:
    /** 기본 생성자 */
    FNetworkMessage();

    /** 메시지 유형과 데이터로 생성하는 생성자 */
    FNetworkMessage(ENetworkMessageType InType, const TArray<uint8>& InData);

    /** 원시 데이터에서 메시지를 파싱하는 생성자 */
    FNetworkMessage(const TArray<uint8>& RawData);

    /** 메시지 직렬화 */
    TArray<uint8> Serialize() const;

    /** 메시지 역직렬화 */
    bool Deserialize(const TArray<uint8>& RawData);

    /** 메시지 유형 가져오기 */
    ENetworkMessageType GetType() const { return Header.Type; }

    /** 메시지 데이터 가져오기 */
    const TArray<uint8>& GetData() const { return Data; }

    /** 프로젝트 ID 가져오기 */
    FGuid GetProjectId() const { return Header.ProjectId; }

    /** 프로젝트 ID 설정하기 */
    void SetProjectId(const FGuid& InProjectId) { Header.ProjectId = InProjectId; }

    /** 시퀀스 번호 가져오기 */
    uint16 GetSequenceNumber() const { return Header.SequenceNumber; }

    /** 시퀀스 번호 설정하기 */
    void SetSequenceNumber(uint16 InSequenceNumber) { Header.SequenceNumber = InSequenceNumber; }

    /** 플래그 가져오기 */
    uint8 GetFlags() const { return Header.Flags; }

    /** 플래그 설정하기 */
    void SetFlags(uint8 InFlags) { Header.Flags = InFlags; }

private:
    /** 메시지 헤더 */
    FNetworkMessageHeader Header;

    /** 메시지 데이터 */
    TArray<uint8> Data;

    /** 매직 넘버 - "MSYN" */
    static const uint32 MESSAGE_MAGIC = 0x4D53594E;

    /** 프로토콜 버전 */
    static const uint8 PROTOCOL_VERSION = 1;
};

/**
 * 서버 엔드포인트 정보 구조체
 * 발견된 각 서버의 정보를 저장
 */
struct FServerEndpoint
{
    /** 엔드포인트 ID */
    FString Id;

    /** 호스트 이름 */
    FString HostName;

    /** IP 주소 */
    FIPv4Address IPAddress;

    /** 포트 번호 */
    uint16 Port;

    /** 프로젝트 ID */
    FGuid ProjectId;

    /** 프로젝트 버전 */
    FString ProjectVersion;

    /** 마지막 통신 시간 */
    double LastCommunicationTime;

    /** 생성자 */
    FServerEndpoint()
        : Port(0)
        , LastCommunicationTime(0.0)
    {
    }

    /** IP 주소와 포트로 구성된 문자열 반환 */
    FString ToString() const
    {
        return FString::Printf(TEXT("%s:%d"), *IPAddress.ToString(), Port);
    }

    /** 두 엔드포인트가 동일한지 비교 */
    bool operator==(const FServerEndpoint& Other) const
    {
        return Id == Other.Id || (IPAddress == Other.IPAddress && Port == Other.Port);
    }
};

/**
 * 마스터 정보 구조체
 * 마스터 서버에 대한 추가 정보를 저장
 */
struct FMasterInfo
{
    FString ServerId;             // 서버 고유 ID
    FIPv4Address IPAddress;       // IP 주소
    uint16 Port;                  // 포트 번호
    float Priority;               // 마스터 우선순위 (높을수록 우선순위 높음)
    double LastUpdateTime;        // 마지막 업데이트 시간
    int32 ElectionTerm;           // 선출 기간 (선출될 때마다 증가)

    FMasterInfo()
        : Port(0)
        , Priority(0.0f)
        , LastUpdateTime(0.0)
        , ElectionTerm(0)
    {
    }

    // 두 마스터 정보가 동일한지 비교
    bool operator==(const FMasterInfo& Other) const
    {
        return ServerId == Other.ServerId;
    }

    // 마스터 정보를 문자열로 변환
    FString ToString() const
    {
        return FString::Printf(TEXT("Master[%s] at %s:%d (Priority: %.2f, Term: %d)"),
            *ServerId, *IPAddress.ToString(), Port, Priority, ElectionTerm);
    }
};

/**
 * 수신 스레드 클래스
 * 별도의 스레드에서 메시지 수신을 담당
 */
class FNetworkReceiverWorker : public FRunnable
{
public:
    /** 생성자 */
    FNetworkReceiverWorker(class FNetworkManager* InOwner, FSocket* InSocket);

    /** 소멸자 */
    virtual ~FNetworkReceiverWorker();

    // FRunnable 인터페이스 구현
    virtual bool Init() override;
    virtual uint32 Run() override;
    virtual void Stop() override;
    virtual void Exit() override;

private:
    /** 소유자 */
    class FNetworkManager* Owner;

    /** 소켓 */
    FSocket* Socket;

    /** 중지 요청 플래그 */
    FThreadSafeBool bStopRequested;

    /** 스레드 동기화 이벤트 */
    FEvent* StopEvent;
};

/**
 * Network manager class that implements the INetworkManager interface
 * Handles all network communication between servers
 */
class FNetworkManager : public INetworkManager
{
public:
    /** Constructor */
    FNetworkManager();

    /** Destructor */
    virtual ~FNetworkManager();

    // Begin INetworkManager interface
    virtual bool Initialize() override;
    virtual void Shutdown() override;
    virtual bool SendMessage(const FString& EndpointId, const TArray<uint8>& Message) override;
    virtual bool BroadcastMessage(const TArray<uint8>& Message) override;
    virtual void RegisterMessageHandler(TFunction<void(const FString&, const TArray<uint8>&)> Handler) override;
    virtual bool DiscoverServers() override;

    // 마스터-슬레이브 프로토콜 관련 메서드
    virtual bool IsMaster() const override;
    virtual FString GetMasterId() const override;
    virtual bool StartMasterElection() override;
    virtual void AnnounceMaster() override;
    virtual void ResignMaster() override;
    virtual FMasterInfo GetMasterInfo() const override;
    virtual void SetMasterPriority(float Priority) override;
    virtual void RegisterMasterChangeHandler(TFunction<void(const FString&, bool)> Handler) override;

    // 설정 관련 메서드
    virtual uint16 GetPort() const override { return Port; }
    virtual bool SendSettingsMessage(const TArray<uint8>& SettingsData) override;
    virtual bool RequestSettings() override;

    // 네트워크 지연 측정 관련 메서드
    virtual void StartLatencyMeasurement(const FIPv4Endpoint& ServerEndpoint, float IntervalSeconds = 1.0f, int32 SampleCount = 0) override;
    virtual void StopLatencyMeasurement(const FIPv4Endpoint& ServerEndpoint) override;
    virtual FNetworkLatencyStats GetLatencyStats(const FIPv4Endpoint& ServerEndpoint) const override;
    virtual int32 EvaluateNetworkQuality(const FIPv4Endpoint& ServerEndpoint) const override;
    virtual FString GetNetworkQualityString(const FIPv4Endpoint& ServerEndpoint) const override;

    // 이상치 필터링 관련 메서드
    virtual void SetOutlierFiltering(const FIPv4Endpoint& ServerEndpoint, bool bEnableFiltering) override;
    virtual bool GetOutlierStats(const FIPv4Endpoint& ServerEndpoint, int32& OutliersDetected, double& OutlierThreshold) const override;

    // 시계열 관련 메서드
    virtual void SetTimeSeriesSampleInterval(const FIPv4Endpoint& ServerEndpoint, double IntervalSeconds) override;
    virtual bool GetTimeSeriesData(const FIPv4Endpoint& ServerEndpoint, TArray<FLatencyTimeSeriesSample>& OutTimeSeries) const override;
    virtual bool GetNetworkTrendAnalysis(const FIPv4Endpoint& ServerEndpoint, FNetworkTrendAnalysis& OutTrendAnalysis) const override;

    // 네트워크 상태 평가 고도화 관련 메서드
    virtual FNetworkQualityAssessment EvaluateNetworkQualityDetailed(const FIPv4Endpoint& ServerEndpoint) const override;
    virtual void RegisterNetworkStateChangeHandler(TFunction<void(const FIPv4Endpoint&, ENetworkEventType, const FNetworkQualityAssessment&)> Handler) override;
    virtual void SetNetworkStateChangeThreshold(const FIPv4Endpoint& ServerEndpoint, double Threshold) override;
    virtual void SetNetworkPerformanceThresholds(const FIPv4Endpoint& ServerEndpoint,
        double LatencyThreshold,
        double JitterThreshold,
        double PacketLossThreshold) override;
    virtual void SetQualityAssessmentInterval(const FIPv4Endpoint& ServerEndpoint, double IntervalSeconds) override;
    virtual void SetNetworkStateMonitoring(const FIPv4Endpoint& ServerEndpoint, bool bEnable) override;
    virtual bool GetNetworkEventHistory(const FIPv4Endpoint& ServerEndpoint, TArray<ENetworkEventType>& OutEvents) const override;
    // End INetworkManager interface

    /** Get the list of discovered servers */
    TArray<FString> GetDiscoveredServers() const;

    /** Generate unique project identifier */
    FGuid GenerateProjectId() const;

    /** Set the project identifier */
    void SetProjectId(const FGuid& ProjectId);

    /** Get the project identifier */
    FGuid GetProjectId() const;

    /** 메시지 수신 처리 함수 (수신 스레드에서 호출) */
    void ProcessReceivedData(const TArray<uint8>& Data, const FIPv4Endpoint& Sender);

    /** 서버 탐색 메시지 전송 */
    bool SendDiscoveryMessage();

    /** 서버 탐색 응답 메시지 전송 */
    bool SendDiscoveryResponse(const FIPv4Endpoint& TargetEndpoint);

    /** 서버 목록에 새 서버 추가 */
    void AddOrUpdateServer(const FServerEndpoint& ServerInfo);

    /** 특정 서버로 메시지 전송 */
    bool SendMessageToEndpoint(const FIPv4Endpoint& Endpoint, const FNetworkMessage& Message);

    /** 모든 발견된 서버로 메시지 브로드캐스트 */
    bool BroadcastMessageToServers(const FNetworkMessage& Message);

    /** 기본 포트 번호 */
    static const int32 DEFAULT_PORT = 7000;

    /** 브로드캐스트 포트 번호 */
    static const int32 BROADCAST_PORT = 7001;

    /** 시간 동기화 메시지 전송 */
    bool SendTimeSyncMessage(const TArray<uint8>& PTPMessage);

    /** 시퀀스 번호 접근자 (FSyncFrameworkManager에서 사용) */
    uint16 GetNextSequenceId() { return GetNextSequenceNumber(); }

    /**
     * 특정 서버에 핑 요청을 보냅니다.
     * @param ServerEndpoint 핑을 보낼 서버의 엔드포인트
     * @return 요청에 사용된 시퀀스 번호
     */
    uint32 SendPingRequest(const FIPv4Endpoint& ServerEndpoint);

    /**
     * 핑 요청에 대한 응답을 보냅니다.
     * @param RequestMessage 수신한 핑 요청 메시지
     * @param SourceEndpoint 요청을 보낸 엔드포인트
     */
    void SendPingResponse(const FPingMessage& RequestMessage, const FIPv4Endpoint& SourceEndpoint);

    /**
     * 지연 시간 측정을 위한 주기적인 핑 요청을 활성화합니다.
     * @param ServerEndpoint 핑을 보낼 서버 엔드포인트
     * @param IntervalSeconds 핑 요청 간격 (초)
     * @param bDynamicSampling 동적 샘플링 활성화 여부
     * @param MinIntervalSeconds 최소 샘플링 간격 (초, 동적 샘플링 시)
     * @param MaxIntervalSeconds 최대 샘플링 간격 (초, 동적 샘플링 시)
     */
    void EnablePeriodicPing(const FIPv4Endpoint& ServerEndpoint, float IntervalSeconds = 1.0f,
        bool bDynamicSampling = false, float MinIntervalSeconds = 0.1f,
        float MaxIntervalSeconds = 5.0f);

    /**
     * 주기적인 핑 요청을 비활성화합니다.
     * @param ServerEndpoint 비활성화할 서버 엔드포인트
     */
    void DisablePeriodicPing(const FIPv4Endpoint& ServerEndpoint);

private:
    /** Broadcast socket for server discovery */
    FSocket* BroadcastSocket;

    /** Receiving socket for messages */
    FSocket* ReceiveSocket;

    /** Thread for receiving messages */
    FRunnableThread* ReceiverThread;

    /** Message handler callback */
    TFunction<void(const FString&, const TArray<uint8>&)> MessageHandler;

    /** 발견된 서버 목록 */
    TMap<FString, FServerEndpoint> DiscoveredServers;

    /** Project unique identifier */
    FGuid ProjectId;

    /** Is the network manager initialized */
    bool bIsInitialized;

    /** 현재 시퀀스 번호 */
    uint16 CurrentSequenceNumber;

    /** 프로젝트 버전 */
    FString ProjectVersion;

    /** 호스트 이름 */
    FString HostName;

    /** 포트 번호 */
    uint16 Port;

    /** 수신 스레드 작업자 */
    FNetworkReceiverWorker* ReceiverWorker;

    // 마스터-슬레이브 관련 멤버 변수
    bool bIsMaster;                       // 현재 노드가 마스터인지 여부
    FMasterInfo CurrentMaster;            // 현재 마스터 정보
    float MasterPriority;                 // 이 서버의 마스터 우선순위
    bool bElectionInProgress;             // 선출 진행중 여부
    int32 CurrentElectionTerm;            // 현재 선출 기간
    TMap<FString, float> ElectionVotes;   // 선출 투표 결과
    double LastMasterAnnouncementTime;    // 마지막 마스터 공지 시간
    double LastElectionStartTime;         // 마지막 선출 시작 시간
    TFunction<void(const FString&, bool)> MasterChangeHandler; // 마스터 변경 핸들러
    const float MASTER_TIMEOUT_SECONDS = 5.0f;    // 마스터 타임아웃 시간
    const float ELECTION_TIMEOUT_SECONDS = 3.0f;  // 선출 타임아웃 시간

    // 네트워크 지연 측정 관련 멤버 변수
    TMap<FString, FNetworkLatencyStats> ServerLatencyStats;    // 서버별 지연 통계
    uint32 NextPingSequenceNumber;                            // 다음 핑 시퀀스 번호
    TMap<uint32, TPair<FIPv4Endpoint, double>> PendingPingRequests;  // 대기 중인 핑 요청
    FTSTicker::FDelegateHandle LatencyMeasurementTickHandle;  // 지연 측정 틱 핸들
    FTSTicker::FDelegateHandle PingTimeoutCheckHandle;        // 핑 타임아웃 체크 핸들
    const int32 MAX_RTT_SAMPLES = 100;                        // 최대 RTT 샘플 수
    const double PING_TIMEOUT_SECONDS = 2.0;                  // 핑 타임아웃 시간 (초)

    // 네트워크 상태 변화 관련 멤버 변수
    TFunction<void(const FIPv4Endpoint&, ENetworkEventType, const FNetworkQualityAssessment&)> NetworkStateChangeHandler;
    FTSTicker::FDelegateHandle QualityAssessmentTickHandle;   // 품질 평가 틱 핸들

    // 주기적 핑 요청 관리를 위한 구조체
    struct FPeriodicPingState
    {
        FIPv4Endpoint ServerEndpoint;     // 서버 엔드포인트
        float IntervalSeconds;            // 핑 간격 (초)
        float TimeRemainingSeconds;       // 다음 핑까지 남은 시간
        bool bIsActive;                   // 활성 상태 여부
        bool bDynamicSampling;            // 동적 샘플링 활성화 여부
        float MinIntervalSeconds;         // 최소 샘플링 간격 (초)
        float MaxIntervalSeconds;         // 최대 샘플링 간격 (초)
        float NetworkQualityFactor;       // 네트워크 품질 계수 (0.0-1.0)
        int32 ConsecutiveTimeouts;        // 연속 타임아웃 횟수

        // 기본 생성자
        FPeriodicPingState()
            : IntervalSeconds(1.0f)
            , TimeRemainingSeconds(0.0f)
            , bIsActive(false)
            , bDynamicSampling(false)
            , MinIntervalSeconds(0.1f)
            , MaxIntervalSeconds(5.0f)
            , NetworkQualityFactor(0.5f)
            , ConsecutiveTimeouts(0)
        {
        }
    };

    // 주기적 핑 상태 관리
    TArray<FPeriodicPingState> PeriodicPingStates;

    /** 소켓 초기화 */
    bool InitializeSockets();

    /** 브로드캐스트 소켓 생성 */
    bool CreateBroadcastSocket();

    /** 수신 소켓 생성 */
    bool CreateReceiveSocket();

    /** 수신 스레드 시작 */
    bool StartReceiverThread();

    /** 현재 서버의 엔드포인트 정보 생성 */
    FServerEndpoint CreateLocalServerInfo() const;

    /** 서버 목록 정리 (비활성 서버 제거) */
    void CleanupServerList();

    /** 메시지 유형에 따른 처리 함수 */
    void HandleDiscoveryMessage(const FNetworkMessage& Message, const FIPv4Endpoint& Sender);
    void HandleDiscoveryResponseMessage(const FNetworkMessage& Message, const FIPv4Endpoint& Sender);
    void HandleTimeSyncMessage(const FNetworkMessage& Message, const FIPv4Endpoint& Sender);
    void HandleFrameSyncMessage(const FNetworkMessage& Message, const FIPv4Endpoint& Sender);
    void HandleCommandMessage(const FNetworkMessage& Message, const FIPv4Endpoint& Sender);
    void HandleDataMessage(const FNetworkMessage& Message, const FIPv4Endpoint& Sender);
    void HandleCustomMessage(const FNetworkMessage& Message, const FIPv4Endpoint& Sender);

    // 마스터-슬레이브 메시지 처리 메서드
    void HandleMasterAnnouncement(const FNetworkMessage& Message, const FIPv4Endpoint& Sender);
    void HandleMasterQuery(const FNetworkMessage& Message, const FIPv4Endpoint& Sender);
    void HandleMasterResponse(const FNetworkMessage& Message, const FIPv4Endpoint& Sender);
    void HandleMasterElection(const FNetworkMessage& Message, const FIPv4Endpoint& Sender);
    void HandleMasterVote(const FNetworkMessage& Message, const FIPv4Endpoint& Sender);
    void HandleMasterResign(const FNetworkMessage& Message, const FIPv4Endpoint& Sender);
    void HandleRoleChange(const FNetworkMessage& Message, const FIPv4Endpoint& Sender);

    // 설정 관련 메시지 처리 메서드
    void HandleSettingsSyncMessage(const FNetworkMessage& Message, const FIPv4Endpoint& Sender);
    void HandleSettingsRequestMessage(const FNetworkMessage& Message, const FIPv4Endpoint& Sender);
    void HandleSettingsResponseMessage(const FNetworkMessage& Message, const FIPv4Endpoint& Sender);

    // 핑 메시지 처리 메서드
    void HandlePingRequest(const TSharedPtr<FMemoryReader>& ReaderPtr, const FIPv4Endpoint& SourceEndpoint);
    void HandlePingResponse(const TSharedPtr<FMemoryReader>& ReaderPtr, const FIPv4Endpoint& SourceEndpoint);

    // 마스터 선출 관련 메서드
    void SendElectionVote(const FString& CandidateId);
    bool TryBecomeMaster();
    void EndElection(const FString& WinnerId, int32 ElectionTerm);
    void CheckMasterTimeout();
    float CalculateVotePriority();
    void UpdateMasterStatus(const FString& NewMasterId, bool bLocalServerIsMaster);
    void SendRoleChangeNotification();

    // 네트워크 지연 측정 관련 메서드
    void UpdateRTTStatistics(const FIPv4Endpoint& ServerEndpoint, double RTT);
    void CheckPingTimeouts();
    bool TickLatencyMeasurement(float DeltaTime);

    // 네트워크 품질 평가 관련 메서드
    int32 CalculateLatencyScore(double RTT, double HighLatencyThreshold) const;
    int32 CalculateJitterScore(double Jitter, double HighJitterThreshold) const;
    int32 CalculatePacketLossScore(double LossRate, double HighPacketLossThreshold) const;
    int32 CalculateStabilityScore(const FNetworkTrendAnalysis& TrendAnalysis) const;
    void ProcessNetworkStateChange(const FIPv4Endpoint& ServerEndpoint, ENetworkEventType EventType, const FNetworkQualityAssessment& Quality);
    bool CheckQualityAssessments(float DeltaTime);

    /** 다음 시퀀스 번호 생성 */
    uint16 GetNextSequenceNumber();

    /** 이벤트 간 충분한 시간이 지났는지 확인 (속도 제한) */
    bool HasEnoughTimePassed(double& LastTime, double Interval) const;

    /** 주기적인 마스터-슬레이브 프로토콜 상태 업데이트 */
    void TickMasterSlaveProtocol();

    /** 틱 델리게이트 핸들 */
    FTSTicker::FDelegateHandle MasterSlaveTickHandle;

    /** 마스터-슬레이브 프로토콜 틱 콜백 */
    bool MasterSlaveProtocolTick(float DeltaTime);

    // 고정밀 타임스탬프 생성 메서드
    uint64 GetHighPrecisionTimestamp() const;

    // 하드웨어 타이머 초기화 시간 (마이크로초)
    uint64 HardwareTimerInitTime;

    // 하드웨어 타이머 오프셋 (마이크로초)
    int64 HardwareTimerOffset;

    // 하드웨어 타이머 보정 메서드
    void CalibrateHardwareTimer();

    // 하드웨어 타이머를 UTC 시간으로 변환
    uint64 HardwareTimeToUTC(uint64 HardwareTime) const;

    // 동적 샘플링 레이트 계산
    float CalculateDynamicSamplingRate(const FPeriodicPingState& PingState) const;

    // 네트워크 품질 계수 업데이트
    void UpdateNetworkQualityFactor(FPeriodicPingState& PingState, const FString& ServerID);

    // 연속 타임아웃 증가
    void IncrementConsecutiveTimeouts(const FIPv4Endpoint& ServerEndpoint);

    // 연속 타임아웃 리셋
    void ResetConsecutiveTimeouts(const FIPv4Endpoint& ServerEndpoint);
};