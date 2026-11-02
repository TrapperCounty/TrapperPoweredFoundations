// Opt-in tests of real foundation holograms in vertical zoop mode.
#include "PFNetworkSubsystem.h"
#include "PFPowerConnection.h"
#include "FGCharacterPlayer.h"
#include "FGPowerInfoComponent.h"
#include "FGBuildableSubsystem.h"
#include "Buildables/FGBuildable.h"
#include "Equipment/FGBuildGun.h"
#include "Hologram/FGFoundationHologram.h"
#include "TimerManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogPFVerticalZoop, Log, All);

namespace
{
    struct FPFVerticalZoopTest : TSharedFromThis<FPFVerticalZoopTest>
    {
        TWeakObjectPtr<UWorld> World;
        TArray<TArray<TWeakObjectPtr<AFGBuildable>>> Columns;
        TArray<FString> Labels;
        FString Report;
        int32 Failures = 0;

        void Check(bool OK, const FString& Label)
        {
            Report += FString::Printf(TEXT("%s: %s\n"), OK ? TEXT("PASS") : TEXT("FAIL"), *Label);
            UE_LOG(LogPFVerticalZoop, Display, TEXT("%s: %s"), OK ? TEXT("PASS") : TEXT("FAIL"), *Label);
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
            for (int32 Height : {1, 2, 4}) for (float Yaw : {0.f, 37.f}) for (int32 Direction : {1, -1})
            {
                const FString Label = FString::Printf(TEXT("%dm, yaw %.0f, %s"), Height, Yaw, Direction > 0 ? TEXT("up") : TEXT("down"));
                const FString RecipePath = FString::Printf(TEXT("/TrapperPoweredFoundations/Recipes/Recipe_PoweredFoundation_%dm.Recipe_PoweredFoundation_%dm_C"), Height, Height);
                UClass* Recipe = LoadObject<UClass>(nullptr, *RecipePath);
                const FTransform TM(FRotator(0, Yaw, 0), FVector(240000, Case++ * 10000, 50000));
                auto* H = Recipe ? Cast<AFGBuildableHologram>(AFGHologram::SpawnHologramFromRecipe(Recipe, Player->GetBuildGun(), TM.GetLocation(), Player)) : nullptr;
                if (!H) { Check(false, Label + TEXT(" recipe creates a foundation hologram")); continue; }
                H->SetActorTickEnabled(false);
                H->SetSuppressBuildEffect(true);
                H->SetActorTransform(TM);
                TArray<TSubclassOf<UFGBuildGunModeDescriptor>> Modes;
                H->GetSupportedBuildModes(Modes);
                TSubclassOf<UFGHologramBuildModeDescriptor> VerticalMode;
                for (auto Mode : Modes)
                    if (Mode && Mode->GetName().Contains(TEXT("Vertical"))) VerticalMode = *Mode;
                Check(VerticalMode != nullptr, Label + TEXT(" advertises vertical zoop mode"));
                if (!VerticalMode) { H->Destroy(); continue; }
                H->SetBuildModeOverride(VerticalMode);
                H->OnBuildModeChanged(VerticalMode);
                H->SetZoopAmount(FIntVector::ZeroValue);
                H->RegenerateZoopInstances();
                const auto Single = H->GetCost(false);
                H->SetZoopAmount(FIntVector(0, 0, Direction * 3));
                H->RegenerateZoopInstances();
                Check(H->IsInZoopBuildMode() && H->GetBaseCostMultiplier() == 4, Label + TEXT(" selects a four-tile vertical zoop"));
                const auto Cost = H->GetCost(false);
                bool CostsMatch = Cost.Num() == Single.Num() && !Single.IsEmpty();
                for (const auto& Ingredient : Single)
                {
                    const auto* Found = Cost.FindByPredicate([&](const FItemAmount& Item) { return Item.ItemClass == Ingredient.ItemClass; });
                    CostsMatch &= Found && Found->Amount == Ingredient.Amount * 4;
                }
                Check(CostsMatch, Label + TEXT(" charges four complete recipes including material costs"));
                TSet<AFGBuildable*> Before;
                for (auto* B : BS->GetAllBuildablesRef()) Before.Add(B);
                TArray<AActor*> Children;
                H->Construct(Children, FNetConstructionID());
                TArray<TWeakObjectPtr<AFGBuildable>> Column;
                for (auto* B : BS->GetAllBuildablesRef())
                    if (!Before.Contains(B) && B->GetClass() == H->GetBuildClass()) Column.Add(B);
                Check(Column.Num() == 4, Label + TEXT(" constructs four actual powered foundation actors"));
                bool Components = Column.Num() == 4;
                bool Alignment = Column.Num() == 4;
                TArray<double> Z;
                for (const auto& B : Column)
                {
                    auto* P = B->FindComponentByClass<UPFPowerConnection>();
                    Components &= P != nullptr && P->GetFName() == TEXT("PoweredFloorConnection");
                    const FVector Local = TM.InverseTransformPosition(B->GetActorLocation());
                    Alignment &= FMath::Abs(Local.X) < 1. && FMath::Abs(Local.Y) < 1. && B->GetActorQuat().Equals(TM.GetRotation(), .001);
                    Z.Add(Local.Z);
                }
                Check(Components, Label + TEXT(" retains all stable power components"));
                Z.Sort();
                for (int32 I = 0; I < Z.Num(); ++I)
                {
                    const double Expected = (Direction > 0 ? I : I - 3) * Height * 100.;
                    Alignment &= FMath::Abs(Z[I] - Expected) < 1.;
                }
                Check(Alignment, Label + TEXT(" stacks flush at the exact foundation height without horizontal drift"));
                if (!Alignment) for (double Offset : Z) UE_LOG(LogPFVerticalZoop, Display, TEXT("%s: local Z %.3f"), *Label, Offset);
                Columns.Add(Column);
                Labels.Add(Label);
                H->Destroy();
            }
            APFNetworkSubsystem::Get(W)->MarkDirty();
            FTimerHandle Timer;
            auto Self = AsShared();
            W->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([Self] { Self->Energize(); }), 2.f, false);
        }

