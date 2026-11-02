// Opt-in native mixed-height snapping and real dismantle/refund coverage.
#include "PFNetworkSubsystem.h"
#include "PFPowerConnection.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "FGPowerInfoComponent.h"
#include "FGDismantleInterface.h"
#include "FGBuildableSubsystem.h"
#include "Buildables/FGBuildable.h"
#include "Equipment/FGBuildGun.h"
#include "Equipment/FGBuildGunDismantle.h"
#include "Hologram/FGFoundationHologram.h"
#include "TimerManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogPFMixedRefund, Log, All);

namespace
{
    struct FPFRefundFixture
    {
        TWeakObjectPtr<AFGBuildable> Actor;
        TArray<FItemAmount> Cost;
        int32 Height = 0;
    };

    struct FPFMixedRefundTest : TSharedFromThis<FPFMixedRefundTest>
    {
        TWeakObjectPtr<UWorld> World;
        TWeakObjectPtr<AFGCharacterPlayer> Player;
        TArray<TWeakObjectPtr<AFGBuildable>> Owned;
        TArray<TPair<TWeakObjectPtr<AFGBuildable>, TWeakObjectPtr<AFGBuildable>>> Pairs;
        TArray<FString> PairLabels;
        TArray<FPFRefundFixture> Refunds;
        TMap<UClass*, int32> ExpectedRefund, InventoryBefore;
        FString Report;
        int32 Failures = 0;
        bool SentRefundRequest = false;

        void Check(bool OK, const FString& Label)
        {
            Report += FString::Printf(TEXT("%s: %s\n"), OK ? TEXT("PASS") : TEXT("FAIL"), *Label);
            UE_LOG(LogPFMixedRefund, Display, TEXT("%s: %s"), OK ? TEXT("PASS") : TEXT("FAIL"), *Label);
            if (!OK) ++Failures;
        }

        void Later(void (FPFMixedRefundTest::*Method)(), float Delay = 2.f)
        {
            FTimerHandle Timer;
            auto Self = AsShared();
            World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([Self, Method] { (Self.Get().*Method)(); }), Delay, false);
        }

        UClass* Recipe(int32 Height, bool Vanilla = false)
        {
            const FString Path = Vanilla ? FString::Printf(TEXT("/Game/FactoryGame/Recipes/Buildings/Foundations/Recipe_Foundation_8x%d_01.Recipe_Foundation_8x%d_01_C"), Height, Height) : FString::Printf(TEXT("/TrapperPoweredFoundations/Recipes/Recipe_PoweredFoundation_%dm.Recipe_PoweredFoundation_%dm_C"), Height, Height);
            return LoadObject<UClass>(nullptr, *Path);
        }

        AFGBuildableHologram* Hologram(UClass* R, const FTransform& TM)
        {
            auto* H = R ? Cast<AFGBuildableHologram>(AFGHologram::SpawnHologramFromRecipe(R, Player->GetBuildGun(), TM.GetLocation(), Player.Get())) : nullptr;
            if (H) { H->SetActorTickEnabled(false); H->SetSuppressBuildEffect(true); H->SetActorTransform(TM); }
            return H;
        }

        bool Snap(AFGBuildableHologram* H, const FHitResult& Hit)
        {
            H->PreHologramPlacement(Hit, true);
            const bool Result = H->TrySnapToActor(Hit);
            if (!Result) H->SetHologramLocationAndRotation(Hit);
            H->PostHologramPlacement(Hit, true);
            return Result;
        }

