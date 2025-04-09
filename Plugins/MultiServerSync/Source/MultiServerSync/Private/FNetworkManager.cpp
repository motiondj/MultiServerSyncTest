#include "FNetworkManager.h"
#include "FSyncLog.h"
#include "FTimeSync.h"
#include "MultiServerSync.h"
#include "ISyncFrameworkManager.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h" // FMemoryReader 정의를 위해 추가
#include "IPAddress.h"
#include "Misc/DateTime.h"
#include "Misc/Timespan.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"

// FNetworkMessage 클래스 구현
FNetworkMessage::FNetworkMessage()
{
    Header.MagicNumber = MESSAGE_MAGIC;
    Header.Type = ENetworkMessageType::Custom;
    Header.Size = sizeof(FNetworkMessageHeader);
    Header.SequenceNumber = 0;
    Header.ProjectId = FGuid();
    Header.Version = PROTOCOL_VERSION;
    Header.Flags = 0;
}

FNetworkMessage::FNetworkMessage(ENetworkMessageType InType, const TArray<uint8>& InData)
{
    Header.MagicNumber = MESSAGE_MAGIC;
    Header.Type = InType;
    Header.Size = sizeof(FNetworkMessageHeader) + InData.Num();
    Header.SequenceNumber = 0;
    Header.ProjectId = FGuid();
    Header.Version = PROTOCOL_VERSION;
    Header.Flags = 0;
    Data = InData;
}

FNetworkMessage::FNetworkMessage(const TArray<uint8>& RawData)
{
    Deserialize(RawData);
}

TArray<uint8> FNetworkMessage::Serialize() const
{
    TArray<uint8> Result;
    Result.SetNumUninitialized(Header.Size);

    // 헤더 복사
    FMemory::Memcpy(Result.GetData(), &Header, sizeof(FNetworkMessageHeader));

    // 데이터 복사 (있는 경우)
    if (Data.Num() > 0)
    {
        FMemory::Memcpy(Result.GetData() + sizeof(FNetworkMessageHeader), Data.GetData(), Data.Num());
    }

    return Result;
}

bool FNetworkMessage::Deserialize(const TArray<uint8>& RawData)
{
    // 최소 크기 확인
    if (RawData.Num() < sizeof(FNetworkMessageHeader))
    {
        return false;
    }

    // 헤더 복사
    FMemory::Memcpy(&Header, RawData.GetData(), sizeof(FNetworkMessageHeader));

    // 매직 넘버 확인
    if (Header.MagicNumber != MESSAGE_MAGIC)
    {
        return false;
    }

    // 크기 확인
    if (Header.Size != RawData.Num())
    {
        return false;
    }

    // 데이터 복사 (있는 경우)
    int32 DataSize = Header.Size - sizeof(FNetworkMessageHeader);
    if (DataSize > 0)
    {
        Data.SetNumUninitialized(DataSize);
        FMemory::Memcpy(Data.GetData(), RawData.GetData() + sizeof(FNetworkMessageHeader), DataSize);
    }
    else
    {
        Data.Empty();
    }

    return true;
}

// FNetworkReceiverWorker 클래스 구현
FNetworkReceiverWorker::FNetworkReceiverWorker(FNetworkManager* InOwner, FSocket* InSocket)
    : Owner(InOwner)
    , Socket(InSocket)
    , bStopRequested(false)
    , StopEvent(nullptr)
{
}

FNetworkReceiverWorker::~FNetworkReceiverWorker()
{
    if (StopEvent)
    {
        FPlatformProcess::ReturnSynchEventToPool(StopEvent);
        StopEvent = nullptr;
    }
}

bool FNetworkReceiverWorker::Init()
{
    // 동기화 이벤트 생성
    StopEvent = FPlatformProcess::GetSynchEventFromPool(false);
    return StopEvent != nullptr;
}

uint32 FNetworkReceiverWorker::Run()
{
    UE_LOG(LogMultiServerSync, Display, TEXT("Network receiver thread started"));

    const int32 BufferSize = 65507; // UDP 최대 크기
    TArray<uint8> ReceiveBuffer;
    ReceiveBuffer.SetNumUninitialized(BufferSize);

    // 소켓 설정
    Socket->SetNonBlocking(true);

    // 수신 주소
    TSharedRef<FInternetAddr> SenderAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();

    while (!bStopRequested)
    {
        // 메시지 수신 대기
        int32 BytesRead = 0;
        if (Socket->RecvFrom(ReceiveBuffer.GetData(), BufferSize, BytesRead, *SenderAddr))
        {
            if (BytesRead > 0)
            {
                // 수신 데이터 복사
                TArray<uint8> ReceivedData;
                ReceivedData.SetNumUninitialized(BytesRead);
                FMemory::Memcpy(ReceivedData.GetData(), ReceiveBuffer.GetData(), BytesRead);

                // 발신자 엔드포인트 생성
                FIPv4Address SenderIP(0);
                uint32 SenderIPValue = 0;
                SenderAddr->GetIp(SenderIPValue);
                SenderIP = FIPv4Address(SenderIPValue);

                int32 SenderPort = SenderAddr->GetPort();
                FIPv4Endpoint SenderEndpoint(SenderIP, SenderPort);

                // 데이터 처리 (메인 스레드에서 처리하면 더 좋을 수 있음)
                Owner->ProcessReceivedData(ReceivedData, SenderEndpoint);
            }
        }

        // 약간의 휴식으로 CPU 사용률 감소
        FPlatformProcess::Sleep(0.001f);
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Network receiver thread stopped"));
    return 0;
}

void FNetworkReceiverWorker::Stop()
{
    bStopRequested = true;

    if (StopEvent)
    {
        StopEvent->Trigger();
    }
}

void FNetworkReceiverWorker::Exit()
{
    // 스레드 종료 시 필요한 정리 작업
}

FNetworkManager::FNetworkManager()
    : BroadcastSocket(nullptr)
    , ReceiveSocket(nullptr)
    , ReceiverThread(nullptr)
    , MessageHandler(nullptr)
    , bIsInitialized(false)
    , CurrentSequenceNumber(0)
    , Port(DEFAULT_PORT)
    , ReceiverWorker(nullptr)
    , bIsMaster(false)
    , MasterPriority(0.0f)
    , bElectionInProgress(false)
    , CurrentElectionTerm(0)
    , LastMasterAnnouncementTime(0.0)
    , LastElectionStartTime(0.0)
    , HardwareTimerInitTime(0)
    , HardwareTimerOffset(0)
{
    // 프로젝트 ID 초기화
    ProjectId = FGuid::NewGuid();

    // 호스트 이름 가져오기
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (SocketSubsystem)
    {
        SocketSubsystem->GetHostName(HostName);
    }

    // 프로젝트 버전 설정
    ProjectVersion = TEXT("1.0");

    // 마스터 우선순위 랜덤 초기화 (0.1 ~ 0.9)
    MasterPriority = 0.1f + 0.8f * FMath::FRand();
}

// 마스터 상태 확인 메서드
bool FNetworkManager::IsMaster() const
{
    return bIsMaster;
}

// 마스터 ID 반환 메서드
FString FNetworkManager::GetMasterId() const
{
    return CurrentMaster.ServerId;
}

// 마스터 정보 반환 메서드
FMasterInfo FNetworkManager::GetMasterInfo() const
{
    return CurrentMaster;
}

// 마스터 우선순위 설정 메서드
void FNetworkManager::SetMasterPriority(float Priority)
{
    MasterPriority = FMath::Clamp(Priority, 0.0f, 1.0f);
    UE_LOG(LogMultiServerSync, Display, TEXT("Master priority set to %.2f"), MasterPriority);
}

// 마스터 변경 핸들러 등록 메서드
void FNetworkManager::RegisterMasterChangeHandler(TFunction<void(const FString&, bool)> Handler)
{
    MasterChangeHandler = Handler;
}

FNetworkManager::~FNetworkManager()
{
    if (bIsInitialized)
    {
        Shutdown();
    }
}

bool FNetworkManager::Initialize()
{
    UE_LOG(LogMultiServerSync, Display, TEXT("Initializing Network Manager..."));

    if (!InitializeSockets())
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to initialize sockets"));
        return false;
    }

    if (!StartReceiverThread())
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to start receiver thread"));
        return false;
    }

    // 하드웨어 타이머 보정
    CalibrateHardwareTimer();

    UE_LOG(LogMultiServerSync, Display, TEXT("Network Manager initialized successfully"));

    bIsInitialized = true;

    // 서버 탐색 시작
    SendDiscoveryMessage();

    // 마스터 정보 요청
    FString QueryData = HostName;
    TArray<uint8> QueryBytes;
    QueryBytes.SetNum(QueryData.Len() * sizeof(TCHAR));
    FMemory::Memcpy(QueryBytes.GetData(), *QueryData, QueryData.Len() * sizeof(TCHAR));

    FNetworkMessage Message(ENetworkMessageType::MasterQuery, QueryBytes);
    Message.SetProjectId(ProjectId);
    Message.SetSequenceNumber(GetNextSequenceNumber());

    // 처음 5초 동안 2초 대기 후 마스터 정보가 없으면 마스터 선출 시작
    FTimerHandle MasterElectionTimerHandle;
    FTimerDelegate MasterElectionTimerDelegate;
    MasterElectionTimerDelegate.BindLambda([this]() {
        if (CurrentMaster.ServerId.IsEmpty() && !bElectionInProgress)
        {
            StartMasterElection();
        }
        });

    // 현재는 타이머 구현을 하지 않지만, 실제 구현에서는 타이머를 사용해야 합니다.
    // 여기서는 간단히 마스터 요청 메시지를 브로드캐스트
    BroadcastMessageToServers(Message);

    // 마스터-슬레이브 프로토콜 틱 등록
    FTickerDelegate TickDelegate = FTickerDelegate::CreateRaw(this, &FNetworkManager::MasterSlaveProtocolTick);
    MasterSlaveTickHandle = FTSTicker::GetCoreTicker().AddTicker(TickDelegate, 1.0f); // 1초마다 호출

    // 네트워크 지연 통계 초기화
    ServerLatencyStats.Empty();

    // 핑 시퀀스 번호 초기화
    NextPingSequenceNumber = 0;

    // 주기적 핑 상태 초기화
    PeriodicPingStates.Empty();

    // 틱 델리게이트가 있으면 제거
    if (LatencyMeasurementTickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(LatencyMeasurementTickHandle);
        LatencyMeasurementTickHandle.Reset();
    }

    if (PingTimeoutCheckHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(PingTimeoutCheckHandle);
        PingTimeoutCheckHandle.Reset();
    }

    // 메시지 재전송 틱 해제
    if (MessageRetryTickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(MessageRetryTickHandle);
        MessageRetryTickHandle.Reset();
    }

    // 확인 대기 중인 메시지 정리
    PendingAcknowledgements.Empty();
    EndpointSequenceMap.Empty();

    // 메시지 재전송 틱 추가
    if (MessageRetryTickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(MessageRetryTickHandle);
        MessageRetryTickHandle.Reset();
    }

    FTickerDelegate RetryTickDelegate = FTickerDelegate::CreateRaw(this, &FNetworkManager::CheckMessageRetries);
    MessageRetryTickHandle = FTSTicker::GetCoreTicker().AddTicker(RetryTickDelegate, MESSAGE_RETRY_INTERVAL);

    // 메시지 확인 관련 변수 초기화
    LastRetryCheckTime = FPlatformTime::Seconds();
    PendingAcknowledgements.Empty();
    EndpointSequenceMap.Empty();

    // 시퀀스 관리 초기화
    bOrderGuaranteedEnabled = false;
    EndpointSequenceTrackers.Empty();

    // 시퀀스 관리 틱 추가
    if (SequenceManagementTickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(SequenceManagementTickHandle);
        SequenceManagementTickHandle.Reset();
    }

    FTickerDelegate SequenceTickDelegate = FTickerDelegate::CreateRaw(this, &FNetworkManager::CheckSequenceManagement);
    SequenceManagementTickHandle = FTSTicker::GetCoreTicker().AddTicker(SequenceTickDelegate, SEQUENCE_MANAGEMENT_INTERVAL);

    // 중복 메시지 추적 틱 추가
    if (DuplicateTrackerTickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(DuplicateTrackerTickHandle);
        DuplicateTrackerTickHandle.Reset();
    }

    // 중복 메시지 추적 초기화
    DuplicateMessageTracker = FMessageTracker();
    DuplicateMessageTracker.LastCleanupTime = FPlatformTime::Seconds();

    // 중복 메시지 추적 틱 등록
    FTickerDelegate DuplicateTrackerTickDelegate = FTickerDelegate::CreateRaw(this, &FNetworkManager::CheckDuplicateTracker);
    DuplicateTrackerTickHandle = FTSTicker::GetCoreTicker().AddTicker(DuplicateTrackerTickDelegate, DUPLICATE_TRACKER_CLEANUP_INTERVAL);

    // 메시지 캐시 초기화
    MessageCache = FMessageCache();
    MessageCache.LastCleanupTime = FPlatformTime::Seconds();

    // 메시지 캐시 틱 등록
    FTickerDelegate MessageCacheTickDelegate = FTickerDelegate::CreateRaw(this, &FNetworkManager::CheckMessageCache);
    MessageCacheTickHandle = FTSTicker::GetCoreTicker().AddTicker(MessageCacheTickDelegate, MESSAGE_CACHE_CLEANUP_INTERVAL);

    // 멱등성 캐시 초기화
    IdempotentResults.Empty();

    // 멱등성 캐시 정리 틱 등록
    FTickerDelegate IdempotentCacheTickDelegate = FTickerDelegate::CreateRaw(this, &FNetworkManager::CheckIdempotentCache);
    IdempotentCacheTickHandle = FTSTicker::GetCoreTicker().AddTicker(IdempotentCacheTickDelegate, IDEMPOTENT_CACHE_CLEANUP_INTERVAL);

    return true;
}

// 하드웨어 타이머 보정
void FNetworkManager::CalibrateHardwareTimer()
{
    // 현재 UTC 시간 (마이크로초)
    uint64 UTCTimeMicros = FDateTime::UtcNow().GetTicks() / 10; // 100나노초 -> 마이크로초

    // 현재 하드웨어 타이머 시간 (마이크로초)
    double CycleTime = FPlatformTime::GetSecondsPerCycle();
    uint64 Cycles = FPlatformTime::Cycles64();
    uint64 HardwareTimeMicros = static_cast<uint64>(Cycles * CycleTime * 1000000.0);

    // 초기 시간 저장
    HardwareTimerInitTime = HardwareTimeMicros;

    // 오프셋 계산 (UTC - 하드웨어 시간)
    HardwareTimerOffset = static_cast<int64>(UTCTimeMicros) - static_cast<int64>(HardwareTimeMicros);

    UE_LOG(LogMultiServerSync, Display, TEXT("Hardware timer calibrated: offset=%lld microseconds"), HardwareTimerOffset);
}

// 하드웨어 타이머를 UTC 시간으로 변환
uint64 FNetworkManager::HardwareTimeToUTC(uint64 HardwareTime) const
{
    int64 UTCTime = static_cast<int64>(HardwareTime) + HardwareTimerOffset;
    return static_cast<uint64>(UTCTime > 0 ? UTCTime : 0);
}

void FNetworkManager::Shutdown()
{
    UE_LOG(LogMultiServerSync, Display, TEXT("Shutting down Network Manager..."));

    // 수신 스레드 중지
    if (ReceiverWorker)
    {
        ReceiverWorker->Stop();
    }

    if (ReceiverThread)
    {
        ReceiverThread->Kill(true);
        delete ReceiverThread;
        ReceiverThread = nullptr;
    }

    // 소켓 정리
    if (BroadcastSocket)
    {
        BroadcastSocket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(BroadcastSocket);
        BroadcastSocket = nullptr;
    }

    if (ReceiveSocket)
    {
        ReceiveSocket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ReceiveSocket);
        ReceiveSocket = nullptr;
    }

    // 틱 해제
    FTSTicker::GetCoreTicker().RemoveTicker(MasterSlaveTickHandle);

    // 메모리 정리
    delete ReceiverWorker;
    ReceiverWorker = nullptr;

    // 중복 메시지 추적 틱 해제
    if (DuplicateTrackerTickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(DuplicateTrackerTickHandle);
        DuplicateTrackerTickHandle.Reset();
    }

    bIsInitialized = false;
    UE_LOG(LogMultiServerSync, Display, TEXT("Network Manager shutdown completed"));

    // 모든 주기적 핑 비활성화
    PeriodicPingStates.Empty();

    // 대기 중인 핑 요청 정리
    PendingPingRequests.Empty();

    // 네트워크 지연 통계 정리
    ServerLatencyStats.Empty();

    // 틱 델리게이트 제거
    if (LatencyMeasurementTickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(LatencyMeasurementTickHandle);
        LatencyMeasurementTickHandle.Reset();
    }

    if (PingTimeoutCheckHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(PingTimeoutCheckHandle);
        PingTimeoutCheckHandle.Reset();
    }

    // 품질 평가 틱 해제 (추가)
    if (QualityAssessmentTickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(QualityAssessmentTickHandle);
        QualityAssessmentTickHandle.Reset();
    }

    // 시퀀스 관리 틱 해제
    if (SequenceManagementTickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(SequenceManagementTickHandle);
        SequenceManagementTickHandle.Reset();
    }

    // 시퀀스 추적 정보 정리
    EndpointSequenceTrackers.Empty();

    // 메시지 캐시 틱 해제
    if (MessageCacheTickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(MessageCacheTickHandle);
        MessageCacheTickHandle.Reset();
    }

    // 멱등성 캐시 정리 틱 해제
    if (IdempotentCacheTickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(IdempotentCacheTickHandle);
        IdempotentCacheTickHandle.Reset();
    }
}

