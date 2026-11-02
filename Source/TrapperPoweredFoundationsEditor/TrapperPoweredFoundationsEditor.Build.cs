using UnrealBuildTool;
public class TrapperPoweredFoundationsEditor : ModuleRules
{
    public TrapperPoweredFoundationsEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UnrealEd", "Kismet", "KismetCompiler", "AssetRegistry", "FactoryGame", "AbstractInstance", "SML", "TrapperPoweredFoundations", "MeshDescription", "StaticMeshDescription" });
    }
}
