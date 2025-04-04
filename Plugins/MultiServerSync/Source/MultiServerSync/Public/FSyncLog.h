// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

// Define log category
DECLARE_LOG_CATEGORY_EXTERN(LogMultiServerSync, Log, All);

/**
 * Logging utility for Multi-Server Sync Framework
 * Provides convenient logging functions with severity levels
 */
class FSyncLog
{
public:
    /** Initialize the logging system */
    static void Initialize();

    /** Shutdown the logging system */
    static void Shutdown();

    /** Log a message with verbose level */
    static void Verbose(const FString& Message);

    /** Log a message with debug level */
    static void Debug(const FString& Message);

    /** Log a message with info level */
    static void Info(const FString& Message);

    /** Log a message with warning level */
    static void Warning(const FString& Message);

    /** Log a message with error level */
    static void Error(const FString& Message);

    /** Log a message with fatal level */
    static void Fatal(const FString& Message);

    /** Set the minimum log level */
    static void SetLogLevel(ELogVerbosity::Type Level);

    /** Get the current log level */
    static ELogVerbosity::Type GetLogLevel();

    /** Enable or disable file logging */
    static void SetFileLogging(bool bEnable);

    /** Check if file logging is enabled */
    static bool IsFileLoggingEnabled();

    /** Set the log file path */
    static void SetLogFilePath(const FString& FilePath);

    /** Get the log file path */
    static FString GetLogFilePath();

private:
    /** Current log level */
    static ELogVerbosity::Type CurrentLogLevel;

    /** File logging enabled flag */
    static bool bFileLoggingEnabled;

    /** Log file path */
    static FString LogFilePath;

    /** Log a message with the specified level */
    static void LogMessage(ELogVerbosity::Type Level, const FString& Message);

    /** Write a message to the log file */
    static void WriteToFile(const FString& Message);
};

// Convenience macros for logging
#define MSYNC_LOG_VERBOSE(Message) FSyncLog::Verbose(Message)
#define MSYNC_LOG_DEBUG(Message) FSyncLog::Debug(Message)
#define MSYNC_LOG_INFO(Message) FSyncLog::Info(Message)
#define MSYNC_LOG_WARNING(Message) FSyncLog::Warning(Message)
#define MSYNC_LOG_ERROR(Message) FSyncLog::Error(Message)
#define MSYNC_LOG_FATAL(Message) FSyncLog::Fatal(Message)