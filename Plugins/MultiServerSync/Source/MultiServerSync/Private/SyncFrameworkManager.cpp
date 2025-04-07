// SyncFrameworkManager.cpp
#include "SyncFrameworkManager.h"
#include "MultiServerSync.h"

ISyncFrameworkManager* FSyncFrameworkManager::Get()
{
    TSharedPtr<ISyncFrameworkManager> FrameworkManager = FMultiServerSyncModule::GetFrameworkManager();
    if (FrameworkManager.IsValid())
    {
        return FrameworkManager.Get();
    }
    return nullptr;
}

bool FSyncFrameworkManager::IsInitialized()
{
    ISyncFrameworkManager* FrameworkManager = Get();
    return FrameworkManager != nullptr;
}