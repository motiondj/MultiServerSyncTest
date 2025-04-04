﻿// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

/**
 * Common interface for all environment detection systems
 */
class IEnvironmentDetector
{
public:
    virtual ~IEnvironmentDetector() {}

    // Initialize the detector
    virtual bool Initialize() = 0;

    // Shutdown the detector
    virtual void Shutdown() = 0;

    // Check if a specific feature is available in the environment
    virtual bool IsFeatureAvailable(const FString& FeatureName) = 0;

    // Get information about a specific feature
    virtual TMap<FString, FString> GetFeatureInfo(const FString& FeatureName) = 0;
};

/**
 * Common interface for network communication systems
 */
class INetworkManager
{
public:
    virtual ~INetworkManager() {}

    // Initialize the network system
    virtual bool Initialize() = 0;

    // Shutdown the network system
    virtual void Shutdown() = 0;

    // Send a message to a specific endpoint
    virtual bool SendMessage(const FString& EndpointId, const TArray<uint8>& Message) = 0;

    // Broadcast a message to all endpoints
    virtual bool BroadcastMessage(const TArray<uint8>& Message) = 0;

    // Register a message handler callback
    virtual void RegisterMessageHandler(TFunction<void(const FString&, const TArray<uint8>&)> Handler) = 0;

    // 서버 탐색 메서드 추가 (override 키워드 없이)
    virtual bool DiscoverServers() = 0;
};

/**
 * Common interface for time synchronization systems
 */
class ITimeSync
{
public:
    virtual ~ITimeSync() {}

    // Initialize the time synchronization
    virtual bool Initialize() = 0;

    // Shutdown the time synchronization
    virtual void Shutdown() = 0;

    // Get current synchronized time in microseconds
    virtual int64 GetSyncedTimeMicroseconds() = 0;

    // Get estimated synchronization error in microseconds
    virtual int64 GetEstimatedErrorMicroseconds() = 0;

    // Check if time synchronization is currently active
    virtual bool IsSynchronized() = 0;
};

/**
 * Common interface for frame synchronization controllers
 */
class IFrameSyncController
{
public:
    virtual ~IFrameSyncController() {}

    // Initialize the frame synchronization
    virtual bool Initialize() = 0;

    // Shutdown the frame synchronization
    virtual void Shutdown() = 0;

    // Get current frame number in the synchronized sequence
    virtual int64 GetSyncedFrameNumber() = 0;

    // Check if frame synchronization is currently active
    virtual bool IsSynchronized() = 0;

    // Set the target frame rate for synchronization
    virtual void SetTargetFrameRate(float FramesPerSecond) = 0;
};