// Copyright Your Company. All Rights Reserved.

#include "FSettingsManager.h"
#include "FSyncLog.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "JsonObjectConverter.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FSettingsManager::FSettingsManager()
    : LastSettingsUpdateTime(0.0)
    , bIsInitialized(false)
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

    // 기본 설정으로 초기화
    CurrentSettings = FProjectSettings();
    LastSettingsUpdateTime = FPlatformTime::Seconds();

    bIsInitialized = true;
    UE_LOG(LogMultiServerSync, Display, TEXT("Settings Manager initialized with default settings: %s"),
        *CurrentSettings.ToString());

    return true;
}

void FSettingsManager::Shutdown()
{
    UE_LOG(LogMultiServerSync, Display, TEXT("Shutting down Settings Manager"));

    // 리소스 정리 및 이벤트 핸들러 정리
    OnSettingsChangedEvent.Clear();

    bIsInitialized = false;
}

const FProjectSettings& FSettingsManager::GetSettings() const
{
    return CurrentSettings;
}

bool FSettingsManager::UpdateSettings(const FProjectSettings& NewSettings)
{
    if (!bIsInitialized)
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Cannot update settings: Settings Manager not initialized"));
        return false;
    }

    // 설정 유효성 검사
    if (!ValidateSettings(NewSettings))
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Settings validation failed, not updating"));
        return false;
    }

    // 이미 동일한 설정인지 확인
    if (CurrentSettings == NewSettings)
    {
        UE_LOG(LogMultiServerSync, Verbose, TEXT("Settings unchanged, skipping update"));
        return true;
    }

    // 설정 복사 및 버전 증가
    CurrentSettings = NewSettings;
    CurrentSettings.SettingsVersion++;
    LastSettingsUpdateTime = FPlatformTime::Seconds();

    UE_LOG(LogMultiServerSync, Display, TEXT("Settings updated to version %d: %s"),
        CurrentSettings.SettingsVersion, *CurrentSettings.ToString());

    // 설정 변경 알림
    NotifySettingsChanged();

    return true;
}

bool FSettingsManager::BroadcastSettings()
{
    if (!bIsInitialized)
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Cannot broadcast settings: Settings Manager not initialized"));
        return false;
    }

    UE_LOG(LogMultiServerSync, Display, TEXT("Broadcasting settings to network: %s"),
        *CurrentSettings.ToString());

    // 직렬화된 설정 준비
    TArray<uint8> SettingsData = CurrentSettings.ToBytes();

    // 설정 브로드캐스트는 NetworkManager를 통해 실행됨
    // 이 부분은 FSyncFrameworkManager에서 처리

    return true;
}

bool FSettingsManager::ProcessReceivedSettings(const TArray<uint8>& SettingsData)
{
    if (!bIsInitialized)
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Cannot process received settings: Settings Manager not initialized"));
        return false;
    }

    if (SettingsData.Num() == 0)
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Received empty settings data"));
        return false;
    }

    // 설정 역직렬화
    FProjectSettings ReceivedSettings;
    if (!ReceivedSettings.FromBytes(SettingsData))
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Failed to deserialize received settings"));
        return false;
    }

    // 버전 확인 - 받은 설정이 더 최신이면 적용
    if (ReceivedSettings.SettingsVersion > CurrentSettings.SettingsVersion)
    {
        UE_LOG(LogMultiServerSync, Display, TEXT("Received newer settings (v%d), updating from current (v%d)"),
            ReceivedSettings.SettingsVersion, CurrentSettings.SettingsVersion);

        CurrentSettings = ReceivedSettings;
        LastSettingsUpdateTime = FPlatformTime::Seconds();

        // 설정 변경 알림
        NotifySettingsChanged();

        return true;
    }
    else
    {
        UE_LOG(LogMultiServerSync, Verbose, TEXT("Received settings (v%d) are older than current (v%d), ignoring"),
            ReceivedSettings.SettingsVersion, CurrentSettings.SettingsVersion);
        return false;
    }
}

