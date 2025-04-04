// FPTPClient.cpp
#include "FPTPClient.h"
#include "Misc/DateTime.h"
#include "FSyncLog.h"
#include "HAL/PlatformTime.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"

// PTP 메시지 구조체 정의
#pragma pack(push, 1)
struct FPTPMessageHeader {
    uint8 MessageType;
    uint8 VersionPTP;
    uint16 MessageLength;
    uint8 DomainNumber;
    uint8 Reserved;
    uint16 Flags;
    int64 CorrectionField;
    uint32 Reserved2;
    uint8 SourcePortIdentity[10];
    uint16 SequenceId;
    uint8 ControlField;
    int8 LogMessageInterval;
};

struct FPTPTimestamp {
    uint32 Seconds;
    uint32 NanoSeconds;
};

struct FPTPSyncMessage {
    FPTPMessageHeader Header;
    FPTPTimestamp OriginTimestamp;
};

struct FPTPDelayReqMessage {
    FPTPMessageHeader Header;
    FPTPTimestamp OriginTimestamp;
};

struct FPTPFollowUpMessage {
    FPTPMessageHeader Header;
    FPTPTimestamp PreciseOriginTimestamp;
};

struct FPTPDelayRespMessage {
    FPTPMessageHeader Header;
    FPTPTimestamp ReceiveTimestamp;
    uint8 RequestingPortIdentity[10];
};
#pragma pack(pop)

FPTPClient::FPTPClient()
    : bIsMaster(false)
    , bIsInitialized(false)
    , bIsSynchronized(false)
    , TimeOffsetMicroseconds(0)
    , PathDelayMicroseconds(0)
    , EstimatedErrorMicroseconds(0)
    , LastSyncTime(0)
    , SyncSequenceNumber(0)
    , SyncInterval(1.0) // 1초 간격으로 동기화
    , LastSyncMessageTimestamp(0)
    , LastDelayReqTimestamp(0)
    , T1(0) // Sync 메시지 발신 시간
    , T2(0) // Sync 메시지 수신 시간
    , T3(0) // DelayReq 메시지 발신 시간
    , T4(0) // DelayReq 메시지 수신 시간
{
}

FPTPClient::~FPTPClient()
{
    if (bIsInitialized)
    {
        Shutdown();
    }
}

bool FPTPClient::Initialize()
{
    UE_LOG(LogMultiServerSync, Display, TEXT("Initializing PTP Client"));
    LastSyncTime = GetTimestampMicroseconds();
    bIsInitialized = true;
    return true;
}

void FPTPClient::Shutdown()
{
    UE_LOG(LogMultiServerSync, Display, TEXT("Shutting down PTP Client"));
    bIsInitialized = false;
    bIsSynchronized = false;
}

void FPTPClient::SetMasterMode(bool bInIsMaster)
{
    bIsMaster = bInIsMaster;
    UE_LOG(LogMultiServerSync, Display, TEXT("PTP Client set to %s mode"), bIsMaster ? TEXT("master") : TEXT("slave"));
}

bool FPTPClient::IsMasterMode() const
{
    return bIsMaster;
}

void FPTPClient::SendSyncMessage()
{
    if (!bIsInitialized || !bIsMaster)
    {
        return;
    }

    // 현재 시간을 마이크로초 단위로 가져옴
    int64 CurrentTime = GetTimestampMicroseconds();

    // 마지막 동기화 메시지 이후 충분한 시간이 지났는지 확인
    if (CurrentTime - LastSyncTime < static_cast<int64>(SyncInterval * 1000000))
    {
        return;
    }

    // T1 타임스탬프 저장 (Sync 메시지 발신 시간)
    T1 = CurrentTime;
    LastSyncMessageTimestamp = T1;
    LastSyncTime = CurrentTime;

    // Sync 메시지 생성
    TArray<uint8> SyncMessage = CreatePTPMessage(EPTPMessageType::Sync);

    // 메시지 헤더와 데이터를 로그로 출력 (디버깅용)
    UE_LOG(LogMultiServerSync, Verbose, TEXT("Sending Sync message, sequence: %d, timestamp: %lld"),
        SyncSequenceNumber, T1);

    // 실제로는 여기서 네트워크 매니저를 통해 메시지 전송
    // NetworkManager->BroadcastMessage(SyncMessage);

    // 정확한 타임스탬프를 포함한 Follow-Up 메시지 전송
    SendFollowUpMessage(T1);

    // 시퀀스 번호 증가
    SyncSequenceNumber++;
}

