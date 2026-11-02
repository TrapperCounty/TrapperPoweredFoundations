#include "PFGenerateAssetsCommandlet.h"
#include "PFNetworkSubsystem.h"
#include "PFPowerConnection.h"
#include "PFGameWorldModule.h"
#include "PFFoundationMesh.h"
#include "Module/GameWorldModule.h"
#include "Buildables/FGBuildableFoundation.h"
#include "FGRecipe.h"
#include "FGColoredInstanceMeshProxy.h"
#include "FGSchematic.h"
#include "FGSchematicCategory.h"
#include "AvailabilityDependencies/FGAvailabilityDependency.h"
#include "Resources/FGBuildingDescriptor.h"
#include "Unlocks/FGUnlockRecipe.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/BoxComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"
#include "Engine/StaticMesh.h"
#include "InstanceData.h"
#include "StaticMeshAttributes.h"
#include "MeshDescription.h"

DEFINE_LOG_CATEGORY_STATIC(LogPFAssets, Log, All);

namespace
{
    UClass* Class(const FString& Path)
    {
        UClass* Result = LoadObject<UClass>(nullptr, *(Path + TEXT(".") + FPackageName::GetShortName(Path) + TEXT("_C")));
        checkf(Result, TEXT("Required class missing: %s"), *Path);
        return Result;
    }

    template<typename T> T& Value(UObject* Object, const FName Name)
    {
        FProperty* Property = Object->GetClass()->FindPropertyByName(Name);
        checkf(Property, TEXT("Missing property: %s on %s"), *Name.ToString(), *Object->GetClass()->GetName());
        checkf(Property->GetElementSize() == sizeof(T), TEXT("Unexpected property size: %s"), *Name.ToString());
        return *Property->ContainerPtrToValuePtr<T>(Object);
    }

    void Boolean(UObject* Object, const FName Name, bool Enabled)
    {
        FBoolProperty* Property = FindFProperty<FBoolProperty>(Object->GetClass(), Name);
        check(Property);
        Property->SetPropertyValue_InContainer(Object, Enabled);
    }