bool FSettingsManager::SaveSettingsToFile(const FString& FilePath)
{
    if (!bIsInitialized)
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Cannot save settings: Settings Manager not initialized"));
        return false;
    }

    // 설정을 JSON으로 변환
    TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();

    JsonObject->SetNumberField(TEXT("SettingsVersion"), CurrentSettings.SettingsVersion);
    JsonObject->SetStringField(TEXT("ProjectName"), CurrentSettings.ProjectName);

    JsonObject->SetBoolField(TEXT("EnableMasterSlaveProtocol"), CurrentSettings.bEnableMasterSlaveProtocol);
    JsonObject->SetNumberField(TEXT("MasterElectionInterval"), CurrentSettings.MasterElectionInterval);
    JsonObject->SetNumberField(TEXT("MasterAnnouncementInterval"), CurrentSettings.MasterAnnouncementInterval);

    JsonObject->SetBoolField(TEXT("EnableTimeSync"), CurrentSettings.bEnableTimeSync);
    JsonObject->SetNumberField(TEXT("TimeSyncIntervalMs"), CurrentSettings.TimeSyncIntervalMs);
    JsonObject->SetNumberField(TEXT("MaxTimeOffsetToleranceMs"), CurrentSettings.MaxTimeOffsetToleranceMs);

    JsonObject->SetBoolField(TEXT("EnableFrameSync"), CurrentSettings.bEnableFrameSync);
    JsonObject->SetNumberField(TEXT("TargetFrameRate"), CurrentSettings.TargetFrameRate);
    JsonObject->SetNumberField(TEXT("MaxFrameDelayTolerance"), CurrentSettings.MaxFrameDelayTolerance);

    JsonObject->SetNumberField(TEXT("NetworkPort"), CurrentSettings.NetworkPort);
    JsonObject->SetBoolField(TEXT("EnableBroadcast"), CurrentSettings.bEnableBroadcast);
    JsonObject->SetStringField(TEXT("PreferredNetworkInterface"), CurrentSettings.PreferredNetworkInterface);

    // JSON 문자열로 직렬화
    FString JsonString;
    TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), JsonWriter);

    // 파일에 저장
    bool bSuccess = FFileHelper::SaveStringToFile(JsonString, *FilePath);

    if (bSuccess)
    {
        UE_LOG(LogMultiServerSync, Display, TEXT("Settings successfully saved to file: %s"), *FilePath);
    }
    else
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to save settings to file: %s"), *FilePath);
    }

    return bSuccess;
}

