using UnrealBuildTool;

public class TrapperPoweredFoundations : ModuleRules
{
    public TrapperPoweredFoundations(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        PublicDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "FactoryGame", "SML", "DummyHeaders",
            "InputCore", "PhysicsCore", "NetCore", "GameplayTags", "GameplayTasks",
            "AIModule", "NavigationSystem", "UMG", "SlateCore", "Slate",
            "AssetRegistry", "AbstractInstance", "DeveloperSettings", "Json", "JsonUtilities"
        });
    }
}
