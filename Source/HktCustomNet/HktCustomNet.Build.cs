// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HktCustomNet : ModuleRules
{
    public HktCustomNet(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "Networking",
                "Sockets"
            }
        );
    }
}



