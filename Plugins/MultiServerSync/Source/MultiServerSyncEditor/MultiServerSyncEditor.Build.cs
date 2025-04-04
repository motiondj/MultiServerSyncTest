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

        // 모듈 바인딩 설정
        PrivateIncludePathModuleNames.AddRange(
            new string[]
            {
                "MultiServerSync",
            }
        );

        // 제거: DynamicallyLoadedModuleNames 설정 부분 제거
        // 이미 PublicDependencyModuleNames에 "MultiServerSync"가 있으므로 
        // 동적으로 추가할 필요가 없습니다.

        // 추가적인 설정 (필요 시)
        bLegacyPublicIncludePaths = false;
        OptimizeCode = CodeOptimization.Never; // 디버깅을 위한 최적화 없음
    }
}