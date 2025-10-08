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
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "HktCustomNet",
            }
        );
    }
}



