// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HktENet : ModuleRules
{
    public HktENet(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "ENet"
            }
        );
    }
}



