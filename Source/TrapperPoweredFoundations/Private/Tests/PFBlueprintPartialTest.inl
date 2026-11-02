#include "FGCrate.h"
#include "FGInventoryLibrary.h"
#include "EngineUtils.h"
namespace
{
    struct FPFBPPartialTest : FPFSnappedBPTest
    {
        TArray<TWeakObjectPtr<AFGCrate>> Crates;
        TArray<FString> CrateManifest;
        TMap<UClass*,int32> TotalExpected,ReceivedTotals;
        TMap<FString,TArray<TWeakObjectPtr<AActor>>> PendingRemovals;
        bool ReloadCrates=false;
        FString CrateManifestPath()const{return FPaths::ProjectSavedDir()/TEXT("TrapperPoweredFoundations-BlueprintPartialManifest.txt");}
        TMap<UClass*,int32> Totals(const TArray<FInventoryStack>& Stacks)const
        {TMap<UClass*,int32> R;for(const auto& S:Stacks)if(S.HasItems())R.FindOrAdd(S.Item.GetItemClass())+=S.NumItems;return R;}
        bool Equal(const TMap<UClass*,int32>& A,const TMap<UClass*,int32>& B)const
        {if(A.Num()!=B.Num())return false;for(const auto& I:A)if(B.FindRef(I.Key)!=I.Value)return false;return true;}
        bool SameSlots(const TArray<FInventoryStack>& A,const TArray<FInventoryStack>& B)const
        {if(A.Num()!=B.Num())return false;for(int32 I=0;I<A.Num();++I)if(A[I].HasItems()!=B[I].HasItems()||(A[I].HasItems()&&(A[I].Item.GetItemClass()!=B[I].Item.GetItemClass()||A[I].NumItems!=B[I].NumItems)))return false;return true;}
        bool RestoreSlots()
        {
            auto* Inv=Player->GetInventory();bool OK=Inv->GetSizeLinear()==InventoryBefore.Num();
            for(int32 I=0;I<InventoryBefore.Num();++I)
            {
                FInventoryStack Now;Inv->GetStackFromIndex(I,Now);const auto& Was=InventoryBefore[I];
                if(Was.HasItems()&&Now.Item.GetItemClass()!=Was.Item.GetItemClass()){OK=false;continue;}
                const int32 Before=Was.HasItems()?Was.NumItems:0,Current=Now.HasItems()?Now.NumItems:0;
                if(Current>Before)Inv->RemoveFromIndex(I,Current-Before);OK &= Current>=Before;
            }
            TArray<FInventoryStack> Now;Inv->GetInventoryStacks(Now,true);return OK&&SameSlots(Now,InventoryBefore);
        }
        bool Gone(AActor* A)const{return !IsValid(A)||A->IsActorBeingDestroyed()||(Cast<AFGBuildable>(A)&&Cast<AFGBuildable>(A)->GetIsDismantled());}
        void CleanupBuildings()
        {
            for(int32 I=Owned.Num()-1;I>=0;--I)if(!Gone(Owned[I].Get()))Owned[I]->TurnOffAndDestroy();
            for(auto P:Proxies)if(!Gone(P.Get()))P->Destroy();Owned.Reset();Proxies.Reset();
        }
        virtual void VerifyAndSave()override
        {
            VerifyPower(TEXT(" before partial-inventory dismantle"));if(Failures){Finish();return;}
            auto* Inv=Player->GetInventory();auto* State=Player->GetBuildGun()->GetBuildGunStateFor(EBuildGunState::BGS_DISMANTLE);
            auto* Function=State?State->FindFunction(TEXT("Server_DismantleActors")):nullptr;Check(Function!=nullptr,TEXT("Native whole-blueprint dismantle handler is available"));
            if(!Function){Finish();return;}
            Name=TEXT("PF_BlueprintPartial_")+FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));CrateManifest.Add(TEXT("SAVE|")+Name);
            for(int32 GI=0;GI<Large.Num();GI+=2)
            {
                auto& G=Large[GI];const FString Label=G.Buildings.Label+TEXT(" partial inventory");auto* Proxy=G.Buildings.Tiles[0]->GetBlueprintProxy();
                Check(Proxy&&Proxy->GetBuildables().Num()==G.Width+2,Label+TEXT(" selects a complete powered blueprint with floors, pole and foundry"));if(!Proxy)continue;
                TMap<UClass*,int32> Expected,NativeRefund;TArray<TWeakObjectPtr<AActor>> Targets;Targets.Add(Proxy);
                auto* Wire=G.Buildings.Wire.Get();if(Wire){Wire->SetBuiltWithRecipe(LoadObject<UClass>(nullptr,TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerLine.Recipe_PowerLine_C")));Targets.Add(Wire);}
                for(auto* B:Proxy->GetBuildables())
                {
                    Targets.Add(B);auto Recipe=B->GetBuiltWithRecipe();auto* H=Recipe?AFGHologram::SpawnHologramFromRecipe(Recipe,Player->GetBuildGun(),B->GetActorLocation(),Player.Get()):nullptr;
                    Check(H!=nullptr,Label+TEXT(" resolves complete cost of ")+B->GetClass()->GetName());
                    if(H){for(const auto& I:H->GetCost(false))Expected.FindOrAdd(I.ItemClass)+=I.Amount;H->Destroy();}
                    TArray<FInventoryStack> R;IFGDismantleInterface::Execute_GetDismantleRefund(B,R,false);for(const auto& I:Totals(R))NativeRefund.FindOrAdd(I.Key)+=I.Value;
                }
                auto* Desc=AFGBlueprintSubsystem::Get(World.Get())->GetBlueprintDescriptorByNameString(G.BlueprintName);TArray<FItemAmount> DescriptorCost;if(Desc)Desc->GetBlueprintCost(DescriptorCost);
                TMap<UClass*,int32> SavedCost;for(const auto& I:DescriptorCost)SavedCost.FindOrAdd(I.ItemClass)+=I.Amount;
                Check(Desc&&Equal(Expected,SavedCost),Label+TEXT(" construction costs match the saved blueprint including extra foundation wire"));
                if(Wire){TArray<FInventoryStack> R;IFGDismantleInterface::Execute_GetDismantleRefund(Wire,R,false);for(const auto& I:Totals(R))Expected.FindOrAdd(I.Key)+=I.Value;}
                Check(Equal(Expected,NativeRefund),Label+TEXT(" native refund includes the whole blueprint and dependent feed cable exactly once"));
                if(Failures)break;
                TArray<FInventoryStack> CaseStart;Inv->GetInventoryStacks(CaseStart,true);
                TArray<UClass*> Ingredients;Expected.GetKeys(Ingredients);Ingredients.Sort([](const UClass& A,const UClass& B){return A.GetName()<B.GetName();});
                TArray<int32> Added;Added.Init(0,Inv->GetSizeLinear());int32 EmptyIndex=0;
                for(int32 Slot=0;Slot<Added.Num();++Slot)
                {
                    FInventoryStack S;Inv->GetStackFromIndex(Slot,S);const int32 Before=S.HasItems()?S.NumItems:0;
                    if(!S.HasItems())S=FInventoryStack(1,Ingredients[EmptyIndex++%Ingredients.Num()]);
                    const int32 Fill=FMath::Max(0,Inv->GetSlotSizeForItem(Slot,S.Item.GetItemClass(),&S.Item)-Before);
                    if(Fill){S.NumItems=Fill;Added[Slot]=Inv->AddStackToIndex(Slot,S,true);}
                }
                TMap<UClass*,int32> Headroom;
                for(const auto& I:Expected)
                {
                    const FString N=I.Key->GetName();int32 Room=0;
                    if(G.Tier==2){if(N==TEXT("Desc_Cement_C"))Room=7;else if(N==TEXT("Desc_IronPlate_C"))Room=10;else if(N==TEXT("Desc_ModularFrame_C"))Room=3;else if(N==TEXT("Desc_Rotor_C"))Room=10;else if(N==TEXT("Desc_Cable_C"))Room=1;}
                    else{if(N==TEXT("Desc_Wire_C"))Room=13;else if(N==TEXT("Desc_IronPlate_C"))Room=4;else if(N==TEXT("Desc_IronRod_C"))Room=1;else if(N==TEXT("Desc_ModularFrame_C"))Room=10;else if(N==TEXT("Desc_Cable_C"))Room=3;}
                    Headroom.Add(I.Key,Room);int32 Remaining=Room;
                    for(int32 Slot=0;Slot<Added.Num()&&Remaining>0;++Slot)
                    {
                        FInventoryStack S;Inv->GetStackFromIndex(Slot,S);if(S.Item.GetItemClass()!=I.Key)continue;
                        const int32 Remove=FMath::Min(Added[Slot],Remaining);if(Remove){Inv->RemoveFromIndex(Slot,Remove);Added[Slot]-=Remove;Remaining-=Remove;}
                    }
                    Check(Remaining==0&&!Inv->HasEnoughSpaceForStack(FInventoryStack(Room+1,I.Key))&&(Room==0||Inv->HasEnoughSpaceForStack(FInventoryStack(Room,I.Key))),Label+TEXT(" establishes exact available capacity for ")+N);
                }
                Check(Inv->FindEmptyIndex()==INDEX_NONE,Label+TEXT(" leaves only the specified partial-stack capacity"));
                TArray<FInventoryStack> Prepared;Inv->GetInventoryStacks(Prepared,true);const auto BeforeTotals=Totals(Prepared);TSet<TWeakObjectPtr<AFGCrate>> Before;
                for(TActorIterator<AFGCrate> It(World.Get());It;++It)Before.Add(*It);
                if(!Failures){struct FRequest{TArray<AActor*> selectedActors;TArray<FDismantleLightweightBundle> lightweightBundles;} Request;Request.selectedActors.Add(Proxy);State->ProcessEvent(Function,&Request);}
                TArray<FInventoryStack> After;Inv->GetInventoryStacks(After,true);const auto AfterTotals=Totals(After);
                auto ExpectedPrepared=BeforeTotals;for(const auto& I:Headroom)if(I.Value)ExpectedPrepared.FindOrAdd(I.Key)+=I.Value;
                Check(Equal(AfterTotals,ExpectedPrepared),Label+TEXT(" sends exactly the capacity-limited portion into inventory"));
                for(int32 Slot=0;Slot<Added.Num();++Slot)if(Added[Slot])Inv->RemoveFromIndex(Slot,Added[Slot]);
                After.Reset();Inv->GetInventoryStacks(After,true);auto Retained=Totals(CaseStart);for(const auto& I:Headroom)if(I.Value){Retained.FindOrAdd(I.Key)+=I.Value;ReceivedTotals.FindOrAdd(I.Key)+=I.Value;}
                Check(Equal(Totals(After),Retained),Label+TEXT(" removes temporary filler while retaining the actual received refund"));
                // The server refunds synchronously; proxy removal finishes later.
                PendingRemovals.Add(Label,Targets);
                TMap<UClass*,int32> CrateTotals;int32 NewCount=0;bool Types=true;
                for(TActorIterator<AFGCrate> It(World.Get());It;++It)if(!Before.Contains(*It))
                {
                    ++NewCount;Crates.Add(*It);Types &= It->GetCrateType()==EFGCrateType::CT_DismantleCrate;
                    TArray<FInventoryStack> Contents;It->GetInventory()->GetInventoryStacks(Contents);const auto Counts=Totals(Contents);const FVector P=It->GetActorLocation();
                    CrateManifest.Add(FString::Printf(TEXT("CRATE|%s|%s|%.6f|%.6f|%.6f"),*It->GetName(),*It->GetClass()->GetPathName(),P.X,P.Y,P.Z));
                    for(const auto& I:Counts){CrateTotals.FindOrAdd(I.Key)+=I.Value;CrateManifest.Add(FString::Printf(TEXT("CONTENT|%s|%s|%d"),*It->GetName(),*I.Key->GetPathName(),I.Value));}
                }
                Check(NewCount>0&&Types,Label+TEXT(" creates native dismantle refund crates"));
                TMap<UClass*,int32> ExpectedCrates;for(const auto& I:Expected)if(I.Value-Headroom.FindRef(I.Key)>0)ExpectedCrates.Add(I.Key,I.Value-Headroom.FindRef(I.Key));
                Check(Equal(CrateTotals,ExpectedCrates),Label+TEXT(" crates contain exactly the portion that could not fit"));
                bool Conserved=true;for(const auto& I:Expected)Conserved &= Headroom.FindRef(I.Key)+CrateTotals.FindRef(I.Key)==I.Value;
                Check(Conserved,Label+TEXT(" inventory plus crates conserve every blueprint and cable ingredient"));
                for(const auto& I:Expected){TotalExpected.FindOrAdd(I.Key)+=I.Value;Report+=FString::Printf(TEXT("DETAIL: %s %s expected=%d inventory=%d crates=%d\n"),*Label,*I.Key->GetName(),I.Value,Headroom.FindRef(I.Key),CrateTotals.FindRef(I.Key));CrateManifest.Add(FString::Printf(TEXT("SPLIT|%d|%s|%d|%d|%d"),G.Tier,*I.Key->GetPathName(),I.Value,Headroom.FindRef(I.Key),CrateTotals.FindRef(I.Key)));}
                Report+=FString::Printf(TEXT("DETAIL: %s produced %d crates\n"),*Label,NewCount);
            }
            FTimerHandle Timer;auto Self=StaticCastSharedRef<FPFBPPartialTest>(AsShared());
            World->GetTimerManager().SetTimer(Timer,FTimerDelegate::CreateLambda([Self]{Self->VerifyRemovalAndSave();}),4.f,false);
        }
        void VerifyRemovalAndSave()
        {
            for(const auto& Pair:PendingRemovals){bool Removed=true;for(auto T:Pair.Value)Removed &= Gone(T.Get());Check(Removed,Pair.Key+TEXT(" native request removes the entire blueprint and attached feed cable"));}
            CleanupBuildings();if(Failures){Finish();return;}
            for(const auto& I:TotalExpected)CrateManifest.Add(FString::Printf(TEXT("EXPECTED|%s|%d"),*I.Key->GetPathName(),I.Value));
            for(const auto& I:ReceivedTotals)CrateManifest.Add(FString::Printf(TEXT("RECEIVED|%s|%d"),*I.Key->GetPathName(),I.Value));
            TArray<FInventoryStack> SavedSlots;Player->GetInventory()->GetInventoryStacks(SavedSlots,true);
            auto ExpectedInventory=Totals(InventoryBefore);for(const auto& I:ReceivedTotals)ExpectedInventory.FindOrAdd(I.Key)+=I.Value;
            Check(Equal(Totals(SavedSlots),ExpectedInventory),TEXT("Save retains the original inventory plus both received refund portions without filler"));
            for(int32 I=0;I<SavedSlots.Num();++I){const auto& S=SavedSlots[I];CrateManifest.Add(FString::Printf(TEXT("SAVED_SLOT|%d|%s|%d"),I,S.HasItems()?*S.Item.GetItemClass()->GetPathName():TEXT("EMPTY"),S.HasItems()?S.NumItems:0));}
            for(int32 I=0;I<InventoryBefore.Num();++I){const auto& S=InventoryBefore[I];CrateManifest.Add(FString::Printf(TEXT("SLOT|%d|%s|%d"),I,S.HasItems()?*S.Item.GetItemClass()->GetPathName():TEXT("EMPTY"),S.HasItems()?S.NumItems:0));}
            Check(FFileHelper::SaveStringArrayToFile(CrateManifest,*CrateManifestPath()),TEXT("Records every crate identity, class, location, item quantity and original inventory slot"));
            Report+=TEXT("Separate persistence save: ")+Name+TEXT("\n");
            auto Self=StaticCastSharedRef<FPFBPPartialTest>(AsShared());
            UFGSaveSession::Get(World.Get())->SaveGame(Name,FOnSaveGameComplete::CreateLambda([Self](bool OK,const FText& Error,void*){Self->Check(OK,TEXT("Saves uncollected whole-blueprint refund crates with received refunds retained in inventory: ")+Error.ToString());Self->Finish();}),nullptr);
        }
        void RunReload()
        {
            auto* PC=World->GetFirstPlayerController();Player=PC?Cast<AFGCharacterPlayer>(PC->GetPawn()):nullptr;
            if(!Player.IsValid()){Check(false,TEXT("Player available for reload verification"));Finish();return;}
            auto* Inv=Player->GetInventory();TArray<FInventoryStack> LoadedSlots;Inv->GetInventoryStacks(LoadedSlots,true);InventoryBefore.SetNum(LoadedSlots.Num());
            Check(FFileHelper::LoadFileToStringArray(CrateManifest,*CrateManifestPath()),TEXT("Reads whole-blueprint crate persistence manifest"));
            struct FRecord{FString Name,Class;FVector Location;TMap<UClass*,int32> Contents;};TMap<FString,FRecord> Records;int32 SlotCount=0,OriginalCount=0,SplitCount=0;bool SlotsOK=true,SplitOK=true;
            for(const auto& Line:CrateManifest)
            {
                TArray<FString> F;Line.ParseIntoArray(F,TEXT("|"),false);
                if(F.Num()==6&&F[0]==TEXT("CRATE")){auto& R=Records.FindOrAdd(F[1]);R.Name=F[1];R.Class=F[2];R.Location=FVector(FCString::Atod(*F[3]),FCString::Atod(*F[4]),FCString::Atod(*F[5]));}
                if(F.Num()==4&&F[0]==TEXT("CONTENT")){auto* C=LoadObject<UClass>(nullptr,*F[2]);Check(C!=nullptr,TEXT("Resolves saved crate item class"));if(C)Records.FindOrAdd(F[1]).Contents.FindOrAdd(C)+=FCString::Atoi(*F[3]);}
                if(F.Num()==3&&F[0]==TEXT("EXPECTED")){auto* C=LoadObject<UClass>(nullptr,*F[1]);if(C)TotalExpected.FindOrAdd(C)+=FCString::Atoi(*F[2]);else Check(false,TEXT("Resolves expected ingredient class"));}
                if(F.Num()==3&&F[0]==TEXT("RECEIVED")){auto* C=LoadObject<UClass>(nullptr,*F[1]);if(C)ReceivedTotals.FindOrAdd(C)+=FCString::Atoi(*F[2]);else Check(false,TEXT("Resolves saved received-refund ingredient"));}
                if(F.Num()==4&&F[0]==TEXT("SLOT")){++OriginalCount;const int32 I=FCString::Atoi(*F[1]);if(!InventoryBefore.IsValidIndex(I)){SlotsOK=false;continue;}if(F[2]!=TEXT("EMPTY")){auto* C=LoadObject<UClass>(nullptr,*F[2]);if(C)InventoryBefore[I]=FInventoryStack(FCString::Atoi(*F[3]),C);else SlotsOK=false;}}
                if(F.Num()==4&&F[0]==TEXT("SAVED_SLOT")){++SlotCount;const int32 I=FCString::Atoi(*F[1]);if(!LoadedSlots.IsValidIndex(I)){SlotsOK=false;continue;}const auto& S=LoadedSlots[I];SlotsOK &= F[2]==TEXT("EMPTY")?!S.HasItems():S.HasItems()&&S.Item.GetItemClass()->GetPathName()==F[2]&&S.NumItems==FCString::Atoi(*F[3]);}
                if(F.Num()==6&&F[0]==TEXT("SPLIT")){++SplitCount;SplitOK &= FCString::Atoi(*F[3])==FCString::Atoi(*F[4])+FCString::Atoi(*F[5]);}
            }
            Check(SlotsOK&&SlotCount==LoadedSlots.Num()&&OriginalCount==InventoryBefore.Num(),TEXT("Reload preserves every saved inventory slot, including received refunds, without filler"));
            auto ExpectedLoaded=Totals(InventoryBefore);for(const auto& I:ReceivedTotals)ExpectedLoaded.FindOrAdd(I.Key)+=I.Value;
            Check(Equal(Totals(LoadedSlots),ExpectedLoaded),TEXT("The inventory portion of both refunds survives the full restart exactly"));
            Check(SplitOK&&SplitCount==14,TEXT("Both per-ingredient split records conserve their complete refunds"));
            Check(Records.Num()>0&&TotalExpected.Num()==7,TEXT("Manifest contains whole-blueprint crates and all seven refund ingredient types"));
            TMap<UClass*,int32> Combined;
            for(const auto& Pair:Records)
            {
                const auto& R=Pair.Value;AFGCrate* Found=nullptr;int32 Matches=0;for(TActorIterator<AFGCrate> It(World.Get());It;++It)if(It->GetName()==R.Name){Found=*It;++Matches;}
                Check(Found&&Matches==1,TEXT("Crate survives exactly once: ")+R.Name);if(!Found)continue;Crates.Add(Found);
                Check(Found->GetCrateType()==EFGCrateType::CT_DismantleCrate&&Found->GetClass()->GetPathName()==R.Class&&Found->GetActorLocation().Equals(R.Location,.1),TEXT("Crate class, type and position survive: ")+R.Name);
                TArray<FInventoryStack> Contents;Found->GetInventory()->GetInventoryStacks(Contents);const auto Counts=Totals(Contents);
                Check(Equal(Counts,R.Contents),TEXT("Every item quantity survives in crate: ")+R.Name);for(const auto& I:Counts)Combined.FindOrAdd(I.Key)+=I.Value;
            }
            TMap<UClass*,int32> ExpectedCrates;for(const auto& I:TotalExpected)if(I.Value-ReceivedTotals.FindRef(I.Key)>0)ExpectedCrates.Add(I.Key,I.Value-ReceivedTotals.FindRef(I.Key));
            Check(Equal(Combined,ExpectedCrates),TEXT("Saved crates retain exactly the overflow portions of both blueprints"));
            auto Conserved=Combined;for(const auto& I:ReceivedTotals)Conserved.FindOrAdd(I.Key)+=I.Value;
            Check(Equal(Conserved,TotalExpected),TEXT("Saved inventory plus saved crates retain every refund ingredient without loss or duplication"));
            TArray<FInventoryStack> ExpectedStacks;for(const auto& I:ExpectedCrates)ExpectedStacks.Add(FInventoryStack(I.Value,I.Key));
            const bool Space=Inv->HasEnoughSpaceForStacks(ExpectedStacks);Check(Space,TEXT("Inventory with retained refunds has room to collect the saved overflow"));
            if(!Failures&&Space)
            {
                for(auto C:Crates)if(C.IsValid())UFGInventoryLibrary::GrabAllItemsFromInventory(C->GetInventory(),Inv);
                TArray<FInventoryStack> Now;Inv->GetInventoryStacks(Now,true);auto Expected=Totals(InventoryBefore);for(const auto& I:TotalExpected)Expected.FindOrAdd(I.Key)+=I.Value;
                Check(Equal(Totals(Now),Expected),TEXT("Every persisted refund remains collectible through native inventory transfer"));
                bool Empty=true;for(auto C:Crates)if(C.IsValid()){TArray<FInventoryStack> S;C->GetInventory()->GetInventoryStacks(S);Empty &= Totals(S).IsEmpty();}
                Check(Empty,TEXT("Collected crates contain no residual or duplicate materials"));
                Check(RestoreSlots(),TEXT("Removes collected test refunds and restores every original slot"));
            }
            Finish();
        }
        virtual void Finish()override
        {
            CleanupBuildings();if(Player.IsValid())Check(RestoreSlots(),TEXT("Restores original inventory after saving or collecting test refunds"));
            if(Player.IsValid()){TArray<FInventoryStack> Now;Player->GetInventory()->GetInventoryStacks(Now,true);Check(SameSlots(Now,InventoryBefore),TEXT("Final player inventory is unchanged in every slot"));}
            if(!Failures)for(auto C:Crates)if(C.IsValid())C->Destroy();
            Report+=FString::Printf(TEXT("Failures: %d\n"),Failures);
            FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/(ReloadCrates?TEXT("TrapperPoweredFoundations-BlueprintPartialReloadTest.txt"):TEXT("TrapperPoweredFoundations-BlueprintPartialTest.txt"))));
            UE_LOG(LogPFBlueprintTest,Display,TEXT("Blueprint partial %s complete: %d failures"),ReloadCrates?TEXT("reload"):TEXT("save"),Failures);
        }
    };
}
void PFStartBlueprintPartialTest(UWorld* World,bool Reload)
{
    auto Test=MakeShared<FPFBPPartialTest>();Test->World=World;Test->ReloadCrates=Reload;FTimerHandle Timer;
    World->GetTimerManager().SetTimer(Timer,FTimerDelegate::CreateLambda([Test]{if(Test->ReloadCrates)Test->RunReload();else Test->Run();}),15.f,false);
}
