// Opt-in packaged regression. The player's inventory and buildings are untouched.
namespace
{
    struct FPFSideOutletCase
    {
        TWeakObjectPtr<AFGBuildable> Tile, Neighbor, Outlet, Load, Feed, Cable;
        FString Label;
        bool ShouldConnect = true;
    };
    struct FPFSideOutletTest : FPFExtendedTest
    {
        TArray<FPFSideOutletCase> Cases;
        FPFOutletCase Covered;
        FPFOutletCase AcrossGap;
        TWeakObjectPtr<AFGBuildable> UpperTile;
        int32 Step = 0;

        void SetupSides()
        {
            UFGSaveSession::Get(World.Get())->SetAutoSaveEnabled(false);
            if (auto* PC = World->GetFirstPlayerController()) Player = Cast<AFGCharacterPlayer>(PC->GetPawn());
            if (Player.IsValid()) Player->GetInventory()->GetInventoryStacks(InventoryBefore, true);
            // Two independent floors joined only by the visible outlet cable (issue #1).
            Covered.Center = FVector(140000, -40000, 0);
            Covered.Rotation = FQuat::Identity;
            Covered.Tile = TileAt(Covered.Center, 2);
            const FVector Upper = Covered.Center + FVector(0, 0, 1000);
            UpperTile = TileAt(Upper, 2);
            const FString OutletPath = TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleWall/Build_PowerPoleWall");
            const FVector SideAnchor(370, 200, -100);
            Covered.Outlet = Spawn(OutletPath, Covered.Center + SideAnchor);
            auto* UpperOutlet = Spawn(OutletPath, Upper + SideAnchor);
            auto* Cable = Cast<AFGBuildableWire>(Spawn(TEXT("/Game/FactoryGame/Buildable/Factory/PowerLine/Build_PowerLine"), Covered.Center));
            Check(Cable && Connection(Covered.Outlet) && Connection(UpperOutlet) &&
                Cable->Connect(Connection(Covered.Outlet), Connection(UpperOutlet)), TEXT("Connects cable between floors"));
            auto* FeedPole = Spawn(TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleMk1/Build_PowerPoleMk1"), Covered.Center);
            Supply(FeedPole, Covered.Center + FVector(-1600, -1600, 0));
            // The outlet trace reaches the tile behind this free-standing wall,
            // but neither the wall nor the outlet actually touches the tile.
            AcrossGap.Center = Covered.Center + FVector(4000, 0, 0);
            AcrossGap.Rotation = FQuat::Identity;
            AcrossGap.Tile = TileAt(AcrossGap.Center, 2);
            const FVector GapWallBase = AcrossGap.Center + FVector(460, 0, -400);
            AcrossGap.WallCenter = Origin + GapWallBase + FVector(0, 0, 200);
            AcrossGap.Wall = Spawn(TEXT("/Game/FactoryGame/Buildable/Building/Wall/Build_Wall_8x4_01"), GapWallBase, true);
            AcrossGap.Outlet = Spawn(OutletPath, AcrossGap.Center + FVector(460, 0, -100));
            Supply(AcrossGap.Outlet, AcrossGap.Center + FVector(-1600, -1600, 0));
            const TCHAR* Paths[] = {TEXT("PowerPoleWall/Build_PowerPoleWall"), TEXT("PowerPoleWall/Build_PowerPoleWall_Mk2"), TEXT("PowerPoleWall/Build_PowerPoleWall_Mk3"), TEXT("PowerPoleWallDouble/Build_PowerPoleWallDouble"), TEXT("PowerPoleWallDouble/Build_PowerPoleWallDouble_Mk2"), TEXT("PowerPoleWallDouble/Build_PowerPoleWallDouble_Mk3")};
            for (int32 V = 0; V < 6; ++V) for (int32 H : {1, 2, 4}) for (int32 Side = 0; Side < 4; ++Side)
            {
                const int32 I = Cases.Num();
                const FVector P(100000 + (I % 10) * 4000, -100000 + (I / 10) * 4000, 0);
                const float Yaw = Side % 2 ? 37.f : 0.f;
                const FRotator R(0, Yaw + Side * 90, 0);
                const FVector Normal = R.Vector();
                FPFSideOutletCase Case;
                Case.Label = FString::Printf(TEXT("%s %dm side=%d yaw=%.0f"), Paths[V], H, Side, Yaw);
                Case.Tile = TileAt(P, H, Yaw);
                Case.Neighbor = TileAt(P - Normal * 800, H, Yaw);
                Case.Outlet = Spawn(FString(TEXT("/Game/FactoryGame/Buildable/Factory/")) + Paths[V], P + Normal * 400 + FVector(0, 0, -H * 50));
                if (Case.Outlet.IsValid()) Case.Outlet->SetActorRotation(R);
                Case.Load = Spawn(TEXT("/Game/FactoryGame/Buildable/Factory/SmelterMk1/Build_SmelterMk1"), P - Normal * 800, true);
                Supply(Case.Outlet, P + FVector(-1600, -1600, 0), &Case.Feed, &Case.Cable);
                Cases.Add(Case);
            }
            // Negative controls: the trace must not reach a remote face or an
            // outlet above the tile; a nearby powered tile cannot be a shortcut.
            for (int32 V = 0; V < 6; ++V) for (int32 Mode = 0; Mode < 2; ++Mode)
            {
                const FVector P(100000 + V * 4000, -50000 + Mode * 4000, 0);
                FPFSideOutletCase Case; Case.ShouldConnect = false;
                Case.Label = FString::Printf(TEXT("%s negative %s"), Paths[V], Mode ? TEXT("above top") : TEXT("150cm air gap"));
                Case.Tile = TileAt(P, 2);
                Case.Outlet = Spawn(FString(TEXT("/Game/FactoryGame/Buildable/Factory/")) + Paths[V], P + (Mode ? FVector(400, 0, 100) : FVector(550, 0, -100)));
                Supply(Case.Outlet, P + FVector(-1600, -1600, 0), &Case.Feed, &Case.Cable);
                Cases.Add(Case);
            }
            // Preserve the separate-wall behavior that predates this fix.
            for (int32 V = 0; V < 6; ++V)
            {
                FPFOutletCase G; G.Center = FVector(100000 + V * 4000, -40000, 0); G.Rotation = FQuat::Identity;
                G.Label = FString(TEXT("Separate wall ")) + Paths[V]; G.Tile = TileAt(G.Center, 2);
                const FVector WP = G.Center + FVector(0, 350, 0);
                G.Wall = Spawn(TEXT("/Game/FactoryGame/Buildable/Building/Wall/Build_Wall_8x4_01"), WP, true);
                G.WallCenter = Origin + WP + FVector(0, 0, 200);
                G.Outlet = Spawn(FString(TEXT("/Game/FactoryGame/Buildable/Factory/")) + Paths[V], WP + FVector(0, 0, 200));
                Supply(G.Outlet, G.Center + FVector(-1600, -1600, 0)); Outlets.Add(G);
            }
        }
        bool Linked(const FPFSideOutletCase& Case)
        {
            if (!Case.Outlet.IsValid() || !Case.Tile.IsValid()) return false;
            TInlineComponentArray<UFGPowerConnectionComponent*> T(Case.Outlet.Get());
            bool All = T.Num() > 0;
            for (auto* Terminal : T) All &= Terminal->HasHiddenConnection(Connection(Case.Tile));
            return All;
        }
        void TickSides()
        {
            if (!World.IsValid()) return;
            if (Step++ == 0) { SetupSides(); return; }
            if (Step == 2)
            {
                Check(Same(Covered.Tile, UpperTile) && Connection(UpperTile)->HasPower(), TEXT("Upper floor receives power before wall placement"));
                const FVector WallBase = Covered.Center + FVector(400, 0, -400);
                Covered.WallCenter = Origin + WallBase + FVector(0, 0, 200);
                Covered.Wall = Spawn(TEXT("/Game/FactoryGame/Buildable/Building/Wall/Build_Wall_8x4_01"), WallBase, true);
                for (auto& Case : Cases)
                {
                    Check(Linked(Case) == Case.ShouldConnect, Case.Label + TEXT(" all terminals have expected foundation links"));
                    Check(Connection(Case.Tile) && Connection(Case.Tile)->HasPower() == Case.ShouldConnect, Case.Label + TEXT(" expected floor power"));
                    if (Case.ShouldConnect)
                        Check(Connection(Case.Load) && Connection(Case.Load)->HasPower() && Same(Case.Load, Case.Outlet), Case.Label + TEXT(" powers machine through neighboring tile"));
                    if (auto* W = Cast<AFGBuildableWire>(Case.Cable.Get())) W->Disconnect();
                }
                for (auto& G : Outlets)
                {
                    Check(Connection(G.Tile) && Connection(G.Tile)->HasPower() && Same(G.Tile, G.Outlet), G.Label + TEXT(" still supplies floor"));
                    RemoveWall(G);
                }
                return;
            }
            if (Step == 3)
            {
                Check(Connection(Covered.Tile) && Connection(Covered.Tile)->HasPower(), TEXT("Source floor stays powered with covering wall"));
                Check(Same(Covered.Tile, UpperTile) && Connection(UpperTile)->HasPower(), TEXT("Upper floor stays powered with covering wall"));
                Check(Connection(AcrossGap.Outlet) && Connection(AcrossGap.Outlet)->HasPower() &&
                    Connection(AcrossGap.Tile) && !Connection(AcrossGap.Tile)->HasPower(), TEXT("Separate wall outlet cannot feed tile across air gap"));
                RemoveWall(Covered);
                RemoveWall(AcrossGap);
                for (auto& Case : Cases)
                {
                    Check(Connection(Case.Tile) && !Connection(Case.Tile)->HasPower(), Case.Label + TEXT(" loses power when feed is disconnected"));
                    // Reconnect in the opposite direction: floor supplies outlet.
                    if (Case.ShouldConnect)
                    {
                        if (auto* W = Cast<AFGBuildableWire>(Case.Cable.Get()))
                            Check(W->Connect(Connection(Case.Feed), Connection(Case.Load)), Case.Label + TEXT(" reverse feed connected"));
                    }
                }
                for (auto& G : Outlets)
                    Check(Connection(G.Tile) && !Connection(G.Tile)->HasPower(), G.Label + TEXT(" wall removal disconnects floor"));
                return;
            }
            if (Step == 4)
            {
                Check(Same(Covered.Tile, UpperTile) && Connection(UpperTile)->HasPower(), TEXT("Upper floor stays powered after removing covering wall"));
                for (auto& Case : Cases) if (Case.ShouldConnect)
                {
                    Check(Connection(Case.Outlet) && Connection(Case.Outlet)->HasPower() && Linked(Case), Case.Label + TEXT(" receives power from floor in reverse direction"));
                    if (Case.Tile.IsValid()) Case.Tile->TurnOffAndDestroy();
                }
                return;
            }
            for (auto& Case : Cases) if (Case.ShouldConnect)
            {
                bool NoFloorLinks = Case.Outlet.IsValid();
                TInlineComponentArray<UFGPowerConnectionComponent*> T(Case.Outlet.Get());
                for (auto* Terminal : T)
                {
                    TArray<UFGCircuitConnectionComponent*> Hidden; Terminal->GetHiddenConnections(Hidden);
                    for (auto* H : Hidden) NoFloorLinks &= !Cast<UPFPowerConnection>(H);
                }
                Check(NoFloorLinks && Connection(Case.Outlet) && !Connection(Case.Outlet)->HasPower(), Case.Label + TEXT(" support removal detaches and depowers outlet"));
                Check(Connection(Case.Load) && Connection(Case.Load)->HasPower(), Case.Label + TEXT(" remaining independently fed machine stays powered"));
            }
            FinishSides();
        }
        void FinishSides()
        {
            const auto KeepAlive = AsShared();
            for (auto& G : Outlets) RemoveWall(G);
            for (int32 I = Actors.Num() - 1; I >= 0; --I) if (Actors[I].IsValid()) Actors[I]->TurnOffAndDestroy();
            if (Player.IsValid())
            {
                TArray<FInventoryStack> After; Player->GetInventory()->GetInventoryStacks(After, true);
                bool Equal = After.Num() == InventoryBefore.Num();
                for (int32 I = 0; I < After.Num() && I < InventoryBefore.Num(); ++I)
                    Equal &= After[I].HasItems() == InventoryBefore[I].HasItems() && (!After[I].HasItems() || (After[I].NumItems == InventoryBefore[I].NumItems && After[I].Item.GetItemClass() == InventoryBefore[I].Item.GetItemClass()));
                Check(Equal, TEXT("Original inventory unchanged in every slot"));
            }
            Report += FString::Printf(TEXT("Failures: %d\n"), Failed);
            FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("TrapperPoweredFoundations-SideOutletTest.txt")));
            UE_LOG(LogPFTest, Display, TEXT("Side outlet regression complete: %d failures"), Failed);
            World->GetTimerManager().ClearTimer(Timer);
        }
    };
}
void PFStartSideOutletTest(UWorld* World)
{
    auto T = MakeShared<FPFSideOutletTest>(); T->World = World;
    World->GetTimerManager().SetTimer(T->Timer, FTimerDelegate::CreateLambda([T] { T->TickSides(); }), 12.f, true, 15.f);
}
