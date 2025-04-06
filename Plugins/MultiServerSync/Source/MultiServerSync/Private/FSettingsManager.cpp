// FSettingsManager.cpp
#include "FSettingsManager.h"
#include "FSyncLog.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Misc/Paths.h"
#include "Misc/App.h"

FSettingsManager::FSettingsManager()
    : bIsInitialized(false)
    , LastSettingsUpdateSequence(0)
    , LastSettingsUpdateTime(0.0)
    , SettingsUpdateAttempts(0)
    , bSettingsUpdatePending(false)
    , LastSettingsBroadcastTime(0.0)
    , SettingsBroadcastInterval(5.0) // 5초마다 브로드캐스트
{
}

FSettingsManager::~FSettingsManager()
{
    if (bIsInitialized)
    {
        Shutdown();
    }
}

bool FSettingsManager::Initialize()
{
    UE_LOG(LogMultiServerSync, Display, TEXT("Initializing Settings Manager"));

    // 기본 설정 초기화
    CurrentSettings = FGlobalSettings();

    // 호스트 이름을 마지막 업데이트자로 설정
    CurrentSettings.LastUpdatedBy = FPlatformProcess::ComputerName();
    CurrentSettings.LastUpdatedTimeMs = FDateTime::Now().ToUnixTimestamp() * 1000;

    // 로컬 머신의 고유 ID를 포함
    FString MachineId = FPlatformMisc::GetMachineId().ToString();
    FString HostName = FPlatformProcess::ComputerName();
    CurrentSettings.ProjectName = FString::Printf(TEXT("%s_%s"), *FApp::GetProjectName(), *HostName);

    bIsInitialized = true;

    // 기본 파일 위치에서 설정 로드 시도
    FString DefaultFilePath = GetDefaultSettingsFilePath();
    if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*DefaultFilePath))
    {
        if (LoadSettingsFromFile(DefaultFilePath))
        {
            UE_LOG(LogMultiServerSync, Display, TEXT("Loaded settings from file: %s"), *DefaultFilePath);
        }
    }
    else
    {
        // 파일이 없으면 새 설정 저장
        SaveSettingsToFile(DefaultFilePath);
        UE_LOG(LogMultiServerSync, Display, TEXT("Created new settings file: %s"), *DefaultFilePath);
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Settings Manager initialized"));
    return true;
}

void FSettingsManager::Shutdown()
{
    UE_LOG(LogMultiServerSync, Display, TEXT("Shutting down Settings Manager"));

    // 설정이 변경되었다면 저장
    if (bIsInitialized)
    {
        FString DefaultPath = GetDefaultSettingsFilePath();
        SaveSettingsToFile(DefaultPath);
    }

    bIsInitialized = false;
}

const FGlobalSettings& FSettingsManager::GetSettings() const
{
    return CurrentSettings;
}

bool FSettingsManager::UpdateSettings(const FGlobalSettings& NewSettings)
{
    if (!bIsInitialized)
    {
        return false;
    }

    // 새 설정의 유효성 확인
    if (!ValidateSettings(NewSettings))
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Invalid settings provided for update"));
        return false;
    }

    // 설정이 실제로 변경되었는지 확인
    if (!CurrentSettings.IsDifferentFrom(NewSettings))
    {
        // 설정이 동일하면 업데이트 필요 없음
        return true;
    }

    // 설정 업데이트
    CurrentSettings = NewSettings;

    // 버전 증가 및 타임스탬프 업데이트
    CurrentSettings.SettingsVersion++;
    CurrentSettings.LastUpdatedBy = FPlatformProcess::ComputerName();
    CurrentSettings.LastUpdatedTimeMs = FDateTime::Now().ToUnixTimestamp() * 1000;

    // 콜백 실행
    TriggerSettingsChangedEvent();

    UE_LOG(LogMultiServerSync, Display, TEXT("Settings updated to version %d"), CurrentSettings.SettingsVersion);
    return true;
}

