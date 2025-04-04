// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModuleInterfaces.h"

class FSyncFrameworkManager
{
public:
    /** Constructor */
    FSyncFrameworkManager();

    /** Destructor */
    ~FSyncFrameworkManager();

    /** Initialize the manager and all subsystems */
    bool Initialize();

    /** Shutdown the manager and all subsystems */
    void Shutdown();

    /** Get the environment detector subsystem */
    TSharedPtr<IEnvironmentDetector> GetEnvironmentDetector() const;

    /** Get the network manager subsystem */
    TSharedPtr<INetworkManager> GetNetworkManager() const;

    /** Get the time synchronization subsystem */
    TSharedPtr<ITimeSync> GetTimeSync() const;

    /** Get the frame synchronization controller */
    TSharedPtr<IFrameSyncController> GetFrameSyncController() const;

    /** Get the singleton instance */
    static FSyncFrameworkManager& Get();

private:
    /** Singleton instance */
    static TUniquePtr<FSyncFrameworkManager> Instance;

    /** Environment detector subsystem */
    TSharedPtr<IEnvironmentDetector> EnvironmentDetector;

    /** Network manager subsystem */
    TSharedPtr<INetworkManager> NetworkManager;

    /** Time synchronization subsystem */
    TSharedPtr<ITimeSync> TimeSync;

    /** Frame synchronization controller */
    TSharedPtr<IFrameSyncController> FrameSyncController;

    /** Indicates if the manager has been initialized */
    bool bIsInitialized;
};