    UBlueprint* Blueprint(const FString& Path, UClass* Parent)
    {
        if (auto* Existing = LoadObject<UBlueprint>(nullptr, *(Path + TEXT(".") + FPackageName::GetShortName(Path)), nullptr, LOAD_NoWarn))
        {
            if (Existing->ParentClass != Parent)
            {
                check(Parent->IsChildOf(Existing->ParentClass));
                Existing->ParentClass = Parent;
                FBlueprintEditorUtils::RefreshAllNodes(Existing);
                FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Existing);
                FKismetEditorUtilities::CompileBlueprint(Existing);
            }
            return Existing;
        }
        UPackage* Package = CreatePackage(*Path);
        auto* Result = FKismetEditorUtilities::CreateBlueprint(Parent, Package, *FPackageName::GetShortName(Path), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), TEXT("TrapperPoweredFoundations"));
        check(Result);
        FAssetRegistryModule::AssetCreated(Result);
        return Result;
    }

    bool Save(UObject* Asset)
    {
        if (auto* BP = Cast<UBlueprint>(Asset))
        {
            FKismetEditorUtilities::CompileBlueprint(BP);
            if (BP->Status == BS_Error) return false;
            if (BP->GeneratedClass->IsChildOf(AFGBuildableFoundationLightweight::StaticClass()))
            {
                // The public editor stubs create this obsolete native subobject
                // even for lightweight foundations; the shipped game does not.
                // Strip it from our cooked children to avoid a missing archetype.
                if (auto* Ghost = Cast<UActorComponent>(BP->GeneratedClass->GetDefaultObject()->GetDefaultSubobjectByName(TEXT("BuildingMeshProxy"))))
                    Ghost->bIsEditorOnly = true;
            }
        }
        UPackage* Package = Asset->GetOutermost();
        Package->MarkPackageDirty();
        const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        const bool Success = UPackage::SavePackage(Package, Asset, *Filename, Args);
        UE_LOG(LogPFAssets, Display, TEXT("%s %s"), Success ? TEXT("Saved") : TEXT("FAILED"), *Filename);
        return Success;
    }

    UMaterial* RimMaterial()
    {
        const TCHAR* Path = TEXT("/TrapperPoweredFoundations/Materials/M_PoweredRim");
        if (auto* Existing = LoadObject<UMaterial>(nullptr, TEXT("/TrapperPoweredFoundations/Materials/M_PoweredRim.M_PoweredRim"), nullptr, LOAD_NoWarn)) return Existing;
        auto* Material = NewObject<UMaterial>(CreatePackage(Path), TEXT("M_PoweredRim"), RF_Public | RF_Standalone);
        Material->SetShadingModel(MSM_Unlit);
        Material->BlendMode = BLEND_Masked;
        Material->TwoSided = false;
        auto* UV = NewObject<UMaterialExpressionTextureCoordinate>(Material);
        auto* Mask = NewObject<UMaterialExpressionCustom>(Material);
        Mask->Code = TEXT("float e=max(abs(UV.x-0.5),abs(UV.y-0.5)); return step(0.46,e)*(1-step(0.49,e));");
        Mask->OutputType = CMOT_Float1;
        FCustomInput Input;
        Input.InputName = TEXT("UV");
        Input.Input.Expression = UV;
        Mask->Inputs.Add(Input);
        auto* Color = NewObject<UMaterialExpressionVectorParameter>(Material);
        Color->ParameterName = TEXT("RimColor");
        Color->DefaultValue = FLinearColor(.22f, .065f, .006f);
        Material->GetExpressionCollection().AddExpression(UV);
        Material->GetExpressionCollection().AddExpression(Mask);
        Material->GetExpressionCollection().AddExpression(Color);
        Material->GetEditorOnlyData()->EmissiveColor.Expression = Color;
        Material->GetEditorOnlyData()->OpacityMask.Expression = Mask;
        Material->PostEditChange();
        FAssetRegistryModule::AssetCreated(Material);
        return Material;
    }
}

#include "PFCornerAssets.inl"

UPFGenerateAssetsCommandlet::UPFGenerateAssetsCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

