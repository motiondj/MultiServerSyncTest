// Plugins/MultiServerSync/Source/MultiServerSync/Public/ISyncFrameworkManager.h
#pragma once

#include "CoreMinimal.h"
#include "ModuleInterfaces.h"

/**
 * Interface for the synchronization framework manager
 * Provides access to all subsystems of the Multi-Server Sync Framework
 */
class MULTISERVERSYNC_API ISyncFrameworkManager
{
public:
    virtual ~ISyncFrameworkManager() {}

    /** Get the environment detector subsystem */
    virtual TSharedPtr<IEnvironmentDetector> GetEnvironmentDetector() const = 0;

    /** Get the network manager subsystem */
    virtual TSharedPtr<INetworkManager> GetNetworkManager() const = 0;

    /** Get the time synchronization subsystem */
    virtual TSharedPtr<ITimeSync> GetTimeSync() const = 0;

    /** Get the frame synchronization controller */
    virtual TSharedPtr<IFrameSyncController> GetFrameSyncController() const = 0;
};