// Copyright Your Company. All Rights Reserved.

#include "MultiServerSyncEditor.h"
#include "MultiServerSync.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "LevelEditor.h"

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
		FMenuExtensionDelegate::CreateLambda([](FMenuBuilder& Builder)
			{
				Builder.AddMenuEntry(
					LOCTEXT("MultiServerSyncMenu", "Multi-Server Sync"),
					LOCTEXT("MultiServerSyncMenuTooltip", "Open the Multi-Server Synchronization Framework settings"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([]()
						{
							// We'll implement this later
							UE_LOG(LogTemp, Log, TEXT("MultiServerSync: Menu item clicked"));
						}))
				);
			})
	);

	// Add our extender to the menu
	LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMultiServerSyncEditorModule, MultiServerSyncEditor)