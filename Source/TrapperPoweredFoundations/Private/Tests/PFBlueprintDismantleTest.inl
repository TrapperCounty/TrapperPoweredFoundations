// Reuses native pre-spawn initialization, snapping and fueled pair fixtures.
#include "FGDismantleInterface.h"
#include "Equipment/FGBuildGunDismantle.h"
namespace
{
    struct FPFBPDismantleTest : FPFSnappedBPTest
    {
        int32 Pass=0;
        bool RequestSent=false;
        TMap<UClass*,int32> ExpectedRefund;
        TArray<TWeakObjectPtr<AActor>> Victims;
        TArray<TWeakObjectPtr<AFGBlueprintProxy>> RemovedProxies,SurvivingProxies;
        TMap<TWeakObjectPtr<AFGBuildable>,FTransform> SurvivorTransforms;
        void Delay(void(FPFBPDismantleTest::*Method)())
        {
            FTimerHandle Timer;auto Self=StaticCastSharedRef<FPFBPDismantleTest>(AsShared());
            World->GetTimerManager().SetTimer(Timer,FTimerDelegate::CreateLambda([Self,Method]{(Self.Get().*Method)();}),4.f,false);
        }
        bool Equal(const TMap<UClass*,int32>& A,const TMap<UClass*,int32>& B)const
        {if(A.Num()!=B.Num())return false;for(const auto& I:A)if(B.FindRef(I.Key)!=I.Value)return false;return true;}
        TMap<UClass*,int32> Totals(const TArray<FInventoryStack>& Stacks)const
        {TMap<UClass*,int32> Result;for(const auto& S:Stacks)if(S.HasItems())Result.FindOrAdd(S.Item.GetItemClass())+=S.NumItems;return Result;}
        FString Label()const{return Pass==0?TEXT("Remove unfed copy"):TEXT("Remove fed copy");}
        bool Gone(AActor* A)const{return !IsValid(A)||A->IsActorBeingDestroyed()||(Cast<AFGBuildable>(A)&&Cast<AFGBuildable>(A)->GetIsDismantled());}
        virtual void VerifyAndSave() override
        {
            VerifyPower(TEXT(" before dismantle"));if(Failures){Finish();return;}
            ExpectedRefund.Reset();Victims.Reset();RemovedProxies.Reset();SurvivingProxies.Reset();SurvivorTransforms.Reset();
            auto* BS=AFGBlueprintSubsystem::Get(World.Get());
            struct FRequest{TArray<AActor*> selectedActors;TArray<FDismantleLightweightBundle> lightweightBundles;} Request;
            for(int32 I=0;I<Large.Num();I+=2)
            {
                const int32 VictimIndex=I+(Pass==0?1:0),KeepIndex=I+(Pass==0?0:1);
                auto& G=Large[VictimIndex];auto& Keep=Large[KeepIndex];
                auto* Proxy=G.Buildings.Tiles[0]->GetBlueprintProxy();auto* KeepProxy=Keep.Buildings.Tiles[0]->GetBlueprintProxy();
                Check(Proxy&&KeepProxy&&Proxy!=KeepProxy,Label()+G.Buildings.Label+TEXT(" selects only the intended blueprint proxy"));
                if(!Proxy)continue;
                RemovedProxies.Add(Proxy);SurvivingProxies.Add(KeepProxy);Victims.Add(Proxy);Request.selectedActors.Add(Proxy);
                Check(IFGDismantleInterface::Execute_CanDismantle(Proxy),G.Buildings.Label+TEXT(" allows whole-blueprint dismantling"));
                TMap<UClass*,int32> Cost,Refund;
                TMap<UClass*,int32> CableCost;
                if(Pass==1)
                {
                    auto* W=Large[I].Buildings.Wire.Get();
                    if(W)
                    {
                        W->SetBuiltWithRecipe(LoadObject<UClass>(nullptr,TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerLine.Recipe_PowerLine_C")));
                        TArray<FInventoryStack> CableRefund;IFGDismantleInterface::Execute_GetDismantleRefund(W,CableRefund,false);
                        Check(!CableRefund.IsEmpty(),G.Buildings.Label+TEXT(" connected external cable has a material refund"));
                        for(const auto& S:CableRefund)CableCost.FindOrAdd(S.Item.GetItemClass())+=S.NumItems;
                        Victims.Add(W);
                    }
                }
                const auto Buildings=Proxy->GetBuildables();
                Check(Buildings.Num()==G.Width+2,G.Buildings.Label+TEXT(" proxy contains exactly its floors, pole and foundry"));
                for(auto* B:Buildings)
                {
                    Victims.Add(B);
                    auto Recipe=B->GetBuiltWithRecipe();
                    auto* H=Recipe?AFGHologram::SpawnHologramFromRecipe(Recipe,Player->GetBuildGun(),B->GetActorLocation(),Player.Get()):nullptr;
                    Check(H!=nullptr,G.Buildings.Label+TEXT(" resolves complete construction cost for ")+B->GetClass()->GetName());
                    if(H){for(const auto& Item:H->GetCost(false))Cost.FindOrAdd(Item.ItemClass)+=Item.Amount;H->Destroy();}
                    TArray<FInventoryStack> R;IFGDismantleInterface::Execute_GetDismantleRefund(B,R,false);
                    for(const auto& S:R)Refund.FindOrAdd(S.Item.GetItemClass())+=S.NumItems;
                    TArray<FInventoryStack> Free;IFGDismantleInterface::Execute_GetDismantleRefund(B,Free,true);
                    Check(Free.IsEmpty(),G.Buildings.Label+TEXT(" free-build refund excludes construction materials for ")+B->GetClass()->GetName());
                }
                // Native pole refunds include attached cables. Count those once,
                // while still comparing blueprint-only cost against its descriptor.
                auto CompleteCost=Cost;for(const auto& Item:CableCost)CompleteCost.FindOrAdd(Item.Key)+=Item.Value;
                for(const auto& Item:Refund)Report+=FString::Printf(TEXT("DETAIL: %s native refund %s x%d (expected %d)\n"),*G.Buildings.Label,*Item.Key->GetName(),Item.Value,CompleteCost.FindRef(Item.Key));
                Check(Equal(CompleteCost,Refund),G.Buildings.Label+TEXT(" aggregate refund equals complete costs including extra foundation wire and dependent cable"));
                auto* Desc=BS->GetBlueprintDescriptorByNameString(G.BlueprintName);TArray<FItemAmount> BlueprintCost;if(Desc)Desc->GetBlueprintCost(BlueprintCost);
                TMap<UClass*,int32> DescriptorCost;for(const auto& Item:BlueprintCost)DescriptorCost.FindOrAdd(Item.ItemClass)+=Item.Amount;
                Check(Desc&&Equal(Cost,DescriptorCost),G.Buildings.Label+TEXT(" refund matches saved blueprint cost"));
                for(const auto& Item:CompleteCost)ExpectedRefund.FindOrAdd(Item.Key)+=Item.Value;
                for(auto* B:KeepProxy->GetBuildables())SurvivorTransforms.Add(B,B->GetActorTransform());
            }
            auto* Inv=Player->GetInventory();TArray<FInventoryStack> RefundStacks;
            for(const auto& Item:ExpectedRefund){RefundStacks.Add(FInventoryStack(Item.Value,Item.Key));Report+=FString::Printf(TEXT("DETAIL: %s expected %s x%d\n"),*Label(),*Item.Key->GetName(),Item.Value);}
            const bool Space=Inv->HasEnoughSpaceForStacks(RefundStacks);Check(Space,Label()+TEXT(" inventory has room for complete refunds"));
            auto* State=Player->GetBuildGun()->GetBuildGunStateFor(EBuildGunState::BGS_DISMANTLE);auto* Function=State?State->FindFunction(TEXT("Server_DismantleActors")):nullptr;
            Check(Function&&Request.selectedActors.Num()==2,Label()+TEXT(" submits two whole blueprint proxies to the native dismantle handler"));
            if(Failures||!Space||!Function){Finish();return;}
            RequestSent=true;State->ProcessEvent(Function,&Request);Delay(&FPFBPDismantleTest::VerifyRemoved);
        }
        void VerifySurvivors(bool Powered)
        {
            for(int32 I=0;I<Large.Num();I+=2)
            {
                auto& G=Large[I+(Pass==0?0:1)];auto* Proxy=SurvivingProxies.IsValidIndex(I/2)?SurvivingProxies[I/2].Get():nullptr;
                Check(Proxy&&!Gone(Proxy)&&Proxy->GetBuildables().Num()==G.Width+2,G.Buildings.Label+TEXT(" surviving blueprint proxy retains every member"));
                bool Stable=true,Clean=true,PowerOK=true,Glow=true;
                TSet<AActor*> Allowed;for(auto T:G.Buildings.Tiles)Allowed.Add(T.Get());Allowed.Add(G.Buildings.Pole.Get());Allowed.Add(G.Buildings.Machine.Get());
                for(auto* B:Allowed)
                {
                    auto* Buildable=Cast<AFGBuildable>(B);auto* Original=SurvivorTransforms.Find(Buildable);
                    Stable &= Buildable&&!Gone(Buildable)&&Original&&Buildable->GetActorTransform().Equals(*Original,.01)&&Buildable->GetBlueprintProxy()==Proxy;
                    auto* P=Buildable?Power(Buildable):nullptr;PowerOK &= P&&P->HasPower()==Powered;
                    TArray<UFGCircuitConnectionComponent*> Links;if(P)P->GetHiddenConnections(Links);
                    for(auto* C:Links)Clean &= IsValid(C)&&Allowed.Contains(C->GetOwner())&&!Gone(C->GetOwner());
                }
                for(auto T:G.Buildings.Tiles)Glow &= T.IsValid()&&T->FindComponentByClass<UPFPowerConnection>()->IsGridPowered()==Powered;
                Check(Stable,G.Buildings.Label+TEXT(" surviving actors retain transforms and proxy membership"));
                Check(Clean&&Linked(G),G.Buildings.Label+TEXT(" retains internal links without stale cross-blueprint connections"));
                Check(PowerOK&&Glow,G.Buildings.Label+(Powered?TEXT(" remains powered throughout with energized rims"):TEXT(" loses power throughout when its only feed path is dismantled")));
                auto* Source=Power(Large[I].Buildings.Source);auto* Generator=Cast<AFGBuildableGeneratorFuel>(Large[I].Buildings.Source.Get());
                Check(Generator&&!Gone(Generator)&&Generator->HasFuel(),G.Buildings.Label+TEXT(" external fueled generator survives dismantling"));
                if(Powered)
                {
                    bool Circuit=Source!=nullptr;for(auto* B:Allowed){auto* C=Power(Cast<AFGBuildable>(B));Circuit &= C&&Source&&C->GetCircuitID()==Source->GetCircuitID();}
                    Check(Circuit,G.Buildings.Label+TEXT(" all surviving terminals share the real generator circuit"));
                }
            }
        }
        void VerifyRemoved()
        {
            bool Removed=true;for(auto V:Victims)Removed &= Gone(V.Get());
            Check(Removed,Label()+TEXT(" destroys selected proxies, their buildings and dependent cables only"));
            TArray<FInventoryStack> Now;Player->GetInventory()->GetInventoryStacks(Now,true);
            auto Expected=Totals(InventoryBefore);for(const auto& I:ExpectedRefund)Expected.FindOrAdd(I.Key)+=I.Value;
            Check(Equal(Totals(Now),Expected),Label()+TEXT(" native inventory delta matches every expected refund exactly, without extras"));
            RestoreRefunds();VerifySurvivors(Pass==0);
            if(Failures){Finish();return;}
            if(Pass==0)
            {
                Check(Large[0].Buildings.Wire.IsValid()&&Large[2].Buildings.Wire.IsValid(),TEXT("Removing unfed copies preserves both existing feed cables"));
                Cleanup();++Pass;
                for(auto& G:Large){const FString SavedLabel=G.Buildings.Label;G.Buildings=FPFBPGroup();G.Buildings.Label=SavedLabel;}
                PlaceBlueprints();
            }
            else
            {
                for(int32 I=0;I<Large.Num();I+=2)
                {
                    auto* WClass=LoadObject<UClass>(nullptr,TEXT("/Game/FactoryGame/Buildable/Factory/PowerLine/Build_PowerLine.Build_PowerLine_C"));
                    auto* W=Cast<AFGBuildableWire>(AFGBuildableSubsystem::Get(World.Get())->BeginSpawnBuildable(WClass,FTransform(Large[I].Buildings.Source->GetActorLocation())));
                    if(W){W->FinishSpawning(FTransform(Large[I].Buildings.Source->GetActorLocation()));Owned.Add(W);}
                    Check(W&&W->Connect(Power(Large[I].Buildings.Source),Power(Large[I+1].Buildings.Pole)),Large[I+1].Buildings.Label+TEXT(" accepts a replacement feed cable"));
                }
                Delay(&FPFBPDismantleTest::VerifyReconnected);
            }
        }
        void VerifyReconnected(){VerifySurvivors(true);Finish();}
        void RestoreRefunds()
        {
            if(!RequestSent||!Player.IsValid())return;auto* Inv=Player->GetInventory();bool Same=Inv->GetSizeLinear()==InventoryBefore.Num();
            for(int32 I=0;I<InventoryBefore.Num();++I)
            {
                FInventoryStack Now;Inv->GetStackFromIndex(I,Now);const auto& Was=InventoryBefore[I];
                if(Was.HasItems()&&Now.Item.GetItemClass()!=Was.Item.GetItemClass()){Same=false;continue;}
                const int32 Before=Was.HasItems()?Was.NumItems:0,Current=Now.HasItems()?Now.NumItems:0;
                if(Current>Before)Inv->RemoveFromIndex(I,Current-Before);
                Same &= Current>=Before;Inv->GetStackFromIndex(I,Now);
                Same &= Now.HasItems()==Was.HasItems()&&(!Was.HasItems()||Now.NumItems==Was.NumItems);
            }
            Check(Same,Label()+TEXT(" removes test refunds and restores every original inventory slot"));RequestSent=false;
        }
        void Cleanup()
        {
            for(int32 I=Owned.Num()-1;I>=0;--I)if(!Gone(Owned[I].Get()))Owned[I]->TurnOffAndDestroy();
            for(auto P:Proxies)if(!Gone(P.Get()))P->Destroy();Owned.Reset();Proxies.Reset();
        }
        virtual void Finish() override
        {
            RestoreRefunds();Cleanup();
            if(Player.IsValid())
            {
                TArray<FInventoryStack> Now;Player->GetInventory()->GetInventoryStacks(Now,true);
                bool Same=Now.Num()==InventoryBefore.Num();for(int32 I=0;I<Now.Num()&&I<InventoryBefore.Num();++I){const auto& A=Now[I];const auto& B=InventoryBefore[I];Same &= A.HasItems()==B.HasItems()&&(!A.HasItems()||(A.Item.GetItemClass()==B.Item.GetItemClass()&&A.NumItems==B.NumItems));}
                Check(Same,TEXT("Final player inventory is unchanged in every slot"));
            }
            Report+=FString::Printf(TEXT("Failures: %d\n"),Failures);
            FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("TrapperPoweredFoundations-BlueprintDismantleTest.txt")));
            UE_LOG(LogPFBlueprintTest,Display,TEXT("Blueprint dismantle test complete: %d failures"),Failures);
        }
    };
}
void PFStartBlueprintDismantleTest(UWorld* World)
{
    auto Test=MakeShared<FPFBPDismantleTest>();Test->World=World;FTimerHandle Timer;
    World->GetTimerManager().SetTimer(Timer,FTimerDelegate::CreateLambda([Test]{Test->Run();}),15.f,false);
}