bool FNetworkManager::SendMessage(const FString& EndpointId, const TArray<uint8>& Message)
{
    if (!bIsInitialized)
    {
        return false;
    }

    FServerEndpoint* TargetServer = DiscoveredServers.Find(EndpointId);
    if (!TargetServer)
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Endpoint not found: %s"), *EndpointId);
        return false;
    }

    // 메시지 생성
    FNetworkMessage NetworkMessage(ENetworkMessageType::Data, Message);
    NetworkMessage.SetProjectId(ProjectId);
    NetworkMessage.SetSequenceNumber(GetNextSequenceNumber());

    // 엔드포인트로 전송
    FIPv4Endpoint Endpoint(TargetServer->IPAddress, TargetServer->Port);
    return SendMessageToEndpoint(Endpoint, NetworkMessage);
}

bool FNetworkManager::BroadcastMessage(const TArray<uint8>& Message)
{
    if (!bIsInitialized)
    {
        return false;
    }

    // 메시지 생성
    FNetworkMessage NetworkMessage(ENetworkMessageType::Data, Message);
    NetworkMessage.SetProjectId(ProjectId);
    NetworkMessage.SetSequenceNumber(GetNextSequenceNumber());

    return BroadcastMessageToServers(NetworkMessage);
}

void FNetworkManager::RegisterMessageHandler(TFunction<void(const FString&, const TArray<uint8>&)> Handler)
{
    MessageHandler = Handler;
}

bool FNetworkManager::DiscoverServers()
{
    return SendDiscoveryMessage();
}

TArray<FString> FNetworkManager::GetDiscoveredServers() const
{
    TArray<FString> Result;
    for (const auto& Pair : DiscoveredServers)
    {
        Result.Add(Pair.Value.ToString());
    }
    return Result;
}

FGuid FNetworkManager::GenerateProjectId() const
{
    return FGuid::NewGuid();
}

void FNetworkManager::SetProjectId(const FGuid& InProjectId)
{
    ProjectId = InProjectId;
}

FGuid FNetworkManager::GetProjectId() const
{
    return ProjectId;
}

// ProcessReceivedData 함수에 설정 메시지 처리 추가
void FNetworkManager::ProcessReceivedData(const TArray<uint8>& Data, const FIPv4Endpoint& Sender)
{
    // 메시지 파싱
    FNetworkMessage Message(Data);

    // 우리 프로젝트의 메시지가 아니면 무시
    if (Message.GetProjectId().IsValid() && Message.GetProjectId() != ProjectId)
    {
        return;
    }

    // 메시지 역직렬화
    FNetworkMessage DeserializedMessage;
    if (DeserializedMessage.Deserialize(Data))
    {
        // 중복 메시지 체크 (특정 메시지 유형 제외)
        if (Message.GetType() != ENetworkMessageType::Discovery &&
            Message.GetType() != ENetworkMessageType::DiscoveryResponse &&
            Message.GetType() != ENetworkMessageType::MessageAck)
        {
            // 이미 처리한 메시지인지 확인
            if (IsDuplicateMessage(Sender, Message.GetSequenceNumber()))
            {
                UE_LOG(LogMultiServerSync, Verbose, TEXT("Duplicate message detected (Sender: %s, Seq: %u, Type: %d)"),
                    *Sender.ToString(), Message.GetSequenceNumber(), (int)Message.GetType());

                // 캐시된 처리 결과 사용 (멱등성 처리)
                if (ProcessCachedMessage(Sender, Message.GetSequenceNumber()))
                {
                    // 캐시된 처리 결과가 있는 경우
                    return;
                }

                // 캐시된 처리 결과가 없는 경우, 기본 중복 처리
                // 중복 메시지이지만, ACK가 필요한 메시지면 재확인 응답 전송
                if (Message.GetFlags() & 1) // Flag = 1: ACK 필요
                {
                    // ACK 메시지 생성
                    TArray<uint8> AckData;
                    uint16 SequenceNumber = Message.GetSequenceNumber();
                    AckData.SetNum(sizeof(uint16));
                    FMemory::Memcpy(AckData.GetData(), &SequenceNumber, sizeof(uint16));

                    FNetworkMessage AckMessage(ENetworkMessageType::MessageAck, AckData);
                    AckMessage.SetProjectId(ProjectId);
                    AckMessage.SetSequenceNumber(GetNextSequenceNumber());

                    // ACK 메시지 전송
                    SendMessageToEndpoint(Sender, AckMessage);

                    UE_LOG(LogMultiServerSync, Verbose, TEXT("Sent ACK for duplicate message (Seq: %u, to: %s)"),
                        SequenceNumber, *Sender.ToString());
                }

                return; // 중복 메시지는 더 이상 처리하지 않음
            }
        }

        // 메시지 순서 확인 (일부 메시지 유형은 제외)
        if (Message.GetType() != ENetworkMessageType::Discovery &&
            Message.GetType() != ENetworkMessageType::DiscoveryResponse &&
            Message.GetType() != ENetworkMessageType::MessageAck &&
            Message.GetType() != ENetworkMessageType::MessageRetry)
        {
            if (!ShouldProcessMessage(Sender, Message.GetSequenceNumber()))
            {
                return; // 순서가 맞지 않는 메시지는 처리하지 않음
            }
        }

        // 처리할 메시지로 판단되면 추적에 추가 (특정 메시지 유형 제외)
        if (Message.GetType() != ENetworkMessageType::Discovery &&
            Message.GetType() != ENetworkMessageType::DiscoveryResponse &&
            Message.GetType() != ENetworkMessageType::MessageAck)
        {
            // 처리한 메시지 추적에 추가
            AddProcessedMessage(Sender, Message.GetSequenceNumber());

            // 처리한 메시지 캐싱 (메시지 타입에 따라 캐싱 여부 결정)
            if (Message.GetType() == ENetworkMessageType::Data ||
                Message.GetType() == ENetworkMessageType::Command ||
                Message.GetType() == ENetworkMessageType::Custom)
            {
                CacheProcessedMessage(Sender, Message);
            }
        }

        // 메시지 유형에 따라 처리
        switch (Message.GetType())
        {
        case ENetworkMessageType::Discovery:
            HandleDiscoveryMessage(Message, Sender);
            break;
        case ENetworkMessageType::DiscoveryResponse:
            HandleDiscoveryResponseMessage(Message, Sender);
            break;
        case ENetworkMessageType::TimeSync:
            HandleTimeSyncMessage(Message, Sender);
            break;
        case ENetworkMessageType::FrameSync:
            HandleFrameSyncMessage(Message, Sender);
            break;
        case ENetworkMessageType::Command:
            HandleCommandMessage(Message, Sender);
            break;
        case ENetworkMessageType::Data:
            HandleDataMessage(Message, Sender);
            break;
            // 마스터-슬레이브 프로토콜 메시지 처리
        case ENetworkMessageType::MasterAnnouncement:
            HandleMasterAnnouncement(Message, Sender);
            break;
        case ENetworkMessageType::MasterQuery:
            HandleMasterQuery(Message, Sender);
            break;
        case ENetworkMessageType::MasterResponse:
            HandleMasterResponse(Message, Sender);
            break;
        case ENetworkMessageType::MasterElection:
            HandleMasterElection(Message, Sender);
            break;
        case ENetworkMessageType::MasterVote:
            HandleMasterVote(Message, Sender);
            break;
        case ENetworkMessageType::MasterResign:
            HandleMasterResign(Message, Sender);
            break;
        case ENetworkMessageType::RoleChange:
            HandleRoleChange(Message, Sender);
            break;
            // 설정 관련 메시지 처리 - 새로 추가
        case ENetworkMessageType::SettingsSync:
            HandleSettingsSyncMessage(Message, Sender);
            break;
        case ENetworkMessageType::SettingsRequest:
            HandleSettingsRequestMessage(Message, Sender);
            break;
        case ENetworkMessageType::SettingsResponse:
            HandleSettingsResponseMessage(Message, Sender);
            break;
        case ENetworkMessageType::Custom:
            HandleCustomMessage(Message, Sender);
            break;
        case ENetworkMessageType::PingRequest:
        {
            // 수정된 코드 - FMemoryReader 사용
            TArray<uint8> DataCopy = Message.GetData();
            TSharedPtr<FMemoryReader> Reader = MakeShareable(new FMemoryReader(DataCopy));
            HandlePingRequest(Reader, Sender);
        }
        break;
        case ENetworkMessageType::MessageAck:
            HandleMessageAck(Message, Sender);
            break;
        case ENetworkMessageType::MessageRetry:
            HandleMessageRetryRequest(Message, Sender);
            break;
        default:
            UE_LOG(LogMultiServerSync, Warning, TEXT("Unknown message type received: %d"), (int)Message.GetType());
            break;
        }
    }
}

// 핑 타임아웃 검사 함수
void FNetworkManager::CheckPingTimeouts()
{
    // 현재 시간
    double CurrentTime = FPlatformTime::Seconds();

    // 타임아웃된 요청 목록
    TArray<uint32> TimeoutRequests;

    // 모든 대기 중인 요청 검사
    for (auto& Pair : PendingPingRequests)
    {
        uint32 SequenceNumber = Pair.Key;
        TPair<FIPv4Endpoint, double>& RequestInfo = Pair.Value;

        double RequestTime = RequestInfo.Value;
        double ElapsedTime = CurrentTime - RequestTime;

        // 타임아웃 확인
        if (ElapsedTime > PING_TIMEOUT_SECONDS)
        {
            TimeoutRequests.Add(SequenceNumber);

            // 서버 엔드포인트 가져오기
            FIPv4Endpoint ServerEndpoint = RequestInfo.Key;
            FString ServerID = ServerEndpoint.ToString();

            // 타임아웃 로그
            UE_LOG(LogMultiServerSync, Warning, TEXT("Ping request timed out (Seq: %u, Server: %s, Elapsed: %.2f s)"),
                SequenceNumber, *ServerID, ElapsedTime);

            // 패킷 손실 통계 업데이트
            if (ServerLatencyStats.Contains(ServerID))
            {
                ServerLatencyStats[ServerID].LostPackets++;
            }

            // 연속 타임아웃 증가
            IncrementConsecutiveTimeouts(ServerEndpoint);
        }
    }

    // 타임아웃된 요청 제거
    for (uint32 SequenceNumber : TimeoutRequests)
    {
        PendingPingRequests.Remove(SequenceNumber);
    }
}

// SendSettingsMessage 구현
bool FNetworkManager::SendSettingsMessage(const TArray<uint8>& SettingsData)
{
    if (!bIsInitialized)
    {
        return false;
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Sending settings sync message (%d bytes)"), SettingsData.Num());

    // 설정 데이터를 포함한 메시지 생성
    FNetworkMessage Message(ENetworkMessageType::SettingsSync, SettingsData);
    Message.SetProjectId(ProjectId);
    Message.SetSequenceNumber(GetNextSequenceNumber());

    // 모든 서버에 브로드캐스트
    return BroadcastMessageToServers(Message);
}

// RequestSettings 구현
bool FNetworkManager::RequestSettings()
{
    if (!bIsInitialized)
    {
        return false;
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Requesting settings from other servers"));

    // 요청 메시지 생성 (데이터 없음)
    FNetworkMessage Message(ENetworkMessageType::SettingsRequest, TArray<uint8>());
    Message.SetProjectId(ProjectId);
    Message.SetSequenceNumber(GetNextSequenceNumber());

    // 모든 서버에 브로드캐스트
    return BroadcastMessageToServers(Message);
}

// 설정 동기화 메시지 처리 구현
void FNetworkManager::HandleSettingsSyncMessage(const FNetworkMessage& Message, const FIPv4Endpoint& Sender)
{
    if (!bIsInitialized)
    {
        return;
    }

    // 발신자 식별
    FString SenderId;
    for (const auto& Pair : DiscoveredServers)
    {
        if (Pair.Value.IPAddress == Sender.Address && Pair.Value.Port == Sender.Port)
        {
            SenderId = Pair.Key;
            break;
        }
    }

    if (SenderId.IsEmpty())
    {
        SenderId = Sender.ToString();
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Received settings sync message from %s (%d bytes)"),
        *SenderId, Message.GetData().Num());

    // 설정 데이터 전달
    if (MessageHandler)
    {
        MessageHandler(SenderId, Message.GetData());
    }
}

// 설정 요청 메시지 처리 구현
void FNetworkManager::HandleSettingsRequestMessage(const FNetworkMessage& Message, const FIPv4Endpoint& Sender)
{
    if (!bIsInitialized)
    {
        return;
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Received settings request from %s"), *Sender.ToString());

    // 마스터 노드일 때만 응답
    if (bIsMaster)
    {
        UE_LOG(LogMultiServerSync, Display, TEXT("As master, responding to settings request"));

        // 메시지 핸들러를 통해 알림
        if (MessageHandler)
        {
            // 특수 메시지로 설정 요청을 알림
            TArray<uint8> RequestNotification;
            RequestNotification.Add(static_cast<uint8>(ENetworkMessageType::SettingsRequest));
            MessageHandler(Sender.ToString(), RequestNotification);
        }
    }
    else
    {
        UE_LOG(LogMultiServerSync, Verbose, TEXT("Ignoring settings request as non-master node"));
    }
}

// 설정 응답 메시지 처리 구현
void FNetworkManager::HandleSettingsResponseMessage(const FNetworkMessage& Message, const FIPv4Endpoint& Sender)
{
    if (!bIsInitialized)
    {
        return;
    }

    // 발신자 식별
    FString SenderId;
    for (const auto& Pair : DiscoveredServers)
    {
        if (Pair.Value.IPAddress == Sender.Address && Pair.Value.Port == Sender.Port)
        {
            SenderId = Pair.Key;
            break;
        }
    }

    if (SenderId.IsEmpty())
    {
        SenderId = Sender.ToString();
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Received settings response from %s (%d bytes)"),
        *SenderId, Message.GetData().Num());

    // 설정 데이터 전달
    if (MessageHandler)
    {
        MessageHandler(SenderId, Message.GetData());
    }
}

bool FNetworkManager::SendDiscoveryMessage()
{
    if (!bIsInitialized || !BroadcastSocket)
    {
        return false;
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Sending discovery message..."));

    // 디스커버리 메시지 생성
    // 호스트 이름을 데이터로 포함
    TArray<uint8> HostNameData;
    HostNameData.SetNum(HostName.Len() * sizeof(TCHAR));
    FMemory::Memcpy(HostNameData.GetData(), *HostName, HostName.Len() * sizeof(TCHAR));

    FNetworkMessage Message(ENetworkMessageType::Discovery, HostNameData);
    Message.SetProjectId(ProjectId);
    Message.SetSequenceNumber(GetNextSequenceNumber());

    // 직렬화
    TArray<uint8> Data = Message.Serialize();

    // 브로드캐스트 주소 생성
    TSharedRef<FInternetAddr> BroadcastAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
    BroadcastAddr->SetBroadcastAddress();
    BroadcastAddr->SetPort(BROADCAST_PORT);

    int32 BytesSent = 0;
    bool bSuccess = BroadcastSocket->SendTo(Data.GetData(), Data.Num(), BytesSent, *BroadcastAddr);

    if (!bSuccess || BytesSent != Data.Num())
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to send discovery message: %d bytes sent, expected %d"),
            BytesSent, Data.Num());
        return false;
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Discovery message sent successfully"));
    return true;
}

bool FNetworkManager::SendDiscoveryResponse(const FIPv4Endpoint& TargetEndpoint)
{
    if (!bIsInitialized || !ReceiveSocket)
    {
        return false;
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Sending discovery response to %s..."), *TargetEndpoint.ToString());

    // 응답 메시지 생성
    // 호스트 이름과 포트를 데이터로 포함
    FString ResponseData = FString::Printf(TEXT("%s:%d"), *HostName, Port);
    TArray<uint8> ResponseBytes;
    ResponseBytes.SetNum(ResponseData.Len() * sizeof(TCHAR));
    FMemory::Memcpy(ResponseBytes.GetData(), *ResponseData, ResponseData.Len() * sizeof(TCHAR));

    FNetworkMessage Message(ENetworkMessageType::DiscoveryResponse, ResponseBytes);
    Message.SetProjectId(ProjectId);
    Message.SetSequenceNumber(GetNextSequenceNumber());

    return SendMessageToEndpoint(TargetEndpoint, Message);
}

void FNetworkManager::AddOrUpdateServer(const FServerEndpoint& ServerInfo)
{
    // 자신은 목록에 추가하지 않음
    FServerEndpoint LocalInfo = CreateLocalServerInfo();
    if (ServerInfo == LocalInfo)
    {
        return;
    }

    // 서버 추가 또는 업데이트
    DiscoveredServers.Add(ServerInfo.Id, ServerInfo);

    UE_LOG(LogMultiServerSync, Display, TEXT("Server added/updated: %s (%s)"),
        *ServerInfo.Id, *ServerInfo.ToString());
}

bool FNetworkManager::SendMessageToEndpoint(const FIPv4Endpoint& Endpoint, const FNetworkMessage& Message)
{
    if (!bIsInitialized || !ReceiveSocket)
    {
        return false;
    }

    // 메시지 직렬화
    TArray<uint8> Data = Message.Serialize();

    // 엔드포인트 주소 생성
    TSharedRef<FInternetAddr> TargetAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
    TargetAddr->SetIp(Endpoint.Address.Value);
    TargetAddr->SetPort(Endpoint.Port);

    int32 BytesSent = 0;
    bool bSuccess = ReceiveSocket->SendTo(Data.GetData(), Data.Num(), BytesSent, *TargetAddr);

    if (!bSuccess || BytesSent != Data.Num())
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Failed to send message to %s: %d bytes sent, expected %d"),
            *Endpoint.ToString(), BytesSent, Data.Num());
        return false;
    }

    return true;
}

bool FNetworkManager::BroadcastMessageToServers(const FNetworkMessage& Message)
{
    bool bAllSucceeded = true;
    
    for (const auto& Pair : DiscoveredServers)
    {
        FIPv4Endpoint Endpoint(Pair.Value.IPAddress, Pair.Value.Port);
        if (!SendMessageToEndpoint(Endpoint, Message))
        {
            bAllSucceeded = false;
        }
    }
    
    return bAllSucceeded;
}

bool FNetworkManager::InitializeSockets()
{
    UE_LOG(LogMultiServerSync, Display, TEXT("Initializing network sockets..."));
    
    if (!CreateBroadcastSocket())
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to create broadcast socket"));
        return false;
    }
    
    if (!CreateReceiveSocket())
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to create receive socket"));
        return false;
    }
    
    UE_LOG(LogMultiServerSync, Display, TEXT("Sockets initialized successfully"));
    return true;
}

