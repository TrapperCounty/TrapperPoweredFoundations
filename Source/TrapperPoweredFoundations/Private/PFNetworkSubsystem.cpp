#include "PFNetworkSubsystem.h"
#include "PFPowerConnection.h"
#include "PFGeometry.h"
#include "Subsystem/SubsystemActorManager.h"
#include "FGBuildableSubsystem.h"
#include "FGClearanceInterface.h"
#include "FGPowerCircuit.h"
#include "FGLightweightBuildableSubsystem.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Buildables/FGBuildableGenerator.h"
#include "Buildables/FGBuildableWall.h"
#include "Buildables/FGBuildableBlueprintDesigner.h"
#include "AbstractInstanceManager.h"
#include "TimerManager.h"
#include "Misc/ScopeExit.h"

DEFINE_LOG_CATEGORY_STATIC(LogTrapperPoweredFoundations, Log, All);
void PFStartIntegrationTest(UWorld* World);

FPFLink::FPFLink(UFGCircuitConnectionComponent* X, UFGCircuitConnectionComponent* Y)
{
    if (X->GetUniqueID() < Y->GetUniqueID()) { A = X; B = Y; }
    else { A = Y; B = X; }
}

APFNetworkSubsystem::APFNetworkSubsystem()
{
    ReplicationPolicy = ESubsystemReplicationPolicy::SpawnOnServer;
    PrimaryActorTick.bCanEverTick = false;
}

APFNetworkSubsystem* APFNetworkSubsystem::Get(UWorld* World)
{
    if (World)
        if (auto* Manager = World->GetSubsystem<USubsystemActorManager>())
            return Manager->GetSubsystemActor<APFNetworkSubsystem>();
    return nullptr;
}

void APFNetworkSubsystem::BeginPlay()
{
    Super::BeginPlay();
    if (auto* Buildables = AFGBuildableSubsystem::Get(GetWorld()))
    {
        Buildables->mBuildableAddedDelegate.AddDynamic(this, &APFNetworkSubsystem::OnBuilt);
        Buildables->mBuildableRemovedDelegate.AddDynamic(this, &APFNetworkSubsystem::OnRemoved);
    }
    // Let saved actors finish PostLoadGame before repairing their hidden links.
    GetWorldTimerManager().SetTimer(MaintenanceTimer, this, &APFNetworkSubsystem::Maintain, .25f, true, 1.f);
    PFStartIntegrationTest(GetWorld());
}

void APFNetworkSubsystem::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearTimer(MaintenanceTimer);
    for (const auto& Candidate : Candidates)
        if (auto* Designer = Cast<AFGBuildableBlueprintDesigner>(Candidate.Get()))
            Designer->OnBlueprintHasBuildingsChanged.RemoveDynamic(this, &APFNetworkSubsystem::OnDesignerChanged);
    if (auto* Buildables = AFGBuildableSubsystem::Get(GetWorld()))
    {
        Buildables->mBuildableAddedDelegate.RemoveDynamic(this, &APFNetworkSubsystem::OnBuilt);
        Buildables->mBuildableRemovedDelegate.RemoveDynamic(this, &APFNetworkSubsystem::OnRemoved);
    }
    Super::EndPlay(Reason);
}

void APFNetworkSubsystem::OnBuilt(AFGBuildable* Buildable)
{
    if (IsValid(Buildable))
    {
        Candidates.Add(Buildable);
        if (auto* Designer = Cast<AFGBuildableBlueprintDesigner>(Buildable))
            Designer->OnBlueprintHasBuildingsChanged.AddUniqueDynamic(this, &APFNetworkSubsystem::OnDesignerChanged);
    }
    MarkDirty();
}

void APFNetworkSubsystem::OnRemoved(AFGBuildable* Buildable)
{
    if (auto* Designer = Cast<AFGBuildableBlueprintDesigner>(Buildable))
        Designer->OnBlueprintHasBuildingsChanged.RemoveDynamic(this, &APFNetworkSubsystem::OnDesignerChanged);
    Candidates.Remove(Buildable);
    MarkDirty();
}

void APFNetworkSubsystem::OnDesignerChanged()
{
    MarkDirty();
}

