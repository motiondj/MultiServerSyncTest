// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModuleInterfaces.h"

/**
 * Frame synchronization controller class that implements the IFrameSyncController interface
 * Manages frame timing and synchronization between multiple servers
 */
class FFrameSyncController : public IFrameSyncController
{
public:
    /** Constructor */
    FFrameSyncController();

    /** Destructor */
    virtual ~FFrameSyncController();

    // Begin IFrameSyncController interface
    virtual bool Initialize() override;
    virtual void Shutdown() override;
    virtual int64 GetSyncedFrameNumber() override;
    virtual bool IsSynchronized() override;
    virtual void SetTargetFrameRate(float FramesPerSecond) override;
    // End IFrameSyncController interface

    /** Set master mode (true) or slave mode (false) */
    void SetMasterMode(bool bIsMaster);

    /** Check if operating in master mode */
    bool IsMasterMode() const;

    /** Handle engine tick */
    void HandleEngineTick(float DeltaTime);

    /** Update frame timing based on synchronized time */
    void UpdateFrameTiming();

    /** Send frame sync message to other servers */
    void SendFrameSyncMessage();

    /** Process a received frame sync message */
    void ProcessFrameSyncMessage(const TArray<uint8>& Message);

    /** Get the current frame timing adjustment in milliseconds */
    float GetFrameTimingAdjustmentMs() const;

private:
    /** Engine pre-render delegate handle */
    FDelegateHandle PreRenderDelegateHandle;

    /** Engine tick delegate handle */
    FDelegateHandle TickDelegateHandle;

    /** Current synced frame number */
    int64 SyncedFrameNumber;

    /** Target frame rate in frames per second */
    float TargetFrameRate;

    /** Frame timing adjustment in milliseconds */
    float FrameTimingAdjustmentMs;

    /** Is the frame controller operating in master mode */
    bool bIsMaster;

    /** Is the frame controller initialized */
    bool bIsInitialized;

    /** Is frame synchronization active */
    bool bIsSynchronized;

    /** Register engine callbacks */
    void RegisterEngineCallbacks();

    /** Unregister engine callbacks */
    void UnregisterEngineCallbacks();

    /** Handle pre-render event */
    void HandlePreRender();
};