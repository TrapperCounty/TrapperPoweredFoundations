// Opt-in full-volume and nested Blueprint Designer QA; no player costs deducted.
namespace
{
    struct FPFDenseGroup
    {
        int32 Tier=0,Width=0,Layers=0;
        FString Label,BlueprintName;
        FTransform Placement;
        FVector Lower;
        TWeakObjectPtr<AFGBuildableBlueprintDesigner> DesignerActor;
        TArray<TWeakObjectPtr<AFGBuildable>> Tiles;
        TMap<UClass*,int32> Cost;
        TWeakObjectPtr<AFGBuildable> FeedPole,Generator,Cable;
    };
    struct FPFDenseBPTest : FPFBPTest
    {
        TArray<FPFDenseGroup> Dense;
        TArray<TWeakObjectPtr<AFGBuildable>> DesignerActors;
        TArray<FString> DenseManifest;
        int32 Current=0,TileIndex=0;
        bool ReloadDense=false;
        FString DensePath()const{return FPaths::ProjectSavedDir()/TEXT("TrapperPoweredFoundations-DenseBlueprintManifest.txt");}
        void Next(void(FPFDenseBPTest::*Fn)(),float Seconds=5)
        {FTimerHandle T;auto Self=StaticCastSharedRef<FPFDenseBPTest>(AsShared());World->GetTimerManager().SetTimer(T,FTimerDelegate::CreateLambda([Self,Fn]{(Self.Get().*Fn)();}),Seconds,false);}
        bool CostSame(const TMap<UClass*,int32>& A,const TMap<UClass*,int32>& B)const
        {if(A.Num()!=B.Num())return false;for(const auto& I:A)if(B.FindRef(I.Key)!=I.Value)return false;return true;}
        void Begin()
        {
            auto* PC=World->GetFirstPlayerController();Player=PC?Cast<AFGCharacterPlayer>(PC->GetPawn()):nullptr;
            if(!Player.IsValid()||!Player->GetBuildGun()){Check(false,TEXT("Player and build gun available"));FinishDense();return;}
            Player->GetInventory()->GetInventoryStacks(InventoryBefore,true);UFGSaveSession::Get(World.Get())->SetAutoSaveEnabled(false);
            if(ReloadDense){ReloadSaved();return;}
            for(int32 Tier=1;Tier<=3;++Tier)
            {
                FPFDenseGroup G;G.Tier=Tier;G.Label=FString::Printf(TEXT("Dense Mk.%d"),Tier);
                const FString Recipe=Tier==1?TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_BlueprintDesigner"):FString::Printf(TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_BlueprintDesigner_Mk%d"),Tier);
                Designer=Cast<AFGBuildableBlueprintDesigner>(Build(Recipe,FTransform(FVector(160000,-240000+Tier*20000,60000))));G.DesignerActor=Designer;
                Check(Designer.IsValid(),G.Label+TEXT(" constructs native Designer"));if(!Designer.IsValid())continue;
                const auto D=Designer->GetBlueprintDimensions();G.Width=D.X;G.Layers=D.Z*2;
                auto* Box=Designer->GetCollisionComponent();G.Lower=Box->GetComponentLocation()-Box->GetScaledBoxExtent();
                Check(D.X==Tier+3&&D.Y==D.X&&D.Z==D.X,G.Label+TEXT(" uses actual native build volume"));
                Report+=FString::Printf(TEXT("DETAIL: %s volume=%s box=%s lower=%s tiles=%d\n"),*G.Label,*D.ToString(),*Box->GetScaledBoxExtent().ToString(),*G.Lower.ToString(),G.Width*G.Width*G.Layers);
                Dense.Add(G);
            }
            if(Failures||Dense.Num()!=3){FinishDense();return;}ExpectedCost.Reset();BuildChunk();
        }
        void BuildChunk()
        {
            if(Current>=3){Next(&FPFDenseBPTest::SaveDense);return;}
            auto& G=Dense[Current];Designer=G.DesignerActor;const int32 N=G.Width*G.Width*G.Layers,End=FMath::Min(TileIndex+32,N);
            for(;TileIndex<End;++TileIndex)
            {
                const int32 X=TileIndex%G.Width,Y=(TileIndex/G.Width)%G.Width,Z=TileIndex/(G.Width*G.Width);
                const FVector P=G.Lower+FVector(400+X*800,400+Y*800,200+Z*400);
                auto* B=Build(TEXT("/TrapperPoweredFoundations/Recipes/Recipe_PoweredFoundation_4m"),FTransform(P),true);
                if(B)G.Tiles.Add(B);else Check(false,G.Label+TEXT(" constructs dense tile"));
            }
            if(TileIndex==N)
            {
                G.Cost=ExpectedCost;ExpectedCost.Reset();TileIndex=0;++Current;
                Check(G.Tiles.Num()==N&&G.DesignerActor->GetBuildablesInBlueprintDesigner().Num()==N,G.Label+TEXT(" fills full width, depth and height with 4m foundations"));
            }
            Next(&FPFDenseBPTest::BuildChunk,1);
        }
        bool InspectGrid(FPFDenseGroup& G,const FString& Phase,bool CheckPower=false,bool FeedOn=true)
        {
            bool Shape=G.Tiles.Num()==G.Width*G.Width*G.Layers,Links=true,Isolated=true,PowerOK=true;
            FVector Min(UE_BIG_NUMBER);for(auto T:G.Tiles)if(T.IsValid())Min=Min.ComponentMin(G.Placement.InverseTransformPosition(T->GetActorLocation()));
            TMap<FIntVector,AFGBuildable*> Map;TSet<AFGBuildable*> Members;
            for(auto T:G.Tiles)
            {
                if(!T.IsValid()){Shape=false;continue;}const FVector P=G.Placement.InverseTransformPosition(T->GetActorLocation())-Min;
                const FIntVector Key(FMath::RoundToInt(P.X/800),FMath::RoundToInt(P.Y/800),FMath::RoundToInt(P.Z/400));
                Shape &= Key.X>=0&&Key.X<G.Width&&Key.Y>=0&&Key.Y<G.Width&&Key.Z>=0&&Key.Z<G.Layers&&!Map.Contains(Key)&&P.Equals(FVector(Key.X*800,Key.Y*800,Key.Z*400),.2);
                Map.Add(Key,T.Get());Members.Add(T.Get());auto* C=T->FindComponentByClass<UPFPowerConnection>();Shape &= C&&C->GetFName()==TEXT("PoweredFloorConnection");
                if(CheckPower)PowerOK &= C&&C->HasPower()==FeedOn&&C->IsGridPowered()==FeedOn;
            }
            int32 Edges=0;
            for(const auto& Pair:Map)
            {
                auto* C=Power(Pair.Value);if(!C){Links=false;continue;}
                for(auto Offset:{FIntVector(1,0,0),FIntVector(0,1,0)})if(auto* N=Map.FindRef(Pair.Key+Offset)){const bool Has=C->HasHiddenConnection(Power(N));Links &= Has;if(Has)++Edges;}
                if(auto* Above=Map.FindRef(Pair.Key+FIntVector(0,0,1)))Links &= C->HasHiddenConnection(Power(Above));
                TArray<UFGCircuitConnectionComponent*> Hidden;C->GetHiddenConnections(Hidden);
                for(auto* H:Hidden)if(auto* PF=Cast<UPFPowerConnection>(H))Isolated &= Members.Contains(Cast<AFGBuildable>(PF->GetOwner()));
            }
            Check(Shape,G.Label+Phase+TEXT(" retains exact 3D layout and stable components"));
            Check(Links&&Edges==G.Layers*2*G.Width*(G.Width-1),G.Label+Phase+TEXT(" connects every horizontal grid edge"));
            Check(Isolated,G.Label+Phase+TEXT(" keeps links within the blueprint group"));
            if(CheckPower)Check(PowerOK,G.Label+Phase+TEXT(" energizes every touching layer from the top feed"));
            return Shape&&Links&&Isolated&&PowerOK;
        }
        void SaveDense()
        {
            if(Failures){FinishDense();return;}auto* BS=AFGBlueprintSubsystem::Get(World.Get());
            for(auto& G:Dense)
            {
                G.Placement=FTransform::Identity;InspectGrid(G,TEXT(" inside Designer"));
                TArray<FItemAmount> Cost;G.DesignerActor->CalculateBlueprintCost(Cost);TMap<UClass*,int32> Actual;for(auto I:Cost)Actual.FindOrAdd(I.ItemClass)+=I.Amount;
                Check(CostSame(Actual,G.Cost),G.Label+TEXT(" saves complete construction cost including extra wire"));
                G.BlueprintName=G.Label+TEXT(" Full Volume ")+FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));FBlueprintRecord R;R.BlueprintName=G.BlueprintName;R.BlueprintDescription=TEXT("QA full-volume 4m floors; touching vertical layers conduct power.");BS->AssignCategoryAndSubcategoryToBlueprintRecord(R);G.DesignerActor->SaveBlueprint(R,Cast<AFGPlayerController>(Player->GetController()));
            }
            Next(&FPFDenseBPTest::PlaceDense);
        }
        TArray<AFGBuildable*> PlaceSaved(const FString& BlueprintName,const FTransform& TM,AFGBuildableBlueprintDesigner* Inside=nullptr)
        {
            auto* BS=AFGBlueprintSubsystem::Get(World.Get());BS->RefreshBlueprintsAndDescriptors();auto* Desc=BS->GetBlueprintDescriptorByNameString(BlueprintName);TArray<AFGBuildable*> Created;
            Check(Desc&&BS->ReadBlueprintFromDisc(BlueprintName),BlueprintName+TEXT(" reads saved archive and descriptor"));if(!Desc)return Created;
            BS->SetActiveBlueprintHologramDescriptor(Desc);auto* Recipe=LoadObject<UClass>(nullptr,TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_Blueprint.Recipe_Blueprint_C"));
            auto* H=Recipe?Cast<AFGBlueprintHologram>(AFGHologram::SpawnHologramFromRecipe(Recipe,Player->GetBuildGun(),TM.GetLocation(),Player.Get(),[Desc](AFGHologram* NewH){CastChecked<AFGBlueprintHologram>(NewH)->SetBlueprintDescriptor(Desc);})):nullptr;
            Check(H!=nullptr,BlueprintName+TEXT(" constructs native preview with pre-spawn descriptor"));if(!H)return Created;
            if(Inside)
            {
                auto* Allowed=FindFProperty<FBoolProperty>(H->GetClass(),TEXT("mCanBePlacedInBlueprintDesigner"));
                const bool CanPlace=Allowed&&Allowed->GetPropertyValue_InContainer(H);
                Check(CanPlace,BlueprintName+TEXT(" native hologram permits placement inside a Designer"));
                if(!CanPlace){H->Destroy();BS->SetActiveBlueprintHologramDescriptor(nullptr);return Created;}
            }
            H->SetActorTickEnabled(false);H->SetSuppressBuildEffect(true);H->SetActorTransform(TM);if(Inside)H->SetInsideBlueprintDesigner(Inside);
            TArray<FItemAmount> Saved;Desc->GetBlueprintCost(Saved);TMap<UClass*,int32> A,B;for(auto I:Saved)A.FindOrAdd(I.ItemClass)+=I.Amount;for(auto I:H->GetCost(false))B.FindOrAdd(I.ItemClass)+=I.Amount;
            Check(CostSame(A,B),BlueprintName+TEXT(" preview costs equal saved blueprint costs"));
            TSet<AFGBuildable*> Before;auto* Builds=AFGBuildableSubsystem::Get(World.Get());for(auto* X:Builds->GetAllBuildablesRef())Before.Add(X);
            TSet<AFGBuildable*> BeforeInside;if(Inside)for(auto* X:Inside->GetBuildablesInBlueprintDesigner())BeforeInside.Add(X);
            TArray<AActor*> Children;auto* Root=H->Construct(Children,FNetConstructionID());if(Root)Proxies.Add(Root);
            if(Inside){for(auto* X:Inside->GetBuildablesInBlueprintDesigner())if(!BeforeInside.Contains(X)){Created.AddUnique(X);Owned.AddUnique(X);}}
            else for(auto* X:Builds->GetAllBuildablesRef())if(!Before.Contains(X)){Created.AddUnique(X);Owned.AddUnique(X);}
            H->Destroy();BS->SetActiveBlueprintHologramDescriptor(nullptr);return Created;
        }
        void PlaceDense()
        {
            if(Failures){FinishDense();return;}DesignerActors=Owned;Owned.Reset();Designer=nullptr;
            for(auto& G:Dense)
            {
                G.Placement=FTransform(FRotator(0,G.Tier==2?37:0,0),FVector(240000,-240000+G.Tier*20000,60000));
                auto Created=PlaceSaved(G.BlueprintName,G.Placement);G.Tiles.Reset();bool Context=true;
                for(auto* X:Created){if(X->FindComponentByClass<UPFPowerConnection>())G.Tiles.Add(X);Context &= !X->GetBlueprintDesigner()&&X->GetBlueprintProxy()!=nullptr;}
                Check(Context&&G.Tiles.Num()==G.Width*G.Width*G.Layers,G.Label+TEXT(" places every dense floor with correct ownership"));
                if(!Context||G.Tiles.Num()!=G.Width*G.Width*G.Layers){FinishDense();return;}
                FVector Top(-UE_BIG_NUMBER);for(auto T:G.Tiles)if(T->GetActorLocation().Z>Top.Z)Top=T->GetActorLocation();Top.Z+=200;
                G.FeedPole=Build(TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleMk1"),FTransform(Top));
                G.Generator=Build(TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_GeneratorBiomass_Automated"),FTransform(Top+FVector(-5000,-5000,0)));
                auto* Gen=Cast<AFGBuildableGeneratorFuel>(G.Generator.Get());auto* Fuel=LoadObject<UClass>(nullptr,TEXT("/Game/FactoryGame/Resource/Parts/BioFuel/Desc_Biofuel.Desc_Biofuel_C"));
                Check(Gen&&Fuel&&Gen->GetFuelInventory()->AddStackToIndex(Gen->GetFuelInventoryIndex(),FInventoryStack(100,Fuel),true)==100,G.Label+TEXT(" provides an ordinary fueled generator"));
                auto* WireClass=LoadObject<UClass>(nullptr,TEXT("/Game/FactoryGame/Buildable/Factory/PowerLine/Build_PowerLine.Build_PowerLine_C"));auto* Builds=AFGBuildableSubsystem::Get(World.Get());auto* Wire=WireClass?Builds->BeginSpawnBuildable(WireClass,FTransform(Top)):nullptr;
                if(Wire){Wire->FinishSpawning(FTransform(Top));if(!Builds->GetAllBuildablesRef().Contains(Wire))Builds->AddBuildable(Wire);Owned.Add(Wire);}G.Cable=Wire;
                Check(Wire&&Power(G.FeedPole)&&Power(G.Generator)&&CastChecked<AFGBuildableWire>(Wire)->Connect(Power(G.FeedPole),Power(G.Generator)),G.Label+TEXT(" feeds the top floor through a real pole and cable"));
            }
            for(int32 I=DesignerActors.Num()-1;I>=0;--I)if(DesignerActors[I].IsValid())DesignerActors[I]->TurnOffAndDestroy();DesignerActors.Reset();
            Next(&FPFDenseBPTest::VerifyPlaced);
        }
        void VerifyPlaced()
        {
            for(auto& G:Dense)InspectGrid(G,TEXT(" placed"),true);
            if(Failures){FinishDense();return;}StartNested();
        }
        void StartNested()
        {
            // Place the full-volume Mk.1 blueprint inside a real Mk.3 Designer,
            // then save that Designer and place its flattened result outside.
            Designer=Cast<AFGBuildableBlueprintDesigner>(Build(TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_BlueprintDesigner_Mk3"),FTransform(FVector(160000,-140000,60000))));
            Check(Designer.IsValid(),TEXT("Nested case constructs outer Mk.3 Designer"));if(!Designer.IsValid()){FinishDense();return;}
            auto* Box=Designer->GetCollisionComponent();FVector P=Box->GetComponentLocation();P.Z-=Box->GetScaledBoxExtent().Z;
            FPFDenseGroup G;G.Tier=4;G.Width=Dense[0].Width;G.Layers=Dense[0].Layers;G.Label=TEXT("Nested Mk.1 in Mk.3");G.DesignerActor=Designer;G.Placement=FTransform(P);
            auto Created=PlaceSaved(Dense[0].BlueprintName,G.Placement,Designer.Get());bool Context=true;
            for(auto* X:Created){if(X->FindComponentByClass<UPFPowerConnection>())G.Tiles.Add(X);Context &= X->GetBlueprintDesigner()==Designer.Get();}
            G.Cost=Dense[0].Cost;Check(Context&&G.Tiles.Num()==G.Width*G.Width*G.Layers,G.Label+TEXT(" registers every nested floor with the outer Designer"));Dense.Add(G);Next(&FPFDenseBPTest::SaveNested);
        }
        void SaveNested()
        {
            auto& G=Dense.Last();InspectGrid(G,TEXT(" inside outer Designer"));if(Failures){FinishDense();return;}
            TArray<FItemAmount> Cost;Designer->CalculateBlueprintCost(Cost);TMap<UClass*,int32> Actual;for(auto I:Cost)Actual.FindOrAdd(I.ItemClass)+=I.Amount;Check(CostSame(Actual,G.Cost),TEXT("Nested Designer costs contain each child once"));
            G.BlueprintName=TEXT("Powered Foundations Nested QA ")+FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));FBlueprintRecord R;R.BlueprintName=G.BlueprintName;R.BlueprintDescription=TEXT("QA nested full-volume Mk.1 blueprint saved through Mk.3 Designer.");auto* BS=AFGBlueprintSubsystem::Get(World.Get());BS->AssignCategoryAndSubcategoryToBlueprintRecord(R);Designer->SaveBlueprint(R,Cast<AFGPlayerController>(Player->GetController()));Next(&FPFDenseBPTest::PlaceNested);
        }
        void PlaceNested()
        {
            auto& G=Dense.Last();const auto Old=G.Tiles;G.Placement=FTransform(FRotator(0,90,0),FVector(240000,-140000,60000));auto Created=PlaceSaved(G.BlueprintName,G.Placement);G.Tiles.Reset();bool Context=true;
            for(auto* X:Created){if(X->FindComponentByClass<UPFPowerConnection>())G.Tiles.Add(X);Context &= !X->GetBlueprintDesigner()&&X->GetBlueprintProxy()!=nullptr;}
            Check(Context&&G.Tiles.Num()==G.Width*G.Width*G.Layers,TEXT("Nested blueprint saves and places all children once with normal ownership"));
            for(auto T:Old)if(T.IsValid())T->TurnOffAndDestroy();if(G.DesignerActor.IsValid())G.DesignerActor->TurnOffAndDestroy();Designer=nullptr;
            Next(&FPFDenseBPTest::SaveWorld);
        }
        void SaveWorld()
        {
            for(auto& G:Dense)InspectGrid(G,TEXT(" before save"),G.Tier<4);
            if(Failures){FinishDense();return;}
            Name=TEXT("PF_DenseBlueprint_")+FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));DenseManifest.Add(TEXT("SAVE|")+Name);
            for(int32 I=0;I<Dense.Num();++I)
            {
                const auto& G=Dense[I];DenseManifest.Add(FString::Printf(TEXT("GROUP|%d|%d|%d|%d|%s|%.6f"),I,G.Tier,G.Width,G.Layers,*G.BlueprintName,G.Placement.Rotator().Yaw));
                auto Record=[&](AFGBuildable* B,const TCHAR* Role){if(!B)return;auto P=B->GetActorLocation();DenseManifest.Add(FString::Printf(TEXT("ACTOR|%d|%s|%s|%s|%.6f|%.6f|%.6f|%.6f|%s"),I,Role,*B->GetName(),*B->GetClass()->GetPathName(),P.X,P.Y,P.Z,B->GetActorRotation().Yaw,*GetNameSafe(B->GetBlueprintProxy())));};
                for(auto T:G.Tiles)Record(T.Get(),TEXT("TILE"));Record(G.FeedPole.Get(),TEXT("POLE"));Record(G.Generator.Get(),TEXT("GEN"));Record(G.Cable.Get(),TEXT("CABLE"));
            }
            for(int32 I=0;I<InventoryBefore.Num();++I){auto& S=InventoryBefore[I];DenseManifest.Add(FString::Printf(TEXT("ITEM|%d|%s|%d"),I,S.HasItems()?*S.Item.GetItemClass()->GetPathName():TEXT("EMPTY"),S.HasItems()?S.NumItems:0));}
            Check(FFileHelper::SaveStringArrayToFile(DenseManifest,*DensePath()),TEXT("Writes dense/nested persistence manifest"));Report+=TEXT("Separate save: ")+Name+TEXT("\n");auto Self=StaticCastSharedRef<FPFDenseBPTest>(AsShared());
            UFGSaveSession::Get(World.Get())->SaveGame(Name,FOnSaveGameComplete::CreateLambda([Self](bool OK,const FText& Error,void*){Self->Check(OK,TEXT("Saves dense and nested copies with real powered top layers: ")+Error.ToString());Self->FinishDense();}),nullptr);
        }
        void ReloadSaved()
        {
            Check(FFileHelper::LoadFileToStringArray(DenseManifest,*DensePath()),TEXT("Reads dense persistence manifest"));TMap<FString,AFGBuildable*> Actors;for(auto* B:AFGBuildableSubsystem::Get(World.Get())->GetAllBuildablesRef())Actors.Add(B->GetName(),B);
            bool InventoryOK=true,ActorsOK=true;int32 Items=0,Count=0;
            for(const auto& Line:DenseManifest)
            {
                TArray<FString> F;Line.ParseIntoArray(F,TEXT("|"),false);
                if(F.Num()==7&&F[0]==TEXT("GROUP")){int32 I=FCString::Atoi(*F[1]);if(I<0||I>3)continue;Dense.SetNum(FMath::Max(Dense.Num(),I+1));auto& G=Dense[I];G.Tier=FCString::Atoi(*F[2]);G.Width=FCString::Atoi(*F[3]);G.Layers=FCString::Atoi(*F[4]);G.BlueprintName=F[5];G.Label=FString::Printf(TEXT("Reload dense group %d"),I);G.Placement=FTransform(FRotator(0,FCString::Atof(*F[6]),0));}
                if(F.Num()==10&&F[0]==TEXT("ACTOR"))
                {
                    int32 I=FCString::Atoi(*F[1]);if(!Dense.IsValidIndex(I)){ActorsOK=false;continue;}auto& G=Dense[I];auto* B=Actors.FindRef(F[3]);++Count;
                    if(!B){ActorsOK=false;continue;}Owned.Add(B);if(B->GetBlueprintProxy())Proxies.AddUnique(B->GetBlueprintProxy());
                    ActorsOK &= B->GetClass()->GetPathName()==F[4]&&B->GetActorLocation().Equals(FVector(FCString::Atod(*F[5]),FCString::Atod(*F[6]),FCString::Atod(*F[7])),.1)&&FMath::Abs(FMath::FindDeltaAngleDegrees(B->GetActorRotation().Yaw,FCString::Atof(*F[8])))<.01&&GetNameSafe(B->GetBlueprintProxy())==F[9]&&!B->GetBlueprintDesigner();
                    if(F[2]==TEXT("TILE"))G.Tiles.Add(B);else if(F[2]==TEXT("POLE"))G.FeedPole=B;else if(F[2]==TEXT("GEN"))G.Generator=B;else if(F[2]==TEXT("CABLE"))G.Cable=B;
                }
                if(F.Num()==4&&F[0]==TEXT("ITEM")){++Items;int32 I=FCString::Atoi(*F[1]);if(!InventoryBefore.IsValidIndex(I)){InventoryOK=false;continue;}auto& S=InventoryBefore[I];InventoryOK &= F[2]==TEXT("EMPTY")?!S.HasItems():S.HasItems()&&S.Item.GetItemClass()->GetPathName()==F[2]&&S.NumItems==FCString::Atoi(*F[3]);}
            }
            Check(Dense.Num()==4&&ActorsOK&&Count==947,TEXT("All 938 dense/nested tiles and nine feed actors preserve identity, class, transforms and proxies"));
            Check(InventoryOK&&Items==InventoryBefore.Num(),TEXT("Saved inventory remains identical"));
            for(auto& G:Dense){InspectGrid(G,TEXT(" after fresh restart"),G.Tier<4);if(G.Tier<4){auto* Gen=Cast<AFGBuildableGeneratorFuel>(G.Generator.Get());Check(Gen&&Gen->HasFuel(),G.Label+TEXT(" retains fuel without refilling"));if(auto* W=Cast<AFGBuildableWire>(G.Cable.Get()))W->Disconnect();}}
            Next(&FPFDenseBPTest::VerifyOff);
        }
        void VerifyOff()
        {
            for(auto& G:Dense)if(G.Tier<4){InspectGrid(G,TEXT(" feed disconnected"),true,false);auto* W=Cast<AFGBuildableWire>(G.Cable.Get());Check(W&&W->Connect(Power(G.FeedPole),Power(G.Generator)),G.Label+TEXT(" reconnects real feed"));}
            Next(&FPFDenseBPTest::VerifyReconnected);
        }
        void VerifyReconnected(){for(auto& G:Dense)InspectGrid(G,TEXT(" feed restored"),G.Tier<4);FinishDense();}
        void FinishDense()
        {
            for(int32 I=DesignerActors.Num()-1;I>=0;--I)if(DesignerActors[I].IsValid())DesignerActors[I]->TurnOffAndDestroy();
            for(int32 I=Owned.Num()-1;I>=0;--I)if(Owned[I].IsValid())Owned[I]->TurnOffAndDestroy();for(auto P:Proxies)if(P.IsValid())P->Destroy();
            if(Player.IsValid()){TArray<FInventoryStack> Now;Player->GetInventory()->GetInventoryStacks(Now,true);bool Same=Now.Num()==InventoryBefore.Num();for(int32 I=0;I<Now.Num()&&I<InventoryBefore.Num();++I)Same &= Now[I].HasItems()==InventoryBefore[I].HasItems()&&(!Now[I].HasItems()||(Now[I].NumItems==InventoryBefore[I].NumItems&&Now[I].Item.GetItemClass()==InventoryBefore[I].Item.GetItemClass()));Check(Same,TEXT("Every original inventory slot remains unchanged"));}
            Report+=FString::Printf(TEXT("Failures: %d\n"),Failures);FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/(ReloadDense?TEXT("TrapperPoweredFoundations-DenseBlueprintReloadTest.txt"):TEXT("TrapperPoweredFoundations-DenseBlueprintTest.txt"))));
        }
    };
}
void PFStartDenseBlueprintTest(UWorld* World,bool Reload)
{auto T=MakeShared<FPFDenseBPTest>();T->World=World;T->ReloadDense=Reload;FTimerHandle Timer;World->GetTimerManager().SetTimer(Timer,FTimerDelegate::CreateLambda([T]{T->Begin();}),15.f,false);}
