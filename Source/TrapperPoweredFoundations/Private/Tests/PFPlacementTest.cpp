// Opt-in packaged-game coverage of the actual vanilla hologram implementation.
#include "PFNetworkSubsystem.h"
#include "PFPowerConnection.h"
#include "PFGeometry.h"
#include "FGCharacterPlayer.h"
#include "FGRecipe.h"
#include "FGPowerInfoComponent.h"
#include "FGBuildableSubsystem.h"
#include "Buildables/FGBuildable.h"
#include "Equipment/FGBuildGun.h"
#include "Hologram/FGFoundationHologram.h"
#include "Components/PrimitiveComponent.h"
#include "TimerManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogPFPlacement, Log, All);

namespace
{
    struct FPFPlacementTest : TSharedFromThis<FPFPlacementTest>
    {
        TWeakObjectPtr<UWorld> World;
        TArray<TWeakObjectPtr<AFGBuildable>> Owned;
        TArray<TArray<TWeakObjectPtr<AFGBuildable>>> Rows;
        FString Report;
        int32 Failures = 0;

        void Check(bool OK, const FString& Label)
        {
            Report += FString::Printf(TEXT("%s: %s\n"), OK ? TEXT("PASS") : TEXT("FAIL"), *Label);
            UE_LOG(LogPFPlacement, Display, TEXT("%s: %s"), OK ? TEXT("PASS") : TEXT("FAIL"), *Label);
            if (!OK) ++Failures;
        }

