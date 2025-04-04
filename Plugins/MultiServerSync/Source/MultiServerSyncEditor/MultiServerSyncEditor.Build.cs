// Copyright Your Company. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class MultiServerSyncEditor : ModuleRules
{
    public MultiServerSyncEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
            new string[] {
                // ... add public include paths required here ...
            }
        );

        PrivateIncludePaths.AddRange(
            new string[] {
                // MultiServerSync 모듈의 Private 경로 추가
                Path.Combine(ModuleDirectory, "../MultiServerSync/Private"),
                // ... add private include paths required here ...
            }
        );

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "MultiServerSync"
                // ... add other public dependencies that you statically link with here ...
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Projects",
                "InputCore",
                "UnrealEd",
                "LevelEditor",
                "Slate",
                "SlateCore",
                "EditorStyle",
                // ... add private dependencies that you statically link with here ...
            }
        );

        // 이 부분을 추가합니다
        PrivateIncludePathModuleNames.AddRange(
            new string[]
            {
                "MultiServerSync",
            }
        );

        // 모듈 바인딩 방식 설정 (런타임 바인딩 대신 컴파일타임에 링크)
        if (Target.LinkType == TargetLinkType.Monolithic)
        {
            PublicAdditionalLibraries.Add("MultiServerSync.lib");
        }

        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
                // ... add any modules that your module loads dynamically here ...
            }
        );
    }
}