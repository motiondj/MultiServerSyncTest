// Copyright Your Company. All Rights Reserved.

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
};