#include "FNetworkManager.h"
#include "FSyncLog.h"
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
    // 시간 동기화 메시지 처리 (모듈 3에서 구현 예정)
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