bool FNetworkManager::CreateBroadcastSocket()
{
    // 브로드캐스트 소켓 생성
    BroadcastSocket = FUdpSocketBuilder(TEXT("MultiServerSync_BroadcastSocket"))
        .AsReusable()
        .WithBroadcast()
        .WithSendBufferSize(65507) // UDP 최대 크기
        .Build();
    
    if (!BroadcastSocket)
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to create broadcast socket"));
        return false;
    }
    
    return true;
}

bool FNetworkManager::CreateReceiveSocket()
{
    // 수신 소켓 생성
    ReceiveSocket = FUdpSocketBuilder(TEXT("MultiServerSync_ReceiveSocket"))
        .AsReusable()
        .BoundToPort(Port)
        .WithReceiveBufferSize(65507) // UDP 최대 크기
        .Build();
    
    if (!ReceiveSocket)
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to create receive socket"));
        return false;
    }
    
    // 브로드캐스트 수신을 위한 추가 소켓
    FSocket* BroadcastReceiveSocket = FUdpSocketBuilder(TEXT("MultiServerSync_BroadcastReceiveSocket"))
        .AsReusable()
        .BoundToPort(BROADCAST_PORT)
        .WithReceiveBufferSize(65507) // UDP 최대 크기
        .Build();
    
    if (!BroadcastReceiveSocket)
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to create broadcast receive socket"));
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ReceiveSocket);
        ReceiveSocket = nullptr;
        return false;
    }
    
    // 이미 있는 수신 소켓을 정리하고 브로드캐스트 수신 소켓을 기본 수신 소켓으로 설정
    ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ReceiveSocket);
    ReceiveSocket = BroadcastReceiveSocket;
    
    return true;
}

bool FNetworkManager::StartReceiverThread()
{
    // 수신 작업자 생성
    ReceiverWorker = new FNetworkReceiverWorker(this, ReceiveSocket);
    if (!ReceiverWorker)
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to create receiver worker"));
        return false;
    }
    
    // 수신 스레드 시작
    ReceiverThread = FRunnableThread::Create(ReceiverWorker, TEXT("MultiServerSync_ReceiverThread"));
    if (!ReceiverThread)
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to create receiver thread"));
        delete ReceiverWorker;
        ReceiverWorker = nullptr;
        return false;
    }
    
    UE_LOG(LogMultiServerSync, Display, TEXT("Receiver thread started"));
    return true;
}

FServerEndpoint FNetworkManager::CreateLocalServerInfo() const
{
    FServerEndpoint Info;
    Info.Id = HostName;
    Info.HostName = HostName;
    
    // 로컬 IP 주소 가져오기
    bool bCanBindAll = false;
    TSharedPtr<FInternetAddr> LocalAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalHostAddr(*GLog, bCanBindAll);
    if (LocalAddr.IsValid())
    {
        uint32 LocalIP = 0;
        LocalAddr->GetIp(LocalIP);
        Info.IPAddress = FIPv4Address(LocalIP);
    }
    
    Info.Port = Port;
    Info.ProjectId = ProjectId;
    Info.ProjectVersion = ProjectVersion;
    Info.LastCommunicationTime = FPlatformTime::Seconds();
    
    return Info;
}

void FNetworkManager::CleanupServerList()
{
    const double CurrentTime = FPlatformTime::Seconds();
    const double TimeoutSeconds = 10.0; // 10초 이상 통신이 없으면 제거
    
    TArray<FString> ServersToRemove;
    
    for (const auto& Pair : DiscoveredServers)
    {
        if (CurrentTime - Pair.Value.LastCommunicationTime > TimeoutSeconds)
        {
            ServersToRemove.Add(Pair.Key);
        }
    }
    
    for (const FString& ServerId : ServersToRemove)
    {
        DiscoveredServers.Remove(ServerId);
        UE_LOG(LogMultiServerSync, Display, TEXT("Server removed due to timeout: %s"), *ServerId);
    }
}

void FNetworkManager::HandleDiscoveryMessage(const FNetworkMessage& Message, const FIPv4Endpoint& Sender)
{
    // 디스커버리 요청에 응답
    UE_LOG(LogMultiServerSync, Display, TEXT("Discovery message received from %s"), *Sender.ToString());
    
    // 발신자 정보 파싱
    FString SenderHostName;
    if (Message.GetData().Num() > 0)
    {
        SenderHostName = FString((TCHAR*)Message.GetData().GetData(), Message.GetData().Num() / sizeof(TCHAR));
    }
    
    // 서버 정보 생성
    FServerEndpoint ServerInfo;
    ServerInfo.Id = SenderHostName.IsEmpty() ? Sender.ToString() : SenderHostName;
    ServerInfo.HostName = SenderHostName;
    ServerInfo.IPAddress = Sender.Address;
    ServerInfo.Port = Sender.Port;
    ServerInfo.ProjectId = Message.GetProjectId();
    ServerInfo.LastCommunicationTime = FPlatformTime::Seconds();
    
    // 서버 목록에 추가
    AddOrUpdateServer(ServerInfo);
    
    // 디스커버리 응답 전송
    SendDiscoveryResponse(Sender);
}

void FNetworkManager::HandleDiscoveryResponseMessage(const FNetworkMessage& Message, const FIPv4Endpoint& Sender)
{
    UE_LOG(LogMultiServerSync, Display, TEXT("Discovery response received from %s"), *Sender.ToString());

    // 발신자 정보 파싱
    FString ResponseData;
    if (Message.GetData().Num() > 0)
    {
        ResponseData = FString((TCHAR*)Message.GetData().GetData(), Message.GetData().Num() / sizeof(TCHAR));
    }

    // 호스트 이름과 포트 파싱
    FString SenderHostName = ResponseData;  // HostName을 SenderHostName으로 변경
    uint16 SenderPort = DEFAULT_PORT;       // Port를 SenderPort로 변경

    int32 ColonPos = ResponseData.Find(TEXT(":"));
    if (ColonPos != INDEX_NONE)
    {
        SenderHostName = ResponseData.Left(ColonPos);
        FString PortStr = ResponseData.Mid(ColonPos + 1);
        SenderPort = FCString::Atoi(*PortStr);
    }

    // 서버 정보 생성
    FServerEndpoint ServerInfo;
    ServerInfo.Id = SenderHostName.IsEmpty() ? Sender.ToString() : SenderHostName;
    ServerInfo.HostName = SenderHostName;
    ServerInfo.IPAddress = Sender.Address;
    ServerInfo.Port = SenderPort;
    ServerInfo.ProjectId = Message.GetProjectId();
    ServerInfo.LastCommunicationTime = FPlatformTime::Seconds();

    // 서버 목록에 추가
    AddOrUpdateServer(ServerInfo);
}

void FNetworkManager::HandleTimeSyncMessage(const FNetworkMessage& Message, const FIPv4Endpoint& Sender)
{
    // 발신자 ID 찾기
    FString SenderId;
    for (const auto& Pair : DiscoveredServers)
    {
        if (Pair.Value.IPAddress == Sender.Address && Pair.Value.Port == Sender.Port)
        {
            SenderId = Pair.Key;
            break;
        }
    }

    if (SenderId.IsEmpty())
    {
        SenderId = Sender.ToString();
    }

    UE_LOG(LogMultiServerSync, Verbose, TEXT("Received time sync message from %s"), *SenderId);

    // 메시지를 TimeSync 모듈로 전달
    // 이 부분은 직접 구현하기보다 주석으로 표시만 하고,
    // 나중에 필요할 때 FSyncFrameworkManager에서 구현하는 것이 좋습니다.

    /*
    // 직접적인 모듈 참조는 순환 의존성을 일으킬 수 있으므로 주석 처리
    TSharedPtr<ISyncFrameworkManager> FrameworkManager = FMultiServerSyncModule::GetFrameworkManager();
    if (FrameworkManager.IsValid())
    {
        TSharedPtr<ITimeSync> TimeSync = FrameworkManager->GetTimeSync();
        if (TimeSync.IsValid())
        {
            // TimeSync를 FTimeSync로 캐스팅하여 PTP 메시지 처리 함수 호출
            FTimeSync* TimeSyncImpl = static_cast<FTimeSync*>(TimeSync.Get());
            TimeSyncImpl->ProcessPTPMessage(Message.GetData());
        }
    }
    */

    // 대신, 메시지 핸들러가 설정되어 있으면 그것을 호출합니다
    if (MessageHandler)
    {
        MessageHandler(SenderId, Message.GetData());
    }
}

bool FNetworkManager::SendTimeSyncMessage(const TArray<uint8>& PTPMessage)
{
    if (!bIsInitialized)
    {
        return false;
    }

    // 시간 동기화 메시지 생성
    FNetworkMessage Message(ENetworkMessageType::TimeSync, PTPMessage);
    Message.SetProjectId(ProjectId);
    Message.SetSequenceNumber(GetNextSequenceNumber());

    // 모든 서버에 브로드캐스트
    return BroadcastMessageToServers(Message);
}

void FNetworkManager::HandleFrameSyncMessage(const FNetworkMessage& Message, const FIPv4Endpoint& Sender)
{
    // 프레임 동기화 메시지 처리 (모듈 5에서 구현 예정)
}

void FNetworkManager::HandleCommandMessage(const FNetworkMessage& Message, const FIPv4Endpoint& Sender)
{
    // 명령 메시지 처리
    if (MessageHandler)
    {
        // 발신자 ID 찾기
        FString SenderId;
        for (const auto& Pair : DiscoveredServers)
        {
            if (Pair.Value.IPAddress == Sender.Address && Pair.Value.Port == Sender.Port)
            {
                SenderId = Pair.Key;
                break;
            }
        }
        
        if (SenderId.IsEmpty())
        {
            SenderId = Sender.ToString();
        }
        
        // 메시지 핸들러 호출
        MessageHandler(SenderId, Message.GetData());
    }
}

void FNetworkManager::HandleDataMessage(const FNetworkMessage& Message, const FIPv4Endpoint& Sender)
{
    // 데이터 메시지 처리 (HandleCommandMessage와 동일한 로직)
    HandleCommandMessage(Message, Sender);

    // 멱등성 예제 - 중요한 상태 변경을 일으키는 메시지인 경우
    if (Message.GetFlags() & 2) // 가정: Flag = 2는 중요한 상태 변경 메시지
    {
        // 작업 ID 생성 (메시지 유형 + 시퀀스 번호 + 발신자 엔드포인트)
        FString OperationId = FString::Printf(TEXT("DataMsg_%d_%u_%s"),
            static_cast<int>(Message.GetType()),
            Message.GetSequenceNumber(),
            *Sender.ToString());

        // 멱등성 보장 작업 수행
        ExecuteIdempotentOperation(OperationId, Message.GetSequenceNumber(), [&]() {
            // 여기에 멱등성이 필요한 중요한 작업 수행
            // 예: 중요한 상태 변경, 데이터베이스 업데이트 등

            // 간단한 예제:
            UE_LOG(LogMultiServerSync, Display, TEXT("Executing important state change operation [%s]"), *OperationId);

            // 작업 성공 여부와 결과 반환
            return FIdempotentResult::Success();
            });
    }

    if (Message.GetFlags() & 1) // Flag = 1: ACK 필요
    {
        // ACK 메시지 생성
        TArray<uint8> AckData;
        uint16 SequenceNumber = Message.GetSequenceNumber();
        AckData.SetNum(sizeof(uint16));
        FMemory::Memcpy(AckData.GetData(), &SequenceNumber, sizeof(uint16));

        FNetworkMessage AckMessage(ENetworkMessageType::MessageAck, AckData);
        AckMessage.SetProjectId(ProjectId);
        AckMessage.SetSequenceNumber(GetNextSequenceNumber());

        // ACK 메시지 전송
        SendMessageToEndpoint(Sender, AckMessage);

        UE_LOG(LogMultiServerSync, Verbose, TEXT("Sent ACK for message (Seq: %u, to: %s)"),
            SequenceNumber, *Sender.ToString());
    }
}

void FNetworkManager::HandleCustomMessage(const FNetworkMessage& Message, const FIPv4Endpoint& Sender)
{
    // 사용자 정의 메시지 처리 (HandleCommandMessage와 동일한 로직)
    HandleCommandMessage(Message, Sender);

    if (Message.GetFlags() & 1) // Flag = 1: ACK 필요
    {
        // ACK 메시지 생성
        TArray<uint8> AckData;
        uint16 SequenceNumber = Message.GetSequenceNumber();
        AckData.SetNum(sizeof(uint16));
        FMemory::Memcpy(AckData.GetData(), &SequenceNumber, sizeof(uint16));

        FNetworkMessage AckMessage(ENetworkMessageType::MessageAck, AckData);
        AckMessage.SetProjectId(ProjectId);
        AckMessage.SetSequenceNumber(GetNextSequenceNumber());

        // ACK 메시지 전송
        SendMessageToEndpoint(Sender, AckMessage);

        UE_LOG(LogMultiServerSync, Verbose, TEXT("Sent ACK for message (Seq: %u, to: %s)"),
            SequenceNumber, *Sender.ToString());
    }
}

uint16 FNetworkManager::GetNextSequenceNumber()
{
    return ++CurrentSequenceNumber;
}

bool FNetworkManager::HasEnoughTimePassed(double& LastTime, double Interval) const
{
    const double CurrentTime = FPlatformTime::Seconds();
    
    if (CurrentTime - LastTime >= Interval)
    {
        LastTime = CurrentTime;
        return true;
    }
    
    return false;
}