void FPTPClient::SendFollowUpMessage(int64 OriginTimestampMicros)
{
    if (!bIsInitialized || !bIsMaster)
    {
        return;
    }

    // Follow-Up 메시지 생성
    TArray<uint8> FollowUpMessage = CreatePTPMessage(EPTPMessageType::FollowUp);

    // 메시지 데이터에 정확한 타임스탬프 추가
    FMemoryWriter Writer(FollowUpMessage);
    Writer.Seek(sizeof(FPTPMessageHeader)); // 헤더 이후 위치로 이동

    // 마이크로초를 초 및 나노초로 변환
    uint32 Seconds = static_cast<uint32>(OriginTimestampMicros / 1000000);
    uint32 NanoSeconds = static_cast<uint32>((OriginTimestampMicros % 1000000) * 1000);

    Writer << Seconds;
    Writer << NanoSeconds;

    UE_LOG(LogMultiServerSync, Verbose, TEXT("Sending Follow-Up message, sequence: %d, precise timestamp: %lld"),
        SyncSequenceNumber - 1, OriginTimestampMicros);

    // 실제로는 여기서 네트워크 매니저를 통해 메시지 전송
    // NetworkManager->BroadcastMessage(FollowUpMessage);
}

void FPTPClient::SendDelayReqMessage()
{
    if (!bIsInitialized || bIsMaster)
    {
        return;
    }

    // T3 타임스탬프 저장 (DelayReq 메시지 발신 시간)
    T3 = GetTimestampMicroseconds();
    LastDelayReqTimestamp = T3;

    // DelayReq 메시지 생성
    TArray<uint8> DelayReqMessage = CreatePTPMessage(EPTPMessageType::DelayReq);

    UE_LOG(LogMultiServerSync, Verbose, TEXT("Sending Delay Request message, timestamp: %lld"), T3);

    // 실제로는 여기서 네트워크 매니저를 통해 메시지 전송
    // NetworkManager->BroadcastMessage(DelayReqMessage);
}

void FPTPClient::SendDelayRespMessage(int64 RequestReceivedTimestamp, uint16 SequenceId)
{
    if (!bIsInitialized || !bIsMaster)
    {
        return;
    }

    // DelayResp 메시지 생성
    TArray<uint8> DelayRespMessage = CreatePTPMessage(EPTPMessageType::DelayResp);

    // 메시지 데이터에 수신 타임스탬프 추가
    FMemoryWriter Writer(DelayRespMessage);
    Writer.Seek(sizeof(FPTPMessageHeader)); // 헤더 이후 위치로 이동

    // 마이크로초를 초 및 나노초로 변환
    uint32 Seconds = static_cast<uint32>(RequestReceivedTimestamp / 1000000);
    uint32 NanoSeconds = static_cast<uint32>((RequestReceivedTimestamp % 1000000) * 1000);

    Writer << Seconds;
    Writer << NanoSeconds;

    // 요청 포트 식별자 추가 (간소화: 시퀀스 ID만 사용)
    uint8 RequestingPortId[10] = { 0 };
    FMemory::Memcpy(RequestingPortId, &SequenceId, sizeof(uint16));
    Writer.Serialize(RequestingPortId, 10);

    UE_LOG(LogMultiServerSync, Verbose, TEXT("Sending Delay Response message, request received at: %lld"),
        RequestReceivedTimestamp);

    // 실제로는 여기서 네트워크 매니저를 통해 메시지 전송
    // NetworkManager->BroadcastMessage(DelayRespMessage);
}

void FPTPClient::ProcessMessage(const TArray<uint8>& Message)
{
    if (!bIsInitialized || Message.Num() < sizeof(FPTPMessageHeader))
    {
        return;
    }

    EPTPMessageType Type = ParsePTPMessageType(Message);
    switch (Type)
    {
    case EPTPMessageType::Sync:
        ProcessSyncMessage(Message);
        break;
    case EPTPMessageType::FollowUp:
        ProcessFollowUpMessage(Message);
        break;
    case EPTPMessageType::DelayReq:
        ProcessDelayReqMessage(Message);
        break;
    case EPTPMessageType::DelayResp:
        ProcessDelayRespMessage(Message);
        break;
    default:
        UE_LOG(LogMultiServerSync, Warning, TEXT("Unknown PTP message type"));
        break;
    }
}

