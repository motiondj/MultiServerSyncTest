#include "FNetworkManager.h"
#include "FSyncLog.h"
#include "FTimeSync.h"
#include "MultiServerSync.h"
#include "ISyncFrameworkManager.h"
#include "Serialization/BufferArchive.h"
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

// FNetworkManager 클래스 구현
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

    return true;
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

    bIsInitialized = false;
    UE_LOG(LogMultiServerSync, Display, TEXT("Network Manager shutdown completed"));
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

void FNetworkManager::ProcessReceivedData(const TArray<uint8>& Data, const FIPv4Endpoint& Sender)
{
    // 메시지 파싱
    FNetworkMessage Message(Data);

    // 우리 프로젝트의 메시지가 아니면 무시
    if (Message.GetProjectId().IsValid() && Message.GetProjectId() != ProjectId)
    {
        return;
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
        // 추가: 마스터-슬레이브 프로토콜 메시지 처리
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
    case ENetworkMessageType::Custom:
        HandleCustomMessage(Message, Sender);
        break;
    default:
        UE_LOG(LogMultiServerSync, Warning, TEXT("Unknown message type received: %d"), (int)Message.GetType());
        break;
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
}

void FNetworkManager::HandleCustomMessage(const FNetworkMessage& Message, const FIPv4Endpoint& Sender)
{
    // 사용자 정의 메시지 처리 (HandleCommandMessage와 동일한 로직)
    HandleCommandMessage(Message, Sender);
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