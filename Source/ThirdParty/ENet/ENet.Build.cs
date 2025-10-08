using UnrealBuildTool;
using System.IO;

public class ENet : ModuleRules
{
    public ENet(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        string ThirdPartyPath = Path.Combine(ModuleDirectory);
        string LibPath = Path.Combine(ThirdPartyPath, "lib");

        PublicSystemIncludePaths.AddRange(
            new string[] {
                Path.Combine(ThirdPartyPath, "include"),
            }
        );

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string PlatformLibPath = Path.Combine(LibPath, "Win64", "Release");
            // 링크할 라이브러리 목록 (Release 빌드 기준)
            string[] gRPCLibraries = new string[]
            {
                "enet.lib"
            };

            foreach (string lib in gRPCLibraries)
            {
                PublicAdditionalLibraries.Add(Path.Combine(PlatformLibPath, lib));
            }
        }
    }
}
