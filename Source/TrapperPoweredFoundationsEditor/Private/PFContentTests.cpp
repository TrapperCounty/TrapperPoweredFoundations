#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "UObject/UnrealType.h"
#include "PFPowerConnection.h"
#include "PFNetworkSubsystem.h"
#include "PFGameWorldModule.h"
#include "Buildables/FGBuildableFoundation.h"
#include "FGRecipe.h"
#include "FGColoredInstanceMeshProxy.h"
#include "Components/BoxComponent.h"
#include "FGSchematic.h"
#include "Resources/FGBuildingDescriptor.h"
#include "Unlocks/FGUnlockRecipe.h"
#include "Module/GameWorldModule.h"
#include "InstanceData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPFContentTest, "TrapperPoweredFoundations.Content.SavedAssets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPFContentTest::RunTest(const FString&)
{
    const int32 Heights[] = {1, 2, 4};
    const int32 Wires[] = {2, 6, 12};
    for (const TCHAR* Finish : {TEXT(""), TEXT("_Concrete"), TEXT("_PolishedConcrete"), TEXT("_Asphalt"), TEXT("_GripMetal")})
    {
    for (int32 I = 0; I < 3; ++I)
    {
        const FString BuildPath = FString::Printf(TEXT("/TrapperPoweredFoundations/Foundations/Build_PoweredFoundation_%dm%s.Build_PoweredFoundation_%dm%s"), Heights[I], Finish, Heights[I], Finish);
        auto* Build = LoadObject<UBlueprint>(nullptr, *BuildPath);
        if (!TestNotNull(TEXT("Foundation blueprint saved"), Build)) return false;
        auto* CDO = Build->GeneratedClass->GetDefaultObject<AFGBuildableFoundation>();
        TestEqual(TEXT("Correct foundation height"), CDO->mHeight, Heights[I] * 100.f);
        TestFalse(TEXT("Actor must retain its connection component"), FindFProperty<FBoolProperty>(CDO->GetClass(), TEXT("mManagedByLightweightBuildableSubsystem"))->GetPropertyValue_InContainer(CDO));
        TestTrue(TEXT("Foundation replicates"), CDO->GetIsReplicated());
        USCS_Node* MeshNode = Build->SimpleConstructionScript->FindSCSNode(TEXT("PoweredFloorMesh"));
        if (!TestNotNull(TEXT("Explicit floor mesh retained outside lightweight management"), MeshNode)) return false;
        auto* Mesh = Cast<UFGColoredInstanceMeshProxy>(MeshNode->ComponentTemplate);
        if (!TestNotNull(TEXT("Colored foundation mesh proxy"), Mesh)) return false;
        TestTrue(TEXT("Foundation mesh reference"), Mesh->GetStaticMesh() != nullptr);
        const auto* VanillaCDO = Build->ParentClass->GetDefaultObject();
        auto* DataProperty = FindFProperty<FObjectPropertyBase>(VanillaCDO->GetClass(), TEXT("mInstanceDataCDO"));
        auto* VanillaData = Cast<UAbstractInstanceDataObject>(DataProperty->GetObjectPropertyValue_InContainer(VanillaCDO));
        if (!TestNotNull(TEXT("Vanilla foundation mesh instance data"), VanillaData)) return false;
        const auto Instances = VanillaData->GetInstanceData();
        const auto* VanillaMesh = Instances.FindByPredicate([&](const FInstanceData& D) { return D.StaticMesh == Mesh->GetStaticMesh(); });
        if (!TestNotNull(TEXT("Powered mesh matches vanilla foundation mesh"), VanillaMesh)) return false;
        TestTrue(FString::Printf(TEXT("%dm rendered mesh transform matches vanilla, including vertical pivot offset"), Heights[I]), Mesh->GetRelativeTransform().Equals(VanillaMesh->RelativeTransform, .001));
        for (int32 M = 0; M < VanillaMesh->OverridenMaterials.Num(); ++M)
            TestTrue(TEXT("Finish-specific vanilla material override retained"), Mesh->GetMaterial(M) == VanillaMesh->OverridenMaterials[M]);
        USCS_Node* CollisionNode = Build->SimpleConstructionScript->FindSCSNode(TEXT("PoweredFloorCollision"));
        if (!TestNotNull(TEXT("Independent solid floor collider"), CollisionNode)) return false;
        auto* Collision = CastChecked<UBoxComponent>(CollisionNode->ComponentTemplate);
        TestEqual(TEXT("Solid floor collision"), Collision->GetCollisionProfileName(), FName(TEXT("BlockAll")));
        TestEqual(TEXT("Floor collider matches height"), Collision->GetUnscaledBoxExtent().Z, Heights[I] * 50.);
        USCS_Node* Node = Build->SimpleConstructionScript->FindSCSNode(TEXT("PoweredFloorConnection"));
        if (!TestNotNull(TEXT("Persistent power node"), Node)) return false;
        auto* Connection = Cast<UPFPowerConnection>(Node->ComponentTemplate);
        if (!TestNotNull(TEXT("Correct power component type"), Connection)) return false;
        TestNotNull(TEXT("Indicator material saved"), Connection->IndicatorMaterial.Get());
        TestNotNull(TEXT("Indicator mesh saved"), Connection->IndicatorMesh.Get());
        TestEqual(TEXT("Corner light mesh used for every finish"), Connection->IndicatorMesh->GetName(), FString(TEXT("SM_PowerCorners")));
        TestEqual(TEXT("Independent power-status material"), Connection->IndicatorMaterial->GetName(), FString(TEXT("M_PowerStatus")));
        const FString RecipePath = FString::Printf(TEXT("/TrapperPoweredFoundations/Recipes/Recipe_PoweredFoundation_%dm%s.Recipe_PoweredFoundation_%dm%s_C"), Heights[I], Finish, Heights[I], Finish);
        UClass* RecipeClass = LoadObject<UClass>(nullptr, *RecipePath);
        if (!TestNotNull(TEXT("Recipe class saved"), RecipeClass)) return false;
        const UFGRecipe* Recipe = RecipeClass->GetDefaultObject<UFGRecipe>();
        const auto& Ingredients = Recipe->GetIngredients();
        const auto& Vanilla = RecipeClass->GetSuperClass()->GetDefaultObject<UFGRecipe>()->GetIngredients();
        TestEqual(TEXT("Vanilla costs plus wire"), Ingredients.Num(), Vanilla.Num() + 1);
        for (int32 J = 0; J < Vanilla.Num(); ++J)
        {
            TestTrue(TEXT("Vanilla ingredient retained"), Ingredients[J].ItemClass == Vanilla[J].ItemClass);
            TestEqual(TEXT("Vanilla amount retained"), Ingredients[J].Amount, Vanilla[J].Amount);
        }
        TestEqual(TEXT("Wire surcharge"), Ingredients.Last().Amount, Wires[I]);
        TestEqual(TEXT("Wire ingredient"), Ingredients.Last().ItemClass->GetName(), FString(TEXT("Desc_Wire_C")));
        TestEqual(TEXT("Single output"), Recipe->GetProducts().Num(), 1);
        const UObject* Descriptor = Recipe->GetProducts()[0].ItemClass->GetDefaultObject();
        auto* BuildProperty = FindFProperty<FClassProperty>(Descriptor->GetClass(), TEXT("mBuildableClass"));
        TestTrue(TEXT("Recipe produces the powered buildable"), BuildProperty->GetObjectPropertyValue_InContainer(Descriptor) == Build->GeneratedClass.Get());
    }
    }
    auto* ModuleClass = LoadObject<UClass>(nullptr, TEXT("/TrapperPoweredFoundations/RootGameWorld_PoweredFoundations.RootGameWorld_PoweredFoundations_C"));
    if (!TestNotNull(TEXT("Root module saved"), ModuleClass)) return false;
    auto* Module = ModuleClass->GetDefaultObject<UPFGameWorldModule>();
    if (!TestNotNull(TEXT("Native powered Customizer module"), Module)) return false;
    TestEqual(TEXT("All five finishes map to the four other finishes at each height"), Module->MaterialMappings.Num(), 60);
    for (const auto& Mapping : Module->MaterialMappings)
    {
        TestTrue(TEXT("Customizer source belongs to this mod"), Mapping.Source && Mapping.Source->GetPathName().StartsWith(TEXT("/TrapperPoweredFoundations/")));
        auto* Product = Mapping.Recipe->GetDefaultObject<UFGRecipe>();
        auto* Descriptor = Product->GetProducts()[0].ItemClass->GetDefaultObject();
        auto* Destination = Cast<UClass>(FindFProperty<FClassProperty>(Descriptor->GetClass(), TEXT("mBuildableClass"))->GetObjectPropertyValue_InContainer(Descriptor));
        TestTrue(TEXT("Finish replacement remains a powered foundation"), Destination && Destination->GetPathName().StartsWith(TEXT("/TrapperPoweredFoundations/")));
        if (Destination) TestEqual(TEXT("Finish replacement preserves height"), Destination->GetDefaultObject<AFGBuildableFoundation>()->mHeight, Mapping.Source->GetDefaultObject<AFGBuildableFoundation>()->mHeight);
    }
    TestTrue(TEXT("Root discovery flag"), Module->bRootModule);
    TestTrue(TEXT("Server subsystem registered"), Module->ModSubsystems.Contains(APFNetworkSubsystem::StaticClass()));
    if (!TestEqual(TEXT("One shop unlock"), Module->mSchematics.Num(), 1)) return false;
    auto* Schematic = Module->mSchematics[0]->GetDefaultObject();
    auto* CostProperty = FindFProperty<FArrayProperty>(Schematic->GetClass(), TEXT("mCost"));
    const auto& Cost = *CostProperty->ContainerPtrToValuePtr<TArray<FItemAmount>>(Schematic);
    TestEqual(TEXT("Unlock cost is 25 coupons"), Cost[0].Amount, 25);
    TestEqual(TEXT("Cost uses coupons"), Cost[0].ItemClass->GetName(), FString(TEXT("Desc_ResourceSinkCoupon_C")));
    auto* UnlockProperty = FindFProperty<FArrayProperty>(Schematic->GetClass(), TEXT("mUnlocks"));
    const auto& Unlocks = *UnlockProperty->ContainerPtrToValuePtr<TArray<TObjectPtr<UFGUnlock>>>(Schematic);
    if (!TestEqual(TEXT("One recipe unlock object"), Unlocks.Num(), 1)) return false;
    auto* Unlock = Cast<UFGUnlockRecipe>(Unlocks[0]);
    if (!TestNotNull(TEXT("Recipe unlock type"), Unlock)) return false;
    TestEqual(TEXT("All three heights and five finishes unlocked"), Unlock->GetRecipesToUnlock().Num(), 15);
    return true;
}
#endif
