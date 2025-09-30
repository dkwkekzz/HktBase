// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using System.Linq;

public class HktBase : ModuleRules
{
    public HktBase(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "gRPC"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject"
            }
        );

        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
            }
        );
        
        var GeneratedPath = Path.Combine(PluginDirectory, "Tools", "generated");
        PublicIncludePaths.Add(GeneratedPath);

        // For GrpcGeneratedHeaders.h
        var HeadersFile = Path.Combine(PluginDirectory, "Source", "HktBase", "Public", "HktGrpcHeaders.generated.h");
        var HeaderLines = "// This file is generated automatically. DO NOT EDIT.\n" +
            string.Join("\n", Directory.GetFiles(GeneratedPath, "*.h", SearchOption.TopDirectoryOnly)
            .Select(p => $"#include \"{Path.GetFileName(p)}\""));
        if (!File.Exists(HeadersFile) || File.ReadAllText(HeadersFile) != HeaderLines)
        {
            File.WriteAllText(HeadersFile, HeaderLines);
        }

        // For GrpcGeneratedSources.h
        var SourcesFile = Path.Combine(PluginDirectory, "Source", "HktBase", "Private", "HktGrpcSources.generated.h");
        var SourceLines = "// This file is generated automatically. DO NOT EDIT.\n" +
            string.Join("\n", Directory.GetFiles(GeneratedPath, "*.cc", SearchOption.TopDirectoryOnly)
            .Select(p => $"#include \"{Path.GetFileName(p)}\""));
        if (!File.Exists(SourcesFile) || File.ReadAllText(SourcesFile) != SourceLines)
        {
            File.WriteAllText(SourcesFile, SourceLines);
        }
    }
}