        void Run()
        {
            auto* W = World.Get();
            if (!W) return;
            auto* PC = W->GetFirstPlayerController();
            auto* Player = PC ? Cast<AFGCharacterPlayer>(PC->GetPawn()) : nullptr;
            if (!Player || !Player->GetBuildGun()) { Check(false, TEXT("Player and build gun available")); Finish(); return; }
            auto* BS = AFGBuildableSubsystem::Get(W);
            int32 Case = 0;
            for (int32 Height : {1, 2, 4}) for (float Yaw : {0.f, 37.f})
            {
                const FString Label = FString::Printf(TEXT("%dm, yaw %.0f"), Height, Yaw);
                const FString RecipePath = FString::Printf(TEXT("/TrapperPoweredFoundations/Recipes/Recipe_PoweredFoundation_%dm.Recipe_PoweredFoundation_%dm_C"), Height, Height);
                UClass* Recipe = LoadObject<UClass>(nullptr, *RecipePath);
                auto* H = Recipe ? Cast<AFGBuildableHologram>(AFGHologram::SpawnHologramFromRecipe(Recipe, Player->GetBuildGun(), FVector(200000, Case * 15000, 50000), Player)) : nullptr;
                if (!H) { Check(false, Label + TEXT(" recipe creates a foundation hologram")); continue; }
                H->SetActorTickEnabled(false);
                H->SetSuppressBuildEffect(true);
                UClass* BuildClass = H->GetBuildClass();
                const FTransform AnchorTM(FRotator(0, Yaw, 0), FVector(200000, Case++ * 15000, 50000));
                auto* Anchor = BuildClass ? BS->BeginSpawnBuildable(BuildClass, AnchorTM) : nullptr;
                if (!Anchor) { Check(false, Label + TEXT(" anchor spawn")); H->Destroy(); continue; }
                Anchor->FinishSpawning(AnchorTM);
                if (!BS->GetAllBuildablesRef().Contains(Anchor)) BS->AddBuildable(Anchor);
                Owned.Add(Anchor);
                auto* Power = Anchor->FindComponentByClass<UPFPowerConnection>();
                if (!Power) { Check(false, Label + TEXT(" powered anchor component")); H->Destroy(); continue; }
                const auto AnchorTile = Power->GetTile();
                for (const FVector LocalSide : {FVector(1,0,0), FVector(-1,0,0), FVector(0,1,0), FVector(0,-1,0)})
                {
                    const FVector Normal = AnchorTM.TransformVectorNoScale(LocalSide);
                    const FVector Face = AnchorTM.TransformPosition(LocalSide * 400.);
                    FHitResult Hit;
                    FCollisionQueryParams Params(SCENE_QUERY_STAT(PFSnapTest), true, H);
                    const bool HitAnchor = W->LineTraceSingleByChannel(Hit, Face + Normal * 100., Face - Normal * 100., ECC_GameTraceChannel5, Params) && Hit.GetActor() == Anchor;
                    const FString Side = Label + TEXT(" side ") + LocalSide.ToString();
                    Check(HitAnchor, Side + TEXT(" is targetable by build-gun trace"));
                    if (!HitAnchor) continue;
                    H->PreHologramPlacement(Hit, true);
                    const bool Snapped = H->TrySnapToActor(Hit);
                    if (!Snapped) H->SetHologramLocationAndRotation(Hit);
                    H->PostHologramPlacement(Hit, true);
                    PFGeometry::FTile Tile = AnchorTile;
                    Tile.Transform = H->GetActorTransform();
                    const FVector Expected = AnchorTM.GetLocation() + Normal * 800.;
                    Check(Snapped && FVector::Dist(Tile.Transform.GetLocation(), Expected) < 1. && PFGeometry::ShareSide(AnchorTile, Tile), Side + TEXT(" snaps flush at 8m spacing"));
                    if (!Snapped || FVector::Dist(Tile.Transform.GetLocation(), Expected) >= 1.)
                        UE_LOG(LogPFPlacement, Display, TEXT("Expected %s, actual %s, snapped=%d"), *Expected.ToString(), *H->GetActorLocation().ToString(), Snapped);
                }

                TArray<TSubclassOf<UFGBuildGunModeDescriptor>> Modes;
                H->GetSupportedBuildModes(Modes);
                TSubclassOf<UFGHologramBuildModeDescriptor> ZoopMode;
                for (auto Mode : Modes)
                    if (Mode && Mode->GetName().Contains(TEXT("Zoop")) && !Mode->GetName().Contains(TEXT("Vertical"))) ZoopMode = *Mode;
                Check(ZoopMode != nullptr, Label + TEXT(" supports horizontal zoop mode"));
                if (ZoopMode)
                {
                    H->SetActorTransform(FTransform(AnchorTM.GetRotation(), AnchorTM.GetLocation() + FVector(0, 6000, 0)));
                    H->SetBuildModeOverride(ZoopMode);
                    H->OnBuildModeChanged(ZoopMode);
                    // Native material customization adds its own ingredients
                    // (FICSIT foundations add iron plates). Use the complete
                    // single-hologram cost as the scaling reference.
                    H->SetZoopAmount(FIntVector::ZeroValue);
                    H->RegenerateZoopInstances();
                    const auto Single = H->GetCost(false);
                    H->SetZoopAmount(FIntVector(3, 0, 0));
                    H->RegenerateZoopInstances();
                    Check(H->GetBaseCostMultiplier() == 4, Label + TEXT(" four-tile zoop cost multiplier"));
                    const auto Cost = H->GetCost(false);
                    bool CostsMatch = Cost.Num() == Single.Num();
                    for (const auto& Ingredient : Single)
                    {
                        const auto* Found = Cost.FindByPredicate([&](const FItemAmount& Item) { return Item.ItemClass == Ingredient.ItemClass; });
                        CostsMatch &= Found && Found->Amount == Ingredient.Amount * 4;
                    }
                    Check(CostsMatch, Label + TEXT(" zoop preview charges four complete recipes"));
                    if (!CostsMatch)
                    {
                        for (const auto& Item : Cost) UE_LOG(LogPFPlacement, Display, TEXT("Actual zoop cost %s: %s x%d"), *Label, *GetNameSafe(Item.ItemClass), Item.Amount);
                        for (const auto& Item : Single) UE_LOG(LogPFPlacement, Display, TEXT("Single recipe %s: %s x%d"), *Label, *GetNameSafe(Item.ItemClass), Item.Amount);
                    }
                    TSet<AFGBuildable*> Before;
                    for (auto* B : BS->GetAllBuildablesRef()) Before.Add(B);
                    TArray<AActor*> Children;
                    H->Construct(Children, FNetConstructionID());
                    TArray<TWeakObjectPtr<AFGBuildable>> Row;
                    for (auto* B : BS->GetAllBuildablesRef())
                        if (!Before.Contains(B) && B->GetClass() == BuildClass) { Owned.Add(B); Row.Add(B); }
                    Check(Row.Num() == 4, Label + TEXT(" native zoop constructs four powered actors"));
                    bool Components = Row.Num() == 4;
                    for (const auto& B : Row) Components &= B.IsValid() && B->FindComponentByClass<UPFPowerConnection>() != nullptr;
                    Check(Components, Label + TEXT(" every zooped actor retains its power component"));
                    bool Spacing = Row.Num() == 4;
                    TArray<double> Offsets;
                    for (const auto& B : Row) Offsets.Add(H->GetActorTransform().InverseTransformPosition(B->GetActorLocation()).X);
                    Offsets.Sort();
                    for (int32 I = 1; I < Offsets.Num(); ++I) Spacing &= FMath::Abs(Offsets[I] - Offsets[I-1] - 800.) < 1.;
                    Check(Spacing, Label + TEXT(" zooped tiles have exact 8m spacing"));
                    // A circuit of passive terminals may legitimately have no
                    // allocated power circuit. Supply one tile before checking
                    // native circuit IDs and propagation to its neighbors.
                    if (!Row.IsEmpty())
                        if (auto* P = Row[0]->FindComponentByClass<UPFPowerConnection>())
                        {
                            auto* Info = NewObject<UFGPowerInfoComponent>(Row[0].Get());
                            Info->RegisterComponent();
                            P->SetPowerInfo(Info);
                            Info->SetBaseProduction(1);
                        }
                    Rows.Add(Row);
                }
                H->Destroy();
            }
            APFNetworkSubsystem::Get(W)->MarkDirty();
            FTimerHandle Timer;
            auto Self = AsShared();
            W->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([Self] { Self->Finish(); }), 2.f, false);
        }

        void Finish()
        {
            for (int32 I = 0; I < Rows.Num(); ++I)
            {
                int32 Circuit = INDEX_NONE;
                bool Connected = Rows[I].Num() == 4;
                for (const auto& B : Rows[I])
                {
                    auto* P = B.IsValid() ? B->FindComponentByClass<UPFPowerConnection>() : nullptr;
                    if (!P) { Connected = false; continue; }
                    if (Circuit == INDEX_NONE) Circuit = P->GetCircuitID();
                    Connected &= Circuit != INDEX_NONE && Circuit == P->GetCircuitID() && P->HasPower();
                }
                Check(Connected, FString::Printf(TEXT("Zoop row %d forms one native power circuit"), I + 1));
            }
            for (const auto& B : Owned) if (B.IsValid()) B->TurnOffAndDestroy();
            Report += FString::Printf(TEXT("Failures: %d\n"), Failures);
            FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("TrapperPoweredFoundations-PlacementTest.txt")));
            UE_LOG(LogPFPlacement, Display, TEXT("Placement test complete: %d failures"), Failures);
        }
    };
}

void PFStartPlacementTest(UWorld* World)
{
    auto Test = MakeShared<FPFPlacementTest>();
    Test->World = World;
    FTimerHandle Timer;
    World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([Test] { Test->Run(); }), 15.f, false);
}
