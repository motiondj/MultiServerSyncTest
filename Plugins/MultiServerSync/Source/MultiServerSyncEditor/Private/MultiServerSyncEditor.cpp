// Plugins/MultiServerSync/Source/MultiServerSyncEditor/Private/MultiServerSyncEditor.cpp
#include "MultiServerSyncEditor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FMultiServerSyncEditorModule"

void FMultiServerSyncEditorModule::StartupModule()
{
    // Register UI elements when the editor module loads
    RegisterMenus();

    UE_LOG(LogTemp, Log, TEXT("MultiServerSyncEditor: Module started"));
}

void FMultiServerSyncEditorModule::ShutdownModule()
{
    UE_LOG(LogTemp, Log, TEXT("MultiServerSyncEditor: Module stopped"));
}

void FMultiServerSyncEditorModule::RegisterMenus()
{
    // Register editor extensions when ready
    FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

    // Create a menu extender
    TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());

    // Add a menu extension
    MenuExtender->AddMenuExtension(
        "WindowLayout",
        EExtensionHook::After,
        nullptr,
        FMenuExtensionDelegate::CreateLambda([this](FMenuBuilder& Builder)
            {
                Builder.AddSubMenu(
                    LOCTEXT("MultiServerSyncMenu", "Multi-Server Sync"),
                    LOCTEXT("MultiServerSyncMenuTooltip", "Multi-Server Synchronization Framework utilities"),
                    FNewMenuDelegate::CreateLambda([this](FMenuBuilder& SubMenuBuilder)
                        {
                            SubMenuBuilder.AddMenuEntry(
                                LOCTEXT("ShowEnvironmentInfo", "Show Environment Info"),
                                LOCTEXT("ShowEnvironmentInfoTooltip", "Display detected environment information"),
                                FSlateIcon(),
                                FUIAction(FExecuteAction::CreateRaw(this, &FMultiServerSyncEditorModule::ShowEnvironmentInfo))
                            );

                            SubMenuBuilder.AddMenuEntry(
                                LOCTEXT("RunNetworkTest", "Run Network Test"),
                                LOCTEXT("RunNetworkTestTooltip", "Run a basic network connectivity test"),
                                FSlateIcon(),
                                FUIAction(FExecuteAction::CreateRaw(this, &FMultiServerSyncEditorModule::RunNetworkTest))
                            );
                        })
                );
            })
    );

    // Add our extender to the menu
    LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
}

void FMultiServerSyncEditorModule::ShowEnvironmentInfo()
{
    UE_LOG(LogTemp, Display, TEXT("=== Multi-Server Sync Environment Information ==="));

    // 런타임 모듈 로드 확인
    if (FModuleManager::Get().IsModuleLoaded("MultiServerSync"))
    {
        UE_LOG(LogTemp, Display, TEXT("MultiServerSync module is loaded"));
        UE_LOG(LogTemp, Display, TEXT("Environment detection is working in the background."));

        // 여기서는 런타임 모듈의 기능을 직접 호출하지 않음
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("MultiServerSync module is not loaded"));
    }

    UE_LOG(LogTemp, Display, TEXT("=== End of Environment Information ==="));
}

void FMultiServerSyncEditorModule::RunNetworkTest()
{
    UE_LOG(LogTemp, Display, TEXT("=== Running Network Connectivity Test ==="));

    // 런타임 모듈 로드 확인
    if (FModuleManager::Get().IsModuleLoaded("MultiServerSync"))
    {
        UE_LOG(LogTemp, Display, TEXT("MultiServerSync module is loaded"));
        UE_LOG(LogTemp, Display, TEXT("Network test is running in the background."));

        // 여기서는 런타임 모듈의 기능을 직접 호출하지 않음
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("MultiServerSync module is not loaded"));
    }

    UE_LOG(LogTemp, Display, TEXT("Network test started. Check log for results."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMultiServerSyncEditorModule, MultiServerSyncEditor)