        void Run()
        {
            auto* W = World.Get();
            if (!W) return;
            auto* PC = W->GetFirstPlayerController();
            Player = PC ? Cast<AFGCharacterPlayer>(PC->GetPawn()) : nullptr;
            if (!Player.IsValid() || !Player->GetBuildGun()) { Check(false, TEXT("Player and build gun available")); Finish(); return; }
            auto* BS = AFGBuildableSubsystem::Get(W);
            TSet<int32> RefundHeights;
            int32 Case = 0;
            for (int32 AnchorHeight : {1, 2, 4}) for (int32 NewHeight : {1, 2, 4}) for (float Yaw : {0.f, 37.f})
            {
                if (AnchorHeight == NewHeight) continue;
                const FString Label = FString::Printf(TEXT("%dm beside %dm, yaw %.0f"), NewHeight, AnchorHeight, Yaw);
                const FTransform TM(FRotator(0, Yaw, 0), FVector(280000, Case++ * 10000, 50000));
                auto* AH = Hologram(Recipe(AnchorHeight), TM);
                auto* H = Hologram(Recipe(NewHeight), TM);
                auto* Vanilla = Hologram(Recipe(NewHeight, true), TM);
                if (!AH || !H || !Vanilla)
                {
                    Check(false, Label + TEXT(" loads mod and vanilla holograms"));
                    if (AH) AH->Destroy(); if (H) H->Destroy(); if (Vanilla) Vanilla->Destroy();
                    continue;
                }
                auto* Anchor = BS->BeginSpawnBuildable(AH->GetBuildClass().Get(), TM);
                AH->Destroy();
                if (!Anchor) { Check(false, Label + TEXT(" creates anchor")); H->Destroy(); Vanilla->Destroy(); continue; }
                Anchor->FinishSpawning(TM);
                if (!BS->GetAllBuildablesRef().Contains(Anchor)) BS->AddBuildable(Anchor);
                Owned.Add(Anchor);
                const auto AnchorTile = Anchor->FindComponentByClass<UPFPowerConnection>()->GetTile();
                FTransform FirstSnap;
                bool FirstValid = false;
                for (const FVector Side : {FVector(1,0,0), FVector(-1,0,0), FVector(0,1,0), FVector(0,-1,0)})
                {
                    const FVector Normal = TM.TransformVectorNoScale(Side);
                    const FVector Face = TM.TransformPosition(Side * 400.);
                    FCollisionQueryParams Params(SCENE_QUERY_STAT(PFMixedSnap), true, H);
                    Params.AddIgnoredActor(Vanilla);
                    FHitResult Hit;
                    const FString SideLabel = Label + TEXT(" side ") + Side.ToString();
                    const bool HitAnchor = W->LineTraceSingleByChannel(Hit, Face + Normal * 100., Face - Normal * 100., ECC_GameTraceChannel5, Params) && Hit.GetActor() == Anchor;
                    Check(HitAnchor, SideLabel + TEXT(" build-gun trace targets the anchor"));
                    if (!HitAnchor) continue;
                    const bool Snapped = Snap(H, Hit);
                    const bool VanillaSnapped = Snap(Vanilla, Hit);
                    Check(Snapped && VanillaSnapped && H->GetActorTransform().Equals(Vanilla->GetActorTransform(), .1), SideLabel + TEXT(" matches vanilla mixed-height snapping"));
                    PFGeometry::FTile Tile = AnchorTile;
                    Tile.HalfSize.Z = NewHeight * 50.;
                    Tile.Transform = H->GetActorTransform();
                    const FVector Local = TM.InverseTransformPosition(H->GetActorLocation());
                    Check(Snapped && FMath::Abs(Local.X - Side.X * 800.) < 1. && FMath::Abs(Local.Y - Side.Y * 800.) < 1. && PFGeometry::ShareSide(AnchorTile, Tile), SideLabel + TEXT(" has flush side contact at 8m spacing"));
                    if (!FirstValid && Snapped) { FirstSnap = H->GetActorTransform(); FirstValid = true; }
                }
                if (FirstValid)
                {
                    H->SetActorTransform(FirstSnap);
                    const auto Cost = H->GetCost(false);
                    TArray<AActor*> Children;
                    auto* Built = Cast<AFGBuildable>(H->Construct(Children, FNetConstructionID()));
                    Check(Built && Built->FindComponentByClass<UPFPowerConnection>(), Label + TEXT(" constructs the snapped powered tile"));
                    if (Built)
                    {
                        Owned.Add(Built);
                        Pairs.Add({Anchor, Built});
                        PairLabels.Add(Label);
                        if (!RefundHeights.Contains(NewHeight))
                        {
                            RefundHeights.Add(NewHeight);
                            Refunds.Add({Built, Cost, NewHeight});
                        }
                    }
                }
                H->Destroy(); Vanilla->Destroy();
            }
            APFNetworkSubsystem::Get(W)->MarkDirty();
            Later(&FPFMixedRefundTest::Energize);
        }

        void Energize()
        {
            for (const auto& Pair : Pairs)
                if (Pair.Key.IsValid())
                {
                    auto* P = Pair.Key->FindComponentByClass<UPFPowerConnection>();
                    auto* Source = NewObject<UFGPowerConnectionComponent>(Pair.Key.Get());
                    Source->SetIsHidden(true); Source->RegisterComponent();
                    auto* Info = NewObject<UFGPowerInfoComponent>(Pair.Key.Get());
                    Info->RegisterComponent(); Source->SetPowerInfo(Info); Info->SetBaseProduction(1);
                    P->AddHiddenConnection(Source);
                }
            Later(&FPFMixedRefundTest::Dismantle);
        }