// 마스터 선출 시작 메서드
bool FNetworkManager::StartMasterElection()
{
    if (!bIsInitialized)
    {
        return false;
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Starting master election..."));

    // 이미 선출 진행 중이면 무시
    if (bElectionInProgress)
    {
        UE_LOG(LogMultiServerSync, Display, TEXT("Election already in progress"));
        return true;
    }

    // 선출 정보 초기화
    bElectionInProgress = true;
    CurrentElectionTerm++;
    ElectionVotes.Empty();
    LastElectionStartTime = FPlatformTime::Seconds();

    // 자신에게 투표
    float SelfVotePriority = CalculateVotePriority();
    ElectionVotes.Add(HostName, SelfVotePriority);

    // 선출 메시지 생성
    FString ElectionData = FString::Printf(TEXT("%s:%d:%f"),
        *HostName, CurrentElectionTerm, SelfVotePriority);
    TArray<uint8> ElectionBytes;
    ElectionBytes.SetNum(ElectionData.Len() * sizeof(TCHAR));
    FMemory::Memcpy(ElectionBytes.GetData(), *ElectionData, ElectionData.Len() * sizeof(TCHAR));

    // 선출 메시지 브로드캐스트
    FNetworkMessage Message(ENetworkMessageType::MasterElection, ElectionBytes);
    Message.SetProjectId(ProjectId);
    Message.SetSequenceNumber(GetNextSequenceNumber());

    UE_LOG(LogMultiServerSync, Display, TEXT("Broadcasting election message: %s"), *ElectionData);
    return BroadcastMessageToServers(Message);
}

// 마스터 공지 메서드
void FNetworkManager::AnnounceMaster()
{
    if (!bIsInitialized)
    {
        return;
    }

    // 로컬 서버가 마스터가 아니면 무시
    if (!bIsMaster)
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Cannot announce master: local server is not master"));
        return;
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Announcing master status..."));

    // 마스터 정보 생성
    FMasterInfo MasterInfo;
    MasterInfo.ServerId = HostName;
    MasterInfo.Priority = MasterPriority;
    MasterInfo.LastUpdateTime = FPlatformTime::Seconds();
    MasterInfo.ElectionTerm = CurrentElectionTerm;

    // 로컬 IP 주소 가져오기
    bool bCanBindAll = false;
    TSharedPtr<FInternetAddr> LocalAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalHostAddr(*GLog, bCanBindAll);
    if (LocalAddr.IsValid())
    {
        uint32 LocalIP = 0;
        LocalAddr->GetIp(LocalIP);
        MasterInfo.IPAddress = FIPv4Address(LocalIP);
    }
    // 여기서 클래스 멤버 변수를 사용
    MasterInfo.Port = this->Port;

    // 마스터 정보를 문자열로 직렬화
    FString MasterData = FString::Printf(TEXT("%s:%s:%d:%f:%d"),
        *MasterInfo.ServerId,
        *MasterInfo.IPAddress.ToString(),
        MasterInfo.Port,
        MasterInfo.Priority,
        MasterInfo.ElectionTerm);

    TArray<uint8> MasterBytes;
    MasterBytes.SetNum(MasterData.Len() * sizeof(TCHAR));
    FMemory::Memcpy(MasterBytes.GetData(), *MasterData, MasterData.Len() * sizeof(TCHAR));

    // 마스터 공지 메시지 생성 및 브로드캐스트
    FNetworkMessage Message(ENetworkMessageType::MasterAnnouncement, MasterBytes);
    Message.SetProjectId(ProjectId);
    Message.SetSequenceNumber(GetNextSequenceNumber());

    BroadcastMessageToServers(Message);
    LastMasterAnnouncementTime = FPlatformTime::Seconds();

    UE_LOG(LogMultiServerSync, Display, TEXT("Master announcement sent: %s"), *MasterData);
}

// 마스터 사임 메서드
void FNetworkManager::ResignMaster()
{
    if (!bIsInitialized || !bIsMaster)
    {
        return;
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Resigning as master..."));

    // 마스터 사임 메시지 생성
    FString ResignData = HostName;
    TArray<uint8> ResignBytes;
    ResignBytes.SetNum(ResignData.Len() * sizeof(TCHAR));
    FMemory::Memcpy(ResignBytes.GetData(), *ResignData, ResignData.Len() * sizeof(TCHAR));

    // 사임 메시지 브로드캐스트
    FNetworkMessage Message(ENetworkMessageType::MasterResign, ResignBytes);
    Message.SetProjectId(ProjectId);
    Message.SetSequenceNumber(GetNextSequenceNumber());

    BroadcastMessageToServers(Message);

    // 마스터 상태 업데이트
    bIsMaster = false;
    CurrentMaster = FMasterInfo(); // 빈 마스터 정보

    // 로컬 서버 상태 변경 알림
    UE_LOG(LogMultiServerSync, Display, TEXT("Local server is no longer master"));

    // 핸들러 호출
    if (MasterChangeHandler)
    {
        MasterChangeHandler(TEXT(""), false);
    }

    // 새 마스터 선출 시작
    StartMasterElection();
}

// 마스터 투표 전송 메서드 
void FNetworkManager::SendElectionVote(const FString& CandidateId)
{
    if (!bIsInitialized)
    {
        return;
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Sending vote for candidate: %s"), *CandidateId);

    // 투표 메시지 생성
    FString VoteData = FString::Printf(TEXT("%s:%s:%d:%f"),
        *HostName, *CandidateId, CurrentElectionTerm, MasterPriority);
    TArray<uint8> VoteBytes;
    VoteBytes.SetNum(VoteData.Len() * sizeof(TCHAR));
    FMemory::Memcpy(VoteBytes.GetData(), *VoteData, VoteData.Len() * sizeof(TCHAR));

    // 투표 메시지 브로드캐스트
    FNetworkMessage Message(ENetworkMessageType::MasterVote, VoteBytes);
    Message.SetProjectId(ProjectId);
    Message.SetSequenceNumber(GetNextSequenceNumber());

    BroadcastMessageToServers(Message);
}

// 마스터가 되려고 시도하는 메서드
bool FNetworkManager::TryBecomeMaster()
{
    if (!bIsInitialized)
    {
        return false;
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Trying to become master..."));

    // 이미 마스터면 무시
    if (bIsMaster)
    {
        UE_LOG(LogMultiServerSync, Display, TEXT("Already master"));
        return true;
    }

    // 투표 집계 및 우승자 결정
    FString WinnerId;
    float HighestPriority = -1.0f;

    for (const auto& Pair : ElectionVotes)
    {
        if (Pair.Value > HighestPriority)
        {
            HighestPriority = Pair.Value;
            WinnerId = Pair.Key;
        }
    }

    // 우승자가 자신인 경우
    if (WinnerId == HostName)
    {
        UE_LOG(LogMultiServerSync, Display, TEXT("Local server won election with priority %.2f"), HighestPriority);

        // 마스터 상태 업데이트
        bIsMaster = true;

        // 마스터 정보 업데이트
        CurrentMaster.ServerId = HostName;

        // 로컬 IP 주소 가져오기
        bool bCanBindAll = false;
        TSharedPtr<FInternetAddr> LocalAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalHostAddr(*GLog, bCanBindAll);
        if (LocalAddr.IsValid())
        {
            uint32 LocalIP = 0;
            LocalAddr->GetIp(LocalIP);
            CurrentMaster.IPAddress = FIPv4Address(LocalIP);
        }

        CurrentMaster.Port = Port;
        CurrentMaster.Priority = MasterPriority;
        CurrentMaster.LastUpdateTime = FPlatformTime::Seconds();
        CurrentMaster.ElectionTerm = CurrentElectionTerm;

        // 마스터 상태 공지
        AnnounceMaster();

        // 마스터 변경 핸들러 호출
        if (MasterChangeHandler)
        {
            MasterChangeHandler(HostName, true);
        }

        return true;
    }
    else
    {
        UE_LOG(LogMultiServerSync, Display, TEXT("Election lost to %s with priority %.2f"), *WinnerId, HighestPriority);
        return false;
    }
}

// 선출 종료 메서드
void FNetworkManager::EndElection(const FString& WinnerId, int32 ElectionTerm)
{
    if (!bIsInitialized || !bElectionInProgress)
    {
        return;
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Ending election: winner=%s, term=%d"), *WinnerId, ElectionTerm);

    // 선출 진행 플래그 초기화
    bElectionInProgress = false;

    // 자신이 우승자인 경우
    if (WinnerId == HostName)
    {
        TryBecomeMaster();
    }
    else
    {
        // 다른 서버가 우승자인 경우, 해당 서버의 마스터 상태를 기다림
        // 해당 서버로부터 마스터 공지가 오면 HandleMasterAnnouncement에서 처리됨
    }
}

// 마스터 타임아웃 체크 메서드
void FNetworkManager::CheckMasterTimeout()
{
    if (!bIsInitialized || bIsMaster)
    {
        return; // 자신이 마스터면 체크하지 않음
    }

    double CurrentTime = FPlatformTime::Seconds();

    // 선출이 진행 중인 경우 선출 타임아웃 체크
    if (bElectionInProgress)
    {
        if (CurrentTime - LastElectionStartTime > ELECTION_TIMEOUT_SECONDS)
        {
            UE_LOG(LogMultiServerSync, Display, TEXT("Election timeout, resolving election..."));

            // 투표 결과에 따라 마스터 결정
            TryBecomeMaster();

            // 선출 종료
            bElectionInProgress = false;
        }
        return;
    }

    // 마스터 타임아웃 체크
    if (CurrentMaster.ServerId.IsEmpty() ||
        CurrentTime - CurrentMaster.LastUpdateTime > MASTER_TIMEOUT_SECONDS)
    {
        UE_LOG(LogMultiServerSync, Display, TEXT("Master timeout, starting new election..."));

        // 새 마스터 선출 시작
        StartMasterElection();
    }
}

// 투표 우선순위 계산 메서드
float FNetworkManager::CalculateVotePriority()
{
    // 기본적으로 설정된 마스터 우선순위 사용
    float Priority = MasterPriority;

    // 추가 요소를 고려하여 우선순위 조정 가능
    // 예: 시스템 리소스, 네트워크 상태 등

    return Priority;
}

// 마스터 상태 업데이트 메서드
void FNetworkManager::UpdateMasterStatus(const FString& NewMasterId, bool bLocalServerIsMaster)
{
    // 변경 사항이 없으면 무시
    if (bLocalServerIsMaster == bIsMaster && NewMasterId == CurrentMaster.ServerId)
    {
        return;
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Updating master status: new master=%s, local is master=%s"),
        *NewMasterId, bLocalServerIsMaster ? TEXT("true") : TEXT("false"));

    // 마스터 상태 업데이트
    bIsMaster = bLocalServerIsMaster;

    // 핸들러 호출
    if (MasterChangeHandler)
    {
        MasterChangeHandler(NewMasterId, bLocalServerIsMaster);
    }
}

// 역할 변경 알림 전송 메서드
void FNetworkManager::SendRoleChangeNotification()
{
    if (!bIsInitialized)
    {
        return;
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Sending role change notification: master=%s"),
        bIsMaster ? TEXT("true") : TEXT("false"));

    // 역할 정보 생성
    FString RoleData = FString::Printf(TEXT("%s:%s:%d"),
        *HostName, bIsMaster ? TEXT("true") : TEXT("false"), CurrentElectionTerm);
    TArray<uint8> RoleBytes;
    RoleBytes.SetNum(RoleData.Len() * sizeof(TCHAR));
    FMemory::Memcpy(RoleBytes.GetData(), *RoleData, RoleData.Len() * sizeof(TCHAR));

    // 역할 변경 메시지 생성 및 브로드캐스트
    FNetworkMessage Message(ENetworkMessageType::RoleChange, RoleBytes);
    Message.SetProjectId(ProjectId);
    Message.SetSequenceNumber(GetNextSequenceNumber());

    BroadcastMessageToServers(Message);
}

// 마스터 공지 메시지 처리 메서드
void FNetworkManager::HandleMasterAnnouncement(const FNetworkMessage& Message, const FIPv4Endpoint& Sender)
{
    if (!bIsInitialized)
    {
        return;
    }

    // 메시지 데이터 파싱
    FString MasterData;
    if (Message.GetData().Num() > 0)
    {
        MasterData = FString((TCHAR*)Message.GetData().GetData(), Message.GetData().Num() / sizeof(TCHAR));
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Received master announcement: %s"), *MasterData);

    // 마스터 정보 파싱
    TArray<FString> Parts;
    MasterData.ParseIntoArray(Parts, TEXT(":"), true);

    if (Parts.Num() < 5)
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Invalid master announcement data format"));
        return;
    }

    FString MasterId = Parts[0];
    FString IPAddressStr = Parts[1];
    // 'Port' 변수 이름 변경 -> 'PortNumber'
    int32 PortNumber = FCString::Atoi(*Parts[2]);
    float Priority = FCString::Atof(*Parts[3]);
    int32 ElectionTerm = FCString::Atoi(*Parts[4]);

    // 발신자가 자신인 경우 무시
    if (MasterId == HostName)
    {
        return;
    }

    // IP 주소 파싱
    FIPv4Address MasterIP;
    FIPv4Address::Parse(IPAddressStr, MasterIP);

    // 마스터 정보 업데이트
    FMasterInfo NewMasterInfo;
    NewMasterInfo.ServerId = MasterId;
    NewMasterInfo.IPAddress = MasterIP;
    // 변경된 변수명 사용
    NewMasterInfo.Port = PortNumber;
    NewMasterInfo.Priority = Priority;

    // 현재 선출 기간보다 이전이면 무시
    if (ElectionTerm < CurrentElectionTerm)
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Ignoring master announcement from previous election term"));
        return;
    }

    // 다른 서버가 마스터가 됨
    CurrentElectionTerm = ElectionTerm;
    CurrentMaster = NewMasterInfo;
    bElectionInProgress = false;

    // 자신이 이전에 마스터였다면 역할 변경
    if (bIsMaster)
    {
        bIsMaster = false;
        UpdateMasterStatus(MasterId, false);
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Master updated: %s"), *CurrentMaster.ToString());
}

// 마스터 정보 요청 메시지 처리 메서드
void FNetworkManager::HandleMasterQuery(const FNetworkMessage& Message, const FIPv4Endpoint& Sender)
{
    if (!bIsInitialized)
    {
        return;
    }

    // 메시지 데이터 파싱
    FString QueryData;
    if (Message.GetData().Num() > 0)
    {
        QueryData = FString((TCHAR*)Message.GetData().GetData(), Message.GetData().Num() / sizeof(TCHAR));
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Received master query from: %s"), *QueryData);

    // 현재 마스터 정보 확인
    if (bIsMaster)
    {
        // 자신이 마스터인 경우, 마스터 공지 전송
        AnnounceMaster();
    }
    else if (!CurrentMaster.ServerId.IsEmpty())
    {
        // 알고 있는 마스터 정보가 있는 경우, 마스터 정보 응답 전송
        FString MasterData = FString::Printf(TEXT("%s:%s:%d:%f:%d"),
            *CurrentMaster.ServerId,
            *CurrentMaster.IPAddress.ToString(),
            CurrentMaster.Port,
            CurrentMaster.Priority,
            CurrentMaster.ElectionTerm);

        TArray<uint8> MasterBytes;
        MasterBytes.SetNum(MasterData.Len() * sizeof(TCHAR));
        FMemory::Memcpy(MasterBytes.GetData(), *MasterData, MasterData.Len() * sizeof(TCHAR));

        FNetworkMessage Response(ENetworkMessageType::MasterResponse, MasterBytes);
        Response.SetProjectId(ProjectId);
        Response.SetSequenceNumber(GetNextSequenceNumber());

        SendMessageToEndpoint(Sender, Response);
    }
    else
    {
        // 마스터 정보가 없는 경우, 새 마스터 선출 시작
        StartMasterElection();
    }
}

// 마스터 정보 응답 메시지 처리 메서드
void FNetworkManager::HandleMasterResponse(const FNetworkMessage& Message, const FIPv4Endpoint& Sender)
{
    if (!bIsInitialized)
    {
        return;
    }

    // 메시지 데이터 파싱
    FString MasterData;
    if (Message.GetData().Num() > 0)
    {
        MasterData = FString((TCHAR*)Message.GetData().GetData(), Message.GetData().Num() / sizeof(TCHAR));
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Received master response: %s"), *MasterData);

    // 마스터 정보 파싱
    TArray<FString> Parts;
    MasterData.ParseIntoArray(Parts, TEXT(":"), true);

    if (Parts.Num() < 5)
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Invalid master response data format"));
        return;
    }

    FString MasterId = Parts[0];
    FString IPAddressStr = Parts[1];
    // 'Port' 변수 이름 변경 -> 'PortNumber'
    int32 PortNumber = FCString::Atoi(*Parts[2]);
    float Priority = FCString::Atof(*Parts[3]);
    int32 ElectionTerm = FCString::Atoi(*Parts[4]);

    // IP 주소 파싱
    FIPv4Address MasterIP;
    FIPv4Address::Parse(IPAddressStr, MasterIP);

    // 마스터 정보 업데이트
    FMasterInfo NewMasterInfo;
    NewMasterInfo.ServerId = MasterId;
    NewMasterInfo.IPAddress = MasterIP;
    // 여기서 변경된 변수명 사용
    NewMasterInfo.Port = PortNumber;
    NewMasterInfo.Priority = Priority;
    NewMasterInfo.ElectionTerm = ElectionTerm;

    // 현재 선출 기간보다 이전이면 무시
    if (ElectionTerm < CurrentElectionTerm)
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Ignoring master response from previous election term"));
        return;
    }

    // 마스터 정보 업데이트
    CurrentElectionTerm = ElectionTerm;
    CurrentMaster = NewMasterInfo;
    bElectionInProgress = false;

    // 자신이 이전에 마스터였다면 역할 변경
    if (bIsMaster && MasterId != HostName)
    {
        bIsMaster = false;
        UpdateMasterStatus(MasterId, false);
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Master updated from response: %s"), *CurrentMaster.ToString());
}

// 마스터 선출 메시지 처리 메서드
void FNetworkManager::HandleMasterElection(const FNetworkMessage& Message, const FIPv4Endpoint& Sender)
{
    if (!bIsInitialized)
    {
        return;
    }

    // 메시지 데이터 파싱
    FString ElectionData;
    if (Message.GetData().Num() > 0)
    {
        ElectionData = FString((TCHAR*)Message.GetData().GetData(), Message.GetData().Num() / sizeof(TCHAR));
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Received election message: %s"), *ElectionData);

    // 선출 정보 파싱
    TArray<FString> Parts;
    ElectionData.ParseIntoArray(Parts, TEXT(":"), true);

    if (Parts.Num() < 3)
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Invalid election data format"));
        return;
    }

    FString CandidateId = Parts[0];
    int32 ElectionTerm = FCString::Atoi(*Parts[1]);
    float Priority = FCString::Atof(*Parts[2]);

    // 발신자가 자신인 경우 무시
    if (CandidateId == HostName)
    {
        return;
    }

    // 현재 선출 기간보다 이전이면 무시
    if (ElectionTerm < CurrentElectionTerm)
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Ignoring election from previous term"));
        return;
    }

    // 더 높은 선출 기간이면 자신의 선출 기간 업데이트
    if (ElectionTerm > CurrentElectionTerm)
    {
        CurrentElectionTerm = ElectionTerm;
        bElectionInProgress = true;
        ElectionVotes.Empty();
        LastElectionStartTime = FPlatformTime::Seconds();
    }

    // 후보자에게 투표
    ElectionVotes.Add(CandidateId, Priority);
    SendElectionVote(CandidateId);

    UE_LOG(LogMultiServerSync, Display, TEXT("Voted for candidate: %s with priority %.2f"), *CandidateId, Priority);
}

// 마스터 투표 메시지 처리 메서드
void FNetworkManager::HandleMasterVote(const FNetworkMessage& Message, const FIPv4Endpoint& Sender)
{
    if (!bIsInitialized || !bElectionInProgress)
    {
        return;
    }

    // 메시지 데이터 파싱
    FString VoteData;
    if (Message.GetData().Num() > 0)
    {
        VoteData = FString((TCHAR*)Message.GetData().GetData(), Message.GetData().Num() / sizeof(TCHAR));
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Received vote: %s"), *VoteData);

    // 투표 정보 파싱
    TArray<FString> Parts;
    VoteData.ParseIntoArray(Parts, TEXT(":"), true);

    if (Parts.Num() < 4)
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Invalid vote data format"));
        return;
    }

    FString VoterId = Parts[0];
    FString CandidateId = Parts[1];
    int32 ElectionTerm = FCString::Atoi(*Parts[2]);
    float VoterPriority = FCString::Atof(*Parts[3]);

    // 발신자가 자신인 경우 무시
    if (VoterId == HostName)
    {
        return;
    }

    // 선출 기간이 맞지 않으면 무시
    if (ElectionTerm != CurrentElectionTerm)
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Ignoring vote from different election term"));
        return;
    }

    // 자신이 후보자인 경우에만 투표 수집
    if (CandidateId == HostName)
    {
        float* ExistingVote = ElectionVotes.Find(VoterId);
        if (!ExistingVote || *ExistingVote < VoterPriority)
        {
            ElectionVotes.Add(VoterId, VoterPriority);
            UE_LOG(LogMultiServerSync, Display, TEXT("Received vote from %s with priority %.2f"), *VoterId, VoterPriority);
        }

        // 투표 수가 서버 총 개수의 과반수 이상이면 마스터 결정
        if (ElectionVotes.Num() > (DiscoveredServers.Num() + 1) / 2) // +1은 자신을 포함
        {
            UE_LOG(LogMultiServerSync, Display, TEXT("Received majority votes, becoming master"));
            TryBecomeMaster();
            bElectionInProgress = false;
        }
    }
}

// 마스터 사임 메시지 처리 메서드
void FNetworkManager::HandleMasterResign(const FNetworkMessage& Message, const FIPv4Endpoint& Sender)
{
    if (!bIsInitialized)
    {
        return;
    }

    // 메시지 데이터 파싱
    FString ResignData;
    if (Message.GetData().Num() > 0)
    {
        ResignData = FString((TCHAR*)Message.GetData().GetData(), Message.GetData().Num() / sizeof(TCHAR));
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Received master resign: %s"), *ResignData);

    // 사임한 마스터 ID
    FString ResignedMasterId = ResignData;

    // 현재 마스터가 사임한 경우에만 처리
    if (ResignedMasterId == CurrentMaster.ServerId)
    {
        UE_LOG(LogMultiServerSync, Display, TEXT("Current master has resigned, starting new election"));

        // 마스터 정보 초기화
        CurrentMaster = FMasterInfo();

        // 새 마스터 선출 시작
        StartMasterElection();
    }
}

// 역할 변경 메시지 처리 메서드 (계속)
void FNetworkManager::HandleRoleChange(const FNetworkMessage& Message, const FIPv4Endpoint& Sender)
{
    if (!bIsInitialized)
    {
        return;
    }

    // 메시지 데이터 파싱
    FString RoleData;
    if (Message.GetData().Num() > 0)
    {
        RoleData = FString((TCHAR*)Message.GetData().GetData(), Message.GetData().Num() / sizeof(TCHAR));
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Received role change: %s"), *RoleData);

    // 역할 정보 파싱
    TArray<FString> Parts;
    RoleData.ParseIntoArray(Parts, TEXT(":"), true);

    if (Parts.Num() < 3)
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Invalid role change data format"));
        return;
    }

    FString ServerId = Parts[0];
    bool bServerIsMaster = Parts[1].ToBool();
    int32 ElectionTerm = FCString::Atoi(*Parts[2]);

    // 선출 기간이 맞지 않으면 무시
    if (ElectionTerm < CurrentElectionTerm)
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Ignoring role change from previous election term"));
        return;
    }

    // 발신자가 자신인 경우 무시
    if (ServerId == HostName)
    {
        return;
    }

    // 서버가 마스터가 된 경우
    if (bServerIsMaster)
    {
        UE_LOG(LogMultiServerSync, Display, TEXT("Server %s is now master (term: %d)"), *ServerId, ElectionTerm);

        // 선출 상태 업데이트
        CurrentElectionTerm = ElectionTerm;
        bElectionInProgress = false;

        // 마스터 정보 업데이트
        // 여기서는 IP 주소와 포트를 알 수 없으므로 마스터 공지를 기다림

        // 자신이 마스터였다면 역할 변경
        if (bIsMaster)
        {
            bIsMaster = false;
            UpdateMasterStatus(ServerId, false);
        }
    }
}

// 주기적인 마스터 상태 업데이트 메서드
void FNetworkManager::TickMasterSlaveProtocol()
{
    if (!bIsInitialized)
    {
        return;
    }

    double CurrentTime = FPlatformTime::Seconds();

    // 마스터 타임아웃 체크
    CheckMasterTimeout();

    // 마스터인 경우 주기적으로 마스터 상태 공지
    if (bIsMaster)
    {
        // 마지막 마스터 공지 이후 MASTER_ANNOUNCEMENT_INTERVAL 시간이 지났으면 다시 공지
        const float MASTER_ANNOUNCEMENT_INTERVAL = 2.0f; // 2초마다 공지
        if (CurrentTime - LastMasterAnnouncementTime > MASTER_ANNOUNCEMENT_INTERVAL)
        {
            AnnounceMaster();
        }
    }

    // 선출 진행 중인 경우 타임아웃 체크
    if (bElectionInProgress)
    {
        // 선출 시작 이후 ELECTION_TIMEOUT_SECONDS 시간이 지났으면 선출 종료
        if (CurrentTime - LastElectionStartTime > ELECTION_TIMEOUT_SECONDS)
        {
            UE_LOG(LogMultiServerSync, Display, TEXT("Election timeout, resolving election..."));
            TryBecomeMaster();
            bElectionInProgress = false;
        }
    }
}

// 마스터-슬레이브 프로토콜 틱 콜백
bool FNetworkManager::MasterSlaveProtocolTick(float DeltaTime)
{
    TickMasterSlaveProtocol();
    return true; // 계속 틱 유지
}

void FPingMessage::Serialize(FMemoryWriter& Writer) const
{
    // 메시지 타입 직렬화
    uint8 TypeValue = static_cast<uint8>(Type);
    Writer << TypeValue;

    // 타임스탬프 직렬화 - 임시 변수를 사용하여 수정
    uint64 TempTimestamp = Timestamp;
    Writer << TempTimestamp;

    // 시퀀스 번호 직렬화 - 임시 변수를 사용하여 수정
    uint32 TempSequenceNumber = SequenceNumber;
    Writer << TempSequenceNumber;
}

void FPingMessage::Deserialize(FMemoryReader& Reader)
{
    // 메시지 타입 역직렬화
    uint8 TypeValue;
    Reader << TypeValue;
    Type = static_cast<EPingMessageType>(TypeValue);

    // 타임스탬프 역직렬화
    Reader << Timestamp;

    // 시퀀스 번호 역직렬화
    Reader << SequenceNumber;
}

// 핑 요청 전송 함수 구현
uint32 FNetworkManager::SendPingRequest(const FIPv4Endpoint& ServerEndpoint)
{
    // 시퀀스 번호 생성
    uint32 SequenceNumber = NextPingSequenceNumber++;

    // 고정밀 타임스탬프 생성
    uint64 CurrentTimestamp = GetHighPrecisionTimestamp();

    // 핑 요청 메시지 생성
    FPingMessage PingRequest;
    PingRequest.Type = EPingMessageType::Request;
    PingRequest.Timestamp = CurrentTimestamp;
    PingRequest.SequenceNumber = SequenceNumber;

    // 메시지 직렬화
    TArray<uint8> MessageData;
    FMemoryWriter Writer(MessageData);

    // 핑 메시지 직렬화
    PingRequest.Serialize(Writer);

    // 네트워크 메시지 생성
    FNetworkMessage NetworkMessage(ENetworkMessageType::PingRequest, MessageData);

    // 메시지 전송
    SendMessageToEndpoint(ServerEndpoint, NetworkMessage);

    // 요청 기록 (시간은 초 단위로)
    double CurrentTimeSeconds = FPlatformTime::Seconds();
    PendingPingRequests.Add(SequenceNumber, TPair<FIPv4Endpoint, double>(ServerEndpoint, CurrentTimeSeconds));

    UE_LOG(LogMultiServerSync, Verbose, TEXT("Sent ping request to %s (Seq: %u, Timestamp: %llu)"),
        *ServerEndpoint.ToString(), SequenceNumber, CurrentTimestamp);

    return SequenceNumber;
}

void FNetworkManager::SendPingResponse(const FPingMessage& RequestMessage, const FIPv4Endpoint& SourceEndpoint)
{
    // 핑 응답 메시지 생성
    FPingMessage PingResponse;
    PingResponse.Type = EPingMessageType::Response;
    PingResponse.Timestamp = RequestMessage.Timestamp; // 원본 타임스탬프 유지
    PingResponse.SequenceNumber = RequestMessage.SequenceNumber; // 원본 시퀀스 번호 유지

    // 현재 처리 시간 기록 (새로 추가)
    uint64 ProcessTime = GetHighPrecisionTimestamp();

    // 메시지 직렬화
    TArray<uint8> MessageData;
    FMemoryWriter Writer(MessageData);

    // 핑 메시지 직렬화
    PingResponse.Serialize(Writer);

    // 네트워크 메시지 생성
    FNetworkMessage NetworkMessage(ENetworkMessageType::PingResponse, MessageData);

    // 메시지 전송
    SendMessageToEndpoint(SourceEndpoint, NetworkMessage);

    UE_LOG(LogMultiServerSync, Verbose, TEXT("Sent ping response to %s (Seq: %u, Original timestamp: %llu, Process time: %llu)"),
        *SourceEndpoint.ToString(), RequestMessage.SequenceNumber, RequestMessage.Timestamp, ProcessTime);
}

// 핑 요청 처리 함수
void FNetworkManager::HandlePingRequest(const TSharedPtr<FMemoryReader>& ReaderPtr, const FIPv4Endpoint& SourceEndpoint)
{
    // 수신 시간 기록
    uint64 ReceiveTime = GetHighPrecisionTimestamp();

    // 요청 메시지 파싱
    FPingMessage RequestMessage;
    RequestMessage.Deserialize(*ReaderPtr);

    UE_LOG(LogMultiServerSync, Verbose, TEXT("Received ping request from %s (Seq: %u, Timestamp: %llu, Received: %llu)"),
        *SourceEndpoint.ToString(), RequestMessage.SequenceNumber, RequestMessage.Timestamp, ReceiveTime);

    // 응답 전송
    SendPingResponse(RequestMessage, SourceEndpoint);
}

// 약 4270줄 근처, HandlePingResponse 메서드 수정
void FNetworkManager::HandlePingResponse(const TSharedPtr<FMemoryReader>& ReaderPtr, const FIPv4Endpoint& SourceEndpoint)
{
    // 응답 메시지 파싱
    FPingMessage ResponseMessage;
    ResponseMessage.Deserialize(*ReaderPtr);

    // 요청 시간 찾기
    uint32 SequenceNumber = ResponseMessage.SequenceNumber;
    if (PendingPingRequests.Contains(SequenceNumber))
    {
        // 요청 정보 얻기
        TPair<FIPv4Endpoint, double> RequestInfo = PendingPingRequests[SequenceNumber];
        double RequestTime = RequestInfo.Value;

        // 고정밀 타임스탬프로 계산
        uint64 RequestTimestamp = ResponseMessage.Timestamp;
        uint64 CurrentTimestamp = GetHighPrecisionTimestamp();

        // 고정밀 RTT 계산 (마이크로초)
        uint64 PreciseRTT = CurrentTimestamp - RequestTimestamp;

        // 밀리초 단위로 변환
        double RTT = static_cast<double>(PreciseRTT) / 1000.0;

        // 통계 업데이트
        UpdateRTTStatistics(SourceEndpoint, RTT);

        // 연속 타임아웃 리셋
        ResetConsecutiveTimeouts(SourceEndpoint);

        // 서버 ID 가져오기
        FString ServerID = SourceEndpoint.ToString();

        // 주기적 핑 상태 업데이트
        for (FPeriodicPingState& PingState : PeriodicPingStates)
        {
            if (PingState.ServerEndpoint == SourceEndpoint && PingState.bIsActive && PingState.bDynamicSampling)
            {
                // 네트워크 품질 계수 업데이트
                UpdateNetworkQualityFactor(PingState, ServerID);

                // 새 샘플링 간격 계산
                float NewInterval = CalculateDynamicSamplingRate(PingState);

                // 간격이 크게 변경된 경우만 업데이트 (작은 변동 무시)
                if (FMath::Abs(NewInterval - PingState.IntervalSeconds) > 0.1f)
                {
                    float OldInterval = PingState.IntervalSeconds;
                    PingState.IntervalSeconds = NewInterval;

                    UE_LOG(LogMultiServerSync, Verbose, TEXT("Updated sampling interval for %s: %.2f -> %.2f seconds"),
                        *ServerID, OldInterval, NewInterval);
                }

                break;
            }
        }

        // 요청 목록에서 제거
        PendingPingRequests.Remove(SequenceNumber);

        UE_LOG(LogMultiServerSync, Verbose, TEXT("Received ping response from %s (Seq: %u, Precise RTT: %llu μs, RTT: %.2f ms)"),
            *SourceEndpoint.ToString(), SequenceNumber, PreciseRTT, RTT);
    }
    else
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Received ping response with unknown sequence number %u from %s"),
            SequenceNumber, *SourceEndpoint.ToString());
    }
}

// RTT 통계 업데이트
void FNetworkManager::UpdateRTTStatistics(const FIPv4Endpoint& ServerEndpoint, double RTT)
{
    // 서버 ID 가져오기
    FString ServerID = ServerEndpoint.ToString();

    // 해당 서버의 통계 가져오기 또는 생성
    if (!ServerLatencyStats.Contains(ServerID))
    {
        ServerLatencyStats.Add(ServerID, FNetworkLatencyStats());
    }

    // 통계 업데이트
    FNetworkLatencyStats& Stats = ServerLatencyStats[ServerID];
    Stats.AddRTTSample(RTT);
}

// 주기적 핑 활성화 함수 구현
void FNetworkManager::EnablePeriodicPing(const FIPv4Endpoint& ServerEndpoint, float IntervalSeconds,
    bool bDynamicSampling, float MinIntervalSeconds,
    float MaxIntervalSeconds)
{
    // 이미 존재하는 주기적 핑 상태 확인
    for (FPeriodicPingState& PingState : PeriodicPingStates)
    {
        if (PingState.ServerEndpoint == ServerEndpoint)
        {
            // 이미 존재하는 핑 상태 업데이트
            PingState.IntervalSeconds = IntervalSeconds;
            PingState.TimeRemainingSeconds = 0.0f; // 즉시 핑 전송
            PingState.bIsActive = true;

            // 동적 샘플링 설정 업데이트
            PingState.bDynamicSampling = bDynamicSampling;
            PingState.MinIntervalSeconds = FMath::Max(0.1f, MinIntervalSeconds);
            PingState.MaxIntervalSeconds = FMath::Max(PingState.MinIntervalSeconds, MaxIntervalSeconds);
            PingState.NetworkQualityFactor = 0.5f; // 초기값 (중간)
            PingState.ConsecutiveTimeouts = 0;

            UE_LOG(LogMultiServerSync, Verbose, TEXT("Updated periodic ping to %s (Interval: %.2f seconds, Dynamic: %s)"),
                *ServerEndpoint.ToString(), IntervalSeconds, bDynamicSampling ? TEXT("true") : TEXT("false"));
            return;
        }
    }

    // 새로운 주기적 핑 상태 추가
    FPeriodicPingState NewPingState;
    NewPingState.ServerEndpoint = ServerEndpoint;
    NewPingState.IntervalSeconds = IntervalSeconds;
    NewPingState.TimeRemainingSeconds = 0.0f; // 즉시 핑 전송
    NewPingState.bIsActive = true;

    // 동적 샘플링 설정
    NewPingState.bDynamicSampling = bDynamicSampling;
    NewPingState.MinIntervalSeconds = FMath::Max(0.1f, MinIntervalSeconds);
    NewPingState.MaxIntervalSeconds = FMath::Max(NewPingState.MinIntervalSeconds, MaxIntervalSeconds);
    NewPingState.NetworkQualityFactor = 0.5f; // 초기값 (중간)
    NewPingState.ConsecutiveTimeouts = 0;

    PeriodicPingStates.Add(NewPingState);

    // 틱 함수가 등록되어 있지 않으면 등록
    if (!LatencyMeasurementTickHandle.IsValid())
    {
        LatencyMeasurementTickHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateRaw(this, &FNetworkManager::TickLatencyMeasurement),
            0.1f); // 0.1초마다 틱 (더 작은 간격 지원을 위해)
    }

    // 타임아웃 체크 함수가 등록되어 있지 않으면 등록
    if (!PingTimeoutCheckHandle.IsValid())
    {
        PingTimeoutCheckHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateLambda([this](float DeltaTime)
                {
                    CheckPingTimeouts();
                    return true;
                }),
            1.0f); // 1초마다 타임아웃 체크
    }

    UE_LOG(LogMultiServerSync, Verbose, TEXT("Enabled periodic ping to %s (Interval: %.2f seconds, Dynamic: %s)"),
        *ServerEndpoint.ToString(), IntervalSeconds, bDynamicSampling ? TEXT("true") : TEXT("false"));
}

