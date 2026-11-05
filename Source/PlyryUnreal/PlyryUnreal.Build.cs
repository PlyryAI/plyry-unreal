using UnrealBuildTool;

public class PlyryUnreal : ModuleRules
{
    public PlyryUnreal(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UMG", "SlateCore" });
        PrivateDependencyModuleNames.AddRange(new[] { "HTTP", "Json" });
    }
}