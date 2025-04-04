// Plugins/MultiServerSync/Source/MultiServerSync/Private/MultiServerSync.cpp
#include "MultiServerSync.h"
#include "ISyncFrameworkManager.h"
#include "FSyncFrameworkManager.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FMultiServerSyncModule"

// Static member initialization
TSharedPtr<ISyncFrameworkManager> FMultiServerSyncModule::FrameworkManager = nullptr;

void FMultiServerSyncModule::StartupModule()
{
    // 프레임워크 매니저 생성
    FrameworkManager = MakeShared<FSyncFrameworkManager>();

    // 초기화
    if (FrameworkManager.IsValid())
    {
        FSyncFrameworkManager* Manager = static_cast<FSyncFrameworkManager*>(FrameworkManager.Get());
        Manager->Initialize();
    }

    UE_LOG(LogTemp, Log, TEXT("MultiServerSync: Module started"));
}

void FMultiServerSyncModule::ShutdownModule()
{
    // 프레임워크 매니저 종료
    if (FrameworkManager.IsValid())
    {
        FSyncFrameworkManager* Manager = static_cast<FSyncFrameworkManager*>(FrameworkManager.Get());
        Manager->Shutdown();
        FrameworkManager.Reset();
    }

    UE_LOG(LogTemp, Log, TEXT("MultiServerSync: Module stopped"));
}

TSharedPtr<ISyncFrameworkManager> FMultiServerSyncModule::GetFrameworkManager()
{
    return FrameworkManager;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMultiServerSyncModule, MultiServerSync)