// 주기적 핑 비활성화 함수 구현
void FNetworkManager::DisablePeriodicPing(const FIPv4Endpoint& ServerEndpoint)
{
    for (FPeriodicPingState& PingState : PeriodicPingStates)
    {
        if (PingState.ServerEndpoint == ServerEndpoint)
        {
            PingState.bIsActive = false;

            UE_LOG(LogMultiServerSync, Verbose, TEXT("Disabled periodic ping to %s"),
                *ServerEndpoint.ToString());
            return;
        }
    }
}

// 핑 타이머 틱 함수 구현
// 약 4350줄 근처, TickLatencyMeasurement 함수 수정
bool FNetworkManager::TickLatencyMeasurement(float DeltaTime)
{
    // 활성화된 핑 상태가 없으면 틱 비활성화
    bool bHasActivePings = false;

    // 각 주기적 핑 상태 업데이트
    for (FPeriodicPingState& PingState : PeriodicPingStates)
    {
        if (!PingState.bIsActive)
            continue;

        bHasActivePings = true;

        // 남은 시간 감소
        PingState.TimeRemainingSeconds -= DeltaTime;

        // 시간이 다 되었으면 핑 전송
        if (PingState.TimeRemainingSeconds <= 0.0f)
        {
            // 핑 요청 전송
            SendPingRequest(PingState.ServerEndpoint);

            // 타이머 리셋 (동적 샘플링인 경우 최신 간격 사용)
            PingState.TimeRemainingSeconds = PingState.IntervalSeconds;
        }
    }

    // 활성화된 핑이 없으면 틱 함수 제거
    if (!bHasActivePings && LatencyMeasurementTickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(LatencyMeasurementTickHandle);
        LatencyMeasurementTickHandle.Reset();
    }

    return true; // 틱 유지
}

// 네트워크 지연 통계 가져오기
FNetworkLatencyStats FNetworkManager::GetLatencyStats(const FIPv4Endpoint& ServerEndpoint) const
{
    FString ServerID = ServerEndpoint.ToString();

    if (ServerLatencyStats.Contains(ServerID))
    {
        return ServerLatencyStats[ServerID];
    }

    // 서버 통계가 없으면 기본값 반환
    return FNetworkLatencyStats();
}

