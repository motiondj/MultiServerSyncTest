// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModuleInterfaces.h"

/**
 * Environment detector class that implements the IEnvironmentDetector interface
 * Responsible for discovering hardware and software capabilities in the environment
 */
class FEnvironmentDetector : public IEnvironmentDetector
{
public:
    /** Constructor */
    FEnvironmentDetector();

    /** Destructor */
    virtual ~FEnvironmentDetector();

    // Begin IEnvironmentDetector interface
    virtual bool Initialize() override;
    virtual void Shutdown() override;
    virtual bool IsFeatureAvailable(const FString& FeatureName) override;
    virtual TMap<FString, FString> GetFeatureInfo(const FString& FeatureName) override;
    // End IEnvironmentDetector interface

    /** Detect genlock hardware (Quadro Sync) */
    bool DetectGenlockHardware();

    /** Detect nDisplay module */
    bool DetectNDisplay();

    /** Scan network interfaces */
    bool ScanNetworkInterfaces();

    /** Get the list of available network interfaces */
    TArray<FString> GetNetworkInterfaces() const;

    /** Check if genlock hardware is available */
    bool HasGenlockHardware() const;

    /** Check if nDisplay module is available */
    bool HasNDisplay() const;

private:
    /** Available network interfaces */
    TArray<FString> NetworkInterfaces;

    /** Genlock hardware detected flag */
    bool bHasGenlockHardware;

    /** nDisplay module detected flag */
    bool bHasNDisplay;

    /** Is the detector initialized */
    bool bIsInitialized;
};