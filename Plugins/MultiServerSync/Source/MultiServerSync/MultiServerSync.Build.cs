// Copyright Your Company. All Rights Reserved.

using UnrealBuildTool;

public class MultiServerSync : ModuleRules
{
    public MultiServerSync(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
            new string[] {
				// ... add public include paths required here ...
			}
        );

        PrivateIncludePaths.AddRange(
            new string[] {
				// ... add private include paths required here ...
			}
        );

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "Sockets",
                "Networking"
				// ... add other public dependencies that you statically link with here ...
			}
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Projects",
                "InputCore",
                "Slate",
                "SlateCore",
				// ... add private dependencies that you statically link with here ...	
			}
        );

        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
				// ... add any modules that your module loads dynamically here ...
			}
        );
    }
}