// 네트워크 품질 평가 함수
int32 FNetworkManager::EvaluateNetworkQuality(const FIPv4Endpoint& ServerEndpoint) const
{
    FString ServerID = ServerEndpoint.ToString();

    if (!ServerLatencyStats.Contains(ServerID))
    {
        return 0; // 불량 (데이터 없음)
    }

    const FNetworkLatencyStats& Stats = ServerLatencyStats[ServerID];

    // 기본 품질 점수
    int32 QualityScore = 3; // 최고 점수부터 시작

    // 샘플 수가 너무 적으면 낮은 점수
    if (Stats.SampleCount < 10)
    {
        return 1; // 데이터 불충분
    }

    // 평균 RTT에 따른 감점
    if (Stats.AvgRTT > 150.0)
        QualityScore--;
    if (Stats.AvgRTT > 300.0)
        QualityScore--;

    // 지터에 따른 감점
    if (Stats.Jitter > 50.0)
        QualityScore--;
    if (Stats.Jitter > 100.0)
        QualityScore--;

    // 패킷 손실에 따른 감점
    float PacketLossRate = 0.0f;
    if (Stats.SampleCount > 0)
    {
        PacketLossRate = static_cast<float>(Stats.LostPackets) / (Stats.SampleCount + Stats.LostPackets);
    }

    if (PacketLossRate > 0.05f) // 5% 이상 손실
        QualityScore--;
    if (PacketLossRate > 0.10f) // 10% 이상 손실
        QualityScore--;

    // 최종 점수 제한
    return FMath::Clamp(QualityScore, 0, 3);
}

// 네트워크 품질 문자열 가져오기
FString FNetworkManager::GetNetworkQualityString(const FIPv4Endpoint& ServerEndpoint) const
{
    int32 Quality = EvaluateNetworkQuality(ServerEndpoint);

    switch (Quality)
    {
    case 0:
        return TEXT("Poor");
    case 1:
        return TEXT("Fair");
    case 2:
        return TEXT("Good");
    case 3:
        return TEXT("Excellent");
    default:
        return TEXT("Unknown");
    }
}

// 네트워크 지연 측정 시작
void FNetworkManager::StartLatencyMeasurement(const FIPv4Endpoint& ServerEndpoint, float IntervalSeconds, int32 SampleCount)
{
    // 주기적 핑 활성화
    EnablePeriodicPing(ServerEndpoint, IntervalSeconds);

    // 로그 출력
    UE_LOG(LogMultiServerSync, Log, TEXT("Started latency measurement to %s (Interval: %.2f s, Samples: %d)"),
        *ServerEndpoint.ToString(), IntervalSeconds, SampleCount == 0 ? -1 : SampleCount);
}

// 네트워크 지연 측정 중지
void FNetworkManager::StopLatencyMeasurement(const FIPv4Endpoint& ServerEndpoint)
{
    // 주기적 핑 비활성화
    DisablePeriodicPing(ServerEndpoint);

    // 로그 출력
    UE_LOG(LogMultiServerSync, Log, TEXT("Stopped latency measurement to %s"),
        *ServerEndpoint.ToString());

    // 지연 통계 정보 출력
    FString ServerID = ServerEndpoint.ToString();
    if (ServerLatencyStats.Contains(ServerID))
    {
        const FNetworkLatencyStats& Stats = ServerLatencyStats[ServerID];
        UE_LOG(LogMultiServerSync, Log, TEXT("Latency statistics for %s: Min=%.2f ms, Max=%.2f ms, Avg=%.2f ms, Jitter=%.2f ms, Samples=%d, Lost=%d"),
            *ServerID, Stats.MinRTT, Stats.MaxRTT, Stats.AvgRTT, Stats.Jitter, Stats.SampleCount, Stats.LostPackets);
    }
}

// 고정밀 타임스탬프 생성 함수
uint64 FNetworkManager::GetHighPrecisionTimestamp() const
{
    // 하드웨어 타이머를 이용하여 고정밀 시간 측정
    double CycleTime = FPlatformTime::GetSecondsPerCycle();
    uint64 Cycles = FPlatformTime::Cycles64();
    uint64 HardwareTimeMicros = static_cast<uint64>(Cycles * CycleTime * 1000000.0);

    // 하드웨어 타이머 초기화가 되지 않은 경우 UTC 시간 사용
    if (HardwareTimerInitTime == 0)
    {
        return FDateTime::UtcNow().GetTicks() / 10; // 100나노초 -> 마이크로초
    }

    // 하드웨어 타이머를 UTC 시간으로 변환
    return HardwareTimeToUTC(HardwareTimeMicros);
}

// 동적 샘플링 레이트 계산
float FNetworkManager::CalculateDynamicSamplingRate(const FPeriodicPingState& PingState) const
{
    // 동적 샘플링이 비활성화된 경우 기본 간격 반환
    if (!PingState.bDynamicSampling)
    {
        return PingState.IntervalSeconds;
    }

    // 네트워크 품질 기반 간격 계산 (0.0: 최고품질 = 최소간격, 1.0: 최저품질 = 최대간격)
    float QualityFactor = PingState.NetworkQualityFactor;

    // 연속 타임아웃이 있으면 간격 증가 (불안정 네트워크 부하 방지)
    if (PingState.ConsecutiveTimeouts > 0)
    {
        // 타임아웃당 10%씩 간격 증가 (최대 3배)
        float TimeoutFactor = FMath::Min(3.0f, 1.0f + (PingState.ConsecutiveTimeouts * 0.1f));
        QualityFactor = FMath::Min(1.0f, QualityFactor * TimeoutFactor);
    }

    // 간격 계산 (선형 보간)
    float NewInterval = FMath::Lerp(
        PingState.MinIntervalSeconds,
        PingState.MaxIntervalSeconds,
        QualityFactor
    );

    return NewInterval;
}

// 네트워크 품질 계수 업데이트
void FNetworkManager::UpdateNetworkQualityFactor(FPeriodicPingState& PingState, const FString& ServerID)
{
    // 서버 통계가 없으면 기본값 유지
    if (!ServerLatencyStats.Contains(ServerID))
    {
        return;
    }

    const FNetworkLatencyStats& Stats = ServerLatencyStats[ServerID];

    // 샘플 수가 너무 적으면 업데이트하지 않음
    if (Stats.SampleCount < 5)
    {
        return;
    }

    // 네트워크 품질 계산 (0.0: 최고품질, 1.0: 최저품질)
    float QualityFactor = 0.5f; // 기본값 (중간)

    // 평균 RTT 기반 품질 계산 (임계값: 300ms)
    float RttFactor = FMath::Clamp(Stats.AvgRTT / 300.0, 0.0, 1.0);

    // 지터 기반 품질 계산 (임계값: 100ms)
    float JitterFactor = FMath::Clamp(Stats.Jitter / 100.0, 0.0, 1.0);

    // 패킷 손실 기반 품질 계산
    float LossRate = 0.0f;
    if (Stats.SampleCount + Stats.LostPackets > 0)
    {
        LossRate = static_cast<float>(Stats.LostPackets) / (Stats.SampleCount + Stats.LostPackets);
    }
    float LossFactor = FMath::Clamp(LossRate * 10.0, 0.0, 1.0); // 10% 이상 손실 시 최저품질

    // 종합 품질 계산 (각 요소 가중치 조정 가능)
    QualityFactor = RttFactor * 0.4f + JitterFactor * 0.3f + LossFactor * 0.3f;

    // 품질 계수 업데이트 (급격한 변화 방지를 위한 평활화)
    PingState.NetworkQualityFactor = PingState.NetworkQualityFactor * 0.7f + QualityFactor * 0.3f;

    UE_LOG(LogMultiServerSync, Verbose, TEXT("Updated network quality factor for %s: %.2f (RTT: %.2f ms, Jitter: %.2f ms, Loss: %.2f%%)"),
        *ServerID, PingState.NetworkQualityFactor, Stats.AvgRTT, Stats.Jitter, LossRate * 100.0f);
}

// 연속 타임아웃 증가
void FNetworkManager::IncrementConsecutiveTimeouts(const FIPv4Endpoint& ServerEndpoint)
{
    for (FPeriodicPingState& PingState : PeriodicPingStates)
    {
        if (PingState.ServerEndpoint == ServerEndpoint && PingState.bIsActive)
        {
            PingState.ConsecutiveTimeouts++;

            // 타임아웃 로깅
            UE_LOG(LogMultiServerSync, Warning, TEXT("Consecutive timeouts for %s: %d"),
                *ServerEndpoint.ToString(), PingState.ConsecutiveTimeouts);

            // 연속 타임아웃이 많으면 샘플링 간격 증가
            if (PingState.bDynamicSampling && PingState.ConsecutiveTimeouts > 3)
            {
                float NewInterval = CalculateDynamicSamplingRate(PingState);
                PingState.IntervalSeconds = NewInterval;

                UE_LOG(LogMultiServerSync, Warning, TEXT("Increasing sampling interval due to timeouts: %.2f seconds"),
                    NewInterval);
            }

            return;
        }
    }
}

// 연속 타임아웃 리셋
void FNetworkManager::ResetConsecutiveTimeouts(const FIPv4Endpoint& ServerEndpoint)
{
    for (FPeriodicPingState& PingState : PeriodicPingStates)
    {
        if (PingState.ServerEndpoint == ServerEndpoint && PingState.bIsActive)
        {
            // 타임아웃이 있었으면 로깅
            if (PingState.ConsecutiveTimeouts > 0)
            {
                UE_LOG(LogMultiServerSync, Verbose, TEXT("Reset consecutive timeouts for %s (was: %d)"),
                    *ServerEndpoint.ToString(), PingState.ConsecutiveTimeouts);
            }

            PingState.ConsecutiveTimeouts = 0;
            return;
        }
    }
}

// 이상치 필터링 설정
void FNetworkManager::SetOutlierFiltering(const FIPv4Endpoint& ServerEndpoint, bool bEnableFiltering)
{
    FString ServerID = ServerEndpoint.ToString();

    if (ServerLatencyStats.Contains(ServerID))
    {
        ServerLatencyStats[ServerID].bFilterOutliers = bEnableFiltering;

        UE_LOG(LogMultiServerSync, Display, TEXT("Outlier filtering for %s: %s"),
            *ServerID, bEnableFiltering ? TEXT("Enabled") : TEXT("Disabled"));
    }
}

// 이상치 통계 가져오기
bool FNetworkManager::GetOutlierStats(const FIPv4Endpoint& ServerEndpoint, int32& OutliersDetected, double& OutlierThreshold) const
{
    FString ServerID = ServerEndpoint.ToString();

    if (!ServerLatencyStats.Contains(ServerID))
    {
        OutliersDetected = 0;
        OutlierThreshold = 0.0;
        return false;
    }

    const FNetworkLatencyStats& Stats = ServerLatencyStats[ServerID];
    OutliersDetected = Stats.OutliersDetected;
    OutlierThreshold = Stats.OutlierThreshold;

    return true;
}

// 시계열 샘플링 간격 설정
void FNetworkManager::SetTimeSeriesSampleInterval(const FIPv4Endpoint& ServerEndpoint, double IntervalSeconds)
{
    FString ServerID = ServerEndpoint.ToString();

    if (ServerLatencyStats.Contains(ServerID))
    {
        ServerLatencyStats[ServerID].SetTimeSeriesSampleInterval(IntervalSeconds);

        UE_LOG(LogMultiServerSync, Display, TEXT("Time series sampling interval for %s set to %.2f seconds"),
            *ServerID, IntervalSeconds);
    }
}

// 시계열 데이터 가져오기
bool FNetworkManager::GetTimeSeriesData(const FIPv4Endpoint& ServerEndpoint, TArray<FLatencyTimeSeriesSample>& OutTimeSeries) const
{
    FString ServerID = ServerEndpoint.ToString();

    if (!ServerLatencyStats.Contains(ServerID))
    {
        OutTimeSeries.Empty();
        return false;
    }

    const FNetworkLatencyStats& Stats = ServerLatencyStats[ServerID];
    OutTimeSeries = Stats.GetTimeSeries();

    return OutTimeSeries.Num() > 0;
}

// 추세 분석 결과 가져오기
bool FNetworkManager::GetNetworkTrendAnalysis(const FIPv4Endpoint& ServerEndpoint, FNetworkTrendAnalysis& OutTrendAnalysis) const
{
    FString ServerID = ServerEndpoint.ToString();

    if (!ServerLatencyStats.Contains(ServerID))
    {
        OutTrendAnalysis = FNetworkTrendAnalysis();
        return false;
    }

    const FNetworkLatencyStats& Stats = ServerLatencyStats[ServerID];
    OutTrendAnalysis = Stats.GetTrendAnalysis();

    return true;
}

FNetworkQualityAssessment FNetworkManager::EvaluateNetworkQualityDetailed(const FIPv4Endpoint& ServerEndpoint) const
{
    FNetworkQualityAssessment Result;
    FString ServerID = ServerEndpoint.ToString();

    // 서버 통계가 없으면 기본 품질 평가 반환
    if (!ServerLatencyStats.Contains(ServerID))
    {
        Result.QualityLevel = 0;
        Result.QualityString = TEXT("Unknown");
        Result.DetailedDescription = TEXT("No network statistics available for this server.");
        Result.Recommendations.Add(TEXT("Start latency measurement to gather network statistics."));
        return Result;
    }

    const FNetworkLatencyStats& Stats = ServerLatencyStats[ServerID];

    // 샘플 수가 너무 적으면 낮은 신뢰도 표시
    if (Stats.SampleCount < 10)
    {
        Result.QualityLevel = 0;
        Result.QualityString = TEXT("Insufficient Data");
        Result.DetailedDescription = TEXT("Not enough samples to evaluate network quality reliably.");
        Result.Recommendations.Add(TEXT("Continue latency measurement to gather more data."));
        return Result;
    }

    // 성능 임계값 가져오기
    double LatencyThreshold = Stats.HighLatencyThreshold;
    double JitterThreshold = Stats.HighJitterThreshold;
    double PacketLossThreshold = Stats.HighPacketLossThreshold;

    // 패킷 손실율 계산
    double PacketLossRate = 0.0;
    if (Stats.SampleCount + Stats.LostPackets > 0)
    {
        PacketLossRate = static_cast<double>(Stats.LostPackets) / (Stats.SampleCount + Stats.LostPackets);
    }

    // 각 지표별 점수 계산
    Result.LatencyScore = CalculateLatencyScore(Stats.AvgRTT, LatencyThreshold);
    Result.JitterScore = CalculateJitterScore(Stats.Jitter, JitterThreshold);
    Result.PacketLossScore = CalculatePacketLossScore(PacketLossRate, PacketLossThreshold);
    Result.StabilityScore = CalculateStabilityScore(Stats.TrendAnalysis);

    // 종합 점수 계산 (가중치 적용)
    Result.QualityScore = static_cast<int32>(
        Result.LatencyScore * 0.4f +    // 지연 시간 40% 비중
        Result.JitterScore * 0.3f +     // 지터 30% 비중
        Result.PacketLossScore * 0.2f + // 패킷 손실 20% 비중
        Result.StabilityScore * 0.1f    // 안정성 10% 비중
        );

    // 품질 레벨 결정
    if (Result.QualityScore >= 80)
    {
        Result.QualityLevel = 3;
        Result.QualityString = TEXT("Excellent");
    }
    else if (Result.QualityScore >= 60)
    {
        Result.QualityLevel = 2;
        Result.QualityString = TEXT("Good");
    }
    else if (Result.QualityScore >= 40)
    {
        Result.QualityLevel = 1;
        Result.QualityString = TEXT("Fair");
    }
    else
    {
        Result.QualityLevel = 0;
        Result.QualityString = TEXT("Poor");
    }

    // 상세 설명 생성
    Result.DetailedDescription = FString::Printf(
        TEXT("Network quality is %s (%d/100). RTT: %.2f ms, Jitter: %.2f ms, Packet Loss: %.2f%%."),
        *Result.QualityString,
        Result.QualityScore,
        Stats.AvgRTT,
        Stats.Jitter,
        PacketLossRate * 100.0
    );

    // 추세 분석 정보 추가
    if (Stats.TrendAnalysis.ShortTermTrend > 1.0)
    {
        Result.DetailedDescription += TEXT(" Network quality is degrading.");
        Result.QualityChangeTrend = -1.0f;
    }
    else if (Stats.TrendAnalysis.ShortTermTrend < -1.0)
    {
        Result.DetailedDescription += TEXT(" Network quality is improving.");
        Result.QualityChangeTrend = 1.0f;
    }
    else
    {
        Result.DetailedDescription += TEXT(" Network quality is stable.");
        Result.QualityChangeTrend = 0.0f;
    }

    // 권장 사항 생성
    if (Result.LatencyScore < 50)
    {
        Result.Recommendations.Add(TEXT("High latency detected. Consider reducing network load or connecting to a closer server."));
    }

    if (Result.JitterScore < 50)
    {
        Result.Recommendations.Add(TEXT("High jitter detected. Check for network congestion or interface issues."));
    }

    if (Result.PacketLossScore < 50)
    {
        Result.Recommendations.Add(TEXT("Significant packet loss detected. Check network connectivity and signal quality."));
    }

    // 이벤트 정보 추가 (통계에 있는 경우)
    if (Stats.CurrentQuality.LatestEvent != ENetworkEventType::None)
    {
        Result.LatestEvent = Stats.CurrentQuality.LatestEvent;
        Result.EventTimestamp = Stats.CurrentQuality.EventTimestamp;
    }

    return Result;
}

// 네트워크 상태 변화 이벤트 핸들러 등록
void FNetworkManager::RegisterNetworkStateChangeHandler(TFunction<void(const FIPv4Endpoint&, ENetworkEventType, const FNetworkQualityAssessment&)> Handler)
{
    NetworkStateChangeHandler = Handler;

    UE_LOG(LogMultiServerSync, Display, TEXT("Network state change handler registered"));
}

// 네트워크 상태 변화 임계값 설정
void FNetworkManager::SetNetworkStateChangeThreshold(const FIPv4Endpoint& ServerEndpoint, double Threshold)
{
    FString ServerID = ServerEndpoint.ToString();

    if (ServerLatencyStats.Contains(ServerID))
    {
        FNetworkLatencyStats& Stats = ServerLatencyStats[ServerID];
        Stats.StateChangeThreshold = FMath::Max(5.0, Threshold);  // 최소 5점 이상의 변화

        UE_LOG(LogMultiServerSync, Display, TEXT("Network state change threshold for %s set to %.2f"),
            *ServerID, Stats.StateChangeThreshold);
    }
    else
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Cannot set network state change threshold: No statistics for server %s"),
            *ServerID);
    }
}