int32 UPFGenerateAssetsCommandlet::Main(const FString& Params)
{
    if (Params.Contains(TEXT("InspectCustomization")))
    {
        for (const TCHAR* Finish : {TEXT("Default"), TEXT("Concrete"), TEXT("PolishedConcrete"), TEXT("Asphalt"), TEXT("GripMetal")})
        {
            auto* Material = Class(FString(TEXT("/Game/FactoryGame/Buildable/-Shared/Customization/Materials/MaterialDesc_Foundation_")) + Finish)->GetDefaultObject<UFGFactoryCustomizationDescriptor_Material>();
            for (int32 Height : {1, 2, 4})
            {
                UClass* Base = Class(FString::Printf(TEXT("/Game/FactoryGame/Buildable/Building/Foundation/Build_Foundation_8x%d_01"), Height));
                const auto* Recipe = Material->GetBuildableMap().Find(Base);
                UE_LOG(LogPFAssets, Display, TEXT("CUSTOM %s %dm recipe=%s"), Finish, Height, Recipe ? *Recipe->Get()->GetPathName() : TEXT("NONE"));
                if (Recipe)
                {
                    auto Products = Recipe->Get()->GetDefaultObject<UFGRecipe>()->GetProducts();
                    auto* Target = Value<TSubclassOf<AFGBuildable>>(Products[0].ItemClass->GetDefaultObject(), TEXT("mBuildableClass")).Get();
                    UE_LOG(LogPFAssets, Display, TEXT("CUSTOM target=%s"), *Target->GetPathName());
                    auto* Data = Value<TObjectPtr<UAbstractInstanceDataObject>>(Target->GetDefaultObject(), TEXT("mInstanceDataCDO")).Get();
                    if (Data) for (const auto& D : Data->GetInstanceData()) UE_LOG(LogPFAssets, Display, TEXT("CUSTOM mesh=%s transform=%s"), *GetPathNameSafe(D.StaticMesh), *D.RelativeTransform.ToString());
                }
            }
        }
        return 0;
    }
    if (Params.Contains(TEXT("InspectFoundationGeometry")))
    {
        for (int32 Height : {1, 2, 4})
        {
            const FString Size = FString::Printf(TEXT("8x%d_01"), Height);
            auto* CDO = Class(TEXT("/Game/FactoryGame/Buildable/Building/Foundation/Build_Foundation_") + Size)->GetDefaultObject();
            auto* Property = FindFProperty<FObjectPropertyBase>(CDO->GetClass(), TEXT("mInstanceDataCDO"));
            UObject* Data = Property ? Property->GetObjectPropertyValue_InContainer(CDO) : nullptr;
            UE_LOG(LogPFAssets, Display, TEXT("GEOMETRY %dm instance data %s"), Height, *GetPathNameSafe(Data));
            if (Data) for (TFieldIterator<FProperty> It(Data->GetClass()); It; ++It)
            {
                FString Text;
                It->ExportText_InContainer(0, Text, Data, nullptr, Data, PPF_None);
                UE_LOG(LogPFAssets, Display, TEXT("GEOMETRY %s = %s"), *It->GetName(), *Text);
            }
            const FString Path = TEXT("/Game/FactoryGame/Buildable/Building/Foundation/FicsitSet/SM_Foundation_FicsitSet_") + Size;
            auto* Mesh = LoadObject<UStaticMesh>(nullptr, *(Path + TEXT(".") + FPackageName::GetShortName(Path)));
            if (Mesh) UE_LOG(LogPFAssets, Display, TEXT("GEOMETRY %dm mesh bounds %s"), Height, *Mesh->GetBoundingBox().ToString());
        }
        return 0;
    }
    UMaterial* Rim = CornerMaterial();
    UStaticMesh* Lights = CornerMesh(Rim);
    bool Success = Save(Rim) && Save(Lights);
    UClass* Wire = Class(TEXT("/Game/FactoryGame/Resource/Parts/Wire/Desc_Wire"));
    TArray<TSubclassOf<UFGRecipe>> Recipes;
    const int32 Heights[] = {1, 2, 4};
    const int32 ExtraWire[] = {2, 6, 12};
    TArray<FPFMaterialMapping> Mappings;
    TArray<TSubclassOf<AFGBuildable>> Builds;
    TArray<TSubclassOf<UFGFactoryCustomizationDescriptor_Material>> Finishes;
    const TCHAR* FinishNames[] = {TEXT("Default"), TEXT("Concrete"), TEXT("PolishedConcrete"), TEXT("Asphalt"), TEXT("GripMetal")};
    for (int32 FinishIndex = 0; FinishIndex < 5; ++FinishIndex)
    {
    auto* MaterialClass = Class(FString(TEXT("/Game/FactoryGame/Buildable/-Shared/Customization/Materials/MaterialDesc_Foundation_")) + FinishNames[FinishIndex]);
    Finishes.Add(MaterialClass);
    for (int32 I = 0; I < 3; ++I)
    {
        const FString Size = FString::Printf(TEXT("8x%d_01"), Heights[I]);
        const FString Folder = TEXT("/Game/FactoryGame/Buildable/Building/Foundation/");
        UClass* VanillaBuild = Class(Folder + TEXT("Build_Foundation_") + Size);
        UClass* VanillaRecipe = Class(TEXT("/Game/FactoryGame/Recipes/Buildings/Foundations/Recipe_Foundation_") + Size);
        UClass* VanillaDesc = Class(Folder + TEXT("Desc_Foundation_") + Size);
        if (FinishIndex != 0)
        {
            const auto& Map = MaterialClass->GetDefaultObject<UFGFactoryCustomizationDescriptor_Material>()->GetBuildableMap();
            VanillaRecipe = Map.FindChecked(VanillaBuild).Get();
            VanillaDesc = VanillaRecipe->GetDefaultObject<UFGRecipe>()->GetProducts()[0].ItemClass;
            VanillaBuild = Value<TSubclassOf<AFGBuildable>>(VanillaDesc->GetDefaultObject(), TEXT("mBuildableClass"));
        }
        const FString Suffix = FString::Printf(TEXT("%dm"), Heights[I]) + (FinishIndex ? FString(TEXT("_")) + FinishNames[FinishIndex] : TEXT(""));
        const FString Name = FinishIndex ? TEXT("Powered ") + Value<FText>(VanillaBuild->GetDefaultObject(), TEXT("mDisplayName")).ToString() : FString::Printf(TEXT("Powered Foundation (%dm)"), Heights[I]);
        UBlueprint* Build = Blueprint(TEXT("/TrapperPoweredFoundations/Foundations/Build_PoweredFoundation_") + Suffix, VanillaBuild);
        USCS_Node* Node = Build->SimpleConstructionScript->FindSCSNode(TEXT("PoweredFloorConnection"));
        if (!Node)
        {
            Node = Build->SimpleConstructionScript->CreateNode(UPFPowerConnection::StaticClass(), TEXT("PoweredFloorConnection"));
            Build->SimpleConstructionScript->AddNode(Node);
        }
        auto* Connection = CastChecked<UPFPowerConnection>(Node->ComponentTemplate);
        Connection->IndicatorMaterial = Rim;
        Connection->IndicatorMesh = Lights;
        check(Connection->IndicatorMesh);
        USCS_Node* MeshNode = Build->SimpleConstructionScript->FindSCSNode(TEXT("PoweredFloorMesh"));
        if (!MeshNode)
        {
            MeshNode = Build->SimpleConstructionScript->CreateNode(UPFFoundationMesh::StaticClass(), TEXT("PoweredFloorMesh"));
            Build->SimpleConstructionScript->AddNode(MeshNode);
        }
        auto* FloorMesh = CastChecked<UFGColoredInstanceMeshProxy>(MeshNode->ComponentTemplate);
        FloorMesh->mBlockInstancing = true;
        // Foundation actors snap around their center, but the FICSIT mesh
        // pivot is at its bottom. Preserve the vanilla instance transform so
        // the visible surface agrees with both snapping and our solid box.
        auto* VanillaCDO = Build->ParentClass->GetDefaultObject();
        auto* DataProperty = FindFProperty<FObjectPropertyBase>(VanillaCDO->GetClass(), TEXT("mInstanceDataCDO"));
        auto* VanillaData = Cast<UAbstractInstanceDataObject>(DataProperty->GetObjectPropertyValue_InContainer(VanillaCDO));
        check(VanillaData);
        const auto Instances = VanillaData->GetInstanceData();
        const auto* VanillaMesh = Instances.FindByPredicate([](const FInstanceData& D) { return D.StaticMesh != nullptr; });
        check(VanillaMesh);
        FloorMesh->SetStaticMesh(VanillaMesh->StaticMesh);
        FloorMesh->EmptyOverrideMaterials();
        for (int32 M = 0; M < VanillaMesh->OverridenMaterials.Num(); ++M)
            FloorMesh->SetMaterial(M, VanillaMesh->OverridenMaterials[M]);
        FloorMesh->SetNumCustomDataFloats(VanillaMesh->NumCustomDataFloats);
        FloorMesh->SetRelativeTransform(VanillaMesh->RelativeTransform);
        FloorMesh->SetMobility(EComponentMobility::Static);
        FloorMesh->SetCollisionProfileName(TEXT("NoCollision"));
        FloorMesh->SetGenerateOverlapEvents(false);
        FloorMesh->SetCanEverAffectNavigation(false);
        USCS_Node* CollisionNode = Build->SimpleConstructionScript->FindSCSNode(TEXT("PoweredFloorCollision"));
        if (!CollisionNode)
        {
            CollisionNode = Build->SimpleConstructionScript->CreateNode(UBoxComponent::StaticClass(), TEXT("PoweredFloorCollision"));
            Build->SimpleConstructionScript->AddNode(CollisionNode);
        }
        auto* Collision = CastChecked<UBoxComponent>(CollisionNode->ComponentTemplate);
        Collision->SetBoxExtent(FVector(400, 400, Heights[I] * 50.f));
        Collision->SetCollisionProfileName(TEXT("BlockAll"));
        Collision->SetGenerateOverlapEvents(false);
        Collision->SetMobility(EComponentMobility::Static);
        Collision->SetCanEverAffectNavigation(true);
        // Actor-free vanilla meshes are not instantiated when lightweight
        // management is disabled. Keep an explicit colored mesh proxy instead.
        Boolean(Build->GeneratedClass->GetDefaultObject(), TEXT("mCanContainLightweightInstances"), false);
        Boolean(Build->GeneratedClass->GetDefaultObject(), TEXT("mManagedByLightweightBuildableSubsystem"), false);
        FKismetEditorUtilities::CompileBlueprint(Build);
        auto* BuildCDO = Build->GeneratedClass->GetDefaultObject<AFGBuildableFoundation>();
        // The base game's foundation class normally discards the actor entirely.
        // Retain this actor and its saved/replicated connection component.
        Boolean(BuildCDO, TEXT("mManagedByLightweightBuildableSubsystem"), false);
        BuildCDO->SetReplicates(true);
        Value<FText>(BuildCDO, TEXT("mDisplayName")) = FText::FromString(Name);

        Builds.Add(Build->GeneratedClass.Get());
        UBlueprint* Desc = Blueprint(TEXT("/TrapperPoweredFoundations/Foundations/Desc_PoweredFoundation_") + Suffix, VanillaDesc);
        UObject* DescCDO = Desc->GeneratedClass->GetDefaultObject();
        Value<TSubclassOf<AFGBuildable>>(DescCDO, TEXT("mBuildableClass")) = Build->GeneratedClass;
        Value<FText>(DescCDO, TEXT("mDisplayName")) = FText::FromString(Name);
        Value<FText>(DescCDO, TEXT("mDescription")) = FText::FromString(TEXT("Carries power through touching side edges. Supply it through a power pole or wall outlet. Machines resting on the floor connect automatically. Corner lights are green with power and red without power. Supports the standard foundation finishes in the Customizer."));

        UBlueprint* Recipe = Blueprint(TEXT("/TrapperPoweredFoundations/Recipes/Recipe_PoweredFoundation_") + Suffix, VanillaRecipe);
        UObject* RecipeCDO = Recipe->GeneratedClass->GetDefaultObject();
        // Always start from the parent so regeneration cannot add wire twice.
        TArray<FItemAmount> Ingredients = Recipe->ParentClass->GetDefaultObject<UFGRecipe>()->GetIngredients();
        FItemAmount WireCost;
        WireCost.ItemClass = Wire;
        WireCost.Amount = ExtraWire[I];
        Ingredients.Add(WireCost);
        Value<TArray<FItemAmount>>(RecipeCDO, TEXT("mIngredients")) = Ingredients;
        FItemAmount Product;
        Product.ItemClass = Desc->GeneratedClass;
        Product.Amount = 1;
        Value<TArray<FItemAmount>>(RecipeCDO, TEXT("mProduct")) = {Product};
        Boolean(RecipeCDO, TEXT("mDisplayNameOverride"), true);
        Value<FText>(RecipeCDO, TEXT("mDisplayName")) = FText::FromString(Name);
        Recipes.Add(TSubclassOf<UFGRecipe>(Recipe->GeneratedClass.Get()));
        Success &= Save(Build) && Save(Desc) && Save(Recipe);
        UE_LOG(LogPFAssets, Display, TEXT("Validated %s: dimensions %.0fx%.0fx%.0f, %d ingredients, +%d Wire"), *Name, BuildCDO->mWidth, BuildCDO->mDepth, BuildCDO->mHeight, Ingredients.Num(), ExtraWire[I]);
    }
    }
    for (int32 Target = 0; Target < 5; ++Target)
        for (int32 Source = 0; Source < 5; ++Source)
            if (Target != Source)
                for (int32 HeightIndex = 0; HeightIndex < 3; ++HeightIndex)
                {
                    FPFMaterialMapping Mapping;
                    Mapping.Material = Finishes[Target]; Mapping.Source = Builds[Source * 3 + HeightIndex]; Mapping.Recipe = Recipes[Target * 3 + HeightIndex];
                    Mappings.Add(Mapping);
                }

    UBlueprint* Shop = Blueprint(TEXT("/TrapperPoweredFoundations/Schematics/Schematic_PoweredFoundations"), Class(TEXT("/Game/FactoryGame/Schematics/ResourceSink/ResourceSink_FoundationExpansionPack")));
    UObject* ShopCDO = Shop->GeneratedClass->GetDefaultObject();
    Value<ESchematicType>(ShopCDO, TEXT("mType")) = ESchematicType::EST_ResourceSink;
    Value<FText>(ShopCDO, TEXT("mDisplayName")) = FText::FromString(TEXT("Powered Foundations"));
    Value<FText>(ShopCDO, TEXT("mDescription")) = FText::FromString(TEXT("Unlocks 1m, 2m and 4m powered foundations. Connect adjacent tiles through their side edges, supply them through standard power poles or wall outlets, and power machines without visible cables."));
    Value<TSubclassOf<UFGSchematicCategory>>(ShopCDO, TEXT("mSchematicCategory")) = Class(TEXT("/Game/FactoryGame/Schematics/ResourceSinkShopCategories/SC_RSS_Foundations"));
    Value<TArray<TObjectPtr<UFGAvailabilityDependency>>>(ShopCDO, TEXT("mSchematicDependencies")).Reset();
    FItemAmount Cost;
    Cost.ItemClass = Class(TEXT("/Game/FactoryGame/Resource/Parts/ResourceSinkCoupon/Desc_ResourceSinkCoupon"));
    Cost.Amount = 25;
    Value<TArray<FItemAmount>>(ShopCDO, TEXT("mCost")) = {Cost};
    Value<float>(ShopCDO, TEXT("mTimeToComplete")) = 0;
    auto* Unlock = NewObject<UFGUnlockRecipe>(ShopCDO, Class(TEXT("/Game/FactoryGame/Unlocks/BP_UnlockRecipe")), NAME_None, RF_Transactional);
    Unlock->Init(Recipes);
    Value<TArray<TObjectPtr<UFGUnlock>>>(ShopCDO, TEXT("mUnlocks")) = {Unlock};
    Success &= Save(Shop);

    UBlueprint* Module = Blueprint(TEXT("/TrapperPoweredFoundations/RootGameWorld_PoweredFoundations"), UPFGameWorldModule::StaticClass());
    auto* ModuleCDO = Module->GeneratedClass->GetDefaultObject<UPFGameWorldModule>();
    ModuleCDO->MaterialMappings = Mappings;
    ModuleCDO->bRootModule = true;
    ModuleCDO->mSchematics = {TSubclassOf<UFGSchematic>(Shop->GeneratedClass.Get())};
    ModuleCDO->ModSubsystems = {APFNetworkSubsystem::StaticClass()};
    Success &= Save(Module);
    UE_LOG(LogPFAssets, Display, TEXT("Powered Foundations content generation %s."), Success ? TEXT("succeeded") : TEXT("FAILED"));
    return Success ? 0 : 1;
}
