#include "UObject/UnrealType.h"
// Included by PFBlueprintTest.cpp to reuse its native construction helpers.
#include "FGSaveSession.h"
#include "Buildables/FGBuildableGeneratorFuel.h"

namespace
{
    struct FPFSnappedGroup
    {
        FPFBPGroup Buildings;
        TWeakObjectPtr<AFGBuildableBlueprintDesigner> Designer;
        TMap<UClass*,int32> Cost;
        FString BlueprintName;
        int32 Tier=0, Width=0;
    };
    struct FPFSnappedBPTest : FPFBPTest
    {
        virtual ~FPFSnappedBPTest() = default;
        TArray<FPFSnappedGroup> Large;
        TArray<TWeakObjectPtr<AFGBuildable>> DesignerOwned;
        TArray<FString> Manifest;
        bool Reload=false;
        FString LargeManifestPath()const{return FPaths::ProjectSavedDir()/TEXT("TrapperPoweredFoundations-SnappedBlueprintManifest.txt");}
        void Later(void(FPFSnappedBPTest::*Method)())
        {
            FTimerHandle Timer;auto Self=StaticCastSharedRef<FPFSnappedBPTest>(AsShared());
            World->GetTimerManager().SetTimer(Timer,FTimerDelegate::CreateLambda([Self,Method]{(Self.Get().*Method)();}),4.f,false);
        }
        void Run()
        {
            auto* PC=World->GetFirstPlayerController();Player=PC?Cast<AFGCharacterPlayer>(PC->GetPawn()):nullptr;
            if(!Player.IsValid()||!Player->GetBuildGun()){Check(false,TEXT("Player/build gun available"));Finish();return;}
            Player->GetInventory()->GetInventoryStacks(InventoryBefore,true);
            if(Reload){ReadPlacedSave();return;}
            TArray<FString> Previous;
            Check(FFileHelper::LoadFileToStringArray(Previous,*(FPaths::ProjectSavedDir()/TEXT("TrapperPoweredFoundations-LargeBlueprintManifest.txt"))),TEXT("Reads previously validated Mk.2 and Mk.3 blueprint names"));
            for(const auto& Line:Previous)
            {
                TArray<FString> F;Line.ParseIntoArray(F,TEXT("|"),false);
                if(F.Num()==5&&F[0]==TEXT("GROUP"))for(int32 Copy=0;Copy<2;++Copy)
                {
                    FPFSnappedGroup G;G.Tier=FCString::Atoi(*F[2]);G.Width=FCString::Atoi(*F[3]);G.BlueprintName=F[4];
                    G.Buildings.Label=FString::Printf(TEXT("Mk.%d copy %d"),G.Tier,Copy+1);Large.Add(G);
                }
            }
            Check(Large.Num()==4,TEXT("Exercises two placed copies of both larger blueprint sizes"));
            if(Failures){Finish();return;}PlaceBlueprints();
        }
        bool Linked(const FPFSnappedGroup& G)
        {
            bool OK=G.Buildings.Tiles.Num()==G.Width;
            for(int32 I=0;I+1<G.Buildings.Tiles.Num();++I)
            {
                auto* A=Power(G.Buildings.Tiles[I]);auto* B=Power(G.Buildings.Tiles[I+1]);OK &= A&&B&&A->HasHiddenConnection(B);
            }
            return OK;
        }
        void PlaceBlueprints()
        {
            DesignerOwned=Owned;Owned.Reset();Designer=nullptr;
            auto* BS=AFGBlueprintSubsystem::Get(World.Get());BS->RefreshBlueprintsAndDescriptors();
            auto* MachineH=Hologram(TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_SmelterMk1"),FTransform(FVector(260000,-260000,50000)));
            UClass* MachineClass=MachineH?MachineH->GetBuildClass().Get():nullptr;if(MachineH)MachineH->Destroy();
            for(int32 GI=0;GI<Large.Num();++GI)
            {
                auto& G=Large[GI];
                const FString Base=BS->GetSessionBlueprintPath()/G.BlueprintName;
                Check(IFileManager::Get().FileSize(*(Base+TEXT(".sbp")))>0&&IFileManager::Get().FileSize(*(Base+TEXT(".sbpcfg")))>0,G.Buildings.Label+TEXT(" saves blueprint and metadata files"));
                auto* Desc=BS->GetBlueprintDescriptorByNameString(G.BlueprintName);
                Check(Desc&&BS->ReadBlueprintFromDisc(G.BlueprintName),G.Buildings.Label+TEXT(" reads saved blueprint from disk"));
                if(!Desc)continue;
                BS->SetActiveBlueprintHologramDescriptor(Desc);
                const float Yaw=G.Tier==2?0.f:37.f;FTransform TM(FRotator(0,Yaw,0),FVector(260000,-290000+(G.Tier-2)*20000,50000));
                auto* BlueprintRecipe=LoadObject<UClass>(nullptr,TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_Blueprint.Recipe_Blueprint_C"));
                // BeginPlay computes snap bounds from the descriptor. Supply it in the
                // native pre-spawn callback, before BeginPlay, as the build gun does.
                auto* H=BlueprintRecipe?Cast<AFGBlueprintHologram>(AFGHologram::SpawnHologramFromRecipe(BlueprintRecipe,Player->GetBuildGun(),TM.GetLocation(),Player.Get(),[Desc](AFGHologram* NewH){CastChecked<AFGBlueprintHologram>(NewH)->SetBlueprintDescriptor(Desc);})):nullptr;
                Check(H!=nullptr,G.Buildings.Label+TEXT(" creates native blueprint placement hologram"));if(!H)continue;
                H->SetActorTickEnabled(false);H->SetSuppressBuildEffect(true);H->SetActorTransform(TM);
                int32 Preview=0;for(auto* B:BS->GetBlueprintWorldBuildables())if(B->FindComponentByClass<UPFPowerConnection>())++Preview;
                Check(Preview==G.Width,G.Buildings.Label+TEXT(" previews the full-width powered floor"));
                if(GI%2==1)
                {
                    auto* Proxy=Large[GI-1].Buildings.Tiles.Num()?Large[GI-1].Buildings.Tiles[0]->GetBlueprintProxy():nullptr;
                    auto* Box=Proxy?Proxy->GetBoundingBox():nullptr;
                    auto* ModeProperty=FindFProperty<FClassProperty>(H->GetClass(),TEXT("mBlueprintSnapBuildMode"));
                    auto* Mode=ModeProperty?Cast<UClass>(ModeProperty->GetObjectPropertyValue_InContainer(H)):nullptr;
                    Check(Box&&Mode,G.Buildings.Label+TEXT(" finds real placed blueprint bounds and native snap build mode"));
                    if(!Box||!Mode){H->Destroy();continue;}
                    Check(FMath::Abs(Box->GetUnscaledBoxExtent().X*2-G.Width*800.f)<1.f,G.Buildings.Label+TEXT(" target snap bounds span the actual full foundation row"));
                    H->SetBuildModeOverride(Mode);
                    Check(H->ShouldBuildGunHitProxies(),G.Buildings.Label+TEXT(" enables blueprint-to-blueprint snap targeting"));
                    const FVector Normal=Box->GetComponentQuat().RotateVector(FVector::ForwardVector);
                    const FVector HitPoint=Box->GetComponentTransform().TransformPosition(FVector(Box->GetUnscaledBoxExtent().X,0,0));
                    FHitResult Hit(Proxy,Box,HitPoint,Normal);Hit.bBlockingHit=true;Hit.TraceStart=HitPoint+Normal*1000;Hit.TraceEnd=HitPoint-Normal*100;
                    H->PreHologramPlacement(Hit,false);
                    const bool Snapped=H->TrySnapToActor(Hit);
                    H->PostHologramPlacement(Hit,false);
                    Check(Snapped,G.Buildings.Label+TEXT(" native TrySnapToActor accepts the first placed blueprint side"));
                    TM=H->GetActorTransform();
                    Report+=FString::Printf(TEXT("DETAIL: %s snap position %s rotation %s target face %s normal %s\n"),*G.Buildings.Label,*TM.GetLocation().ToString(),*TM.Rotator().ToString(),*HitPoint.ToString(),*Normal.ToString());
                    if(!Snapped){H->Destroy();continue;}
                }
                auto* Builds=AFGBuildableSubsystem::Get(World.Get());TSet<AFGBuildable*> Before;for(auto* B:Builds->GetAllBuildablesRef())Before.Add(B);
                TArray<AActor*> Children;auto* Root=H->Construct(Children,FNetConstructionID());if(Root)Proxies.Add(Root);
                G.Buildings.Tiles.Reset();G.Buildings.Pole=nullptr;G.Buildings.Machine=nullptr;
                int32 Count=0;bool Context=true;
                for(auto* B:Builds->GetAllBuildablesRef())if(!Before.Contains(B))
                {
                    Owned.Add(B);++Count;Context &= !B->GetBlueprintDesigner()&&B->GetBlueprintProxy()!=nullptr;
                    if(B->FindComponentByClass<UPFPowerConnection>())G.Buildings.Tiles.Add(B);
                    else if(B->GetClass()==MachineClass)G.Buildings.Machine=B;
                    else if(B->GetClass()->GetName()==TEXT("Build_PowerPoleMk1_C"))G.Buildings.Pole=B;
                }
                G.Buildings.Tiles.Sort([TM](const auto& A,const auto& B){return TM.InverseTransformPosition(A->GetActorLocation()).X<TM.InverseTransformPosition(B->GetActorLocation()).X;});
                Check(Count==G.Width+2&&G.Buildings.Tiles.Num()==G.Width&&G.Buildings.Pole.IsValid()&&G.Buildings.Machine.IsValid(),G.Buildings.Label+TEXT(" constructs every saved building"));
                Check(Context,G.Buildings.Label+TEXT(" uses a blueprint proxy without retaining Designer ownership"));
                bool Geometry=G.Buildings.Tiles.Num()==G.Width;
                for(int32 I=0;I<G.Buildings.Tiles.Num();++I)
                {
                    auto* P=G.Buildings.Tiles[I]->FindComponentByClass<UPFPowerConnection>();Geometry &= P&&P->GetFName()==TEXT("PoweredFloorConnection")&&G.Buildings.Tiles[I]->GetActorQuat().Equals(TM.GetRotation(),.001);
                    if(I>0)Geometry &= PFGeometry::ShareSide(G.Buildings.Tiles[I-1]->FindComponentByClass<UPFPowerConnection>()->GetTile(),P->GetTile());
                }
                Check(Geometry,G.Buildings.Label+TEXT(" retains stable components, rotation and full-width side contact"));
                H->Destroy();
            }
            BS->SetActiveBlueprintHologramDescriptor(nullptr);
            for(int32 I=DesignerOwned.Num()-1;I>=0;--I)if(DesignerOwned[I].IsValid())DesignerOwned[I]->TurnOffAndDestroy();DesignerOwned.Reset();
            Later(&FPFSnappedBPTest::SupplyCopies);
        }
        void SupplyCopies()
        {
            auto* Fuel=LoadObject<UClass>(nullptr,TEXT("/Game/FactoryGame/Resource/Parts/BioFuel/Desc_Biofuel.Desc_Biofuel_C"));
            for(int32 GI=0;GI<Large.Num();GI+=2)
            {
                auto& G=Large[GI];
                if(!G.Buildings.Pole.IsValid()){Check(false,G.Buildings.Label+TEXT(" has a feed pole"));continue;}
                const FVector P=G.Buildings.Pole->GetActorLocation()+FVector(-5000,-5000,0);
                G.Buildings.Source=Build(TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_GeneratorBiomass_Automated"),FTransform(P));
                auto* Generator=Cast<AFGBuildableGeneratorFuel>(G.Buildings.Source.Get());
                bool Fueled=Generator&&Fuel&&Generator->IsValidFuel(Fuel)&&Generator->GetFuelInventory();
                if(Fueled)Fueled=Generator->GetFuelInventory()->AddStackToIndex(Generator->GetFuelInventoryIndex(),FInventoryStack(100,Fuel),true)==100;
                Check(Fueled,G.Buildings.Label+TEXT(" has an ordinary generator with saved fuel, not a synthetic supply"));
                auto* WireClass=LoadObject<UClass>(nullptr,TEXT("/Game/FactoryGame/Buildable/Factory/PowerLine/Build_PowerLine.Build_PowerLine_C"));auto* Builds=AFGBuildableSubsystem::Get(World.Get());
                auto* Cable=WireClass?Builds->BeginSpawnBuildable(WireClass,FTransform(P)):nullptr;
                if(Cable){Cable->FinishSpawning(FTransform(P));if(!Builds->GetAllBuildablesRef().Contains(Cable))Builds->AddBuildable(Cable);Owned.Add(Cable);}G.Buildings.Wire=Cable;
                auto* W=Cast<AFGBuildableWire>(Cable);Check(W&&Power(G.Buildings.Source)&&W->Connect(Power(G.Buildings.Source),Power(G.Buildings.Pole)),G.Buildings.Label+TEXT(" connects generator to placed blueprint with a real cable"));
            }
            Later(&FPFSnappedBPTest::VerifyAndSave);
        }
        void VerifyPower(const FString& Phase)
        {
            for(int32 I=0;I+1<Large.Num();I+=2)
            {
                const auto& A=Large[I];const auto& B=Large[I+1];auto* S=Power(A.Buildings.Source);auto* P=Power(A.Buildings.Pole);
                bool Cross=false,Geometry=false;int32 CrossCount=0;
                for(auto X:A.Buildings.Tiles)for(auto Y:B.Buildings.Tiles)
                {
                    auto* C=X->FindComponentByClass<UPFPowerConnection>();auto* D=Y->FindComponentByClass<UPFPowerConnection>();
                    Geometry |= C&&D&&PFGeometry::ShareSide(C->GetTile(),D->GetTile());
                    if(C&&D&&C->HasHiddenConnection(D)){Cross=true;++CrossCount;}
                }
                Check(Geometry,A.Buildings.Label+Phase+TEXT(" native snap creates touching foundation side faces across blueprint boundary"));
                Check(Cross&&CrossCount==1,A.Buildings.Label+Phase+TEXT(" has exactly one hidden floor link across the two blueprint proxies"));
                Check(A.Buildings.Tiles.Num()&&B.Buildings.Tiles.Num()&&A.Buildings.Tiles[0]->GetBlueprintProxy()!=B.Buildings.Tiles[0]->GetBlueprintProxy(),A.Buildings.Label+Phase+TEXT(" retains separate placed blueprint identities"));
                Check(!B.Buildings.Source.IsValid()&&!B.Buildings.Wire.IsValid(),B.Buildings.Label+Phase+TEXT(" has no independent generator or feed cable"));
                for(int32 J=I;J<I+2;++J)
                {
                    const auto& G=Large[J];bool All=S&&P&&S->HasPower()&&G.Buildings.Tiles.Num()==G.Width;bool Glow=All;
                    for(auto T:G.Buildings.Tiles){auto* C=Power(T);All &= C&&P&&C->HasPower()&&C->GetCircuitID()==P->GetCircuitID();Glow &= T.IsValid()&&T->FindComponentByClass<UPFPowerConnection>()->IsGridPowered();}
                    Check(All,G.Buildings.Label+Phase+TEXT(" powers every floor from the single generator on copy 1"));
                    auto* M=Power(G.Buildings.Machine);auto* Pole=Power(G.Buildings.Pole);
                    Check(M&&Pole&&P&&M->HasPower()&&Pole->HasPower()&&M->GetCircuitID()==P->GetCircuitID()&&Pole->GetCircuitID()==P->GetCircuitID(),G.Buildings.Label+Phase+TEXT(" powers foundry and pole through floor connections"));
                    Check(Linked(G)&&Glow,G.Buildings.Label+Phase+TEXT(" preserves internal floor links and energized rims"));
                }
                auto* W=Cast<AFGBuildableWire>(A.Buildings.Wire.Get());Check(W&&P&&S&&W->GetOppositeConnection(P)==S,A.Buildings.Label+Phase+TEXT(" preserves the only feed cable endpoints"));
            }
            Check(Large.Num()==4&&Power(Large[0].Buildings.Pole)&&Power(Large[2].Buildings.Pole)&&Power(Large[0].Buildings.Pole)->GetCircuitID()!=Power(Large[2].Buildings.Pole)->GetCircuitID(),Phase+TEXT(" keeps the two test pairs electrically isolated"));
        }
        void RecordActor(int32 Group,const TCHAR* Role,TWeakObjectPtr<AFGBuildable> B)
        {
            if(!B.IsValid())return;const FVector P=B->GetActorLocation();const float Yaw=B->GetActorRotation().Yaw;
            Manifest.Add(FString::Printf(TEXT("ACTOR|%d|%s|%s|%s|%.6f|%.6f|%.6f|%.6f|%s"),Group,Role,*B->GetName(),*B->GetClass()->GetPathName(),P.X,P.Y,P.Z,Yaw,*GetNameSafe(B->GetBlueprintProxy())));
        }
        virtual void VerifyAndSave()
        {
            VerifyPower(TEXT(" before save"));if(Failures>0){Finish();return;}
            Name=TEXT("PF_SnappedBlueprint_")+FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));Manifest.Add(TEXT("SAVE|")+Name);
            for(int32 I=0;I<Large.Num();++I)
            {
                const auto& G=Large[I];Manifest.Add(FString::Printf(TEXT("GROUP|%d|%d|%d|%s"),I,G.Tier,G.Width,*G.BlueprintName));
                for(auto T:G.Buildings.Tiles)RecordActor(I,TEXT("TILE"),T);
                RecordActor(I,TEXT("POLE"),G.Buildings.Pole);RecordActor(I,TEXT("MACHINE"),G.Buildings.Machine);RecordActor(I,TEXT("SOURCE"),G.Buildings.Source);RecordActor(I,TEXT("WIRE"),G.Buildings.Wire);
            }
            for(int32 I=0;I<InventoryBefore.Num();++I){const auto& S=InventoryBefore[I];Manifest.Add(FString::Printf(TEXT("ITEM|%d|%s|%d"),I,S.HasItems()?*S.Item.GetItemClass()->GetPathName():TEXT("EMPTY"),S.HasItems()?S.NumItems:0));}
            Check(FFileHelper::SaveStringArrayToFile(Manifest,*LargeManifestPath()),TEXT("Records exact placed actor identities, transforms, proxies and inventory"));
            Report+=TEXT("Separate persistence save: ")+Name+TEXT("\n");
            auto Self=StaticCastSharedRef<FPFSnappedBPTest>(AsShared());
            UFGSaveSession::Get(World.Get())->SaveGame(Name,FOnSaveGameComplete::CreateLambda([Self](bool OK,const FText& Error,void*){Self->Check(OK,TEXT("Saves powered placed blueprints, fueled generators and cables: ")+Error.ToString());Self->Finish();}),nullptr);
        }
        void ReadPlacedSave()
        {
            Check(FFileHelper::LoadFileToStringArray(Manifest,*LargeManifestPath()),TEXT("Reads placed-blueprint persistence manifest"));
            TMap<FString,AFGBuildable*> Actors;for(auto* B:AFGBuildableSubsystem::Get(World.Get())->GetAllBuildablesRef())Actors.Add(B->GetName(),B);
            bool InventoryOK=true;int32 ItemRecords=0;
            for(const auto& Line:Manifest)
            {
                TArray<FString> F;Line.ParseIntoArray(F,TEXT("|"),false);
                if(F.Num()==2&&F[0]==TEXT("SAVE"))Name=F[1];
                if(F.Num()==5&&F[0]==TEXT("GROUP"))
                {
                    const int32 I=FCString::Atoi(*F[1]);if(I<0||I>3)continue;Large.SetNum(FMath::Max(Large.Num(),I+1));auto& G=Large[I];G.Tier=FCString::Atoi(*F[2]);G.Width=FCString::Atoi(*F[3]);G.BlueprintName=F[4];G.Buildings.Label=FString::Printf(TEXT("Mk.%d copy %d"),G.Tier,I%2+1);
                }
                if(F.Num()==10&&F[0]==TEXT("ACTOR"))
                {
                    const int32 I=FCString::Atoi(*F[1]);if(!Large.IsValidIndex(I))continue;auto& G=Large[I];auto* B=Actors.FindRef(F[3]);
                    Check(B!=nullptr,G.Buildings.Label+TEXT(" reloads saved ")+F[2]+TEXT(" actor ")+F[3]);if(!B)continue;Owned.Add(B);
                    const FVector P(FCString::Atod(*F[5]),FCString::Atod(*F[6]),FCString::Atod(*F[7]));
                    Check(B->GetClass()->GetPathName()==F[4]&&B->GetActorLocation().Equals(P,.1)&&FMath::Abs(FMath::FindDeltaAngleDegrees(B->GetActorRotation().Yaw,FCString::Atof(*F[8])))<.01&&GetNameSafe(B->GetBlueprintProxy())==F[9]&&!B->GetBlueprintDesigner(),G.Buildings.Label+TEXT(" preserves class, transform, proxy and ownership for ")+F[2]);
                    if(B->GetBlueprintProxy())Proxies.AddUnique(B->GetBlueprintProxy());
                    if(F[2]==TEXT("TILE")){G.Buildings.Tiles.Add(B);auto* C=B->FindComponentByClass<UPFPowerConnection>();Check(C&&C->GetFName()==TEXT("PoweredFloorConnection"),G.Buildings.Label+TEXT(" retains the stable saved floor power component"));}
                    else if(F[2]==TEXT("POLE"))G.Buildings.Pole=B;else if(F[2]==TEXT("MACHINE"))G.Buildings.Machine=B;else if(F[2]==TEXT("SOURCE"))G.Buildings.Source=B;else if(F[2]==TEXT("WIRE"))G.Buildings.Wire=B;
                }
                if(F.Num()==4&&F[0]==TEXT("ITEM"))
                {
                    ++ItemRecords;const int32 I=FCString::Atoi(*F[1]);FInventoryStack S;Player->GetInventory()->GetStackFromIndex(I,S);InventoryOK &= F[2]==TEXT("EMPTY")?!S.HasItems():S.HasItems()&&S.Item.GetItemClass()->GetPathName()==F[2]&&S.NumItems==FCString::Atoi(*F[3]);
                }
            }
            Check(ItemRecords==InventoryBefore.Num()&&InventoryOK,TEXT("Reload retains every original player inventory slot and count"));
            Check(Large.Num()==4,TEXT("Reload identifies both snapped blueprint pairs"));
            for(const auto& G:Large)
            {
                auto* Generator=Cast<AFGBuildableGeneratorFuel>(G.Buildings.Source.Get());if(G.Buildings.Source.IsValid())Check(Generator&&Generator->HasFuel(),G.Buildings.Label+TEXT(" reloads an ordinary fueled generator without refilling or injecting power"));
                Check(G.Buildings.Tiles.Num()==G.Width,G.Buildings.Label+TEXT(" retains the entire larger floor row"));
            }
            VerifyPower(TEXT(" after reload"));
            if(Failures>0){Finish();return;}
            for(auto& G:Large)if(auto* W=Cast<AFGBuildableWire>(G.Buildings.Wire.Get()))W->Disconnect();
            Later(&FPFSnappedBPTest::VerifyOff);
        }
        void VerifyOff()
        {
            for(auto& G:Large)
            {
                bool Off=G.Buildings.Tiles.Num()==G.Width;for(auto T:G.Buildings.Tiles)Off &= Power(T)&&!Power(T)->HasPower()&&!T->FindComponentByClass<UPFPowerConnection>()->IsGridPowered();
                Off &= Power(G.Buildings.Machine)&&!Power(G.Buildings.Machine)->HasPower();Check(Off,G.Buildings.Label+TEXT(" responds to disconnecting its cable after reload"));
                auto* W=Cast<AFGBuildableWire>(G.Buildings.Wire.Get());if(W)Check(W->Connect(Power(G.Buildings.Source),Power(G.Buildings.Pole)),G.Buildings.Label+TEXT(" reconnects its saved generator cable"));
            }
            Later(&FPFSnappedBPTest::VerifyRestored);
        }
        void VerifyRestored(){VerifyPower(TEXT(" after reconnect"));Finish();}
        virtual void Finish()
        {
            for(int32 I=DesignerOwned.Num()-1;I>=0;--I)if(DesignerOwned[I].IsValid()&&!DesignerOwned[I]->IsActorBeingDestroyed())DesignerOwned[I]->TurnOffAndDestroy();
            for(int32 I=Owned.Num()-1;I>=0;--I)if(Owned[I].IsValid()&&!Owned[I]->IsActorBeingDestroyed())Owned[I]->TurnOffAndDestroy();
            for(auto P:Proxies)if(P.IsValid()&&!P->IsActorBeingDestroyed())P->Destroy();
            if(Player.IsValid())
            {
                auto* Inv=Player->GetInventory();bool Same=Inv->GetSizeLinear()==InventoryBefore.Num();for(int32 I=0;I<InventoryBefore.Num();++I){FInventoryStack S;Inv->GetStackFromIndex(I,S);const auto& B=InventoryBefore[I];Same &= S.HasItems()==B.HasItems()&&(!B.HasItems()||(S.Item.GetItemClass()==B.Item.GetItemClass()&&S.NumItems==B.NumItems));}Check(Same,TEXT("Player inventory remains unchanged after test cleanup"));
            }
            for(const auto& G:Large)Report+=TEXT("Blueprint: ")+G.BlueprintName+TEXT("\n");Report+=FString::Printf(TEXT("Failures: %d\n"),Failures);
            FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/(Reload?TEXT("TrapperPoweredFoundations-SnappedBlueprintReloadTest.txt"):TEXT("TrapperPoweredFoundations-SnappedBlueprintTest.txt"))));
            UE_LOG(LogPFBlueprintTest,Display,TEXT("Snapped blueprint %s complete: %d failures"),Reload?TEXT("reload"):TEXT("save"),Failures);
        }
    };
}
void PFStartSnappedBlueprintTest(UWorld* World,bool Reload)
{
    auto Test=MakeShared<FPFSnappedBPTest>();Test->World=World;Test->Reload=Reload;FTimerHandle Timer;
    World->GetTimerManager().SetTimer(Timer,FTimerDelegate::CreateLambda([Test]{Test->Run();}),15.f,false);
}
