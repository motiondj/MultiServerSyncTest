// Copyright Your Company. All Rights Reserved.

#include "MultiServerSync.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FMultiServerSyncModule"

/**
 * Module implementation details
 */
class FMultiServerSyncModuleImpl
{
public:
	FMultiServerSyncModuleImpl()
	{
		UE_LOG(LogTemp, Log, TEXT("MultiServerSync: Module implementation created"));
	}

	~FMultiServerSyncModuleImpl()
	{
		UE_LOG(LogTemp, Log, TEXT("MultiServerSync: Module implementation destroyed"));
	}

	// Initialize the module
	void Initialize()
	{
		UE_LOG(LogTemp, Log, TEXT("MultiServerSync: Module initialization"));
	}

	// Shutdown the module
	void Shutdown()
	{
		UE_LOG(LogTemp, Log, TEXT("MultiServerSync: Module shutdown"));
	}
};

void FMultiServerSyncModule::StartupModule()
{
	// Create and initialize module implementation
	ModuleImpl = new FMultiServerSyncModuleImpl();
	ModuleImpl->Initialize();

	UE_LOG(LogTemp, Log, TEXT("MultiServerSync: Module started"));
}

void FMultiServerSyncModule::ShutdownModule()
{
	if (ModuleImpl)
	{
		ModuleImpl->Shutdown();
		delete ModuleImpl;
		ModuleImpl = nullptr;
	}

	UE_LOG(LogTemp, Log, TEXT("MultiServerSync: Module stopped"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMultiServerSyncModule, MultiServerSync)