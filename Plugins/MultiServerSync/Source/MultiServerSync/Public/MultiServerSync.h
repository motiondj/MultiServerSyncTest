// Plugins/MultiServerSync/Source/MultiServerSync/Public/MultiServerSync.h
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// 전방 선언
class ISyncFrameworkManager;

/**
 * Multi-Server Synchronization Framework Module
 * Provides precise frame synchronization for Unreal Engine projects running across multiple servers
 */
class FMultiServerSyncModule : public IModuleInterface
{
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    /** Singleton accessor */
    static FMultiServerSyncModule& Get()
    {
        return FModuleManager::LoadModuleChecked<FMultiServerSyncModule>("MultiServerSync");
    }

    /** Check if the module is loaded and ready */
    static bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("MultiServerSync");
    }

    /** Get the framework manager */
    static TSharedPtr<ISyncFrameworkManager> GetFrameworkManager();

private:
    /** Framework manager instance */
    static TSharedPtr<ISyncFrameworkManager> FrameworkManager;
};