void APFNetworkSubsystem::Maintain()
{
    if (!HasAuthority()) return;
    if (!bScanned)
    {
        if (auto* Buildables = AFGBuildableSubsystem::Get(GetWorld()))
        {
            for (AFGBuildable* Buildable : Buildables->GetAllBuildablesRef()) OnBuilt(Buildable);
            bScanned = true;
        }
    }
    // Periodic reconciliation also detects lightweight wall removal and moved
    // actors. It scans our cached candidates, not every actor in the world.
    if (bDirty || ++MaintenanceCounter >= 20)
    {
        bDirty = false;
        MaintenanceCounter = 0;
        Reconcile();
    }
    for (const auto& Tile : Tiles) if (Tile.IsValid()) Tile->RefreshPowerState();
}

namespace
{
    struct FBody { FBox Box; FTransform Transform; };

    TArray<FBody> Bodies(AFGBuildable* Buildable, const FTransform& Transform)
    {
        TArray<FFGClearanceData> Clearance;
        IFGClearanceInterface::Execute_GetClearanceData(Buildable, Clearance);
        TArray<FBody> Result;
        for (const auto& Part : Clearance)
            if (Part.IsValid() && Part.Type != EClearanceType::CT_Soft)
                Result.Add({Part.ClearanceBox, Part.RelativeTransform * Transform});
        if (Result.IsEmpty())
        {
            const FBox Box = Buildable->GetCombinedClearanceBox();
            if (Box.IsValid) Result.Add({Box, Transform});
        }
        return Result;
    }

    // Outlets can mount on a foundation side as well as a separate wall.
    // Resolve the surface actually hit; proximity alone must not join floors.
    TArray<FBody> MountingSurface(AFGBuildablePowerPole* Pole, UPFPowerConnection*& DirectFloor)
    {
        UWorld* World = Pole->GetWorld();
        FCollisionQueryParams Params(SCENE_QUERY_STAT(PFMountingWall), false, Pole);
        FCollisionObjectQueryParams Objects;
        Objects.AddObjectTypesToQuery(ECC_WorldStatic);
        Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
        for (FVector Axis : {Pole->GetActorForwardVector(), Pole->GetActorRightVector()})
        {
            TArray<FHitResult> Hits;
            const FVector Center = Pole->GetActorLocation();
            World->LineTraceMultiByObjectType(Hits, Center + Axis * 100., Center - Axis * 100., Objects, Params);
            for (const auto& Hit : Hits)
            {
                FInstanceHandle Handle;
                auto* Instances = AAbstractInstanceManager::GetInstanceManager(World);
                const bool InstanceHit = Instances && Instances->ResolveHit(Hit, Handle);
                // Foundation mesh collision is instanced even though powered
                // foundations retain an actor for their electrical terminal.
                AActor* Surface = InstanceHit ? Handle.GetOwner() : Hit.GetActor();
                if (auto* Actor = Surface)
                    if (auto* Floor = Actor->FindComponentByClass<UPFPowerConnection>())
                        if (FMath::Abs(FVector::DotProduct(Hit.ImpactNormal, Actor->GetActorUpVector())) < .1)
                        {
                            DirectFloor = Floor;
                            return {};
                        }
                if (auto* Wall = Cast<AFGBuildableWall>(Surface)) return Bodies(Wall, Wall->GetActorTransform());
                FLightweightBuildableInstanceRef Ref;
                if (InstanceHit &&
                    AFGLightweightBuildableSubsystem::ResolveLightweightInstance(Handle, Ref) &&
                    Ref.GetBuildableClass() && Ref.GetBuildableClass()->IsChildOf(AFGBuildableWall::StaticClass()))
                    return Bodies(Ref.GetBuildableClass()->GetDefaultObject<AFGBuildableWall>(), Ref.GetBuildableTransform());
            }
        }
        return {};
    }
}

