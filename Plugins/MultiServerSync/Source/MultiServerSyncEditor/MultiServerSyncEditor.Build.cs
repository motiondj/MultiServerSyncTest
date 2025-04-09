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
                // 공용 헤더 경로 추가
            }
        );

        PrivateIncludePaths.AddRange(
            new string[] {
                // MultiServerSync 모듈의 Public 경로를 명시적으로 포함
                Path.Combine(ModuleDirectory, "../MultiServerSync/Public"),
            }
        );

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "MultiServerSync" // 런타임 모듈에 대한 의존성
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
                "Networking",
                "Sockets"
            }
        );

        bLegacyPublicIncludePaths = false;
        
        // 디버그 빌드에서만 최적화를 비활성화
        if (Target.Configuration == UnrealTargetConfiguration.Debug)
        {
            OptimizeCode = CodeOptimization.Never;
        }
    }
}