        void Energize()
        {
            if (!World.IsValid()) return;
            // Wait for the subsystem's initial private-terminal repair, then
            // feed one tier through a separate source terminal. An isolated
            // production component alone does not allocate a native circuit.
            for (const auto& Column : Columns)
                if (!Column.IsEmpty() && Column[0].IsValid())
                    if (auto* P = Column[0]->FindComponentByClass<UPFPowerConnection>())
                    {
                        auto* Source = NewObject<UFGPowerConnectionComponent>(Column[0].Get());
                        Source->SetIsHidden(true);
                        Source->RegisterComponent();
                        auto* Info = NewObject<UFGPowerInfoComponent>(Column[0].Get());
                        Info->RegisterComponent();
                        Source->SetPowerInfo(Info);
                        Info->SetBaseProduction(1);
                        P->AddHiddenConnection(Source);
                    }
            FTimerHandle Timer;
            auto Self = AsShared();
            World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([Self] { Self->Finish(); }), 2.f, false);
        }

        void Finish()
        {
            for (int32 I = 0; I < Columns.Num(); ++I)
            {
                const auto& Column = Columns[I];
                bool Connected = Column.Num() == 4;
                for (int32 A = 0; A < Column.Num(); ++A)
                {
                    auto* P = Column[A].IsValid() ? Column[A]->FindComponentByClass<UPFPowerConnection>() : nullptr;
                    if (!P) { Connected = false; continue; }
                    UE_LOG(LogPFVerticalZoop, Display, TEXT("%s tier %d: circuit=%d powered=%d"), *Labels[I], A, P->GetCircuitID(), P->HasPower());
                    Connected &= P->HasPower() && P->GetCircuitID() != INDEX_NONE;
                    for (int32 B = A + 1; B < Column.Num(); ++B)
                    {
                        auto* Other = Column[B].IsValid() ? Column[B]->FindComponentByClass<UPFPowerConnection>() : nullptr;
                        Connected &= Other && P->GetCircuitID()==Other->GetCircuitID() && P->HasHiddenConnection(Other)==PFGeometry::ShareFace(P->GetTile(),Other->GetTile());
                    }
                }
                Check(Connected, Labels[I] + TEXT(" powers all stacked tiers through touching faces"));
            }
            for (const auto& Column : Columns) for (const auto& B : Column) if (B.IsValid()) B->TurnOffAndDestroy();
            Report += FString::Printf(TEXT("Failures: %d\n"), Failures);
            FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("TrapperPoweredFoundations-VerticalZoopTest.txt")));
            UE_LOG(LogPFVerticalZoop, Display, TEXT("Vertical zoop test complete: %d failures"), Failures);
        }
    };
}

void PFStartVerticalZoopTest(UWorld* World)
{
    auto Test = MakeShared<FPFVerticalZoopTest>();
    Test->World = World;
    FTimerHandle Timer;
    World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([Test] { Test->Run(); }), 15.f, false);
}