        void Dismantle()
        {
            for (int32 I = 0; I < Pairs.Num(); ++I)
            {
                auto* A = Pairs[I].Key.IsValid() ? Pairs[I].Key->FindComponentByClass<UPFPowerConnection>() : nullptr;
                auto* B = Pairs[I].Value.IsValid() ? Pairs[I].Value->FindComponentByClass<UPFPowerConnection>() : nullptr;
                Check(A && B && A->HasPower() && B->HasPower() && A->GetCircuitID() == B->GetCircuitID(), PairLabels[I] + TEXT(" conducts power through mixed-height side contact"));
            }
            Check(Refunds.Num() == 3, TEXT("Dismantle fixtures cover all three foundation sizes"));
            struct FRequest { TArray<AActor*> selectedActors; TArray<FDismantleLightweightBundle> lightweightBundles; } Request;
            for (const auto& Fixture : Refunds)
            {
                if (!Fixture.Actor.IsValid()) continue;
                const FString Label = FString::Printf(TEXT("%dm dismantle"), Fixture.Height);
                TArray<FInventoryStack> Refund;
                IFGDismantleInterface::Execute_GetDismantleRefund(Fixture.Actor.Get(), Refund, false);
                TMap<UClass*, int32> Actual, Expected;
                for (const auto& Stack : Refund) Actual.FindOrAdd(Stack.Item.GetItemClass()) += Stack.NumItems;
                for (const auto& Item : Fixture.Cost) Expected.FindOrAdd(Item.ItemClass) += Item.Amount;
                bool Correct = Actual.Num() == Expected.Num();
                for (const auto& Item : Expected) Correct &= Actual.FindRef(Item.Key) == Item.Value;
                Check(Correct, Label + TEXT(" refund equals the complete construction cost"));
                if (!Correct)
                {
                    for (const auto& Item : Actual) UE_LOG(LogPFMixedRefund, Display, TEXT("Actual %s refund %s x%d"), *Label, *GetNameSafe(Item.Key), Item.Value);
                    for (const auto& Item : Expected) UE_LOG(LogPFMixedRefund, Display, TEXT("Expected %s refund %s x%d"), *Label, *GetNameSafe(Item.Key), Item.Value);
                }
                TArray<FInventoryStack> FreeBuildRefund;
                IFGDismantleInterface::Execute_GetDismantleRefund(Fixture.Actor.Get(), FreeBuildRefund, true);
                Check(FreeBuildRefund.IsEmpty(), Label + TEXT(" no-build-cost mode does not generate free materials"));
                Check(IFGDismantleInterface::Execute_CanDismantle(Fixture.Actor.Get()), Label + TEXT(" is eligible for dismantling"));
                for (const auto& Item : Actual) ExpectedRefund.FindOrAdd(Item.Key) += Item.Value;
                Request.selectedActors.Add(Fixture.Actor.Get());
            }
            auto* Inventory = Player.IsValid() ? Player->GetInventory() : nullptr;
            TArray<FInventoryStack> RefundStacks;
            for (const auto& Item : ExpectedRefund) RefundStacks.Add(FInventoryStack(Item.Value, Item.Key));
            const bool Space = Inventory && Inventory->HasEnoughSpaceForStacks(RefundStacks);
            Check(Space, TEXT("Inventory has room for test refunds without overflow crates"));
            auto* State = Player.IsValid() ? Player->GetBuildGun()->GetBuildGunStateFor(EBuildGunState::BGS_DISMANTLE) : nullptr;
            auto* Function = State ? State->FindFunction(TEXT("Server_DismantleActors")) : nullptr;
            Check(Function != nullptr, TEXT("Native dismantle server handler is available"));
            if (Space && Function && Request.selectedActors.Num() == 3)
            {
                for (const auto& Item : ExpectedRefund) InventoryBefore.Add(Item.Key, Inventory->GetNumItems(Item.Key));
                SentRefundRequest = true;
                State->ProcessEvent(Function, &Request);
            }
            Later(&FPFMixedRefundTest::Finish);
        }

        void Finish()
        {
            if (SentRefundRequest && Player.IsValid())
            {
                auto* Inventory = Player->GetInventory();
                bool Received = true, Restored = true;
                for (const auto& Item : ExpectedRefund)
                {
                    const int32 Before = InventoryBefore.FindRef(Item.Key);
                    const int32 Added = Inventory->GetNumItems(Item.Key) - Before;
                    Received &= Added == Item.Value;
                    UE_LOG(LogPFMixedRefund, Display, TEXT("Inventory %s: before=%d added=%d expected=%d"), *GetNameSafe(Item.Key), Before, Added, Item.Value);
                    // Only remove the confirmed excess generated by our fixtures.
                    // Never reduce inventory below its pre-test amount.
                    if (Added > 0) Inventory->Remove(Item.Key, FMath::Min(Added, Item.Value));
                    Restored &= Inventory->GetNumItems(Item.Key) == Before;
                }
                Check(Received, TEXT("Native dismantling delivers all expected refunds to player inventory"));
                Check(Restored, TEXT("Player material totals restored after removing test-only refunds"));
                for (const auto& Fixture : Refunds)
                    Check(!Fixture.Actor.IsValid() || Fixture.Actor->GetIsDismantled() || Fixture.Actor->IsActorBeingDestroyed(), FString::Printf(TEXT("%dm fixture actually dismantled by the server handler"), Fixture.Height));
            }
            for (const auto& B : Owned) if (B.IsValid() && !B->IsActorBeingDestroyed()) B->TurnOffAndDestroy();
            Report += FString::Printf(TEXT("Failures: %d\n"), Failures);
            FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("TrapperPoweredFoundations-MixedRefundTest.txt")));
            UE_LOG(LogPFMixedRefund, Display, TEXT("Mixed-height/refund test complete: %d failures"), Failures);
        }
    };
}

void PFStartMixedRefundTest(UWorld* World)
{
    auto Test = MakeShared<FPFMixedRefundTest>();
    Test->World = World;
    FTimerHandle Timer;
    World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([Test] { Test->Run(); }), 15.f, false);
}
