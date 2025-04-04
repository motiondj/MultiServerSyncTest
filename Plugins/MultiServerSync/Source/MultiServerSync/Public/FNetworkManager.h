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

// 메시지 유형 정의
enum class ENetworkMessageType : uint8
{
    Discovery = 0,    // 서버 탐색 메시지
    DiscoveryResponse = 1, // 탐색 응답 메시지
    TimeSync = 2,     // 시간 동기화 메시지
    FrameSync = 3,    // 프레임 동기화 메시지
    Command = 4,      // 일반 명령 메시지
    Data = 5,         // 데이터 전송 메시지
    Custom = 255      // 사용자 정의 메시지
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
    // End INetworkManager interface

    /** Discover other servers on the network */
    bool DiscoverServers();

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

    /** 다음 시퀀스 번호 생성 */
    uint16 GetNextSequenceNumber();

    /** 이벤트 간 충분한 시간이 지났는지 확인 (속도 제한) */
    bool HasEnoughTimePassed(double& LastTime, double Interval) const;
};