bool FSettingsManager::LoadSettingsFromFile(const FString& FilePath)
{
    if (!bIsInitialized)
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Cannot load settings: Settings Manager not initialized"));
        return false;
    }

    // 파일에서 JSON 문자열 로드
    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to load settings file: %s"), *FilePath);
        return false;
    }

    // JSON 파싱
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Failed to parse settings JSON from file: %s"), *FilePath);
        return false;
    }

    // JSON에서 설정 로드
    FProjectSettings LoadedSettings;

    // 필수 필드 확인
    if (!JsonObject->HasField(TEXT("SettingsVersion")) || !JsonObject->HasField(TEXT("ProjectName")))
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Settings file is missing required fields"));
        return false;
    }

    // 일반 설정 로드
    LoadedSettings.SettingsVersion = JsonObject->GetIntegerField(TEXT("SettingsVersion"));
    LoadedSettings.ProjectName = JsonObject->GetStringField(TEXT("ProjectName"));

    // 마스터-슬레이브 설정 로드
    if (JsonObject->HasField(TEXT("EnableMasterSlaveProtocol")))
    {
        LoadedSettings.bEnableMasterSlaveProtocol = JsonObject->GetBoolField(TEXT("EnableMasterSlaveProtocol"));
    }

    if (JsonObject->HasField(TEXT("MasterElectionInterval")))
    {
        LoadedSettings.MasterElectionInterval = JsonObject->GetNumberField(TEXT("MasterElectionInterval"));
    }

    if (JsonObject->HasField(TEXT("MasterAnnouncementInterval")))
    {
        LoadedSettings.MasterAnnouncementInterval = JsonObject->GetNumberField(TEXT("MasterAnnouncementInterval"));
    }

    // 시간 동기화 설정 로드
    if (JsonObject->HasField(TEXT("EnableTimeSync")))
    {
        LoadedSettings.bEnableTimeSync = JsonObject->GetBoolField(TEXT("EnableTimeSync"));
    }

    if (JsonObject->HasField(TEXT("TimeSyncIntervalMs")))
    {
        LoadedSettings.TimeSyncIntervalMs = JsonObject->GetIntegerField(TEXT("TimeSyncIntervalMs"));
    }

    if (JsonObject->HasField(TEXT("MaxTimeOffsetToleranceMs")))
    {
        LoadedSettings.MaxTimeOffsetToleranceMs = JsonObject->GetNumberField(TEXT("MaxTimeOffsetToleranceMs"));
    }

    // 프레임 동기화 설정 로드
    if (JsonObject->HasField(TEXT("EnableFrameSync")))
    {
        LoadedSettings.bEnableFrameSync = JsonObject->GetBoolField(TEXT("EnableFrameSync"));
    }

    if (JsonObject->HasField(TEXT("TargetFrameRate")))
    {
        LoadedSettings.TargetFrameRate = JsonObject->GetNumberField(TEXT("TargetFrameRate"));
    }

    if (JsonObject->HasField(TEXT("MaxFrameDelayTolerance")))
    {
        LoadedSettings.MaxFrameDelayTolerance = JsonObject->GetIntegerField(TEXT("MaxFrameDelayTolerance"));
    }

    // 네트워크 설정 로드
    if (JsonObject->HasField(TEXT("NetworkPort")))
    {
        LoadedSettings.NetworkPort = JsonObject->GetIntegerField(TEXT("NetworkPort"));
    }

    if (JsonObject->HasField(TEXT("EnableBroadcast")))
    {
        LoadedSettings.bEnableBroadcast = JsonObject->GetBoolField(TEXT("EnableBroadcast"));
    }

    if (JsonObject->HasField(TEXT("PreferredNetworkInterface")))
    {
        LoadedSettings.PreferredNetworkInterface = JsonObject->GetStringField(TEXT("PreferredNetworkInterface"));
    }

    // 유효성 검사 및 설정 업데이트
    if (!ValidateSettings(LoadedSettings))
    {
        UE_LOG(LogMultiServerSync, Error, TEXT("Loaded settings failed validation"));
        return false;
    }

    // 현재 설정 업데이트
    CurrentSettings = LoadedSettings;
    LastSettingsUpdateTime = FPlatformTime::Seconds();

    UE_LOG(LogMultiServerSync, Display, TEXT("Settings successfully loaded from file: %s"), *FilePath);

    // 설정 변경 알림
    NotifySettingsChanged();

    return true;
}

FDelegateHandle FSettingsManager::RegisterOnSettingsChanged(const FOnSettingsChanged::FDelegate& Delegate)
{
    return OnSettingsChangedEvent.Add(Delegate);
}

void FSettingsManager::UnregisterOnSettingsChanged(FDelegateHandle Handle)
{
    OnSettingsChangedEvent.Remove(Handle);
}

void FSettingsManager::NotifySettingsChanged()
{
    OnSettingsChangedEvent.Broadcast(CurrentSettings);
}

bool FSettingsManager::ValidateSettings(const FProjectSettings& Settings)
{
    // 포트 번호 유효성 검사
    if (Settings.NetworkPort <= 0 || Settings.NetworkPort > 65535)
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Invalid network port: %d"), Settings.NetworkPort);
        return false;
    }

    // 프레임 레이트 유효성 검사
    if (Settings.TargetFrameRate <= 0.0f || Settings.TargetFrameRate > 1000.0f)
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Invalid target frame rate: %.2f"), Settings.TargetFrameRate);
        return false;
    }

    // 시간 동기화 간격 유효성 검사
    if (Settings.TimeSyncIntervalMs <= 0)
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Invalid time sync interval: %d ms"), Settings.TimeSyncIntervalMs);
        return false;
    }

    // 간격 유효성 검사
    if (Settings.MasterElectionInterval <= 0.0f)
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Invalid master election interval: %.2f"), Settings.MasterElectionInterval);
        return false;
    }

    if (Settings.MasterAnnouncementInterval <= 0.0f)
    {
        UE_LOG(LogMultiServerSync, Warning, TEXT("Invalid master announcement interval: %.2f"), Settings.MasterAnnouncementInterval);
        return false;
    }

    // 기타 필요한 유효성 검사 추가

    return true;
}