// Opt-in real partial refunds, followed by a separate-process save/reload test.
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "FGInventoryLibrary.h"
#include "FGDismantleInterface.h"
#include "FGCrate.h"
#include "FGSaveSession.h"
#include "Buildables/FGBuildable.h"
#include "Equipment/FGBuildGun.h"
#include "Equipment/FGBuildGunDismantle.h"
#include "Hologram/FGBuildableHologram.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogPFPartialPersistence, Log, All);
namespace
{
    struct FPFPartialPersistence : TSharedFromThis<FPFPartialPersistence>
    {
        TWeakObjectPtr<UWorld> World;
        TWeakObjectPtr<AFGCharacterPlayer> Player;
        TArray<TWeakObjectPtr<AFGCrate>> Crates;
        UClass* Items[3] = {nullptr, nullptr, nullptr}; // concrete, plates, wire
        int32 Baseline[3] = {0,0,0};
        FString Report;
        TArray<FString> Manifest;
        int32 Failures = 0;
        bool Reload = false;
        FString ManifestPath() const { return FPaths::ProjectSavedDir() / TEXT("TrapperPoweredFoundations-PartialPersistenceManifest.txt"); }
        void Check(bool OK, const FString& Label)
        {
            Report += FString::Printf(TEXT("%s: %s\n"), OK ? TEXT("PASS") : TEXT("FAIL"), *Label);
            UE_LOG(LogPFPartialPersistence, Display, TEXT("%s: %s"), OK ? TEXT("PASS") : TEXT("FAIL"), *Label);
            if (!OK) ++Failures;
        }
        int32 ItemIndex(UClass* Item) const
        {
            for(int32 I=0; I<3; ++I) if (Items[I] == Item) return I;
            return INDEX_NONE;
        }
        bool RestoreSlots(UFGInventoryComponent* Inv, const TArray<FInventoryStack>& Original)
        {
            bool OK = Inv->GetSizeLinear() == Original.Num();
            for(int32 I=0; I<Original.Num(); ++I)
            {
                FInventoryStack Now; Inv->GetStackFromIndex(I, Now);
                const auto& Was = Original[I];
                if (Was.HasItems() && Now.Item.GetItemClass() != Was.Item.GetItemClass()) { OK=false; continue; }
                const int32 Before = Was.HasItems() ? Was.NumItems : 0;
                const int32 Current = Now.HasItems() ? Now.NumItems : 0;
                if(Current > Before) Inv->RemoveFromIndex(I, Current-Before);
                OK &= Current >= Before;
                Inv->GetStackFromIndex(I, Now);
                OK &= Now.HasItems() == Was.HasItems() && (!Was.HasItems() || Now.NumItems == Was.NumItems);
            }
            return OK;
        }
        void Run()
        {
            auto* PC = World->GetFirstPlayerController();
            Player = PC ? Cast<AFGCharacterPlayer>(PC->GetPawn()) : nullptr;
            if (!Player.IsValid() || !Player->GetBuildGun()) { Check(false,TEXT("Player and build gun available")); Finish(); return; }
            if (Reload) { VerifyReload(); return; }
            auto* Inv = Player->GetInventory();
            auto* State = Player->GetBuildGun()->GetBuildGunStateFor(EBuildGunState::BGS_DISMANTLE);
            auto* Function = State ? State->FindFunction(TEXT("Server_DismantleActors")) : nullptr;
            if (!Function) { Check(false,TEXT("Native dismantle handler available")); Finish(); return; }
            const int32 Heights[3] = {1,2,4};
            const int32 Headrooms[3][3] = {{2,0,1}, {5,1,2}, {0,2,5}};
            const FString SaveName = TEXT("PF_PartialRefund_") + FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
            Manifest.Add(TEXT("SAVE|") + SaveName);
            for(int32 Case=0; Case<3; ++Case)
            {
                const int32 Height = Heights[Case];
                const FString Label = FString::Printf(TEXT("%dm partial overflow"), Height);
                const FString Path = FString::Printf(TEXT("/TrapperPoweredFoundations/Recipes/Recipe_PoweredFoundation_%dm.Recipe_PoweredFoundation_%dm_C"),Height,Height);
                auto* R = LoadObject<UClass>(nullptr,*Path);
                const FTransform TM(FVector(320000,-200000+Case*10000,50000));
                auto* H = R ? Cast<AFGBuildableHologram>(AFGHologram::SpawnHologramFromRecipe(R,Player->GetBuildGun(),TM.GetLocation(),Player.Get())) : nullptr;
                if(!H) { Check(false,Label+TEXT(" creates hologram")); continue; }
                H->SetActorTickEnabled(false); H->SetSuppressBuildEffect(true); H->SetActorTransform(TM);
                const auto Cost = H->GetCost(false);
                int32 Expected[3] = {5,2, Height == 1 ? 2 : Height == 2 ? 6 : 12};
                for(const auto& I : Cost)
                {
                    const FString Name = I.ItemClass->GetName();
                    const int32 Index = Name == TEXT("Desc_Cement_C") ? 0 : Name == TEXT("Desc_IronPlate_C") ? 1 : Name == TEXT("Desc_Wire_C") ? 2 : INDEX_NONE;
                    if(Index != INDEX_NONE) Items[Index] = I.ItemClass;
                }
                if(!Items[0] || !Items[1] || !Items[2]) { Check(false,Label+TEXT(" identifies all ingredients")); H->Destroy(); continue; }
                if(Case == 0) for(int32 I=0; I<3; ++I)
                {
                    Baseline[I]=Inv->GetNumItems(Items[I]);
                    Manifest.Add(FString::Printf(TEXT("ITEM|%d|%s|%d"),I,*Items[I]->GetPathName(),Baseline[I]));
                }
                TArray<AActor*> Children;
                auto* Built = Cast<AFGBuildable>(H->Construct(Children,FNetConstructionID())); H->Destroy();
                if(!Built) { Check(false,Label+TEXT(" constructs fixture")); continue; }
                TWeakObjectPtr<AFGBuildable> Target = Built;
                TArray<FInventoryStack> Refund; IFGDismantleInterface::Execute_GetDismantleRefund(Built,Refund,false);
                int32 Actual[3]={0,0,0}; bool ValidRefund = true;
                for(const auto& S : Refund) { const int32 I=ItemIndex(S.Item.GetItemClass()); if(I==INDEX_NONE) ValidRefund=false; else Actual[I]+=S.NumItems; }
                for(int32 I=0; I<3; ++I) ValidRefund &= Actual[I] == Expected[I];
                Check(ValidRefund,Label+TEXT(" native refund equals the complete recipe"));
                TArray<FInventoryStack> Original; Inv->GetInventoryStacks(Original,true);
                TArray<int32> Added; Added.Init(0,Inv->GetSizeLinear());
                for(int32 Slot=0; Slot<Added.Num(); ++Slot)
                {
                    FInventoryStack S; Inv->GetStackFromIndex(Slot,S);
                    const int32 Before = S.HasItems()?S.NumItems:0;
                    if(!S.HasItems()) S=FInventoryStack(1,Items[Slot%3]);
                    const int32 Fill=FMath::Max(0,Inv->GetSlotSizeForItem(Slot,S.Item.GetItemClass(),&S.Item)-Before);
                    if(Fill>0) { S.NumItems=Fill; Added[Slot]=Inv->AddStackToIndex(Slot,S,true); }
                }
                bool Prepared=Inv->FindEmptyIndex()==INDEX_NONE;
                for(int32 I=0; I<3; ++I)
                {
                    int32 Remaining=Headrooms[Case][I];
                    for(int32 Slot=0; Slot<Added.Num() && Remaining>0; ++Slot)
                    {
                        FInventoryStack S; Inv->GetStackFromIndex(Slot,S);
                        if(S.Item.GetItemClass()!=Items[I]) continue;
                        const int32 Remove=FMath::Min(Added[Slot],Remaining);
                        if(Remove>0) { Inv->RemoveFromIndex(Slot,Remove); Added[Slot]-=Remove; Remaining-=Remove; }
                    }
                    Prepared &= Remaining==0 && !Inv->HasEnoughSpaceForStack(FInventoryStack(Headrooms[Case][I]+1,Items[I]));
                    if(Headrooms[Case][I]>0) Prepared &= Inv->HasEnoughSpaceForStack(FInventoryStack(Headrooms[Case][I],Items[I]));
                }
                Check(Prepared,Label+TEXT(" establishes exact ingredient headroom without removing original items"));
                TSet<TWeakObjectPtr<AFGCrate>> BeforeCrates;
                for(TActorIterator<AFGCrate> It(World.Get()); It; ++It) BeforeCrates.Add(*It);
                int32 Before[3]; for(int32 I=0; I<3; ++I) Before[I]=Inv->GetNumItems(Items[I]);
                if(Prepared && ValidRefund)
                {
                    struct FRequest { TArray<AActor*> selectedActors; TArray<FDismantleLightweightBundle> lightweightBundles; } Request;
                    Request.selectedActors.Add(Built); State->ProcessEvent(Function,&Request);
                }
                int32 Received[3]; for(int32 I=0; I<3; ++I) Received[I]=Inv->GetNumItems(Items[I])-Before[I];
                Check(RestoreSlots(Inv,Original),Label+TEXT(" restores every original inventory slot and count"));
                bool SplitOK=Prepared && ValidRefund;
                for(int32 I=0; I<3; ++I) SplitOK &= Received[I]==Headrooms[Case][I];
                Check(SplitOK,Label+TEXT(" delivers exactly the portion that fits to player inventory"));
                int32 CrateTotals[3]={0,0,0}; int32 NewCrates=0; bool CrateTypes=true;
                for(TActorIterator<AFGCrate> It(World.Get()); It; ++It) if(!BeforeCrates.Contains(*It))
                {
                    ++NewCrates; Crates.Add(*It); CrateTypes &= It->GetCrateType()==EFGCrateType::CT_DismantleCrate;
                    TArray<FInventoryStack> Stacks; It->GetInventory()->GetInventoryStacks(Stacks);
                    int32 PerCrate[3]={0,0,0};
                    for(const auto& S:Stacks) { const int32 I=ItemIndex(S.Item.GetItemClass()); if(I==INDEX_NONE) CrateTypes=false; else { PerCrate[I]+=S.NumItems; CrateTotals[I]+=S.NumItems; } }
                    const FVector P=It->GetActorLocation();
                    Manifest.Add(FString::Printf(TEXT("CRATE|%s|%.6f|%.6f|%.6f|%d|%d|%d"),*It->GetName(),P.X,P.Y,P.Z,PerCrate[0],PerCrate[1],PerCrate[2]));
                }
                bool Exact=NewCrates>0 && CrateTypes;
                for(int32 I=0; I<3; ++I)
                {
                    Exact &= CrateTotals[I]==Expected[I]-Headrooms[Case][I] && Received[I]+CrateTotals[I]==Expected[I];
                    Report+=FString::Printf(TEXT("DETAIL: %dm %s inventory=%d crate=%d total=%d\n"),Height,*Items[I]->GetName(),Received[I],CrateTotals[I],Expected[I]);
                }
                Check(Exact,Label+TEXT(" puts only the remainder in crates, conserving every ingredient"));
                Check(!Target.IsValid() || Target->GetIsDismantled() || Target->IsActorBeingDestroyed(),Label+TEXT(" actually dismantles the tile"));
                if(Target.IsValid() && !Target->IsActorBeingDestroyed()) Target->TurnOffAndDestroy();
            }
            Check(Crates.Num()==3,TEXT("All three partial-overflow fixtures created a persistence crate"));
            if(Failures>0) { Finish(); return; }
            Check(FFileHelper::SaveStringArrayToFile(Manifest,*ManifestPath()),TEXT("Writes crate identities, positions, exact contents and inventory baseline"));
            Report+=TEXT("Persistence save: ")+SaveName+TEXT("\n");
            auto Self=AsShared();
            UFGSaveSession::Get(World.Get())->SaveGame(SaveName,FOnSaveGameComplete::CreateLambda([Self](bool OK,const FText& Error,void*)
            {
                Self->Check(OK,TEXT("Separate save with uncollected partial refund crates completed: ")+Error.ToString()); Self->Finish();
            }),nullptr);
        }
        void VerifyReload()
        {
            Check(FFileHelper::LoadFileToStringArray(Manifest,*ManifestPath()),TEXT("Reads original save-test manifest"));
            struct FCrateRecord { FString Name; FVector Location; int32 Count[3]; };
            TArray<FCrateRecord> Records;
            for(const auto& Line:Manifest)
            {
                TArray<FString> F; Line.ParseIntoArray(F,TEXT("|"),false);
                if(F.Num()==4 && F[0]==TEXT("ITEM"))
                {
                    const int32 I=FCString::Atoi(*F[1]); if(I>=0 && I<3) { Items[I]=LoadObject<UClass>(nullptr,*F[2]); Baseline[I]=FCString::Atoi(*F[3]); }
                }
                if(F.Num()==8 && F[0]==TEXT("CRATE"))
                {
                    FCrateRecord R; R.Name=F[1]; R.Location=FVector(FCString::Atod(*F[2]),FCString::Atod(*F[3]),FCString::Atod(*F[4]));
                    for(int32 I=0; I<3; ++I) R.Count[I]=FCString::Atoi(*F[I+5]); Records.Add(R);
                }
            }
            Check(Items[0] && Items[1] && Items[2] && Records.Num()==3,TEXT("Manifest contains all ingredients and three saved crates"));
            if(!Items[0] || !Items[1] || !Items[2] || Records.Num()!=3) { Finish(); return; }
            auto* Inv=Player->GetInventory();
            bool InventoryOK=true; for(int32 I=0; I<3; ++I) InventoryOK &= Inv->GetNumItems(Items[I])==Baseline[I];
            Check(InventoryOK,TEXT("Reload preserves the original inventory totals with no temporary filler"));
            int32 Expected[3]={0,0,0};
            for(const auto& R:Records)
            {
                AFGCrate* Found=nullptr; int32 Matches=0;
                for(TActorIterator<AFGCrate> It(World.Get()); It; ++It) if(It->GetName()==R.Name) { Found=*It; ++Matches; }
                Check(Matches==1 && Found,TEXT("Saved crate identity survives exactly once: ")+R.Name);
                if(!Found) continue;
                Crates.Add(Found);
                Check(Found->GetCrateType()==EFGCrateType::CT_DismantleCrate && Found->GetActorLocation().Equals(R.Location,.1),TEXT("Crate type and location survive: ")+R.Name);
                int32 Actual[3]={0,0,0}; bool Exact=true;
                TArray<FInventoryStack> Stacks; Found->GetInventory()->GetInventoryStacks(Stacks);
                for(const auto& S:Stacks) { const int32 I=ItemIndex(S.Item.GetItemClass()); if(I==INDEX_NONE) Exact=false; else Actual[I]+=S.NumItems; }
                for(int32 I=0; I<3; ++I) { Exact &= Actual[I]==R.Count[I]; Expected[I]+=R.Count[I]; }
                Check(Exact,TEXT("Every saved refund quantity survives without loss or duplication: ")+R.Name);
            }
            if(Failures==0)
            {
                TArray<FInventoryStack> Original; Inv->GetInventoryStacks(Original,true);
                for(auto C:Crates) if(C.IsValid()) UFGInventoryLibrary::GrabAllItemsFromInventory(C->GetInventory(),Inv);
                bool Collected=true; for(int32 I=0; I<3; ++I) Collected &= Inv->GetNumItems(Items[I])-Baseline[I]==Expected[I];
                Check(Collected,TEXT("All persisted crate refunds remain collectible through native transfer"));
                Check(RestoreSlots(Inv,Original),TEXT("Restores original inventory slots and totals after test collection"));
            }
            Finish();
        }
        void Finish()
        {
            // Only this test's known crates; the separately written save retains
            // them as evidence even after runtime cleanup.
            if(Failures==0) for(auto C:Crates) if(C.IsValid()) C->Destroy();
            Report+=FString::Printf(TEXT("Failures: %d\n"),Failures);
            const FString Name=Reload?TEXT("TrapperPoweredFoundations-PartialRefundReloadTest.txt"):TEXT("TrapperPoweredFoundations-PartialRefundTest.txt");
            FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/Name));
            UE_LOG(LogPFPartialPersistence,Display,TEXT("Partial refund %s complete: %d failures"),Reload?TEXT("reload"):TEXT("save"),Failures);
        }
    };
}
void PFStartPartialPersistenceTest(UWorld* World,bool Reload)
{
    auto Test=MakeShared<FPFPartialPersistence>(); Test->World=World; Test->Reload=Reload;
    FTimerHandle Timer; World->GetTimerManager().SetTimer(Timer,FTimerDelegate::CreateLambda([Test]{Test->Run();}),15.f,false);
}
