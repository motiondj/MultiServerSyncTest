// Copyright Your Company. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

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

private:
	/** Module implementation details */
	class FMultiServerSyncModuleImpl* ModuleImpl;
};