bool FSettingsManager::SaveSettingsToFile(const FString& FilePath)
{
    if (!bIsInitialized)
    {
        return false;
    }

    FString PathToUse = FilePath;
    if (PathToUse.IsEmpty())
    {
        PathToUse = LastSavedFilePath;
        if (PathToUse.IsEmpty())
        {
            PathToUse = GetDefaultSettingsFilePath();
        }
    }

    // 설정 직렬화
    TArray<uint8> FSettingsManager::SerializeSettings() const
    {
        TArray<uint8> Result;
        FMemoryWriter MemWriter(Result);

        // 설정 직렬화 - const 제거를 위해 복사본 사용
        FGlobalSettings SettingsCopy = CurrentSettings;
        MemWriter << SettingsCopy;

        return Result;
    }

    // 파일 저장
    bool bSuccess = FFileHelper::SaveArrayToFile(SerializedData, *PathToUse);
    if (bSuccess)
    {
        LastSavedFilePath = PathToUse;
        UE_LOG(LogMultiServerSync, Display, TEXT("Settings saved to file: %s"), *PathToUse);
    }
    else
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to save settings to file: %s"), *PathToUse);
    }

    return bSuccess;
}

bool FSettingsManager::LoadSettingsFromFile(const FString& FilePath)
{
    if (!bIsInitialized)
    {
        return false;
    }

    FString PathToUse = FilePath;
    if (PathToUse.IsEmpty())
    {
        PathToUse = LastSavedFilePath;
        if (PathToUse.IsEmpty())
        {
            PathToUse = GetDefaultSettingsFilePath();
        }
    }

    // 파일이 존재하는지 확인
    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*PathToUse))
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Settings file not found: %s"), *PathToUse);
        return false;
    }

    // 파일 로드
    TArray<uint8> SerializedData;
    bool bSuccess = FFileHelper::LoadFileToArray(SerializedData, *PathToUse);
    if (!bSuccess)
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to load settings from file: %s"), *PathToUse);
        return false;
    }

    // 역직렬화
    if (!DeserializeSettings(SerializedData))
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to deserialize settings from file: %s"), *PathToUse);
        return false;
    }

    LastSavedFilePath = PathToUse;
    UE_LOG(LogMultiServerSync, Display, TEXT("Settings loaded from file: %s"), *PathToUse);

    // 콜백 실행
    TriggerSettingsChangedEvent();

    return true;
}

TArray<uint8> FSettingsManager::SerializeSettings() const
{
    TArray<uint8> Result;
    FMemoryWriter MemWriter(Result);

    // 설정 직렬화
    MemWriter << CurrentSettings;

    return Result;
}

bool FSettingsManager::DeserializeSettings(const TArray<uint8>& Data)
{
    if (Data.Num() == 0)
    {
        return false;
    }

    FMemoryReader MemReader(Data);

    // 임시 설정 객체 생성
    FGlobalSettings NewSettings;

    // 설정 역직렬화
    MemReader << NewSettings;

    // 설정 유효성 검사
    if (!ValidateSettings(NewSettings))
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Invalid settings in deserialized data"));
        return false;
    }

    // 설정 업데이트
    CurrentSettings = NewSettings;

    return true;
}

