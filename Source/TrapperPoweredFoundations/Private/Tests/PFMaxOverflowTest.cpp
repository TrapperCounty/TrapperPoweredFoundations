// Opt-in native maximum-zoop and full-inventory dismantle regression test.
#include "PFNetworkSubsystem.h"
#include "PFPowerConnection.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "FGInventoryLibrary.h"
#include "FGPowerInfoComponent.h"
#include "FGDismantleInterface.h"
#include "FGCrate.h"
#include "FGBuildableSubsystem.h"
#include "Buildables/FGBuildable.h"
#include "Equipment/FGBuildGun.h"
#include "Equipment/FGBuildGunDismantle.h"
#include "Hologram/FGFoundationHologram.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogPFMaxOverflow, Log, All);

namespace
{
    struct FPFMaxRow
    {
        TArray<TWeakObjectPtr<AFGBuildable>> Actors;
        FString Label;
        bool Vertical = false;
        int32 Count = 0;
    };

    struct FPFMaxOverflowTest : TSharedFromThis<FPFMaxOverflowTest>
    {
        TWeakObjectPtr<UWorld> World;
        TWeakObjectPtr<AFGCharacterPlayer> Player;
        TArray<FPFMaxRow> Rows;
        TArray<TWeakObjectPtr<AFGBuildable>> RefundActors;
        TMap<UClass*, int32> ExpectedRefund, BeforeAmounts;
        TSet<TWeakObjectPtr<AFGCrate>> OldCrates;
        TArray<FInventoryStack> OriginalSlots;
        TArray<int32> FillAdded;
        FString Report;
        int32 Failures = 0;
        bool RequestedRefund = false;

