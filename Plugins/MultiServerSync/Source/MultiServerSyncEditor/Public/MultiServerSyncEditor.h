// MultiServerSyncEditor.h 수정
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * Multi-Server Synchronization Framework Editor Module
 * Provides editor integration for the synchronization framework
 */
class FMultiServerSyncEditorModule : public IModuleInterface
{
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    /** Register editor UI extensions */
    void RegisterMenus();

    /** Show environment information */
    void ShowEnvironmentInfo();

    /** Run network connectivity test */
    void RunNetworkTest();
};