TArray<uint8> FPTPClient::CreatePTPMessage(EPTPMessageType Type)
{
    TArray<uint8> Message;

    // 기본 메시지 크기: 헤더 크기
    uint16 MessageSize = sizeof(FPTPMessageHeader);

    // 메시지 타입에 따라 추가 데이터 크기 계산
    switch (Type)
    {
    case EPTPMessageType::Sync:
    case EPTPMessageType::DelayReq:
        MessageSize += sizeof(FPTPTimestamp); // 오리진 타임스탬프 추가
        break;
    case EPTPMessageType::FollowUp:
        MessageSize += sizeof(FPTPTimestamp); // 정확한 오리진 타임스탬프 추가
        break;
    case EPTPMessageType::DelayResp:
        MessageSize += sizeof(FPTPTimestamp) + 10; // 수신 타임스탬프 + 요청 포트 ID
        break;
    }

    // 메시지 버퍼 초기화
    Message.SetNumZeroed(MessageSize);

    // 메시지 헤더 설정
    FPTPMessageHeader Header;
    FMemory::Memzero(&Header, sizeof(FPTPMessageHeader));

    Header.MessageType = static_cast<uint8>(Type);
    Header.VersionPTP = 2; // PTPv2
    Header.MessageLength = MessageSize;
    Header.DomainNumber = 0;
    Header.Flags = 0;
    Header.SequenceId = SyncSequenceNumber;

    // 헤더를 메시지에 복사
    FMemory::Memcpy(Message.GetData(), &Header, sizeof(FPTPMessageHeader));

    return Message;
}

FPTPClient::EPTPMessageType FPTPClient::ParsePTPMessageType(const TArray<uint8>& Message)
{
    if (Message.Num() < sizeof(FPTPMessageHeader))
    {
        return EPTPMessageType::Unknown;
    }

    // 첫 바이트가 메시지 타입
    uint8 Type = Message[0];

    switch (Type)
    {
    case 0: return EPTPMessageType::Sync;
    case 1: return EPTPMessageType::DelayReq;
    case 2: return EPTPMessageType::FollowUp;
    case 3: return EPTPMessageType::DelayResp;
    default: return EPTPMessageType::Unknown;
    }
}

void FPTPClient::ProcessSyncMessage(const TArray<uint8>& Message)
{
    if (bIsMaster)
    {
        return; // 마스터는 Sync 메시지를 처리하지 않음
    }

    // T2 타임스탬프 저장 (Sync 메시지 수신 시간)
    T2 = GetTimestampMicroseconds();

    // 메시지 시퀀스 ID 추출
    FPTPMessageHeader Header;
    FMemory::Memcpy(&Header, Message.GetData(), sizeof(FPTPMessageHeader));

    UE_LOG(LogMultiServerSync, Verbose, TEXT("Received Sync message, sequence: %d, received at: %lld"),
        Header.SequenceId, T2);

    // 마스터에게 DelayReq 메시지 전송 (지연 측정 시작)
    // 실제 구현에서는 Follow-Up 메시지 수신 후 지연
    if (FMath::FRand() < 0.2) // 20% 확률로 DelayReq 전송 (네트워크 부하 방지)
    {
        SendDelayReqMessage();
    }
}

void FPTPClient::ProcessFollowUpMessage(const TArray<uint8>& Message)
{
    if (bIsMaster)
    {
        return; // 마스터는 Follow-Up 메시지를 처리하지 않음
    }

    // 메시지가 충분히 큰지 확인
    if (Message.Num() < sizeof(FPTPMessageHeader) + sizeof(FPTPTimestamp))
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Follow-Up message too small"));
        return;
    }

    // 정확한 오리진 타임스탬프(T1) 추출
    FMemoryReader Reader(Message);
    Reader.Seek(sizeof(FPTPMessageHeader));

    uint32 Seconds;
    uint32 NanoSeconds;
    Reader << Seconds;
    Reader << NanoSeconds;

    // T1 (마스터의 Sync 전송 시간)
    int64 PreciseT1 = (static_cast<int64>(Seconds) * 1000000) + (NanoSeconds / 1000);

    UE_LOG(LogMultiServerSync, Verbose, TEXT("Received Follow-Up message, precise T1: %lld, T2: %lld"),
        PreciseT1, T2);

    // 마스터와 슬레이브 간의 오프셋 계산 (임시, 경로 지연 보정 필요)
    // 오프셋 = T2 - T1
    int64 TempOffset = T2 - PreciseT1;

    // 경로 지연이 계산되었으면 보정
    if (PathDelayMicroseconds > 0)
    {
        // 경로 지연을 고려한 오프셋: Offset = T2 - T1 - PathDelay/2
        TimeOffsetMicroseconds = TempOffset - (PathDelayMicroseconds / 2);

        // 동기화 플래그 설정
        bIsSynchronized = true;

        UE_LOG(LogMultiServerSync, Display, TEXT("Time offset updated: %lld microseconds (path delay: %lld)"),
            TimeOffsetMicroseconds, PathDelayMicroseconds);
    }
    else
    {
        // 경로 지연이 아직 계산되지 않았으면 임시 오프셋 사용
        TimeOffsetMicroseconds = TempOffset;
        UE_LOG(LogMultiServerSync, Display, TEXT("Temporary time offset: %lld microseconds (no path delay)"),
            TimeOffsetMicroseconds);
    }
}