void APFNetworkSubsystem::Reconcile()
{
    static const bool MeasureQA = FParse::Param(FCommandLine::Get(), TEXT("PFExtendedTest"));
    const double QAStart = MeasureQA ? FPlatformTime::Seconds() : 0;
    ON_SCOPE_EXIT
    {
        if (MeasureQA) { const double Ms=(FPlatformTime::Seconds()-QAStart)*1000.; ++QAPasses; QATotalMs+=Ms; QAMaxMs=FMath::Max(QAMaxMs,Ms); }
    };
    // Designer contents intentionally bypass the normal factory buildable list.
    // Discover them through each Designer's native membership list, while
    // retaining the existing SameDesigner boundary for all electrical links.
    TArray<AFGBuildable*> DesignerContents;
    for (const auto& Candidate : Candidates)
        if (auto* Designer = Cast<AFGBuildableBlueprintDesigner>(Candidate.Get()))
            if (!Designer->IsActorBeingDestroyed())
                DesignerContents.Append(Designer->GetBuildablesInBlueprintDesigner());
    for (auto* Buildable : DesignerContents)
        if (IsValid(Buildable)) Candidates.Add(Buildable);
    Tiles.Reset();
    TArray<TWeakObjectPtr<AFGBuildable>> Consumers;
    for (auto It = Candidates.CreateIterator(); It; ++It)
    {
        AFGBuildable* Actor = It->Get();
        if (!IsValid(Actor) || Actor->IsActorBeingDestroyed()) { It.RemoveCurrent(); continue; }
        if (auto* Tile = Actor->FindComponentByClass<UPFPowerConnection>())
        {
            if (!Tile->HasBegunPlay()) { MarkDirty(); continue; }
            Tiles.Add(Tile);
            if (!InitializedTiles.Contains(Tile))
            {
                // Only our private terminals are cleared. User cables and
                // vanilla/modded hidden links between other actors are untouched.
                Tile->ClearHiddenConnections();
                InitializedTiles.Add(Tile);
            }
        }
        else if (!Actor->IsA<AFGBuildableGenerator>() && Actor->FindComponentByClass<UFGPowerConnectionComponent>()) Consumers.Add(Actor);
    }
    for (auto It = InitializedTiles.CreateIterator(); It; ++It) if (!It->IsValid()) It.RemoveCurrent();

    TArray<PFGeometry::FTile> Geometry;
    TMap<FIntVector, TArray<int32>> Spatial;
    for (int32 I = 0; I < Tiles.Num(); ++I)
    {
        Geometry.Add(Tiles[I]->GetTile());
        for (const auto& Cell : PFGeometry::CellsFor(Geometry[I].WorldBounds().ExpandBy(PFGeometry::ContactTolerance))) Spatial.FindOrAdd(Cell).Add(I);
    }
    auto Query = [&Spatial](const FBox& Bounds)
    {
        TSet<int32> Result;
        for (const auto& Cell : PFGeometry::CellsFor(Bounds))
            if (const auto* Found = Spatial.Find(Cell)) for (int32 Index : *Found) Result.Add(Index);
        return Result;
    };
    auto SameDesigner = [](AActor* A, AActor* B)
    {
        return CastChecked<AFGBuildable>(A)->GetBlueprintDesigner() == CastChecked<AFGBuildable>(B)->GetBlueprintDesigner();
    };
    TSet<FPFLink> Desired;
    for (int32 I = 0; I < Tiles.Num(); ++I)
        for (int32 J : Query(Geometry[I].WorldBounds().ExpandBy(PFGeometry::EdgeTolerance)))
            if (J > I && SameDesigner(Tiles[I]->GetOwner(), Tiles[J]->GetOwner()) && PFGeometry::ShareFace(Geometry[I], Geometry[J]))
                Desired.Add(FPFLink(Tiles[I].Get(), Tiles[J].Get()));

    for (const auto& Weak : Consumers)
    {
        AFGBuildable* Actor = Weak.Get();
        auto* Pole = Cast<AFGBuildablePowerPole>(Actor);
        TArray<FBody> Contact;
        UPFPowerConnection* DirectFloor = nullptr;
        const bool WallOutlet = Pole && (Pole->GetPowerPoleType() == EPowerPoleType::PPT_WALL || Pole->GetPowerPoleType() == EPowerPoleType::PPT_WALL_DOUBLE);
        if (WallOutlet) Contact = MountingSurface(Pole, DirectFloor);
        else if (Pole)
        {
            // Mk.2/Mk.3 clearance starts above the pedestal (Mk.3: 80cm).
            // Their actual construction anchor is the base at local Z=0.
            FBox Base = Pole->GetCombinedClearanceBox();
            if (!Base.IsValid) Base = FBox(FVector(-40, -40, 0), FVector(40, 40, 1));
            Base.Min.Z = 0;
            Base.Max.Z = 1;
            Contact.Add({Base, Pole->GetActorTransform()});
        }
        else Contact = Bodies(Actor, Actor->GetActorTransform());

        TSet<int32> Supported;
        if (DirectFloor && SameDesigner(Actor, DirectFloor->GetOwner()))
        {
            const int32 Index = Tiles.IndexOfByPredicate([&](const auto& Tile) { return Tile.Get() == DirectFloor; });
            if (Index != INDEX_NONE) Supported.Add(Index);
        }
        for (const FBody& Body : Contact)
        {
            FBox Search = Body.Box.TransformBy(Body.Transform);
            // Only the bottom contact plane matters, even for very tall machines.
            Search.Max.Z = Search.Min.Z + PFGeometry::ContactTolerance;
            Search.Min.Z -= PFGeometry::ContactTolerance;
            for (int32 I : Query(Search))
                if (SameDesigner(Actor, Tiles[I]->GetOwner()) && PFGeometry::Supports(Geometry[I], Body.Box, Body.Transform)) Supported.Add(I);
        }
        if (WallOutlet && FParse::Param(FCommandLine::Get(), TEXT("PFOutletDiagnostics")))
            UE_LOG(LogTrapperPoweredFoundations, Log, TEXT("Outlet %s at %s: mount=%s, wallBodies=%d, supportedTiles=%d"),
                *Actor->GetName(), *Actor->GetActorLocation().ToString(), *GetNameSafe(DirectFloor ? DirectFloor->GetOwner() : nullptr), Contact.Num(), Supported.Num());
        if (Supported.IsEmpty()) continue;
        TInlineComponentArray<UFGPowerConnectionComponent*> Terminals(Actor);
        for (auto* Terminal : Terminals)
        {
            // Passive switches must not be bypassed. Poles are intentional feeds;
            // ordinary machines qualify through their actual power-info terminal.
            if (!Pole && !Terminal->GetPowerInfo()) continue;
            int32 Best = INDEX_NONE;
            int32 BestRank = -1;
            for (int32 I : Supported)
            {
                // Any supporting powered island can supply a large machine, but
                // it gets only one link and never shorts separate islands. Use
                // production/storage availability rather than fuse state so a
                // tripped circuit does not cause the load to hop between feeds.
                UFGPowerCircuit* Circuit = Tiles[I]->GetPowerCircuit();
                const bool HasSupply = Circuit && (Circuit->GetPowerProductionCapacity() > 0 || Circuit->GetBatterySumPowerStore() > 0);
                const int32 Rank = (HasSupply ? 2 : 0) + (Links.Contains(FPFLink(Tiles[I].Get(), Terminal)) ? 1 : 0);
                if (Rank > BestRank || (Rank == BestRank && Tiles[I]->GetOwner()->GetFName().LexicalLess(Tiles[Best]->GetOwner()->GetFName())))
                { Best = I; BestRank = Rank; }
            }
            if (Best != INDEX_NONE) Desired.Add(FPFLink(Tiles[Best].Get(), Terminal));
        }
    }

    int32 Added = 0, Removed = 0;
    for (const FPFLink& Link : Links)
        if (!Desired.Contains(Link) && Link.A.IsValid() && Link.B.IsValid())
        { Link.A->RemoveHiddenConnection(Link.B.Get()); ++Removed; }
    for (const FPFLink& Link : Desired)
        if (!Link.A->HasHiddenConnection(Link.B.Get()))
        { Link.A->AddHiddenConnection(Link.B.Get()); ++Added; }
    Links = MoveTemp(Desired);
    if (Added || Removed) UE_LOG(LogTrapperPoweredFoundations, Log, TEXT("Grid: %d tiles, %d hidden links; added %d, removed %d"), Tiles.Num(), Links.Num(), Added, Removed);
}
