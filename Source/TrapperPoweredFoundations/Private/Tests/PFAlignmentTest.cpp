// Opt-in regression for the visual/collision offset reported during real play.
#include "PFPowerConnection.h"
#include "PFFoundationMesh.h"
#include "FGCharacterPlayer.h"
#include "FGSaveSession.h"
#include "FGRecipe.h"
#include "FGBuildableSubsystem.h"
#include "FGLightweightBuildableSubsystem.h"
#include "Buildables/FGBuildableFoundation.h"
#include "Equipment/FGBuildGun.h"
#include "Hologram/FGBuildableHologram.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "InstanceData.h"
#include "UObject/UnrealType.h"
#include "TimerManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

void PFStartAlignmentTest(UWorld* World)
{
    FTimerHandle Timer;
    TWeakObjectPtr<UWorld> WeakWorld = World;
    World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([WeakWorld]
    {
        auto* W = WeakWorld.Get();
        if (!W) return;
        FString Report;
        int32 Failures = 0;
        auto Check = [&](bool OK, const FString& Label)
        {
            Report += FString::Printf(TEXT("%s: %s\n"), OK ? TEXT("PASS") : TEXT("FAIL"), *Label);
            if (!OK) ++Failures;
        };
        auto* PC = W->GetFirstPlayerController();
        auto* Player = PC ? Cast<AFGCharacterPlayer>(PC->GetPawn()) : nullptr;
        if (!Player || !Player->GetBuildGun()) return;
        UFGSaveSession::Get(W)->SetAutoSaveEnabled(false);
        auto* BS = AFGBuildableSubsystem::Get(W);
        auto* LW = AFGLightweightBuildableSubsystem::Get(W);
        int32 SavedFloors = 0;
        bool SavedFloorsAligned = true;
        for (auto* Existing : BS->GetAllBuildablesRef())
        {
            auto* Mesh = Existing->FindComponentByClass<UPFFoundationMesh>();
            if (!Mesh) continue;
            ++SavedFloors;
            auto* Parent = Existing->GetClass()->GetSuperClass()->GetDefaultObject();
            auto* Prop = FindFProperty<FObjectPropertyBase>(Parent->GetClass(), TEXT("mInstanceDataCDO"));
            auto* ParentData = Prop ? Cast<UAbstractInstanceDataObject>(Prop->GetObjectPropertyValue_InContainer(Parent)) : nullptr;
            bool Match = false;
            if (ParentData) for (const auto& D : ParentData->GetInstanceData())
                if (D.StaticMesh == Mesh->GetStaticMesh()) Match = Mesh->GetRelativeTransform().Equals(D.RelativeTransform, .01);
            SavedFloorsAligned &= Match;
        }
        Check(SavedFloors > 0 && SavedFloorsAligned, FString::Printf(TEXT("All %d previously saved powered foundation meshes use corrected vanilla transforms after load"), SavedFloors));
        for (int32 Height : {1, 2, 4})
        {
            const FString Label = FString::Printf(TEXT("%dm"), Height);
            const FString RecipePath = FString::Printf(TEXT("/TrapperPoweredFoundations/Recipes/Recipe_PoweredFoundation_%dm.Recipe_PoweredFoundation_%dm_C"), Height, Height);
            auto* Recipe = LoadObject<UClass>(nullptr, *RecipePath);
            const FTransform TM(FRotator(0, 37, 0), FVector(310000, Height * 10000, 50000));
            auto* H = Recipe ? Cast<AFGBuildableHologram>(AFGHologram::SpawnHologramFromRecipe(Recipe, Player->GetBuildGun(), TM.GetLocation(), Player)) : nullptr;
            Check(H != nullptr, Label + TEXT(" native powered hologram created"));
            if (!H) continue;
            H->SetActorTickEnabled(false);
            H->SetSuppressBuildEffect(true);
            H->SetActorTransform(TM);
            UClass* BuildClass = H->GetBuildClass();
            auto* VanillaCDO = BuildClass->GetSuperClass()->GetDefaultObject<AFGBuildableFoundation>();
            auto* DataProp = FindFProperty<FObjectPropertyBase>(VanillaCDO->GetClass(), TEXT("mInstanceDataCDO"));
            auto* Data = Cast<UAbstractInstanceDataObject>(DataProp->GetObjectPropertyValue_InContainer(VanillaCDO));
            const auto Instances = Data->GetInstanceData();
            Check(Instances.Num() == 1, Label + TEXT(" vanilla reference has one mesh"));
            if (Instances.Num() != 1) { H->Destroy(); continue; }
            const auto& Native = Instances[0];
            const FBox NativeBounds = Native.StaticMesh->GetBoundingBox().TransformBy(Native.RelativeTransform);
            TArray<UStaticMeshComponent*> PreviewMeshes;
            H->GetComponents(PreviewMeshes);
            int32 PreviewCount = 0;
            bool PreviewAligned = true;
            for (auto* Mesh : PreviewMeshes) if (Mesh->GetStaticMesh() == Native.StaticMesh)
            {
                ++PreviewCount;
                const FTransform Relative = Mesh->GetComponentTransform().GetRelativeTransform(H->GetActorTransform());
                PreviewAligned &= Relative.Equals(Native.RelativeTransform, .01);
            }
            Check(PreviewCount > 0 && PreviewAligned, Label + TEXT(" every foundation preview mesh matches vanilla transform"));
            TArray<AActor*> Children;
            auto* Built = Cast<AFGBuildableFoundation>(H->Construct(Children, FNetConstructionID()));
            H->Destroy();
            Check(Built != nullptr, Label + TEXT(" native construction succeeds"));
            if (!Built) continue;
            auto* Floor = Built->FindComponentByClass<UPFFoundationMesh>();
            Check(Floor && Floor->GetStaticMesh() == Native.StaticMesh && Floor->GetRelativeTransform().Equals(Native.RelativeTransform, .01), Label + TEXT(" constructed floor mesh matches vanilla offset"));
            if (Floor)
            {
                const FBox Rendered = Floor->GetStaticMesh()->GetBoundingBox().TransformBy(Floor->GetRelativeTransform());
                Check(Rendered.Min.Equals(NativeBounds.Min, .01) && Rendered.Max.Equals(NativeBounds.Max, .01), Label + TEXT(" actual shipped mesh bounds equal vanilla bounds"));
            }
            // Test the same collision channel used by a player's capsule at
            // the center and near every edge. No player movement is required.
            bool Walkable = true;
            for (FVector XY : {FVector(0,0,0), FVector(330,0,0), FVector(-330,0,0), FVector(0,330,0), FVector(0,-330,0)})
            {
                FHitResult Hit;
                const FVector Center = TM.TransformPosition(XY);
                FCollisionQueryParams Query(SCENE_QUERY_STAT(PFAlignment), false);
                const bool HitFloor = W->SweepSingleByChannel(Hit, Center + FVector(0,0,Height*50.+250.), Center + FVector(0,0,Height*50.-200.), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(30., 88.), Query);
                Walkable &= HitFloor && Hit.GetActor() == Built && FMath::Abs(Hit.Location.Z - 88. - (Center.Z + Height*50.)) < 1.;
            }
            Check(Walkable, Label + TEXT(" player capsule lands on correct surface at center and four edges"));
            // A genuine vanilla lightweight instance beside the powered one
            // verifies rendered alignment with the game's actual world format.
            FFactoryCustomizationData Custom = VanillaCDO->GetCustomizationData_Native();
            const FString VRPath = FString::Printf(TEXT("/Game/FactoryGame/Recipes/Buildings/Foundations/Recipe_Foundation_8x%d_01.Recipe_Foundation_8x%d_01_C"), Height, Height);
            auto* VR = LoadObject<UClass>(nullptr, *VRPath);
            FTransform VTM = TM;
            VTM.SetLocation(TM.TransformPosition(FVector(800,0,0)));
            FRuntimeBuildableInstanceData Runtime(VTM, Custom, VR, nullptr);
            const int32 Index = LW->AddFromBuildableInstanceData(VanillaCDO->GetClass(), Runtime);
            const auto* Saved = LW->GetRuntimeDataForBuildableClassAndIndex(VanillaCDO->GetClass(), Index);
            Check(Saved && Saved->IsValid(), Label + TEXT(" real vanilla lightweight neighbor created"));
            if (Saved && Floor)
            {
                const auto VB = Native.StaticMesh->GetBoundingBox().TransformBy(Native.RelativeTransform * Saved->Transform);
                const auto PB = Floor->GetStaticMesh()->GetBoundingBox().TransformBy(Floor->GetComponentTransform());
                Check(FMath::Abs(VB.Max.Z-PB.Max.Z)<.1 && FMath::Abs(VB.Min.Z-PB.Min.Z)<.1, Label + TEXT(" neighboring vanilla and powered rendered top/bottom are flush"));
            }
            if (Index != INDEX_NONE) LW->RemoveByInstanceIndex(VanillaCDO->GetClass(), Index);
            Built->TurnOffAndDestroy();
        }
        Report += FString::Printf(TEXT("Failures: %d\nTemporary fixtures removed. No inventory or player transform changes.\n"), Failures);
        FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir()/TEXT("TrapperPoweredFoundations-AlignmentTest.txt")));
        UE_LOG(LogTemp, Display, TEXT("PF alignment regression:\n%s"), *Report);
    }), 15.f, false);
}