        void Check(bool OK, const FString& Label)
        {
            Report += FString::Printf(TEXT("%s: %s\n"), OK ? TEXT("PASS") : TEXT("FAIL"), *Label);
            UE_LOG(LogPFMaxOverflow, Display, TEXT("%s: %s"), OK ? TEXT("PASS") : TEXT("FAIL"), *Label);
            if (!OK) ++Failures;
        }
        void Later(void (FPFMaxOverflowTest::*Method)())
        {
            FTimerHandle Timer;
            auto Self = AsShared();
            World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([Self, Method] { (Self.Get().*Method)(); }), 2.f, false);
        }
        AFGBuildableHologram* Hologram(int32 Height, const FTransform& TM, bool Vanilla = false)
        {
            const FString Path = Vanilla ? FString::Printf(TEXT("/Game/FactoryGame/Recipes/Buildings/Foundations/Recipe_Foundation_8x%d_01.Recipe_Foundation_8x%d_01_C"), Height, Height) : FString::Printf(TEXT("/TrapperPoweredFoundations/Recipes/Recipe_PoweredFoundation_%dm.Recipe_PoweredFoundation_%dm_C"), Height, Height);
            auto* R = LoadObject<UClass>(nullptr, *Path);
            auto* H = R ? Cast<AFGBuildableHologram>(AFGHologram::SpawnHologramFromRecipe(R, Player->GetBuildGun(), TM.GetLocation(), Player.Get())) : nullptr;
            if (H) { H->SetActorTickEnabled(false); H->SetSuppressBuildEffect(true); H->SetActorTransform(TM); }
            return H;
        }
        bool Mode(AFGBuildableHologram* H, bool Vertical)
        {
            TArray<TSubclassOf<UFGBuildGunModeDescriptor>> Modes;
            H->GetSupportedBuildModes(Modes);
            for (auto M : Modes)
                if (M && (Vertical ? M->GetName().Contains(TEXT("Vertical")) : M->GetName().Contains(TEXT("Zoop")) && !M->GetName().Contains(TEXT("Vertical"))))
                {
                    H->SetBuildModeOverride(*M); H->OnBuildModeChanged(*M); return true;
                }
            return false;
        }
        void Run()
        {
            auto* W = World.Get();
            if (!W) return;
            auto* PC = W->GetFirstPlayerController();
            Player = PC ? Cast<AFGCharacterPlayer>(PC->GetPawn()) : nullptr;
            if (!Player.IsValid() || !Player->GetBuildGun()) { Check(false, TEXT("Player and build gun available")); Finish(); return; }
            auto* BS = AFGBuildableSubsystem::Get(W);
            int32 Case = 0;
            TSet<int32> RefundHeights;
            for (int32 Height : {1, 2, 4}) for (float Yaw : {0.f, 37.f}) for (int32 Axis : {0, 1, 2}) for (int32 Sign : {-1, 1})
            {
                const FString Label = FString::Printf(TEXT("%dm yaw %.0f axis %d direction %d"), Height, Yaw, Axis, Sign);
                const FTransform TM(FRotator(0, Yaw, 0), FVector(320000, Case++ * 20000, 50000));
                auto* H = Hologram(Height, TM);
                auto* V = Hologram(Height, TM, true);
                if (!H || !V) { Check(false, Label + TEXT(" creates both holograms")); if(H) H->Destroy(); if(V) V->Destroy(); continue; }
                const bool ModesOK = Mode(H, Axis == 2) && Mode(V, Axis == 2);
                const int32 Max = H->GetMaxZoopAmount();
                Check(ModesOK && Max == V->GetMaxZoopAmount() && Max > 0 && Max <= 100, Label + TEXT(" inherits vanilla maximum and build mode"));
                if (!ModesOK || Max <= 0 || Max > 100) { H->Destroy(); V->Destroy(); continue; }
                H->SetZoopAmount(FIntVector::ZeroValue); H->RegenerateZoopInstances();
                const auto Single = H->GetCost(false);
                FIntVector Desired(0,0,0); Desired[Axis] = Sign * Max;
                H->SetZoopAmount(Desired); H->RegenerateZoopInstances();
                V->SetZoopAmount(Desired); V->RegenerateZoopInstances();
                const int32 Count = H->GetBaseCostMultiplier();
                UE_LOG(LogPFMaxOverflow, Display, TEXT("%s: configured max=%d total=%d vanilla=%d"), *Label, Max, Count, V->GetBaseCostMultiplier());
                Check(Count == V->GetBaseCostMultiplier() && Count == 10, Label + TEXT(" maximum preview contains ten foundations"));
                const auto Cost = H->GetCost(false);
                bool CostsOK = !Single.IsEmpty() && Cost.Num() == Single.Num();
                for (const auto& Item : Single)
                {
                    const auto* Found = Cost.FindByPredicate([&](const FItemAmount& X) { return X.ItemClass == Item.ItemClass; });
                    CostsOK &= Found && Found->Amount == Item.Amount * Count;
                }
                Check(CostsOK, Label + TEXT(" maximum cost includes all concrete, plates, and wire"));
                // Exercise the setter beyond the normal maximum, then restore the
                // supported maximum before constructing. The preview must agree
                // with vanilla even if this low-level setter is not the UI clamp.
                Desired[Axis] = Sign * (Max + 5);
                H->SetZoopAmount(Desired); H->RegenerateZoopInstances();
                V->SetZoopAmount(Desired); V->RegenerateZoopInstances();
                Check(H->GetBaseCostMultiplier() == V->GetBaseCostMultiplier(), Label + TEXT(" beyond-limit setter behavior matches vanilla"));
                UE_LOG(LogPFMaxOverflow, Display, TEXT("%s: beyond-limit preview=%d vanilla=%d"), *Label, H->GetBaseCostMultiplier(), V->GetBaseCostMultiplier());
                Desired[Axis] = Sign * Max;
                H->SetZoopAmount(Desired); H->RegenerateZoopInstances();
                TSet<AFGBuildable*> Before;
                for (auto* B : BS->GetAllBuildablesRef()) Before.Add(B);
                TArray<AActor*> Children;
                H->Construct(Children, FNetConstructionID());
                FPFMaxRow Row;
                Row.Label = Label; Row.Vertical = Axis == 2; Row.Count = Count;
                for (auto* B : BS->GetAllBuildablesRef()) if (!Before.Contains(B) && B->GetClass() == H->GetBuildClass()) Row.Actors.Add(B);
                Check(Row.Actors.Num() == Count, Label + TEXT(" constructs the exact maximum number of actors"));
                bool Components = Row.Actors.Num() == Count, Aligned = Components;
                TArray<double> Offsets;
                for (const auto& B : Row.Actors)
                {
                    auto* P = B->FindComponentByClass<UPFPowerConnection>();
                    Components &= P && P->GetFName() == TEXT("PoweredFloorConnection");
                    const FVector Local = TM.InverseTransformPosition(B->GetActorLocation());
                    Aligned &= B->GetActorQuat().Equals(TM.GetRotation(), .001);
                    for (int32 A=0; A<3; ++A) if (A != Axis) Aligned &= FMath::Abs(Local[A]) < 1.;
                    Offsets.Add(Local[Axis]);
                }
                Offsets.Sort();
                for (int32 I=0; I<Offsets.Num(); ++I)
                    Aligned &= FMath::Abs(Offsets[I] - (Sign > 0 ? I : I - Count + 1) * (Axis == 2 ? Height * 100. : 800.)) < 1.;
                Check(Components, Label + TEXT(" retains every stable power component"));
                Check(Aligned, Label + TEXT(" all tiles have exact spacing, rotation and no off-axis drift"));
                if (!Row.Actors.IsEmpty() && !RefundHeights.Contains(Height))
                {
                    RefundHeights.Add(Height); RefundActors.Add(Row.Actors[0]);
                    TArray<FInventoryStack> Refund;
                    IFGDismantleInterface::Execute_GetDismantleRefund(Row.Actors[0].Get(), Refund, false);
                    TMap<UClass*,int32> Actual;
                    for (const auto& Stack : Refund) Actual.FindOrAdd(Stack.Item.GetItemClass()) += Stack.NumItems;
                    bool Correct = Actual.Num() == Single.Num();
                    for (const auto& Item : Single) Correct &= Actual.FindRef(Item.ItemClass) == Item.Amount;
                    Check(Correct, FString::Printf(TEXT("%dm overflow fixture refund equals complete single-tile cost"), Height));
                    for (const auto& Item : Actual) ExpectedRefund.FindOrAdd(Item.Key) += Item.Value;
                }
                Rows.Add(Row); H->Destroy(); V->Destroy();
            }
            APFNetworkSubsystem::Get(W)->MarkDirty();
            Later(&FPFMaxOverflowTest::Energize);
        }
        void Energize()
        {
            for (const auto& Row : Rows) if (!Row.Actors.IsEmpty() && Row.Actors[0].IsValid())
                if (auto* P = Row.Actors[0]->FindComponentByClass<UPFPowerConnection>())
                {
                    auto* Source = NewObject<UFGPowerConnectionComponent>(Row.Actors[0].Get());
                    Source->SetIsHidden(true); Source->RegisterComponent();
                    auto* Info = NewObject<UFGPowerInfoComponent>(Row.Actors[0].Get());
                    Info->RegisterComponent(); Source->SetPowerInfo(Info); Info->SetBaseProduction(1); P->AddHiddenConnection(Source);
                }
            Later(&FPFMaxOverflowTest::Dismantle);
        }
        void Dismantle()
        {
            for (const auto& Row : Rows)
            {
                bool OK = Row.Actors.Num() == Row.Count;
                int32 Circuit = INDEX_NONE;
                for (int32 I=0; I<Row.Actors.Num(); ++I)
                {
                    auto* P = Row.Actors[I].IsValid() ? Row.Actors[I]->FindComponentByClass<UPFPowerConnection>() : nullptr;
                    if (!P) { OK = false; continue; }
                    if (I == 0) Circuit = P->GetCircuitID();
                    OK &= P->HasPower() && Circuit != INDEX_NONE && Circuit == P->GetCircuitID();
                    if (Row.Vertical) for (int32 J=I+1; J<Row.Actors.Num(); ++J)
                    {
                        auto* Q = Row.Actors[J]->FindComponentByClass<UPFPowerConnection>();
                        OK &= Q && P->HasHiddenConnection(Q)==PFGeometry::ShareFace(P->GetTile(),Q->GetTile());
                    }
                }
                Check(OK, Row.Label + TEXT(" maximum row or stack conducts power through all ten tiles"));
            }
            auto* Inv = Player.IsValid() ? Player->GetInventory() : nullptr;
            auto* State = Player.IsValid() ? Player->GetBuildGun()->GetBuildGunStateFor(EBuildGunState::BGS_DISMANTLE) : nullptr;
            auto* Function = State ? State->FindFunction(TEXT("Server_DismantleActors")) : nullptr;
            Check(Inv && Function && RefundActors.Num() == 3, TEXT("Full-inventory refund test has player, native handler, and all three sizes"));
            if (!Inv || !Function || RefundActors.Num() != 3) { Finish(); return; }
            for (const auto& Item : ExpectedRefund) BeforeAmounts.Add(Item.Key, Inv->GetNumItems(Item.Key));
            Inv->GetInventoryStacks(OriginalSlots, true);
            FillAdded.Init(0, Inv->GetSizeLinear());
            auto* Filler = ExpectedRefund.CreateConstIterator().Key();
            for (int32 I=0; I<FillAdded.Num(); ++I)
            {
                FInventoryStack Stack; Inv->GetStackFromIndex(I, Stack);
                const int32 Existing = Stack.HasItems() ? Stack.NumItems : 0;
                if (!Stack.HasItems()) Stack = FInventoryStack(1, Filler);
                const int32 Add = FMath::Max(0, Inv->GetSlotSizeForItem(I, Stack.Item.GetItemClass(), &Stack.Item) - Existing);
                if (Add > 0) { Stack.NumItems = Add; FillAdded[I] = Inv->AddStackToIndex(I, Stack, true); }
            }
            bool Full = Inv->FindEmptyIndex() == INDEX_NONE;
            for (const auto& Item : ExpectedRefund) Full &= !Inv->HasEnoughSpaceForStack(FInventoryStack(1, Item.Key));
            Check(Full, TEXT("Inventory has no empty slots or capacity for even one refund ingredient"));
            for (TActorIterator<AFGCrate> It(World.Get()); It; ++It) OldCrates.Add(*It);
            if (Full)
            {
                struct FRequest { TArray<AActor*> selectedActors; TArray<FDismantleLightweightBundle> lightweightBundles; } Request;
                for (auto B : RefundActors) if (B.IsValid()) Request.selectedActors.Add(B.Get());
                RequestedRefund = true;
                State->ProcessEvent(Function, &Request);
            }
            // The authoritative handler executes synchronously. Restore capacity
            // immediately, before any tick or autosave can observe the filler.
            for (int32 I=0; I<FillAdded.Num(); ++I) if (FillAdded[I] > 0) Inv->RemoveFromIndex(I, FillAdded[I]);
            bool Restored = Inv->GetSizeLinear() == OriginalSlots.Num();
            for (int32 I=0; I<OriginalSlots.Num(); ++I)
            {
                FInventoryStack Now; Inv->GetStackFromIndex(I, Now);
                const auto& Was = OriginalSlots[I];
                Restored &= Now.HasItems() == Was.HasItems() && (!Was.HasItems() || (Now.Item.GetItemClass() == Was.Item.GetItemClass() && Now.NumItems == Was.NumItems));
            }
            Check(Restored, TEXT("Every original inventory slot and count restored immediately after dismantling"));
            Later(&FPFMaxOverflowTest::Finish);
        }
        void Finish()
        {
            if (RequestedRefund && Player.IsValid())
            {
                auto* Inv = Player->GetInventory();
                TArray<TWeakObjectPtr<AFGCrate>> NewCrates;
                TMap<UClass*,int32> Contents;
                bool TypesOK = true;
                for (TActorIterator<AFGCrate> It(World.Get()); It; ++It) if (!OldCrates.Contains(*It))
                {
                    NewCrates.Add(*It);
                    TypesOK &= It->GetCrateType() == EFGCrateType::CT_DismantleCrate;
                    TArray<FInventoryStack> Stacks; It->GetInventory()->GetInventoryStacks(Stacks);
                    for (const auto& S : Stacks) Contents.FindOrAdd(S.Item.GetItemClass()) += S.NumItems;
                    UE_LOG(LogPFMaxOverflow, Display, TEXT("Overflow crate %s at %s"), *It->GetName(), *It->GetActorLocation().ToString());
                }
                Check(!NewCrates.IsEmpty() && TypesOK, TEXT("Native dismantling creates overflow dismantle crates"));
                bool Exact = Contents.Num() == ExpectedRefund.Num();
                for (const auto& Item : ExpectedRefund)
                {
                    Exact &= Contents.FindRef(Item.Key) == Item.Value;
                    UE_LOG(LogPFMaxOverflow, Display, TEXT("Crate refund %s actual=%d expected=%d"), *GetNameSafe(Item.Key), Contents.FindRef(Item.Key), Item.Value);
                }
                Check(Exact, TEXT("Overflow crates contain exactly all concrete, iron plates and wire, without loss or duplication"));
                for (auto B : RefundActors) Check(!B.IsValid() || B->GetIsDismantled() || B->IsActorBeingDestroyed(), TEXT("Full-inventory target actually dismantled"));
                if (Exact)
                {
                    for (auto Crate : NewCrates) if (Crate.IsValid()) UFGInventoryLibrary::GrabAllItemsFromInventory(Crate->GetInventory(), Inv);
                    bool Collected = true, Restored = true;
                    for (const auto& Item : ExpectedRefund)
                    {
                        const int32 Before = BeforeAmounts.FindRef(Item.Key);
                        const int32 Added = Inv->GetNumItems(Item.Key) - Before;
                        Collected &= Added == Item.Value;
                        if (Added > 0) Inv->Remove(Item.Key, FMath::Min(Added, Item.Value));
                        Restored &= Inv->GetNumItems(Item.Key) == Before;
                    }
                    Check(Collected, TEXT("All overflow refunds can be collected through the native inventory transfer"));
                    Check(Restored, TEXT("Original player material totals restored after collecting and removing test refunds"));
                    for (auto Crate : NewCrates) if (Crate.IsValid() && Crate->GetInventory()->IsEmpty()) Crate->Destroy();
                }
                else UE_LOG(LogPFMaxOverflow, Warning, TEXT("Unexpected crate contents retained for diagnosis; no crate contents deleted."));
            }
            for (const auto& Row : Rows) for (auto B : Row.Actors) if (B.IsValid() && !B->IsActorBeingDestroyed()) B->TurnOffAndDestroy();
            Report += FString::Printf(TEXT("Failures: %d\n"), Failures);
            FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("TrapperPoweredFoundations-MaxOverflowTest.txt")));
            UE_LOG(LogPFMaxOverflow, Display, TEXT("Maximum zoop/overflow test complete: %d failures"), Failures);
        }
    };
}

void PFStartMaxOverflowTest(UWorld* World)
{
    auto Test = MakeShared<FPFMaxOverflowTest>(); Test->World = World;
    FTimerHandle Timer;
    World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([Test] { Test->Run(); }), 15.f, false);
}
