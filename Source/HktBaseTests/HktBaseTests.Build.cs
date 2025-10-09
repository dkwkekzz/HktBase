// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HktBaseTests : ModuleRules
{
    public HktBaseTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "HktBase",
                "HktCustomNet"
                // ... add other public dependencies that you statically link with here ...
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
            }
        );
    }
}