void FSettingsManager::RequestSettingsFromMaster()
{
    if (!bIsInitialized)
    {
        return;
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Requesting settings from master server"));

    // 요청 메시지 생성 - 여기서는 간단한 요청 ID만 포함
    TArray<uint8> RequestData;
    FString RequestId = FString::Printf(TEXT("%s:%lld"),
        *FPlatformProcess::ComputerName(),
        FDateTime::Now().ToUnixTimestamp());

    // RequestId를 바이트 배열로 변환
    RequestData.SetNum(RequestId.Len() * sizeof(TCHAR));
    FMemory::Memcpy(RequestData.GetData(), *RequestId, RequestId.Len() * sizeof(TCHAR));

    // 설정 업데이트 요청 상태 설정
    bSettingsUpdatePending = true;
    SettingsUpdateAttempts = 1;
    LastSettingsUpdateTime = FPlatformTime::Seconds();

    // 네트워크 매니저를 통해 요청 전송
    // 이 메서드도 외부에서 NetworkManager 인스턴스를 가져와서 실행해야 함
}

void FSettingsManager::BroadcastSettings()
{
    if (!bIsInitialized)
    {
        return;
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Broadcasting current settings (v%d) to all servers"),
        CurrentSettings.SettingsVersion);

    // 현재 설정을 직렬화
    TArray<uint8> SettingsData = SerializeSettings();

    // 네트워크 매니저를 통해 설정 브로드캐스트
    // 이 메서드는 외부에서 NetworkManager 인스턴스를 가져와서 실행해야 함
    // 프레임워크 매니저가 이 메서드를 호출할 때 적절한 네트워크 매니저를 통해 송신
}

void FSettingsManager::RegisterOnSettingsChangedCallback(TFunction<void(const FGlobalSettings&)> Callback)
{
    OnSettingsChangedCallbacks.Add(Callback);
}

void FSettingsManager::RegisterOnRemoteSettingsReceivedCallback(TFunction<void(const FGlobalSettings&, bool)> Callback)
{
    OnRemoteSettingsReceivedCallbacks.Add(Callback);
}

void FSettingsManager::TriggerSettingsChangedEvent()
{
    for (auto& Callback : OnSettingsChangedCallbacks)
    {
        Callback(CurrentSettings);
    }
}

bool FSettingsManager::ValidateSettings(const FGlobalSettings& Settings) const
{
    // 설정 유효성 검사
    if (!Settings.IsValid())
    {
        return false;
    }

    // 추가 검증 로직이 필요하면 여기에 구현

    return true;
}

FGlobalSettings FSettingsManager::ResolveSettingsConflict(const FGlobalSettings& LocalSettings, const FGlobalSettings& RemoteSettings) const
{
    // 설정 충돌 해결 로직
    // 기본적으로 더 높은 버전이나 더 최근에 업데이트된 설정을 선택

    // 버전이 다르면 더 높은 버전 선택
    if (RemoteSettings.SettingsVersion > LocalSettings.SettingsVersion)
    {
        return RemoteSettings;
    }
    else if (LocalSettings.SettingsVersion > RemoteSettings.SettingsVersion)
    {
        return LocalSettings;
    }

    // 버전이 같으면 더 최근에 업데이트된 설정 선택
    if (RemoteSettings.LastUpdatedTimeMs > LocalSettings.LastUpdatedTimeMs)
    {
        return RemoteSettings;
    }

    // 그 외에는 로컬 설정 유지
    return LocalSettings;
}

FString FSettingsManager::GetDefaultSettingsFilePath() const
{
    // 프로젝트의 Saved/Config 디렉토리에 설정 파일 저장
    return FPaths::ProjectSavedDir() / TEXT("Config") / TEXT("MultiServerSync") / TEXT("ServerSettings.bin");
}

bool FSettingsManager::ProcessMasterSettingsUpdate(const TArray<uint8>& SettingsData, const FString& MasterServerId)
{
    if (!bIsInitialized)
    {
        return false;
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Processing settings update from master: %s"), *MasterServerId);

    // 설정 데이터 역직렬화
    FGlobalSettings MasterSettings;
    FMemoryReader MemReader(SettingsData);
    MemReader << MasterSettings;

    // 설정 유효성 검사
    if (!ValidateSettings(MasterSettings))
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Invalid settings received from master"));
        return false;
    }

    // 로컬 설정과 충돌이 있는지 확인
    if (MasterSettings.SettingsVersion < CurrentSettings.SettingsVersion)
    {
        // 로컬 설정이 더 최신인 경우 충돌 해결
        UE_LOG(LogMultiServerSync, Warning, TEXT("Local settings (v%d) are newer than master settings (v%d)"),
            CurrentSettings.SettingsVersion, MasterSettings.SettingsVersion);

        // 마스터 설정이 우선하므로 로컬 설정 업데이트
        CurrentSettings = MasterSettings;

        // 설정 변경 이벤트 발생
        TriggerSettingsChangedEvent();

        return true;
    }

    // 마스터 설정이 더 최신이거나 같은 경우
    if (CurrentSettings.IsDifferentFrom(MasterSettings))
    {
        // 설정이 실제로 다른 경우 업데이트
        UE_LOG(LogMultiServerSync, Display, TEXT("Updating local settings to match master (v%d)"),
            MasterSettings.SettingsVersion);

        CurrentSettings = MasterSettings;

        // 설정 변경 이벤트 발생
        TriggerSettingsChangedEvent();
    }
    else
    {
        UE_LOG(LogMultiServerSync, Verbose, TEXT("Master settings are identical to local settings (v%d)"),
            MasterSettings.SettingsVersion);
    }

    return true;
}

bool FSettingsManager::ProcessSettingsUpdateRequest(const TArray<uint8>& SettingsData, const FString& RequesterId)
{
    if (!bIsInitialized)
    {
        return false;
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Processing settings update request from: %s"), *RequesterId);

    // 요청 데이터 역직렬화 (필요한 경우)
    // 이 예제에서는 요청 데이터가 간단하다고 가정

    // 현재 설정을 응답으로 보냄
    TArray<uint8> ResponseData = SerializeSettings();

    // 네트워크 매니저를 통해 응답 전송 - 이 부분은 나중에 구현
    // NetworkManager->SendSettingsToServer(RequesterId, ResponseData, ENetworkMessageType::SettingsResponse);

    return true;
}

bool FSettingsManager::ProcessSettingsResponse(const TArray<uint8>& SettingsData, const FString& ResponderId)
{
    if (!bIsInitialized)
    {
        return false;
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Processing settings response from: %s"), *ResponderId);

    // 설정 응답 처리 (설정 요청 후 받은 응답)
    bSettingsUpdatePending = false;
    SettingsUpdateAttempts = 0;

    // 응답 데이터 역직렬화
    FGlobalSettings ReceivedSettings;
    FMemoryReader MemReader(SettingsData);
    MemReader << ReceivedSettings;

    // 설정 유효성 검사
    if (!ValidateSettings(ReceivedSettings))
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Invalid settings in response"));
        return false;
    }

    // 충돌 해결
    FGlobalSettings ResolvedSettings = ResolveSettingsConflict(CurrentSettings, ReceivedSettings);

    // 설정 업데이트
    if (CurrentSettings.IsDifferentFrom(ResolvedSettings))
    {
        CurrentSettings = ResolvedSettings;

        // 설정 변경 이벤트 발생
        TriggerSettingsChangedEvent();

        UE_LOG(LogMultiServerSync, Display, TEXT("Settings updated from response (v%d)"),
            CurrentSettings.SettingsVersion);
    }
    else
    {
        UE_LOG(LogMultiServerSync, Verbose, TEXT("Received settings are identical to local settings (v%d)"),
            CurrentSettings.SettingsVersion);
    }

    // 원격 설정 수신 이벤트 발생
    for (auto& Callback : OnRemoteSettingsReceivedCallbacks)
    {
        Callback(ReceivedSettings, true);
    }

    return true;
}

template<typename T>
bool FSettingsManager::UpdateSettingValue(const FString& SettingName, const T& Value)
{
    if (!bIsInitialized)
    {
        return false;
    }

    bool bUpdated = false;

    // 설정 이름에 따라 해당 값 업데이트
    if (SettingName == TEXT("ProjectName") && CurrentSettings.ProjectName != Value)
    {
        CurrentSettings.ProjectName = Value;
        bUpdated = true;
    }
    else if (SettingName == TEXT("ProjectVersion") && CurrentSettings.ProjectVersion != Value)
    {
        CurrentSettings.ProjectVersion = Value;
        bUpdated = true;
    }
    else if (SettingName == TEXT("SyncPort") && CurrentSettings.SyncPort != static_cast<int32>(Value))
    {
        CurrentSettings.SyncPort = static_cast<int32>(Value);
        bUpdated = true;
    }
    else if (SettingName == TEXT("BroadcastInterval") && !FMath::IsNearlyEqual(CurrentSettings.BroadcastInterval, static_cast<float>(Value)))
    {
        CurrentSettings.BroadcastInterval = static_cast<float>(Value);
        bUpdated = true;
    }
    else if (SettingName == TEXT("ConnectionTimeout") && CurrentSettings.ConnectionTimeout != static_cast<int32>(Value))
    {
        CurrentSettings.ConnectionTimeout = static_cast<int32>(Value);
        bUpdated = true;
    }
    else if (SettingName == TEXT("SyncIntervalMs") && CurrentSettings.SyncIntervalMs != static_cast<int32>(Value))
    {
        CurrentSettings.SyncIntervalMs = static_cast<int32>(Value);
        bUpdated = true;
    }
    else if (SettingName == TEXT("MaxTimeOffsetMs") && CurrentSettings.MaxTimeOffsetMs != static_cast<int32>(Value))
    {
        CurrentSettings.MaxTimeOffsetMs = static_cast<int32>(Value);
        bUpdated = true;
    }
    else if (SettingName == TEXT("PGain") && !FMath::IsNearlyEqual(CurrentSettings.PGain, static_cast<float>(Value)))
    {
        CurrentSettings.PGain = static_cast<float>(Value);
        bUpdated = true;
    }
    else if (SettingName == TEXT("IGain") && !FMath::IsNearlyEqual(CurrentSettings.IGain, static_cast<float>(Value)))
    {
        CurrentSettings.IGain = static_cast<float>(Value);
        bUpdated = true;
    }
    else if (SettingName == TEXT("FilterWeight") && !FMath::IsNearlyEqual(CurrentSettings.FilterWeight, static_cast<float>(Value)))
    {
        CurrentSettings.FilterWeight = static_cast<float>(Value);
        bUpdated = true;
    }
    else if (SettingName == TEXT("TargetFrameRate") && !FMath::IsNearlyEqual(CurrentSettings.TargetFrameRate, static_cast<float>(Value)))
    {
        CurrentSettings.TargetFrameRate = static_cast<float>(Value);
        bUpdated = true;
    }
    else if (SettingName == TEXT("bForceFrameLock") && CurrentSettings.bForceFrameLock != static_cast<bool>(Value))
    {
        CurrentSettings.bForceFrameLock = static_cast<bool>(Value);
        bUpdated = true;
    }
    else if (SettingName == TEXT("MaxFrameSkew") && CurrentSettings.MaxFrameSkew != static_cast<int32>(Value))
    {
        CurrentSettings.MaxFrameSkew = static_cast<int32>(Value);
        bUpdated = true;
    }
    else if (SettingName == TEXT("MasterPriority") && !FMath::IsNearlyEqual(CurrentSettings.MasterPriority, static_cast<float>(Value)))
    {
        CurrentSettings.MasterPriority = static_cast<float>(Value);
        bUpdated = true;
    }
    else if (SettingName == TEXT("bCanBeMaster") && CurrentSettings.bCanBeMaster != static_cast<bool>(Value))
    {
        CurrentSettings.bCanBeMaster = static_cast<bool>(Value);
        bUpdated = true;
    }
    else if (SettingName == TEXT("bForceMaster") && CurrentSettings.bForceMaster != static_cast<bool>(Value))
    {
        CurrentSettings.bForceMaster = static_cast<bool>(Value);
        bUpdated = true;
    }

    if (bUpdated)
    {
        // 버전 증가 및 타임스탬프 업데이트
        CurrentSettings.SettingsVersion++;
        CurrentSettings.LastUpdatedBy = FPlatformProcess::ComputerName();
        CurrentSettings.LastUpdatedTimeMs = FDateTime::Now().ToUnixTimestamp() * 1000;

        // 콜백 실행
        TriggerSettingsChangedEvent();

        UE_LOG(LogMultiServerSync, Display, TEXT("Updated setting %s to version %d"),
            *SettingName, CurrentSettings.SettingsVersion);
    }

    return bUpdated;
}

// 특정 타입에 대한 명시적 템플릿 인스턴스화
template bool FSettingsManager::UpdateSettingValue<FString>(const FString& SettingName, const FString& Value);
template bool FSettingsManager::UpdateSettingValue<int32>(const FString& SettingName, const int32& Value);
template bool FSettingsManager::UpdateSettingValue<float>(const FString& SettingName, const float& Value);
template bool FSettingsManager::UpdateSettingValue<bool>(const FString& SettingName, const bool& Value);

TSharedPtr<FSettingsManager> FSyncFrameworkManager::GetSettingsManager() const
{
    return SettingsManager;
}

// Initialize 메서드에 설정 관리자와 네트워크 관리자 연결 로직 추가
// 네트워크 관리자와 설정 관리자 초기화 후 아래 코드 실행

// 설정 관리자에 네트워크 기능 제공
if (NetworkManager.IsValid() && SettingsManager.IsValid())
{
    // 원래 구현에 추가
    FNetworkManager* NetworkManagerImpl = static_cast<FNetworkManager*>(NetworkManager.Get());

    // 설정 브로드캐스트 메서드 오버라이드
    SettingsManager->BroadcastSettings = [this]()
        {
            if (NetworkManager.IsValid() && SettingsManager.IsValid())
            {
                FNetworkManager* NetMgr = static_cast<FNetworkManager*>(NetworkManager.Get());
                TArray<uint8> SettingsData = SettingsManager->SerializeSettings();
                NetMgr->BroadcastSettingsMessage(SettingsData, ENetworkMessageType::SettingsSync);
            }
        };

    // 설정 요청 메서드 오버라이드
    SettingsManager->RequestSettingsFromMaster = [this]()
        {
            if (NetworkManager.IsValid() && SettingsManager.IsValid())
            {
                FNetworkManager* NetMgr = static_cast<FNetworkManager*>(NetworkManager.Get());

                // 요청 메시지 생성
                FString RequestId = FString::Printf(TEXT("%s:%lld"),
                    *FPlatformProcess::ComputerName(),
                    FDateTime::Now().ToUnixTimestamp());

                // RequestId를 바이트 배열로 변환
                TArray<uint8> RequestData;
                RequestData.SetNum(RequestId.Len() * sizeof(TCHAR));
                FMemory::Memcpy(RequestData.GetData(), *RequestId, RequestId.Len() * sizeof(TCHAR));

                // 마스터 서버가 있는 경우 해당 서버에 직접 요청
                FString MasterId = NetMgr->GetMasterId();
                if (!MasterId.IsEmpty())
                {
                    NetMgr->SendSettingsToServer(MasterId, RequestData, ENetworkMessageType::SettingsRequest);
                }
                else
                {
                    // 마스터를 모르는 경우 브로드캐스트
                    NetMgr->BroadcastSettingsMessage(RequestData, ENetworkMessageType::SettingsRequest);
                }
            }
        };
}

void FSettingsManager::UpdateSettingsSyncStatus()
{
    if (!bIsInitialized)
    {
        return;
    }

    double CurrentTime = FPlatformTime::Seconds();

    // 슬레이브 노드의 설정 업데이트 요청 관리
    if (bSettingsUpdatePending)
    {
        // 설정 업데이트 요청 후 일정 시간이 지나면 재시도
        if (CurrentTime - LastSettingsUpdateTime > 2.0) // 2초 타임아웃
        {
            if (SettingsUpdateAttempts < 3) // 최대 3번 재시도
            {
                UE_LOG(LogMultiServerSync, Display, TEXT("Settings update request timed out, retrying (attempt %d)"),
                    SettingsUpdateAttempts + 1);

                // 설정 요청 재시도
                RequestSettingsFromMaster();
                SettingsUpdateAttempts++;
            }
            else
            {
                // 최대 재시도 횟수 초과
                UE_LOG(LogMultiServerSync, Warning, TEXT("Settings update request failed after %d attempts"),
                    SettingsUpdateAttempts);

                bSettingsUpdatePending = false;
                SettingsUpdateAttempts = 0;
            }
        }
    }

    // 마스터 노드의 주기적인 설정 브로드캐스트
    // NetworkManager를 통해 마스터 상태 확인
    // FNetworkManager* NetworkManagerImpl = ...;
    // if (NetworkManagerImpl && NetworkManagerImpl->IsMaster())
    // {
    //     if (CurrentTime - LastSettingsBroadcastTime > SettingsBroadcastInterval)
    //     {
    //         // 주기적으로 설정 브로드캐스트
    //         BroadcastSettings();
    //         LastSettingsBroadcastTime = CurrentTime;
    //     }
    // }
}