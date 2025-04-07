#include "SyncFrameworkManager.h"
#include "MultiServerSync.h"

ISyncFrameworkManager* FSyncFrameworkManagerUtil::Get()
{
    TSharedPtr<ISyncFrameworkManager> FrameworkManager = FMultiServerSyncModule::GetFrameworkManager();
    if (FrameworkManager.IsValid())
    {
        return FrameworkManager.Get();
    }
    return nullptr;
}

bool FSyncFrameworkManagerUtil::IsInitialized()
{
    ISyncFrameworkManager* FrameworkManager = Get();
    return FrameworkManager != nullptr;
}