// 네트워크 성능 임계값 설정
void FNetworkManager::SetNetworkPerformanceThresholds(const FIPv4Endpoint& ServerEndpoint,
    double LatencyThreshold,
    double JitterThreshold,
    double PacketLossThreshold)
{
    FString ServerID = ServerEndpoint.ToString();

    if (ServerLatencyStats.Contains(ServerID))
    {
        FNetworkLatencyStats& Stats = ServerLatencyStats[ServerID];
        Stats.SetPerformanceThresholds(LatencyThreshold, JitterThreshold, PacketLossThreshold);

        UE_LOG(LogMultiServerSync, Display, TEXT("Network performance thresholds for %s set to: Latency=%.2f ms, Jitter=%.2f ms, Loss=%.2f%%"),
            *ServerID, Stats.HighLatencyThreshold, Stats.HighJitterThreshold, Stats.HighPacketLossThreshold * 100.0);
    }
    else
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Cannot set network performance thresholds: No statistics for server %s"),
            *ServerID);
    }
}

// 품질 평가 간격 설정
void FNetworkManager::SetQualityAssessmentInterval(const FIPv4Endpoint& ServerEndpoint, double IntervalSeconds)
{
    FString ServerID = ServerEndpoint.ToString();

    if (ServerLatencyStats.Contains(ServerID))
    {
        FNetworkLatencyStats& Stats = ServerLatencyStats[ServerID];
        Stats.SetQualityAssessmentInterval(IntervalSeconds);

        UE_LOG(LogMultiServerSync, Display, TEXT("Quality assessment interval for %s set to %.2f seconds"),
            *ServerID, Stats.QualityAssessmentInterval);
    }
    else
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Cannot set quality assessment interval: No statistics for server %s"),
            *ServerID);
    }
}

// 네트워크 상태 변화 모니터링 활성화/비활성화
void FNetworkManager::SetNetworkStateMonitoring(const FIPv4Endpoint& ServerEndpoint, bool bEnable)
{
    FString ServerID = ServerEndpoint.ToString();

    if (ServerLatencyStats.Contains(ServerID))
    {
        FNetworkLatencyStats& Stats = ServerLatencyStats[ServerID];
        Stats.bMonitorStateChanges = bEnable;

        UE_LOG(LogMultiServerSync, Display, TEXT("Network state monitoring for %s %s"),
            *ServerID, bEnable ? TEXT("enabled") : TEXT("disabled"));
    }
    else
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Cannot set network state monitoring: No statistics for server %s"),
            *ServerID);
    }
}

// 네트워크 이벤트 기록 가져오기
bool FNetworkManager::GetNetworkEventHistory(const FIPv4Endpoint& ServerEndpoint, TArray<ENetworkEventType>& OutEvents) const
{
    FString ServerID = ServerEndpoint.ToString();
    OutEvents.Empty();

    if (!ServerLatencyStats.Contains(ServerID))
    {
        return false;
    }

    const FNetworkLatencyStats& Stats = ServerLatencyStats[ServerID];
    OutEvents = Stats.RecentEvents;

    return true;
}

// 지연 시간 점수 계산 (0-100)
int32 FNetworkManager::CalculateLatencyScore(double RTT, double HighLatencyThreshold) const
{
    if (RTT <= 0.0)
        return 100;

    // RTT가 낮을수록 높은 점수 (비선형 매핑)
    // 20ms 이하면 100점
    // 임계값 이상이면 0점

    if (RTT <= 20.0)
        return 100;

    if (RTT >= HighLatencyThreshold)
        return 0;

    // 비선형 점수 계산 (점수가 더 빨리 떨어지도록)
    double NormalizedRTT = (RTT - 20.0) / (HighLatencyThreshold - 20.0);
    double Score = 100.0 * (1.0 - NormalizedRTT * NormalizedRTT);

    return FMath::Clamp<int32>(FMath::RoundToInt(Score), 0, 100);
}

// 지터 점수 계산 (0-100)
int32 FNetworkManager::CalculateJitterScore(double Jitter, double HighJitterThreshold) const
{
    if (Jitter <= 0.0)
        return 100;

    // 지터가 낮을수록 높은 점수
    // 5ms 이하면 100점
    // 임계값 이상이면 0점

    if (Jitter <= 5.0)
        return 100;

    if (Jitter >= HighJitterThreshold)
        return 0;

    // 선형 점수 계산
    double NormalizedJitter = (Jitter - 5.0) / (HighJitterThreshold - 5.0);
    double Score = 100.0 * (1.0 - NormalizedJitter);

    return FMath::Clamp<int32>(FMath::RoundToInt(Score), 0, 100);
}

// 패킷 손실 점수 계산 (0-100)
int32 FNetworkManager::CalculatePacketLossScore(double LossRate, double HighPacketLossThreshold) const
{
    if (LossRate <= 0.0)
        return 100;

    // 패킷 손실이 낮을수록 높은 점수
    // 0.1% 이하면 100점
    // 임계값 이상이면 0점

    const double LowLossThreshold = 0.001;  // 0.1%

    if (LossRate <= LowLossThreshold)
        return 100;

    if (LossRate >= HighPacketLossThreshold)
        return 0;

    // 비선형 점수 계산 (손실이 조금만 있어도 점수가 많이 떨어지도록)
    double NormalizedLoss = (LossRate - LowLossThreshold) / (HighPacketLossThreshold - LowLossThreshold);
    double Score = 100.0 * (1.0 - pow(NormalizedLoss, 0.7));

    return FMath::Clamp<int32>(FMath::RoundToInt(Score), 0, 100);
}

// 안정성 점수 계산 (0-100)
int32 FNetworkManager::CalculateStabilityScore(const FNetworkTrendAnalysis& TrendAnalysis) const
{
    // 안정성 점수 계산 (추세 분석 기반)
    // 변동성이 낮고 장기 추세가 좋을수록 높은 점수

    // 변동성 점수 (변동성이 낮을수록 높은 점수)
    double VolatilityScore = 100.0;
    if (TrendAnalysis.Volatility > 0.0)
    {
        // 변동성이 5ms 이하면 최고 점수, 50ms 이상이면 최저 점수
        VolatilityScore = FMath::Clamp(100.0 - (TrendAnalysis.Volatility / 50.0) * 100.0, 0.0, 100.0);
    }

    // 추세 점수 (추세가 좋을수록 높은 점수)
    double TrendScore = 50.0;  // 기본값: 중립

    // 장기 추세가 개선되고 있으면 점수 증가, 악화되고 있으면 점수 감소
    if (TrendAnalysis.LongTermTrend < 0.0)  // 개선 중
    {
        // 최대 -10ms/s까지 고려 (그 이상은 동일하게 처리)
        TrendScore = 50.0 + FMath::Min(fabs(TrendAnalysis.LongTermTrend) / 10.0 * 50.0, 50.0);
    }
    else if (TrendAnalysis.LongTermTrend > 0.0)  // 악화 중
    {
        // 최대 10ms/s까지 고려 (그 이상은 동일하게 처리)
        TrendScore = 50.0 - FMath::Min(TrendAnalysis.LongTermTrend / 10.0 * 50.0, 50.0);
    }

    // 종합 안정성 점수 (변동성 70%, 추세 30%)
    int32 StabilityScore = FMath::RoundToInt(VolatilityScore * 0.7 + TrendScore * 0.3);
    return FMath::Clamp(StabilityScore, 0, 100);
}

// 네트워크 상태 변화 감지 및 처리
void FNetworkManager::ProcessNetworkStateChange(const FIPv4Endpoint& ServerEndpoint, ENetworkEventType EventType, const FNetworkQualityAssessment& Quality)
{
    FString ServerID = ServerEndpoint.ToString();

    // 통계 객체에 이벤트 기록 (있는 경우만)
    if (ServerLatencyStats.Contains(ServerID))
    {
        FNetworkLatencyStats& Stats = ServerLatencyStats[ServerID];

        // 이벤트 기록에 추가
        Stats.AddNetworkEvent(EventType, FPlatformTime::Seconds());

        // 현재 품질 정보 업데이트
        Stats.CurrentQuality = Quality;
        Stats.CurrentQuality.LatestEvent = EventType;
        Stats.CurrentQuality.EventTimestamp = FPlatformTime::Seconds();

        // 품질 히스토리에 추가
        Stats.QualityHistory.Add(Quality);

        // 최대 히스토리 크기 유지
        while (Stats.QualityHistory.Num() > Stats.MaxQualityHistoryCount)
        {
            Stats.QualityHistory.RemoveAt(0);
        }
    }

    // 이벤트 핸들러가 등록되어 있으면 호출
    if (NetworkStateChangeHandler)
    {
        NetworkStateChangeHandler(ServerEndpoint, EventType, Quality);

        UE_LOG(LogMultiServerSync, Display, TEXT("Network state change event: %s for server %s, quality: %d/100"),
            *FNetworkQualityAssessment::EventTypeToString(EventType), *ServerID, Quality.QualityScore);
    }
}

// 주기적인 품질 평가 수행
bool FNetworkManager::CheckQualityAssessments(float DeltaTime)
{
    double CurrentTime = FPlatformTime::Seconds();

    // 서버별로 품질 평가 시간 확인
    for (auto& Pair : ServerLatencyStats)
    {
        FString ServerID = Pair.Key;
        FNetworkLatencyStats& Stats = Pair.Value;

        // 모니터링이 비활성화되었거나 충분한 샘플이 없으면 건너뜀
        if (!Stats.bMonitorStateChanges || Stats.SampleCount < 10)
            continue;

        // 평가 간격이 지났는지 확인
        if (CurrentTime - Stats.LastQualityAssessmentTime >= Stats.QualityAssessmentInterval)
        {
            // 기존 품질 평가 저장
            FNetworkQualityAssessment PreviousQuality = Stats.CurrentQuality;

            // 새로운 품질 평가 수행
            FIPv4Endpoint ServerEndpoint;

            // 서버 ID에서 엔드포인트 정보 추출
            TArray<FString> Parts;
            ServerID.ParseIntoArray(Parts, TEXT(":"), true);
            if (Parts.Num() >= 2)
            {
                FIPv4Address IPAddress;
                if (FIPv4Address::Parse(Parts[0], IPAddress))
                {
                    uint16 PortNumber = FCString::Atoi(*Parts[1]);
                    ServerEndpoint = FIPv4Endpoint(IPAddress, Port);

                    // 새 품질 평가 수행
                    FNetworkQualityAssessment NewQuality = EvaluateNetworkQualityDetailed(ServerEndpoint);

                    // 품질 변화 확인
                    if (Stats.QualityHistory.Num() > 0)
                    {
                        // 이전 평가가 있으면 변화 감지
                        ENetworkEventType StateChangeEvent = Stats.DetectStateChange(NewQuality, PreviousQuality);

                        // 감지된 이벤트가 있으면 처리
                        if (StateChangeEvent != ENetworkEventType::None)
                        {
                            ProcessNetworkStateChange(ServerEndpoint, StateChangeEvent, NewQuality);
                        }
                    }
                    else
                    {
                        // 첫 번째 평가면 그냥 기록
                        Stats.QualityHistory.Add(NewQuality);
                        Stats.CurrentQuality = NewQuality;
                    }

                    // 평가 시간 갱신
                    Stats.LastQualityAssessmentTime = CurrentTime;
                }
            }
        }
    }

    return true;  // 계속 틱 유지
}

// ACK가 필요한 메시지 전송
bool FNetworkManager::SendMessageWithAck(const FIPv4Endpoint& Endpoint, const FNetworkMessage& Message)
{
    if (!bIsInitialized)
    {
        return false;
    }

    // 현재 시간
    double CurrentTime = FPlatformTime::Seconds();

    // 메시지 전송
    bool bSuccess = SendMessageToEndpoint(Endpoint, Message);
    if (!bSuccess)
    {
        return false;
    }

    // 시퀀스 번호 가져오기
    uint16 SequenceNumber = Message.GetSequenceNumber();

    // 메시지 추적 정보 생성
    FMessageAckData AckData(SequenceNumber, Endpoint);
    AckData.Status = EMessageAckStatus::Sent;
    AckData.SentTime = CurrentTime;
    AckData.LastAttemptTime = CurrentTime;
    AckData.AttemptCount = 1;

    // 추적 목록에 추가
    PendingAcknowledgements.Add(SequenceNumber, AckData);

    // 엔드포인트별 시퀀스 매핑 업데이트
    FString EndpointStr = Endpoint.ToString();
    if (!EndpointSequenceMap.Contains(EndpointStr))
    {
        EndpointSequenceMap.Add(EndpointStr, TArray<uint16>());
    }
    EndpointSequenceMap[EndpointStr].Add(SequenceNumber);

    UE_LOG(LogMultiServerSync, Verbose, TEXT("Sent message with ACK to %s (Seq: %u)"),
        *EndpointStr, SequenceNumber);

    return true;
}

// ACK 메시지 처리
void FNetworkManager::HandleMessageAck(const FNetworkMessage& Message, const FIPv4Endpoint& Sender)
{
    // ACK 메시지에서 원본 시퀀스 번호 추출
    if (Message.GetData().Num() < sizeof(uint16))
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Received invalid ACK message (too small)"));
        return;
    }

    uint16 AckedSequence = 0;
    FMemory::Memcpy(&AckedSequence, Message.GetData().GetData(), sizeof(uint16));

    // 추적 중인 메시지인지 확인
    if (!PendingAcknowledgements.Contains(AckedSequence))
    {
        // 이미 처리되었거나 알 수 없는 메시지일 수 있음
        UE_LOG(LogMultiServerSync, Verbose, TEXT("Received ACK for unknown sequence: %u"), AckedSequence);
        return;
    }

    // 메시지 상태 업데이트
    FMessageAckData& AckData = PendingAcknowledgements[AckedSequence];
    AckData.Status = EMessageAckStatus::Acknowledged;

    UE_LOG(LogMultiServerSync, Verbose, TEXT("Message acknowledged (Seq: %u, Endpoint: %s)"),
        AckedSequence, *AckData.TargetEndpoint.ToString());

    // 확인된 메시지 추적 목록에서 제거
    FString EndpointStr = AckData.TargetEndpoint.ToString();
    if (EndpointSequenceMap.Contains(EndpointStr))
    {
        EndpointSequenceMap[EndpointStr].Remove(AckedSequence);

        // 엔드포인트의 모든 메시지가 확인되면 맵에서 제거
        if (EndpointSequenceMap[EndpointStr].Num() == 0)
        {
            EndpointSequenceMap.Remove(EndpointStr);
        }
    }

    // 확인된 메시지 제거
    PendingAcknowledgements.Remove(AckedSequence);
}

// 메시지 재전송 체크
bool FNetworkManager::CheckMessageRetries(float DeltaTime)
{
    if (!bIsInitialized)
    {
        return true; // 계속 틱 유지
    }

    double CurrentTime = FPlatformTime::Seconds();

    // 타임아웃된 메시지 목록
    TArray<uint16> TimeoutSequences;
    TArray<uint16> RetrySequences;

    // 모든 대기 중인 메시지 검사
    for (auto& Pair : PendingAcknowledgements)
    {
        uint16 SequenceNumber = Pair.Key;
        FMessageAckData& AckData = Pair.Value;

        // 이미 확인된 메시지는 건너뜀
        if (AckData.Status == EMessageAckStatus::Acknowledged)
        {
            continue;
        }

        double ElapsedTime = CurrentTime - AckData.LastAttemptTime;

        // 타임아웃 확인
        if (ElapsedTime > MESSAGE_TIMEOUT_SECONDS)
        {
            // 최대 시도 횟수를 초과하면 실패로 처리
            if (AckData.AttemptCount >= MAX_RETRY_ATTEMPTS)
            {
                // 타임아웃 로그
                UE_LOG(LogMultiServerSync, Warning, TEXT("Message timed out after %d attempts (Seq: %u, Endpoint: %s)"),
                    AckData.AttemptCount, SequenceNumber, *AckData.TargetEndpoint.ToString());

                AckData.Status = EMessageAckStatus::Timeout;
                TimeoutSequences.Add(SequenceNumber);
            }
            else
            {
                // 재전송 목록에 추가
                RetrySequences.Add(SequenceNumber);
            }
        }
    }

    // 타임아웃된 메시지 처리
    for (uint16 Sequence : TimeoutSequences)
    {
        // 엔드포인트별 매핑에서 제거
        FMessageAckData& AckData = PendingAcknowledgements[Sequence];
        FString EndpointStr = AckData.TargetEndpoint.ToString();

        if (EndpointSequenceMap.Contains(EndpointStr))
        {
            EndpointSequenceMap[EndpointStr].Remove(Sequence);

            if (EndpointSequenceMap[EndpointStr].Num() == 0)
            {
                EndpointSequenceMap.Remove(EndpointStr);
            }
        }

        // 추적 목록에서 제거
        PendingAcknowledgements.Remove(Sequence);
    }

    // 재전송 메시지 처리
    for (uint16 Sequence : RetrySequences)
    {
        RetryMessage(Sequence);
    }

    // 마지막 체크 시간 업데이트
    LastRetryCheckTime = CurrentTime;

    return true; // 계속 틱 유지
}

