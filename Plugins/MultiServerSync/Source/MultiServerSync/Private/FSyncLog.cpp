// Copyright Your Company. All Rights Reserved.

#include "FSyncLog.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"

// Define log category
DEFINE_LOG_CATEGORY(LogMultiServerSync);

// Static member initialization
ELogVerbosity::Type FSyncLog::CurrentLogLevel = ELogVerbosity::Verbose;
bool FSyncLog::bFileLoggingEnabled = false;
FString FSyncLog::LogFilePath = FPaths::ProjectLogDir() + TEXT("MultiServerSync.log");

void FSyncLog::Initialize()
{
    // Create log directory if it doesn't exist
    if (bFileLoggingEnabled)
    {
        FString LogDir = FPaths::GetPath(LogFilePath);
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

        if (!PlatformFile.DirectoryExists(*LogDir))
        {
            PlatformFile.CreateDirectoryTree(*LogDir);
        }

        // Clear existing log file
        FFileHelper::SaveStringToFile(TEXT("=== Multi-Server Sync Framework Log ===\n"), *LogFilePath);
    }

    UE_LOG(LogMultiServerSync, Log, TEXT("Multi-Server Sync logging initialized"));
}

void FSyncLog::Shutdown()
{
    UE_LOG(LogMultiServerSync, Log, TEXT("Multi-Server Sync logging shutdown"));
}

void FSyncLog::Verbose(const FString& Message)
{
    LogMessage(ELogVerbosity::Verbose, Message);
}

void FSyncLog::Debug(const FString& Message)
{
    LogMessage(ELogVerbosity::Log, Message);
}

void FSyncLog::Info(const FString& Message)
{
    LogMessage(ELogVerbosity::Display, Message);
}

void FSyncLog::Warning(const FString& Message)
{
    LogMessage(ELogVerbosity::Warning, Message);
}

void FSyncLog::Error(const FString& Message)
{
    LogMessage(ELogVerbosity::Error, Message);
}

void FSyncLog::Fatal(const FString& Message)
{
    LogMessage(ELogVerbosity::Fatal, Message);
}

void FSyncLog::SetLogLevel(ELogVerbosity::Type Level)
{
    CurrentLogLevel = Level;
}

ELogVerbosity::Type FSyncLog::GetLogLevel()
{
    return CurrentLogLevel;
}

void FSyncLog::SetFileLogging(bool bEnable)
{
    bFileLoggingEnabled = bEnable;

    if (bEnable)
    {
        // Create log directory if it doesn't exist
        FString LogDir = FPaths::GetPath(LogFilePath);
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

        if (!PlatformFile.DirectoryExists(*LogDir))
        {
            PlatformFile.CreateDirectoryTree(*LogDir);
        }
    }
}

bool FSyncLog::IsFileLoggingEnabled()
{
    return bFileLoggingEnabled;
}

void FSyncLog::SetLogFilePath(const FString& FilePath)
{
    LogFilePath = FilePath;
}

FString FSyncLog::GetLogFilePath()
{
    return LogFilePath;
}

void FSyncLog::LogMessage(ELogVerbosity::Type Level, const FString& Message)
{
    // Skip logging if below current log level
    if (Level < CurrentLogLevel)
    {
        return;
    }

    // Log to Unreal log system
    switch (Level)
    {
    case ELogVerbosity::Verbose:
        UE_LOG(LogMultiServerSync, Verbose, TEXT("%s"), *Message);
        break;
    case ELogVerbosity::Log:
        UE_LOG(LogMultiServerSync, Log, TEXT("%s"), *Message);
        break;
    case ELogVerbosity::Display:
        UE_LOG(LogMultiServerSync, Display, TEXT("%s"), *Message);
        break;
    case ELogVerbosity::Warning:
        UE_LOG(LogMultiServerSync, Warning, TEXT("%s"), *Message);
        break;
    case ELogVerbosity::Error:
        UE_LOG(LogMultiServerSync, Error, TEXT("%s"), *Message);
        break;
    case ELogVerbosity::Fatal:
        UE_LOG(LogMultiServerSync, Fatal, TEXT("%s"), *Message);
        break;
    default:
        UE_LOG(LogMultiServerSync, Log, TEXT("%s"), *Message);
        break;
    }

    // Write to file if enabled
    if (bFileLoggingEnabled)
    {
        WriteToFile(Message);
    }
}

void FSyncLog::WriteToFile(const FString& Message)
{
    if (!bFileLoggingEnabled || LogFilePath.IsEmpty())
    {
        return;
    }

    // Get current timestamp
    FDateTime Now = FDateTime::Now();
    FString Timestamp = Now.ToString(TEXT("%Y-%m-%d %H:%M:%S.%s"));

    // Format log entry
    FString LogEntry = FString::Printf(TEXT("[%s] %s\n"), *Timestamp, *Message);

    // Append to log file
    FFileHelper::SaveStringToFile(LogEntry, *LogFilePath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), EFileWrite::FILEWRITE_Append);
}