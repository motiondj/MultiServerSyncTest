// Plugins/MultiServerSync/Source/MultiServerSync/Public/FSyncFrameworkManager.h
#pragma once

#include "CoreMinimal.h"
#include "ISyncFrameworkManager.h"

/**
 * Implementation of the synchronization framework manager
 * Manages all subsystems of the Multi-Server Sync Framework
 */
class MULTISERVERSYNC_API FSyncFrameworkManager : public ISyncFrameworkManager
{
public:
    /** Constructor */
    FSyncFrameworkManager();

    /** Destructor */
    virtual ~FSyncFrameworkManager();

    /** Initialize the manager and all subsystems */
    bool Initialize();

    /** Shutdown the manager and all subsystems */
    void Shutdown();

    // Begin ISyncFrameworkManager interface
    virtual TSharedPtr<IEnvironmentDetector> GetEnvironmentDetector() const override;
    virtual TSharedPtr<INetworkManager> GetNetworkManager() const override;
    virtual TSharedPtr<ITimeSync> GetTimeSync() const override;
    virtual TSharedPtr<IFrameSyncController> GetFrameSyncController() const override;
    // End ISyncFrameworkManager interface

private:
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