// 메시지 재전송
void FNetworkManager::RetryMessage(uint16 SequenceNumber)
{
    if (!PendingAcknowledgements.Contains(SequenceNumber))
    {
        return;
    }

    FMessageAckData& AckData = PendingAcknowledgements[SequenceNumber];

    // 현재 시간
    double CurrentTime = FPlatformTime::Seconds();

    // 시도 횟수 증가
    AckData.AttemptCount++;
    AckData.LastAttemptTime = CurrentTime;

    // 메시지 재전송을 위한 가상의 메시지 생성
    // 실제 구현에서는 원본 메시지 내용을 저장해 두어야 함
    TArray<uint8> DummyData; // 실제 구현에서는 저장된 원본 데이터 사용
    FNetworkMessage Message(ENetworkMessageType::Data, DummyData);
    Message.SetProjectId(ProjectId);
    Message.SetSequenceNumber(SequenceNumber);

    // 메시지 재전송
    bool bSuccess = SendMessageToEndpoint(AckData.TargetEndpoint, Message);

    // 상태 업데이트
    if (bSuccess)
    {
        AckData.Status = EMessageAckStatus::Sent;
        UE_LOG(LogMultiServerSync, Verbose, TEXT("Retrying message (Seq: %u, Attempt: %d, Endpoint: %s)"),
            SequenceNumber, AckData.AttemptCount, *AckData.TargetEndpoint.ToString());
    }
    else
    {
        AckData.Status = EMessageAckStatus::Failed;
        UE_LOG(LogMultiServerSync, Warning, TEXT("Failed to retry message (Seq: %u, Endpoint: %s)"),
            SequenceNumber, *AckData.TargetEndpoint.ToString());
    }
}

// 신뢰성 있는 메시지 전송
bool FNetworkManager::SendMessageWithAcknowledgement(const FString& EndpointId, const TArray<uint8>& Message)
{
    if (!bIsInitialized)
    {
        return false;
    }

    FServerEndpoint* TargetServer = DiscoveredServers.Find(EndpointId);
    if (!TargetServer)
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Endpoint not found: %s"), *EndpointId);
        return false;
    }

    // 메시지 생성
    FNetworkMessage NetworkMessage(ENetworkMessageType::Data, Message);
    NetworkMessage.SetProjectId(ProjectId);
    NetworkMessage.SetSequenceNumber(GetNextSequenceNumber());

    // ACK 필요함을 나타내는 플래그 설정
    NetworkMessage.SetFlags(1); // Flag = 1: ACK 필요

    // 엔드포인트로 전송
    FIPv4Endpoint Endpoint(TargetServer->IPAddress, TargetServer->Port);
    return SendMessageWithAck(Endpoint, NetworkMessage);
}

// 확인 대기 중인 메시지 개수 반환
TMap<FString, int32> FNetworkManager::GetPendingAcknowledgements() const
{
    TMap<FString, int32> Result;

    for (const auto& Pair : EndpointSequenceMap)
    {
        Result.Add(Pair.Key, Pair.Value.Num());
    }

    return Result;
}

// 순서 보장 설정
void FNetworkManager::SetOrderGuaranteed(bool bEnable)
{
    bOrderGuaranteedEnabled = bEnable;

    // 모든, 시퀀스 추적기에 설정 적용
    for (auto& Pair : EndpointSequenceTrackers)
    {
        Pair.Value.bOrderGuaranteed = bEnable;
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Message order guarantee %s"),
        bEnable ? TEXT("enabled") : TEXT("disabled"));
}

// 순서 보장 상태 확인
bool FNetworkManager::IsOrderGuaranteed() const
{
    return bOrderGuaranteedEnabled;
}

// 누락된 시퀀스 목록 가져오기
TMap<FString, TArray<int32>> FNetworkManager::GetMissingSequences() const
{
    TMap<FString, TArray<int32>> Result;

    for (const auto& Pair : EndpointSequenceTrackers)
    {
        TArray<uint16> MissingSeqs = Pair.Value.GetMissingSequences();

        if (MissingSeqs.Num() > 0)
        {
            TArray<int32> MissingInts;
            for (uint16 Seq : MissingSeqs)
            {
                MissingInts.Add((int32)Seq);
            }
            Result.Add(Pair.Key, MissingInts);
        }
    }

    return Result;
}

// 수신된 시퀀스 추적
bool FNetworkManager::TrackReceivedSequence(const FIPv4Endpoint& Sender, uint16 SequenceNumber)
{
    FString EndpointStr = Sender.ToString();

    // 엔드포인트의 시퀀스 추적기가 없으면 생성
    if (!EndpointSequenceTrackers.Contains(EndpointStr))
    {
        FMessageSequenceTracker Tracker;
        Tracker.bOrderGuaranteed = bOrderGuaranteedEnabled;
        EndpointSequenceTrackers.Add(EndpointStr, Tracker);
    }

    // 시퀀스 처리
    FMessageSequenceTracker& Tracker = EndpointSequenceTrackers[EndpointStr];
    bool bCanProcess = Tracker.AddSequence(SequenceNumber);

    // 누락된 메시지가 있으면 로그 출력
    TArray<uint16> MissingSeqs = Tracker.GetMissingSequences();
    if (MissingSeqs.Num() > 0)
    {
        UE_LOG(LogMultiServerSync, Verbose, TEXT("Missing sequences from %s: %d"),
            *EndpointStr, MissingSeqs.Num());
    }

    return bCanProcess;
}

// 메시지가 순서대로 도착했는지 확인
bool FNetworkManager::IsMessageInOrder(const FIPv4Endpoint& Sender, uint16 SequenceNumber)
{
    FString EndpointStr = Sender.ToString();

    if (!EndpointSequenceTrackers.Contains(EndpointStr))
    {
        return true; // 첫 메시지는 항상 순서대로 간주
    }

    const FMessageSequenceTracker& Tracker = EndpointSequenceTrackers[EndpointStr];
    return SequenceNumber == Tracker.NextExpectedSequence;
}

// 누락된 메시지 요청
void FNetworkManager::RequestMissingMessages(const FIPv4Endpoint& Endpoint)
{
    FString EndpointStr = Endpoint.ToString();

    if (!EndpointSequenceTrackers.Contains(EndpointStr))
    {
        return;
    }

    const FMessageSequenceTracker& Tracker = EndpointSequenceTrackers[EndpointStr];
    TArray<uint16> MissingSeqs = Tracker.GetMissingSequences();

    if (MissingSeqs.Num() == 0)
    {
        return;
    }

    // 최대 10개 시퀀스만 요청 (과도한 요청 방지)
    const int32 MaxRequestCount = 10;
    if (MissingSeqs.Num() > MaxRequestCount)
    {
        MissingSeqs.SetNum(MaxRequestCount);
    }

    // 재전송 요청 메시지 생성
    TArray<uint8> RequestData;
    RequestData.SetNum(MissingSeqs.Num() * sizeof(uint16));

    for (int32 i = 0; i < MissingSeqs.Num(); i++)
    {
        FMemory::Memcpy(RequestData.GetData() + (i * sizeof(uint16)), &MissingSeqs[i], sizeof(uint16));
    }

    FNetworkMessage Message(ENetworkMessageType::MessageRetry, RequestData);
    Message.SetProjectId(ProjectId);
    Message.SetSequenceNumber(GetNextSequenceNumber());

    // 요청 전송
    SendMessageToEndpoint(Endpoint, Message);

    UE_LOG(LogMultiServerSync, Verbose, TEXT("Requested %d missing messages from %s"),
        MissingSeqs.Num(), *EndpointStr);
}

// 메시지 재전송 요청 처리
void FNetworkManager::HandleMessageRetryRequest(const FNetworkMessage& Message, const FIPv4Endpoint& Sender)
{
    // 요청된 시퀀스 번호 추출
    const TArray<uint8>& Data = Message.GetData();
    int32 SequenceCount = Data.Num() / sizeof(uint16);

    if (SequenceCount == 0)
    {
        return;
    }

    TArray<uint16> RequestedSequences;
    RequestedSequences.SetNumZeroed(SequenceCount);

    for (int32 i = 0; i < SequenceCount; i++)
    {
        uint16 SequenceNumber = 0;
        FMemory::Memcpy(&SequenceNumber, Data.GetData() + (i * sizeof(uint16)), sizeof(uint16));
        RequestedSequences.Add(SequenceNumber);
    }

    // 각 시퀀스에 대해 재전송 처리
    // 실제 구현에서는 원본 메시지를 캐싱해야 함
    for (uint16 Sequence : RequestedSequences)
    {
        UE_LOG(LogMultiServerSync, Verbose, TEXT("Received retry request for sequence %u from %s"),
            Sequence, *Sender.ToString());

        // 가상의 원본 메시지 재전송
        // 실제 구현에서는 캐싱된 메시지 사용
        TArray<uint8> DummyData;
        FNetworkMessage RetryMessage(ENetworkMessageType::Data, DummyData);
        RetryMessage.SetProjectId(ProjectId);
        RetryMessage.SetSequenceNumber(Sequence);
        RetryMessage.SetFlags(1); // ACK 필요 표시

        SendMessageToEndpoint(Sender, RetryMessage);
    }
}

// 시퀀스 관리 정기 체크
bool FNetworkManager::CheckSequenceManagement(float DeltaTime)
{
    if (!bIsInitialized)
    {
        return true; // 계속 틱 유지
    }

    // 누락된 메시지가 있는 엔드포인트에 재전송 요청
    for (const auto& Pair : EndpointSequenceTrackers)
    {
        FString EndpointStr = Pair.Key;
        const FMessageSequenceTracker& Tracker = Pair.Value;

        if (Tracker.NeedsRetransmissionRequest())
        {
            // 엔드포인트 파싱
            FIPv4Address Address;
            uint16 PortNumber = 0;  // Port를 PortNumber로 변경

            TArray<FString> Parts;
            EndpointStr.ParseIntoArray(Parts, TEXT(":"), true);

            if (Parts.Num() >= 2 && FIPv4Address::Parse(Parts[0], Address))
            {
                PortNumber = FCString::Atoi(*Parts[1]);  // Port를 PortNumber로 변경
                FIPv4Endpoint Endpoint(Address, PortNumber);  // 여기도 변경

                // 누락 메시지 요청
                RequestMissingMessages(Endpoint);
            }
        }
    }

    return true; // 계속 틱 유지
}

// 메시지 처리 여부 결정
bool FNetworkManager::ShouldProcessMessage(const FIPv4Endpoint& Sender, uint16 SequenceNumber)
{
    // 수신된 시퀀스 추적
    bool bCanProcess = TrackReceivedSequence(Sender, SequenceNumber);

    // 순서 보장이 활성화되어 있고, 처리할 수 없는 경우
    if (bOrderGuaranteedEnabled && !bCanProcess)
    {
        UE_LOG(LogMultiServerSync, Verbose, TEXT("Message out of order (Seq: %u, from: %s), deferring processing"),
            SequenceNumber, *Sender.ToString());
        return false;
    }

    return true;
}

// 중복 메시지 추적 관리 틱 함수
bool FNetworkManager::CheckDuplicateTracker(float DeltaTime)
{
    if (!bIsInitialized)
    {
        return true; // 계속 틱 유지
    }

    double CurrentTime = FPlatformTime::Seconds();

    // 정리 간격마다 중복 메시지 추적 데이터 정리
    if (CurrentTime - DuplicateMessageTracker.LastCleanupTime >= DuplicateMessageTracker.CleanupIntervalSeconds)
    {
        CleanupMessageTracker();
        DuplicateMessageTracker.LastCleanupTime = CurrentTime;
    }

    return true; // 계속 틱 유지
}

// 메시지가 중복인지 확인하고 처리 여부 반환
bool FNetworkManager::IsDuplicateMessage(const FIPv4Endpoint& Sender, uint16 SequenceNumber)
{
    // 이미 처리한 메시지인지 확인
    return DuplicateMessageTracker.IsMessageProcessed(Sender, SequenceNumber);
}

// 처리된 메시지 추적 추가
void FNetworkManager::AddProcessedMessage(const FIPv4Endpoint& Sender, uint16 SequenceNumber)
{
    DuplicateMessageTracker.AddProcessedMessage(Sender, SequenceNumber);
}

// 중복 메시지 추적 정리
void FNetworkManager::CleanupMessageTracker()
{
    DuplicateMessageTracker.Cleanup();

    UE_LOG(LogMultiServerSync, Verbose, TEXT("Cleaned up duplicate message tracker"));
}

// 메시지 캐시 관리 틱 함수
bool FNetworkManager::CheckMessageCache(float DeltaTime)
{
    if (!bIsInitialized)
    {
        return true; // 계속 틱 유지
    }

    double CurrentTime = FPlatformTime::Seconds();

    // 정리 간격마다 메시지 캐시 정리
    if (CurrentTime - MessageCache.LastCleanupTime >= MESSAGE_CACHE_CLEANUP_INTERVAL)
    {
        CleanupMessageCache();
        MessageCache.LastCleanupTime = CurrentTime;
    }

    return true; // 계속 틱 유지
}

// 메시지 캐싱
void FNetworkManager::CacheProcessedMessage(const FIPv4Endpoint& Sender, const FNetworkMessage& Message, const TArray<uint8>& Response)
{
    MessageCache.CacheMessage(Sender, Message.GetSequenceNumber(), Message, Response);
}

// 캐시된 메시지 처리
bool FNetworkManager::ProcessCachedMessage(const FIPv4Endpoint& Sender, uint16 SequenceNumber)
{
    FCachedMessage CachedMsg;
    if (!MessageCache.GetCachedMessage(Sender, SequenceNumber, CachedMsg))
    {
        return false;
    }

    UE_LOG(LogMultiServerSync, Verbose, TEXT("Using cached message (Sender: %s, Seq: %u, Type: %d)"),
        *Sender.ToString(), SequenceNumber, (int)CachedMsg.Message.GetType());

    // 캐시된 메시지가 응답 데이터를 가지고 있으면 (멱등 처리를 위한 이전 결과)
    if (CachedMsg.ResponseData.Num() > 0)
    {
        // ACK가 필요한 메시지면 ACK 다시 전송
        if (CachedMsg.Message.GetFlags() & 1) // Flag = 1: ACK 필요
        {
            // ACK 메시지 생성
            TArray<uint8> AckData;
            AckData.SetNum(sizeof(uint16));
            FMemory::Memcpy(AckData.GetData(), &SequenceNumber, sizeof(uint16));

            FNetworkMessage AckMessage(ENetworkMessageType::MessageAck, AckData);
            AckMessage.SetProjectId(ProjectId);
            AckMessage.SetSequenceNumber(GetNextSequenceNumber());

            // ACK 메시지 전송
            SendMessageToEndpoint(Sender, AckMessage);

            UE_LOG(LogMultiServerSync, Verbose, TEXT("Sent ACK for cached message (Seq: %u, to: %s)"),
                SequenceNumber, *Sender.ToString());
        }

        // 필요한 경우 캐시된 응답 데이터를 다시 전송
        // (이 예제에서는 구현하지 않음 - 멱등 처리를 위해 필요한 경우 확장)
    }

    return true;
}

// 메시지 캐시 정리
void FNetworkManager::CleanupMessageCache()
{
    MessageCache.Cleanup();

    UE_LOG(LogMultiServerSync, Verbose, TEXT("Cleaned up message cache"));
}

// FNetworkManager.cpp 파일 수정 부분

// 멱등성 처리 결과 저장
void FNetworkLatencyStats::StoreIdempotentResult(uint16 SequenceNumber, const FIdempotentResult& Result)
{
    // 결과 저장 (기존 값 덮어쓰기)
    IdempotentResults.Add(SequenceNumber, Result);
}

// 멱등성 처리 결과 가져오기
bool FNetworkLatencyStats::GetIdempotentResult(uint16 SequenceNumber, FIdempotentResult& OutResult)
{
    FIdempotentResult* Result = IdempotentResults.Find(SequenceNumber);
    if (!Result)
    {
        return false;
    }

    // 캐시 만료 확인
    double CurrentTime = FPlatformTime::Seconds();
    if (CurrentTime - Result->Timestamp > IDEMPOTENT_CACHE_TIMEOUT)
    {
        // 만료된 항목 제거
        IdempotentResults.Remove(SequenceNumber);
        return false;
    }

    // 결과 반환
    OutResult = *Result;
    return true;
}

// 멱등성 캐시 정리
void FNetworkLatencyStats::CleanupIdempotentCache()
{
    double CurrentTime = FPlatformTime::Seconds();
    TArray<uint16> ExpiredItems;

    // 만료된 항목 찾기
    for (auto& Pair : IdempotentResults)
    {
        if (CurrentTime - Pair.Value.Timestamp > IDEMPOTENT_CACHE_TIMEOUT)
        {
            ExpiredItems.Add(Pair.Key);
        }
    }

    // 만료된 항목 제거
    for (uint16 Seq : ExpiredItems)
    {
        IdempotentResults.Remove(Seq);
    }
}

// 멱등성 보장 작업 수행
bool FNetworkManager::ExecuteIdempotentOperation(const FString& OperationId, uint16 SequenceNumber,
    TFunction<FIdempotentResult()> Operation)
{
    // 이미 처리된 작업인지 확인
    FIdempotentResult CachedResult;
    if (GetIdempotentResult(SequenceNumber, CachedResult))
    {
        // 이미 처리된 작업이면 캐시된 결과 반환
        UE_LOG(LogMultiServerSync, Verbose, TEXT("Using cached result for idempotent operation [%s] seq=%u"),
            *OperationId, SequenceNumber);
        return CachedResult.bSuccess;
    }

    // 새로운 작업 실행
    UE_LOG(LogMultiServerSync, Verbose, TEXT("Executing idempotent operation [%s] seq=%u"),
        *OperationId, SequenceNumber);

    // 작업 실행 및 결과 캐싱
    FIdempotentResult Result = Operation();
    StoreIdempotentResult(SequenceNumber, Result);

    // 작업 성공 여부 반환
    return Result.bSuccess;
}

// 멱등성 캐시 정리 틱 함수
bool FNetworkManager::CheckIdempotentCache(float DeltaTime)
{
    if (!bIsInitialized)
    {
        return true; // 계속 틱 유지
    }

    // 주기적으로 캐시 정리
    static double LastCleanupTime = FPlatformTime::Seconds();
    double CurrentTime = FPlatformTime::Seconds();

    if (CurrentTime - LastCleanupTime >= IDEMPOTENT_CACHE_CLEANUP_INTERVAL)
    {
        CleanupIdempotentCache();
        LastCleanupTime = CurrentTime;
    }

    return true; // 계속 틱 유지
}