void FPTPClient::ProcessDelayReqMessage(const TArray<uint8>& Message)
{
    if (!bIsMaster)
    {
        return; // 슬레이브는 DelayReq 메시지를 처리하지 않음
    }

    // T4 타임스탬프 저장 (DelayReq 메시지 수신 시간)
    int64 ReceivedTime = GetTimestampMicroseconds();

    // 메시지 시퀀스 ID 추출
    FPTPMessageHeader Header;
    FMemory::Memcpy(&Header, Message.GetData(), sizeof(FPTPMessageHeader));

    UE_LOG(LogMultiServerSync, Verbose, TEXT("Received Delay Request message, sequence: %d, received at: %lld"),
        Header.SequenceId, ReceivedTime);

    // DelayResp 메시지로 응답 (T4 포함)
    SendDelayRespMessage(ReceivedTime, Header.SequenceId);
}

void FPTPClient::ProcessDelayRespMessage(const TArray<uint8>& Message)
{
    if (bIsMaster)
    {
        return; // 마스터는 DelayResp 메시지를 처리하지 않음
    }

    // 메시지가 충분히 큰지 확인
    if (Message.Num() < sizeof(FPTPMessageHeader) + sizeof(FPTPTimestamp) + 10)
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("DelayResp message too small"));
        return;
    }

    // T4 타임스탬프 추출 (DelayReq 메시지 수신 시간)
    FMemoryReader Reader(Message);
    Reader.Seek(sizeof(FPTPMessageHeader));

    uint32 Seconds;
    uint32 NanoSeconds;
    Reader << Seconds;
    Reader << NanoSeconds;

    // T4 (마스터의 DelayReq 수신 시간)
    int64 MasterReceivedTime = (static_cast<int64>(Seconds) * 1000000) + (NanoSeconds / 1000);

    // 요청 포트 식별자 확인 (간소화: 패스)
    uint8 RequestingPortId[10];
    Reader.Serialize(RequestingPortId, 10);

    UE_LOG(LogMultiServerSync, Verbose, TEXT("Received Delay Response message, T3: %lld, T4: %lld"),
        T3, MasterReceivedTime);

    // 경로 지연 계산: PathDelay = (T4 - T1) - (T3 - T2)
    // = (T4 - T3) + (T2 - T1)
    if (T1 > 0 && T2 > 0 && T3 > 0)
    {
        int64 NewPathDelay = (MasterReceivedTime - T3) + (T2 - T1);

        // 이전 값과 새 값의 가중 평균 (필터링)
        if (PathDelayMicroseconds > 0)
        {
            PathDelayMicroseconds = (PathDelayMicroseconds * 7 + NewPathDelay * 3) / 10; // 70% 이전 값, 30% 새 값
        }
        else
        {
            PathDelayMicroseconds = NewPathDelay;
        }

        UE_LOG(LogMultiServerSync, Display, TEXT("Path delay updated: %lld microseconds"), PathDelayMicroseconds);

        // 경로 지연이 업데이트되었으므로 시간 오프셋 재계산
        // 오프셋 = T2 - T1 - PathDelay/2
        TimeOffsetMicroseconds = (T2 - T1) - (PathDelayMicroseconds / 2);

        // 동기화 플래그 설정
        bIsSynchronized = true;

        UE_LOG(LogMultiServerSync, Display, TEXT("Time offset updated with path delay: %lld microseconds"),
            TimeOffsetMicroseconds);

        // 오차 추정값 업데이트
        EstimatedErrorMicroseconds = FMath::Abs(NewPathDelay - PathDelayMicroseconds) / 2;
    }
}

int64 FPTPClient::GetTimestampMicroseconds() const
{
    // 현재 시스템 시간을 마이크로초 단위로 반환
    return FDateTime::Now().GetTicks() / 10; // 100나노초 -> 마이크로초
}

int64 FPTPClient::GetTimeOffsetMicroseconds() const
{
    return TimeOffsetMicroseconds;
}

int64 FPTPClient::GetPathDelayMicroseconds() const
{
    return PathDelayMicroseconds;
}

int64 FPTPClient::GetEstimatedErrorMicroseconds() const
{
    return EstimatedErrorMicroseconds;
}

bool FPTPClient::IsSynchronized() const
{
    return bIsSynchronized;
}

double FPTPClient::GetSyncInterval() const
{
    return SyncInterval;
}

void FPTPClient::SetSyncInterval(double IntervalSeconds)
{
    SyncInterval = FMath::Max(0.001, IntervalSeconds); // 최소 1ms
}

void FPTPClient::Update()
{
    if (!bIsInitialized)
    {
        return;
    }

    // 마스터 모드인 경우 주기적으로 Sync 메시지 전송
    if (bIsMaster)
    {
        SendSyncMessage();
    }

    // 기타 주기적 작업 (필요 시)
}