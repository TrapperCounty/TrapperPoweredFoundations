#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "HAL/PlatformMemory.h"
namespace
{
    struct FPFOutletCase
    {
        TWeakObjectPtr<AFGBuildable> Tile,Wall,Outlet;
        FVector Center,WallCenter;
        FQuat Rotation;
        FString Label;
    };
    struct FPFMachineCase { TWeakObjectPtr<AFGBuildable> Tile,Machine; FString Label; bool Passive=false; };
    struct FPFExtendedTest : FPFTest
    {
        TArray<FPFOutletCase> Outlets;
        TArray<FPFMachineCase> Machines;
        TArray<TWeakObjectPtr<AFGBuildable>> Grid;
        TWeakObjectPtr<AFGBuildable> GridPole,GridSource,GridWire;
        TWeakObjectPtr<AFGCharacterPlayer> Player;
        TArray<FInventoryStack> InventoryBefore;
        int32 Phase=0,NextTile=0,Samples=0;
        double Started=0,NextTime=0,SoakStart=0;
        uint64 MemoryStart=0,MemoryMax=0,StartPasses=0;
        double StartMs=0;
        const FVector GridBase=FVector(100000,100000,0);
        void WriteProgress(const FString& Status)
        {
            auto* N=APFNetworkSubsystem::Get(World.Get());
            const auto Memory=FPlatformMemory::GetStats();MemoryMax=FMath::Max(MemoryMax,Memory.UsedPhysical);
            const FString S=FString::Printf(TEXT("Status: %s\nPhase: %d\nSamples: %d\nElapsedSeconds: %.1f\nFailures: %d\nWorkingSetMiB: %.1f\nPeakObservedWorkingSetMiB: %.1f\nReconcilePasses: %llu\nReconcileTotalMs: %.3f\nReconcileMaxMs: %.3f\n"),*Status,Phase,Samples,FPlatformTime::Seconds()-Started,Failed,Memory.UsedPhysical/1048576.,MemoryMax/1048576.,N?N->GetQAPasses():0,N?N->GetQATotalMs():0,N?N->GetQAMaxMs():0);
            FFileHelper::SaveStringToFile(S,*(FPaths::ProjectSavedDir()/TEXT("TrapperPoweredFoundations-ExtendedProgress.txt")));
            FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("TrapperPoweredFoundations-ExtendedTest.txt")));
        }
        AFGBuildable* TileAt(FVector P,int32 H=1,float Yaw=0)
        {
            auto* T=Spawn(FString::Printf(TEXT("/TrapperPoweredFoundations/Foundations/Build_PoweredFoundation_%dm"),H),P+FVector(0,0,-H*50));
            if(T)T->SetActorRotation(FRotator(0,Yaw,0));return T;
        }
        void Supply(TWeakObjectPtr<AFGBuildable> Target,FVector P,TWeakObjectPtr<AFGBuildable>* SourceOut=nullptr,TWeakObjectPtr<AFGBuildable>* WireOut=nullptr)
        {
            auto* S=Spawn(TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleMk3/Build_PowerPoleMk3"),P);
            auto* W=Cast<AFGBuildableWire>(Spawn(TEXT("/Game/FactoryGame/Buildable/Factory/PowerLine/Build_PowerLine"),P));
            auto* Terminal=Connection(S);
            if(Terminal){auto* Info=NewObject<UFGPowerInfoComponent>(S);Info->RegisterComponent();Terminal->SetPowerInfo(Info);Info->SetBaseProduction(100000);}
            Check(Terminal&&Connection(Target)&&W&&W->Connect(Terminal,Connection(Target)),TEXT("Connects native cable for independent QA supply"));
            if(SourceOut)*SourceOut=S;if(WireOut)*WireOut=W;
        }
        void RemoveWall(FPFOutletCase& G)
        {
            for(FVector Axis:{G.Rotation.RotateVector(FVector::ForwardVector),G.Rotation.RotateVector(FVector::RightVector)})
            {
                TArray<FHitResult> Hits;FCollisionQueryParams Q(SCENE_QUERY_STAT(PFExtendedWall),false,G.Outlet.Get());
                World->LineTraceMultiByObjectType(Hits,G.WallCenter+Axis*100,G.WallCenter-Axis*100,FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects),Q);
                for(const auto& H:Hits)
                {
                    FInstanceHandle Handle;FLightweightBuildableInstanceRef Ref;auto* M=AAbstractInstanceManager::GetInstanceManager(World.Get());
                    if(M&&M->ResolveHit(H,Handle)&&AFGLightweightBuildableSubsystem::ResolveLightweightInstance(Handle,Ref)&&Ref.GetBuildableClass()&&Ref.GetBuildableClass()->GetName()==TEXT("Build_Wall_8x4_01_C"))Ref.Remove();
                }
            }
            if(G.Wall.IsValid()){IFGDismantleInterface::Execute_Dismantle(G.Wall.Get());G.Wall->Destroy();}
        }
        void Setup()
        {
            Started=FPlatformTime::Seconds();auto* PC=World->GetFirstPlayerController();Player=PC?Cast<AFGCharacterPlayer>(PC->GetPawn()):nullptr;
            if(!Player.IsValid()){Check(false,TEXT("Player available"));FinishExtended();return;}
            Player->GetInventory()->GetInventoryStacks(InventoryBefore,true);
            // This is a disposable long-running session. Do not let temporary
            // fixtures enter the player's rotating saves; ordinary restarts
            // create a fresh save session with the user's normal settings.
            UFGSaveSession::Get(World.Get())->SetAutoSaveEnabled(false);
            const TCHAR* Paths[]={TEXT("PowerPoleWall/Build_PowerPoleWall"),TEXT("PowerPoleWall/Build_PowerPoleWall_Mk2"),TEXT("PowerPoleWall/Build_PowerPoleWall_Mk3"),TEXT("PowerPoleWallDouble/Build_PowerPoleWallDouble"),TEXT("PowerPoleWallDouble/Build_PowerPoleWallDouble_Mk2"),TEXT("PowerPoleWallDouble/Build_PowerPoleWallDouble_Mk3")};
            for(int32 V=0;V<6;++V)for(int32 H:{1,2,4})for(float Yaw:{0.f,90.f})
            {
                const int32 I=Outlets.Num();FPFOutletCase G;G.Center=FVector(100000+(I%6)*4000,-100000+(I/6)*4000,0);G.Rotation=FRotator(0,Yaw,0).Quaternion();
                G.Label=FString::Printf(TEXT("Outlet %s %dm yaw %.0f"),Paths[V],H,Yaw);G.Tile=TileAt(G.Center,H,Yaw);
                const FVector WallP=G.Center+G.Rotation.RotateVector(FVector(0,350,0));
                G.Wall=Spawn(TEXT("/Game/FactoryGame/Buildable/Building/Wall/Build_Wall_8x4_01"),WallP,true);
                if(G.Wall.IsValid())G.Wall->SetActorRotation(G.Rotation);
                G.WallCenter=Origin+WallP+FVector(0,0,200);
                G.Outlet=Spawn(FString(TEXT("/Game/FactoryGame/Buildable/Factory/"))+Paths[V],WallP+FVector(0,0,200));
                if(G.Outlet.IsValid())G.Outlet->SetActorRotation(G.Rotation);
                Supply(G.Outlet,G.Center+FVector(-1600,-1600,0));Outlets.Add(G);
            }
            const TCHAR* MachinePaths[]={TEXT("ConstructorMk1/Build_ConstructorMk1"),TEXT("ManufacturerMk1/Build_ManufacturerMk1"),TEXT("OilRefinery/Build_OilRefinery"),TEXT("Packager/Build_Packager"),TEXT("Blender/Build_Blender"),TEXT("Converter/Build_Converter"),TEXT("PowerStorage/Build_PowerStorageMk1"),TEXT("PowerSwitch/Build_PowerSwitch"),TEXT("PriorityPowerSwitch/Build_PriorityPowerSwitch")};
            for(int32 I=0;I<9;++I)
            {
                FPFMachineCase G;G.Label=MachinePaths[I];G.Passive=I>=7;const FVector P(-100000+I*5000,-100000,0);
                G.Tile=TileAt(P,4);G.Machine=Spawn(FString(TEXT("/Game/FactoryGame/Buildable/Factory/"))+MachinePaths[I],P,true);
                auto* FixturePole=Spawn(TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleMk1/Build_PowerPoleMk1"),P+FVector(200,200,0));Supply(FixturePole,P+FVector(-1600,-1600,0));Machines.Add(G);
            }
            Phase=1;NextTime=FPlatformTime::Seconds()+10;WriteProgress(TEXT("Waiting for outlet and machine circuits"));
        }
        bool GridState(bool Powered,bool Split=false)
        {
            bool OK=Grid.Num()==1024;int32 Links=0;
            for(int32 I=0;I<Grid.Num();++I)
            {
                const int32 X=I%32,Y=I/32;if(Split&&X==16)continue;
                auto* Terminal=Connection(Grid[I]);if(!Terminal){OK=false;continue;}
                const bool Expected=Powered&&(!Split||X<16);
                OK &= Terminal->HasPower()==Expected&&Grid[I]->FindComponentByClass<UPFPowerConnection>()->IsGridPowered()==Expected;
                for(int32 J:{X<31?I+1:-1,Y<31?I+32:-1})if(J>=0&&!(Split&&J%32==16))
                {const bool Linked=Terminal->HasHiddenConnection(Connection(Grid[J]));OK &= Linked;if(Linked)++Links;}
            }
            if(!Split)OK &= Links==1984;
            return OK;
        }
        void Tick()
        {
            const double Now=FPlatformTime::Seconds();if(Now<NextTime)return;
            if(Phase==0){Setup();return;}
            if(Phase==1)
            {
                for(auto& G:Outlets)
                {
                    bool Linked=G.Tile.IsValid()&&G.Outlet.IsValid();TInlineComponentArray<UFGPowerConnectionComponent*> Terminals(G.Outlet.Get());
                    Linked &= Terminals.Num()>0;for(auto* T:Terminals)Linked &= T->HasHiddenConnection(Connection(G.Tile));
                    Check(Linked,G.Label+TEXT(" every outlet terminal attaches through its mounting wall"));
                    Check(Connection(G.Tile)&&Connection(G.Tile)->HasPower(),G.Label+TEXT(" supplies the supporting floor"));RemoveWall(G);
                }
                for(auto& G:Machines)
                {
                    bool Link=false;TInlineComponentArray<UFGPowerConnectionComponent*> Terminals(G.Machine.Get());for(auto* T:Terminals)Link |= T->HasHiddenConnection(Connection(G.Tile));
                    Check(G.Machine.IsValid()&&(G.Passive?!Link:Link),G.Label+(G.Passive?TEXT(" passive switch is not bypassed by floor links"):TEXT(" attaches through supporting floor")));
                    if(!G.Passive)Check(Connection(G.Machine)&&Connection(G.Machine)->HasPower(),G.Label+TEXT(" receives floor power"));
                    if(G.Tile.IsValid())G.Tile->TurnOffAndDestroy();
                }
                Phase=2;NextTime=Now+10;WriteProgress(TEXT("Verifying support removal"));return;
            }
            if(Phase==2)
            {
                for(auto& G:Outlets){bool NoLink=true;TInlineComponentArray<UFGPowerConnectionComponent*> T(G.Outlet.Get());for(auto* Terminal:T)NoLink &= !Terminal->HasHiddenConnection(Connection(G.Tile));Check(NoLink,G.Label+TEXT(" detaches after mounting wall removal"));}
                for(auto& G:Machines)if(!G.Passive){bool NoFloor=true;TInlineComponentArray<UFGPowerConnectionComponent*> T(G.Machine.Get());for(auto* Terminal:T){TArray<UFGCircuitConnectionComponent*> H;Terminal->GetHiddenConnections(H);for(auto* L:H)NoFloor &= !Cast<UPFPowerConnection>(L);}Check(NoFloor,G.Label+TEXT(" drops floor links after support removal"));}
                for(auto FixtureActor:Actors)if(FixtureActor.IsValid())FixtureActor->TurnOffAndDestroy();Actors.Reset();Outlets.Reset();Machines.Reset();
                if(Failed){FinishExtended();return;}
                Phase=3;NextTime=Now+10;WriteProgress(TEXT("Baseline before large grid"));return;
            }
            if(Phase==3)
            {
                auto* N=APFNetworkSubsystem::Get(World.Get());StartPasses=N->GetQAPasses();StartMs=N->GetQATotalMs();
                Phase=4;NextTime=Now+30;return;
            }
            if(Phase==4)
            {
                auto* N=APFNetworkSubsystem::Get(World.Get());const auto Count=N->GetQAPasses()-StartPasses;
                Report+=FString::Printf(TEXT("METRIC: baseline reconcile passes=%llu mean_ms=%.3f\n"),Count,Count?(N->GetQATotalMs()-StartMs)/Count:0);
                Phase=5;
            }
            if(Phase==5)
            {
                const int32 End=FMath::Min(NextTile+64,1024);for(;NextTile<End;++NextTile)Grid.Add(TileAt(GridBase+FVector((NextTile%32)*800,(NextTile/32)*800,0),1));
                if(NextTile<1024){NextTime=Now+1;return;}
                GridPole=Spawn(TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleMk1/Build_PowerPoleMk1"),GridBase);Supply(GridPole,GridBase+FVector(-1600,-1600,0),&GridSource,&GridWire);
                Phase=6;NextTime=Now+15;WriteProgress(TEXT("1024-tile grid constructed"));return;
            }
            if(Phase==6)
            {
                Check(GridState(true),TEXT("All 1024 tiles energize across 1984 expected grid edges"));
                for(int32 Y=0;Y<32;++Y)if(Grid[Y*32+16].IsValid())Grid[Y*32+16]->TurnOffAndDestroy();
                Phase=7;NextTime=Now+10;return;
            }
            if(Phase==7)
            {
                Check(GridState(true,true),TEXT("Removing FixtureActor full column splits the grid and powers only the fed half"));
                for(int32 Y=0;Y<32;++Y)Grid[Y*32+16]=TileAt(GridBase+FVector(16*800,Y*800,0));
                Phase=8;NextTime=Now+10;return;
            }
            if(Phase==8)
            {
                Check(GridState(true),TEXT("Replacing the column restores all links and energized tiles"));
                if(Failed){FinishExtended();return;}
                SoakStart=Now;MemoryStart=FPlatformMemory::GetStats().UsedPhysical;auto* N=APFNetworkSubsystem::Get(World.Get());StartPasses=N->GetQAPasses();StartMs=N->GetQATotalMs();
                Phase=9;NextTime=Now+30;WriteProgress(TEXT("30-minute soak running"));return;
            }
            if(Phase==9)
            {
                // Each six-sample cycle includes FixtureActor power cut and reconnection.
                const int32 Cycle=Samples%6;const bool Powered=Cycle!=5;
                Check(GridState(Powered),FString::Printf(TEXT("Soak sample %d grid/edges/rims %s"),Samples+1,Powered?TEXT("powered"):TEXT("unpowered")));
                auto* W=Cast<AFGBuildableWire>(GridWire.Get());
                if(Cycle==4&&W)W->Disconnect();if(Cycle==5&&W)Check(W->Connect(Connection(GridSource),Connection(GridPole)),TEXT("Soak reconnects native feed cable"));
                ++Samples;WriteProgress(TEXT("30-minute soak running"));
                if(Failed){FinishExtended();return;}
                if(Now-SoakStart>=1800)
                {
                    auto* N=APFNetworkSubsystem::Get(World.Get());const auto Count=N->GetQAPasses()-StartPasses;
                    Report+=FString::Printf(TEXT("METRIC: grid soak duration_s=%.1f samples=%d reconcile_passes=%llu mean_ms=%.3f max_ms=%.3f memory_start_mib=%.1f memory_end_mib=%.1f peak_mib=%.1f\n"),Now-SoakStart,Samples,Count,Count?(N->GetQATotalMs()-StartMs)/Count:0,N->GetQAMaxMs(),MemoryStart/1048576.,FPlatformMemory::GetStats().UsedPhysical/1048576.,MemoryMax/1048576.);
                    FinishExtended();return;
                }
                NextTime=Now+30;
            }
        }
        void FinishExtended()
        {
            const auto KeepAlive=AsShared();
            for(auto& G:Outlets)RemoveWall(G);
            for(int32 I=Actors.Num()-1;I>=0;--I)if(Actors[I].IsValid())Actors[I]->TurnOffAndDestroy();
            if(Player.IsValid())
            {
                TArray<FInventoryStack> After;Player->GetInventory()->GetInventoryStacks(After,true);bool Same=After.Num()==InventoryBefore.Num();
                for(int32 I=0;I<After.Num()&&I<InventoryBefore.Num();++I)Same &= After[I].HasItems()==InventoryBefore[I].HasItems()&&(!After[I].HasItems()||(After[I].NumItems==InventoryBefore[I].NumItems&&After[I].Item.GetItemClass()==InventoryBefore[I].Item.GetItemClass()));
                Check(Same,TEXT("Original inventory remains identical in every slot"));
            }
            // Clearing the repeating timer releases its last shared reference.
            // Persist results before removing that ownership.
            Report+=FString::Printf(TEXT("Failures: %d\n"),Failed);WriteProgress(TEXT("Complete"));
            World->GetTimerManager().ClearTimer(Timer);
        }
    };
}
void PFStartExtendedTest(UWorld* World)
{
    auto T=MakeShared<FPFExtendedTest>();T->World=World;
    World->GetTimerManager().SetTimer(T->Timer,FTimerDelegate::CreateLambda([T]{T->Tick();}),1.f,true,15.f);
}
