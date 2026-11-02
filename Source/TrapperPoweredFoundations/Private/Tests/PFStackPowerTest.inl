// Opt-in, isolated fixtures. Normal gameplay never runs this test.
namespace
{
    struct FPFStackCase
    {
        TWeakObjectPtr<AFGBuildable> Bottom, Middle, Top, Branch, Gap, Corner, Load, Pole, SupplyActor;
        FVector Base;
        int32 LowerHeight=1, MiddleHeight=2, Finish=0;
        FString Label;
    };
    struct FPFStackPowerTest : FPFCustomizationTest
    {
        TArray<FPFStackCase> Stacks;
        int32 StackStep=0;
        FString StackPath(int32 H,int32 F) { return TEXT("/TrapperPoweredFoundations/Foundations/Build_PoweredFoundation_")+PFFinishSuffix(H,F); }
        AFGBuildable* At(FVector P,bool NeedPower=false)
        {
            for(auto FixtureActor:Actors)if(FixtureActor.IsValid()&&!FixtureActor->IsActorBeingDestroyed()&&FixtureActor->GetActorLocation().Equals(Origin+P,.1)&&(!NeedPower||Connection(FixtureActor)))return FixtureActor.Get();
            return nullptr;
        }
        void SetupStacks()
        {
            UFGSaveSession::Get(World.Get())->SetAutoSaveEnabled(false);
            Player=Cast<AFGCharacterPlayer>(World->GetFirstPlayerController()->GetPawn());
            if(!Player.IsValid()){Check(false,TEXT("Player available"));FinishStacks();return;}
            Player->GetInventory()->GetInventoryStacks(InventoryBefore,true);
            if(Reload)
            {
                TArray<FString> Names;
                FFileHelper::LoadFileToStringArray(Names,*(FPaths::ProjectSavedDir()/TEXT("TPF-StackActors.txt")));
                for(auto* FixtureActor:AFGBuildableSubsystem::Get(World.Get())->GetAllBuildablesRef())if(Names.Contains(FixtureActor->GetName()))Actors.Add(FixtureActor);
                Check(Names.Num()>0&&Actors.Num()==Names.Num(),TEXT("All saved fixture actors survived fresh load"));
            }
            for(int32 F=0;F<5;++F)for(int32 H:{1,2,4})
            {
                FPFStackCase G;G.LowerHeight=H;G.MiddleHeight=H==1?2:H==2?4:1;G.Finish=F;
                G.Base=FVector(150000+(Stacks.Num()%5)*5000,-150000+(Stacks.Num()/5)*5000,0);
                G.Label=FString::Printf(TEXT("%s %dm/%dm/1m"),PFFinishes[F],H,G.MiddleHeight);
                auto Floor=[&](FVector P,int32 Height,int32 Finish)->AFGBuildable*
                {return Reload?At(P):Spawn(StackPath(Height,Finish),P);};
                G.Bottom=Floor(G.Base+FVector(0,0,-H*50),H,F);
                G.Middle=Floor(G.Base+FVector(0,0,G.MiddleHeight*50),G.MiddleHeight,(F+1)%5);
                const FVector TopP=G.Base+FVector(0,0,G.MiddleHeight*100+50);
                G.Top=Floor(TopP,1,(F+2)%5);
                G.Branch=Floor(TopP+FVector(800,0,0),1,F);
                G.Gap=Floor(TopP+FVector(0,0,102),1,F);
                G.Corner=Floor(TopP+FVector(-800,-800,100),1,F);
                const FVector PoleP=G.Base+FVector(200,200,0), LoadP=TopP+FVector(800,0,50), SourceP=G.Base+FVector(-1600,-1600,0);
                G.Pole=Reload?At(PoleP):Spawn(TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleMk1/Build_PowerPoleMk1"),PoleP);
                G.Load=Reload?At(LoadP):Spawn(TEXT("/Game/FactoryGame/Buildable/Factory/SmelterMk1/Build_SmelterMk1"),LoadP);
                if(!Reload)Supply(G.Pole,SourceP,&G.SupplyActor);
                else
                {
                    G.SupplyActor=At(SourceP,true);
                    // Synthetic QA generation is not saved; reattach only this
                    // test supply. Floor links must be rebuilt by normal code.
                    if(auto* Terminal=Connection(G.SupplyActor))
                    {auto* Info=NewObject<UFGPowerInfoComponent>(G.SupplyActor.Get());Info->RegisterComponent();Terminal->SetPowerInfo(Info);Info->SetBaseProduction(100000);}
                }
                Check(G.Bottom.IsValid()&&G.Middle.IsValid()&&G.Top.IsValid()&&G.Branch.IsValid()&&G.Load.IsValid()&&G.Pole.IsValid(),G.Label+TEXT(" fixtures available"));
                Stacks.Add(G);
            }
            APFNetworkSubsystem::Get(World.Get())->MarkDirty();
        }
        void VerifyStacks(bool Joined)
        {
            for(auto& G:Stacks)
            {
                auto* BottomTerminal=Connection(G.Bottom);auto* T=Connection(G.Top);
                Check(BottomTerminal&&BottomTerminal->HasPower()&&Lights(G.Bottom.Get(),true),G.Label+TEXT(" lower feed and indicators stay powered"));
                Check(T&&T->HasPower()==Joined&&Lights(G.Top.Get(),Joined),G.Label+TEXT(" upper tier power and indicator match contact"));
                Check(Same(G.Bottom,G.Top)==Joined,G.Label+TEXT(" circuit joins/splits across middle block"));
                auto* Load=Connection(G.Load);auto* Branch=Connection(G.Branch);
                Check(Load&&Branch&&Load->HasPower()==Joined&&Branch->HasPower()==Joined&&Same(G.Top,G.Branch)&&Same(G.Branch,G.Load),G.Label+TEXT(" upper side branch and machine receive correct power"));
                Check(Connection(G.Gap)&&!Connection(G.Gap)->HasPower()&&!Same(G.Top,G.Gap),G.Label+TEXT(" two centimeter vertical gap stays isolated"));
                Check(Connection(G.Corner)&&!Connection(G.Corner)->HasPower(),G.Label+TEXT(" vertical corner-only contact stays isolated"));
                if(Joined)Check(Connection(G.Middle)&&BottomTerminal&&T&&BottomTerminal->HasHiddenConnection(Connection(G.Middle))&&T->HasHiddenConnection(Connection(G.Middle)),G.Label+TEXT(" both vertical contacts have native hidden links"));
            }
        }
        void TickStacks()
        {
            // Clearing the repeating timer releases its captured shared owner.
            // Retain this test until the save callback takes ownership.
            const auto KeepAlive=AsShared();
            if(!World.IsValid())return;
            switch(StackStep++)
            {
            case 0: SetupStacks();break;
            case 1:
                VerifyStacks(true);
                for(auto& G:Stacks)if(G.Middle.IsValid())G.Middle->TurnOffAndDestroy();
                APFNetworkSubsystem::Get(World.Get())->MarkDirty();break;
            case 2:
                VerifyStacks(false);
                for(auto& G:Stacks)G.Middle=Spawn(StackPath(G.MiddleHeight,(G.Finish+1)%5),G.Base+FVector(0,0,G.MiddleHeight*50));
                APFNetworkSubsystem::Get(World.Get())->MarkDirty();break;
            default:
                VerifyStacks(true);
                World->GetTimerManager().ClearTimer(Timer);
                if(!Reload&&Failed==0)
                {
                    FString Names;
                    for(auto FixtureActor:Actors)if(FixtureActor.IsValid()&&!FixtureActor->IsActorBeingDestroyed())Names+=FixtureActor->GetName()+TEXT("\n");
                    FFileHelper::SaveStringToFile(Names,*(FPaths::ProjectSavedDir()/TEXT("TPF-StackActors.txt")));
                    const FString Name=TEXT("TPF_StackQA_")+FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
                    FFileHelper::SaveStringToFile(Name,*(FPaths::ProjectSavedDir()/TEXT("TPF-StackSave.txt")));
                    Report+=TEXT("Evidence save: ")+Name+TEXT("\n");
                    auto Self=StaticCastSharedRef<FPFStackPowerTest>(AsShared());
                    UFGSaveSession::Get(World.Get())->SaveGame(Name,FOnSaveGameComplete::CreateLambda([Self](bool OK,const FText& Error,void*){Self->Check(OK,TEXT("Stack fixture save succeeds: ")+Error.ToString());Self->FinishStacks();}),nullptr);
                }
                else FinishStacks();
                break;
            }
        }
        void FinishStacks()
        {
            const auto KeepAlive=AsShared();
            for(int32 I=Actors.Num()-1;I>=0;--I)if(Actors[I].IsValid())Actors[I]->TurnOffAndDestroy();
            if(Player.IsValid())
            {
                TArray<FInventoryStack> After;Player->GetInventory()->GetInventoryStacks(After,true);bool Equal=After.Num()==InventoryBefore.Num();
                for(int32 I=0;I<After.Num()&&I<InventoryBefore.Num();++I)Equal &= After[I].HasItems()==InventoryBefore[I].HasItems()&&(!After[I].HasItems()||(After[I].NumItems==InventoryBefore[I].NumItems&&After[I].Item.GetItemClass()==InventoryBefore[I].Item.GetItemClass()));
                Check(Equal,TEXT("Every original inventory slot is unchanged"));
            }
            Report+=FString::Printf(TEXT("Failures: %d\n"),Failed);
            FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/(Reload?TEXT("TPF-StackReloadTest.txt"):TEXT("TPF-StackTest.txt"))));
            World->GetTimerManager().ClearTimer(Timer);
        }
    };
}
void PFStartStackPowerTest(UWorld* World,bool Reload)
{
    auto T=MakeShared<FPFStackPowerTest>();T->World=World;T->Reload=Reload;
    World->GetTimerManager().SetTimer(T->Timer,FTimerDelegate::CreateLambda([T]{T->TickStacks();}),6.f,true,15.f);
}
