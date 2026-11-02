// Opt-in packaged-game smoke test: launch with -PFIntegrationTest and enter a
// disposable world. Nothing runs in ordinary games. Fixtures remove themselves.
#include "PFNetworkSubsystem.h"
#include "PFPowerConnection.h"
#include "FGBuildableSubsystem.h"
#include "FGPowerInfoComponent.h"
#include "FGClearanceInterface.h"
#include "FGSaveSession.h"
#include "FGLightweightBuildableSubsystem.h"
#include "AbstractInstanceManager.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableWire.h"
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY_STATIC(LogPFTest, Log, All);

namespace
{
    struct FPFTest : TSharedFromThis<FPFTest>
    {
        TWeakObjectPtr<UWorld> World;
        TArray<TWeakObjectPtr<AFGBuildable>> Actors;
        TWeakObjectPtr<AFGBuildable> A, B, C, Diagonal, Pole, Pole2, Pole3, Wall, Outlet, Machine, LargeMachine, Source, Wire, Source2;
        int32 Stage = 0;
        int32 Failed = 0;
        FString Report;
        FTimerHandle Timer;
        TWeakObjectPtr<AActor> PreviousView;
        TWeakObjectPtr<ACameraActor> Camera;
        const FVector Origin = FVector(0, 0, 50000);

        void Check(bool OK, const FString& Message)
        {
            Report += FString::Printf(TEXT("%s: %s\n"), OK ? TEXT("PASS") : TEXT("FAIL"), *Message);
            UE_LOG(LogPFTest, Display, TEXT("%s: %s"), OK ? TEXT("PASS") : TEXT("FAIL"), *Message);
            if (!OK) ++Failed;
        }

        AFGBuildable* Spawn(const FString& Path, FVector Position, bool AlignBottom = false)
        {
            UClass* Type = LoadObject<UClass>(nullptr, *(Path + TEXT(".") + FPackageName::GetShortName(Path) + TEXT("_C")));
            if (!Type) { Check(false, TEXT("Missing fixture class ") + Path); return nullptr; }
            FTransform Transform(Origin + Position);
            if (AlignBottom)
            {
                FBox Bounds = Type->GetDefaultObject<AFGBuildable>()->GetCombinedClearanceBox();
                if (Bounds.IsValid) Transform.AddToTranslation(FVector(0, 0, -Bounds.Min.Z));
            }
            auto* Buildables = AFGBuildableSubsystem::Get(World.Get());
            auto* Actor = Buildables->BeginSpawnBuildable(Type, Transform);
            if (!Actor) { Check(false, TEXT("Failed fixture spawn ") + Path); return nullptr; }
            Actor->FinishSpawning(Transform);
            if (!Buildables->GetAllBuildablesRef().Contains(Actor)) Buildables->AddBuildable(Actor);
            Actors.Add(Actor);
            UE_LOG(LogPFTest, Display, TEXT("Fixture %s at %s; clearance %s"), *Actor->GetName(), *Actor->GetActorLocation().ToString(), *Actor->GetCombinedClearanceBox().ToString());
            return Actor;
        }

        UFGPowerConnectionComponent* Connection(const TWeakObjectPtr<AFGBuildable>& Actor) const
        { return Actor.IsValid() ? Actor->FindComponentByClass<UFGPowerConnectionComponent>() : nullptr; }

        bool Same(const TWeakObjectPtr<AFGBuildable>& X, const TWeakObjectPtr<AFGBuildable>& Y) const
        {
            auto* XC = Connection(X); auto* YC = Connection(Y);
            return XC && YC && XC->GetCircuitID() != INDEX_NONE && XC->GetCircuitID() == YC->GetCircuitID();
        }

        void RemoveTestWall()
        {
            // Vanilla walls may have already converted into actor-free instances.
            // Removing the transient actor alone does not dismantle that instance.
            FCollisionObjectQueryParams Objects(FCollisionObjectQueryParams::AllObjects);
            FCollisionQueryParams Params(SCENE_QUERY_STAT(PFTestWall), false, Outlet.Get());
            TArray<FHitResult> Hits;
            const FVector Center = Origin + FVector(0, 350, 200);
            World->LineTraceMultiByObjectType(Hits, Center + FVector(100, 0, 0), Center - FVector(100, 0, 0), Objects, Params);
            for (const auto& Hit : Hits)
            {
                FInstanceHandle Instance;
                FLightweightBuildableInstanceRef Ref;
                auto* Manager = AAbstractInstanceManager::GetInstanceManager(World.Get());
                if (Manager && Manager->ResolveHit(Hit, Instance) && AFGLightweightBuildableSubsystem::ResolveLightweightInstance(Instance, Ref) &&
                    Ref.GetBuildableClass() && Ref.GetBuildableClass()->GetName() == TEXT("Build_Wall_8x4_01_C") && Ref.GetBuildableTransform().GetLocation().Equals(Origin + FVector(0, 350, 0), 1))
                    Ref.Remove();
            }
            if (Wall.IsValid()) { IFGDismantleInterface::Execute_Dismantle(Wall.Get()); Wall->Destroy(); }
        }

        void Advance()
        {
            if (!World.IsValid()) return;
            switch (Stage++)
            {
            case 0:
            {
                for (AFGBuildable* Existing : AFGBuildableSubsystem::Get(World.Get())->GetAllBuildablesRef())
                    if (Existing->FindComponentByClass<UPFPowerConnection>() && FMath::Abs(Existing->GetActorLocation().Z - Origin.Z) < 300 && FVector::Dist2D(Existing->GetActorLocation(), Origin) < 4000)
                    {
                        Check(false, TEXT("Existing fixture found. Load the original factory for integration tests, or use -PFReloadTest for a validation save."));
                        FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("TrapperPoweredFoundations-IntegrationTest.txt")));
                        World->GetTimerManager().ClearTimer(Timer);
                        return;
                    }
                A = Spawn(TEXT("/TrapperPoweredFoundations/Foundations/Build_PoweredFoundation_1m"), FVector(0, 0, -50));
                B = Spawn(TEXT("/TrapperPoweredFoundations/Foundations/Build_PoweredFoundation_2m"), FVector(800, 0, -100));
                C = Spawn(TEXT("/TrapperPoweredFoundations/Foundations/Build_PoweredFoundation_4m"), FVector(1600, 0, -200));
                Diagonal = Spawn(TEXT("/TrapperPoweredFoundations/Foundations/Build_PoweredFoundation_1m"), FVector(2400, 800, -50));
                Pole = Spawn(TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleMk1/Build_PowerPoleMk1"), FVector(0, 0, 0), true);
                Pole2 = Spawn(TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleMk2/Build_PowerPoleMk2"), FVector(1600, 200, 0));
                Pole3 = Spawn(TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleMk3/Build_PowerPoleMk3"), FVector(1600, -200, 0));
                Wall = Spawn(TEXT("/Game/FactoryGame/Buildable/Building/Wall/Build_Wall_8x4_01"), FVector(0, 350, 0), true);
                Outlet = Spawn(TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleWall/Build_PowerPoleWall"), FVector(0, 350, 200));
                Machine = Spawn(TEXT("/Game/FactoryGame/Buildable/Factory/SmelterMk1/Build_SmelterMk1"), FVector(1600, 0, 0), true);
                LargeMachine = Spawn(TEXT("/Game/FactoryGame/Buildable/Factory/AssemblerMk1/Build_AssemblerMk1"), FVector(800, 0, 0), true);
                Source = Spawn(TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleMk3/Build_PowerPoleMk3"), FVector(-1600, 0, 0), true);
                Wire = Spawn(TEXT("/Game/FactoryGame/Buildable/Factory/PowerLine/Build_PowerLine"), FVector(-800, 0, 200));
                if (!Connection(A) || !Connection(B) || !Connection(C) || !Connection(Pole) || !Connection(Source) || !Connection(Machine) || !Wire.IsValid())
                { Check(false, TEXT("Fixture components are present")); Finish(); return; }
                auto* Info = NewObject<UFGPowerInfoComponent>(Source.Get());
                Info->RegisterComponent();
                Connection(Source)->SetPowerInfo(Info);
                Info->SetBaseProduction(100);
                Check(CastChecked<AFGBuildableWire>(Wire.Get())->Connect(Connection(Source), Connection(Pole)), TEXT("A real vanilla cable connects source to the floor pole"));
                APFNetworkSubsystem::Get(World.Get())->MarkDirty();
                if (FParse::Param(FCommandLine::Get(), TEXT("PFShowTest")))
                {
                    if (auto* PC = World->GetFirstPlayerController())
                    {
                        PreviousView = PC->GetViewTarget();
                        const FVector Eye = Origin + FVector(-1700, -2500, 1900);
                        Camera = World->SpawnActor<ACameraActor>(Eye, (Origin + FVector(700, 0, 0) - Eye).Rotation());
                        if (Camera.IsValid()) PC->SetViewTarget(Camera.Get());
                    }
                }
                break;
            }
            case 1:
            {
                FHitResult FloorHit;
                const FVector Top = Origin + FVector(2400, 800, 0);
                const bool HitFloor = World->LineTraceSingleByObjectType(FloorHit, Top + FVector(0, 0, 20), Top - FVector(0, 0, 20), FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects));
                Check(HitFloor && FMath::Abs(FloorHit.ImpactPoint.Z - Top.Z) < 2, TEXT("Powered floor has solid collision at the expected top surface"));
                Check(Same(A, B) && Same(B, C), TEXT("All three foundation sizes form one native circuit"));
                Check(Same(Pole, C), TEXT("Vanilla pole supplies the whole connected floor"));
                Check(Same(Machine, C), TEXT("Smelter attaches through its footprint"));
                Check(Same(Pole2, C), TEXT("Mk.2 pole attaches at its physical base"));
                Check(Same(Pole3, C), TEXT("Mk.3 pole attaches at its physical base"));
                Check(Same(Outlet, A), TEXT("Wall outlet attaches through its mounting wall"));
                Check(!Same(C, Diagonal), TEXT("Diagonal foundation is electrically isolated"));
                Check(Connection(C) && Connection(C)->HasPower(), TEXT("Power reaches the distant foundation"));
                Check(C.IsValid() && C->FindComponentByClass<UPFPowerConnection>()->IsGridPowered(), TEXT("Replicated glow state becomes energized"));
                if (B.IsValid()) B->TurnOffAndDestroy();
                APFNetworkSubsystem::Get(World.Get())->MarkDirty();
                break;
            }
            case 2:
                Check(!Same(A, C), TEXT("Dismantling the middle tile splits the native circuit"));
                Check(Connection(C) && !Connection(C)->HasPower(), TEXT("Separated floor loses power"));
                Check(Same(Machine, C), TEXT("Machine stays attached to its remaining floor"));
                Check(Same(LargeMachine, A), TEXT("Large machine spanning disconnected tiles selects a supporting powered island"));
                B = Spawn(TEXT("/TrapperPoweredFoundations/Foundations/Build_PoweredFoundation_2m"), FVector(800, 0, -100));
                APFNetworkSubsystem::Get(World.Get())->MarkDirty();
                break;
            case 3:
            {
                Check(Same(A, C) && Connection(C)->HasPower(), TEXT("Replacing the middle tile restores power"));
                Source2 = Spawn(TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleMk3/Build_PowerPoleMk3"), FVector(3200, 0, 0));
                auto* SecondWire = Cast<AFGBuildableWire>(Spawn(TEXT("/Game/FactoryGame/Buildable/Factory/PowerLine/Build_PowerLine"), FVector(2400, 0, 200)));
                if (Connection(Source2) && Connection(Pole3) && SecondWire)
                {
                    auto* Info = NewObject<UFGPowerInfoComponent>(Source2.Get());
                    Info->RegisterComponent();
                    Connection(Source2)->SetPowerInfo(Info);
                    Info->SetBaseProduction(30);
                    SecondWire->Connect(Connection(Source2), Connection(Pole3));
                }
                break;
            }
            case 4:
                Check(Same(Source, Source2), TEXT("Two independent power feeds merge through the floor"));
                if (Pole.IsValid()) Pole->TurnOffAndDestroy();
                APFNetworkSubsystem::Get(World.Get())->MarkDirty();
                break;
            case 5:
                Check(Connection(C) && Connection(C)->HasPower(), TEXT("Second feed keeps the floor powered after the first is removed"));
                if (Pole3.IsValid()) Pole3->TurnOffAndDestroy();
                RemoveTestWall();
                APFNetworkSubsystem::Get(World.Get())->MarkDirty();
                break;
            case 6:
                Check(Connection(C) && !Connection(C)->HasPower(), TEXT("Removing the feed pole turns off the floor"));
                Check(!Same(Outlet, A), TEXT("Removing the mounting wall disconnects the outlet"));
                if (FParse::Param(FCommandLine::Get(), TEXT("PFSaveTest")))
                {
                    // Exclude synthetic power sources from the persistence check;
                    // the saved fixture uses only stable buildable components.
                    if (Source.IsValid()) Source->Destroy();
                    if (Source2.IsValid()) Source2->Destroy();
                    const FString Name = TEXT("PF_Validation_") + FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
                    Report += TEXT("Persistence save: ") + Name + TEXT("\n");
                    auto Self = AsShared();
                    UFGSaveSession::Get(World.Get())->SaveGame(Name, FOnSaveGameComplete::CreateLambda([Self](bool OK, const FText& Error, void*)
                    {
                        Self->Check(OK, TEXT("Save of powered foundations and their stable components completed: ") + Error.ToString());
                        Self->Finish();
                    }), nullptr);
                    break;
                }
                Finish();
                break;
            }
        }

        void Finish()
        {
            RemoveTestWall();
            if (auto* PC = World->GetFirstPlayerController()) if (PreviousView.IsValid()) PC->SetViewTarget(PreviousView.Get());
            if (Camera.IsValid()) Camera->Destroy();
            for (int32 I = Actors.Num() - 1; I >= 0; --I) if (Actors[I].IsValid()) Actors[I]->TurnOffAndDestroy();
            Report += FString::Printf(TEXT("Failures: %d\n"), Failed);
            const FString Path = FPaths::ProjectSavedDir() / TEXT("TrapperPoweredFoundations-IntegrationTest.txt");
            FFileHelper::SaveStringToFile(Report, *Path);
            UE_LOG(LogPFTest, Display, TEXT("Integration test finished: %d failures. Report: %s"), Failed, *Path);
            World->GetTimerManager().ClearTimer(Timer);
        }
    };
}

#include "PFExtendedTest.inl"
#include "PFSideOutletTest.inl"
#include "PFCustomizationTest.inl"
#include "PFStackPowerTest.inl"
#include "PFMultiplayerTest.inl"

void PFStartIntegrationTest(UWorld* World)
{
    if (FParse::Param(FCommandLine::Get(), TEXT("PFStackTest")) || FParse::Param(FCommandLine::Get(), TEXT("PFStackReloadTest")))
    {
        PFStartStackPowerTest(World,FParse::Param(FCommandLine::Get(),TEXT("PFStackReloadTest")));
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("TPFMigratePrivateSave")) || FParse::Param(FCommandLine::Get(), TEXT("TPFMigrationReloadTest")))
    {
        extern void TPFStartPrivateMigrationTest(UWorld*, bool);
        TPFStartPrivateMigrationTest(World,FParse::Param(FCommandLine::Get(),TEXT("TPFMigrationReloadTest")));
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("PFCustomizationTest")) || FParse::Param(FCommandLine::Get(), TEXT("PFCustomizationReloadTest")))
    {
        PFStartCustomizationTest(World,FParse::Param(FCommandLine::Get(),TEXT("PFCustomizationReloadTest")));
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("PFCustomizationBlueprintTest")) || FParse::Param(FCommandLine::Get(), TEXT("PFCustomizationBlueprintPlaceTest")))
    {
        extern void PFStartBlueprintTest(UWorld* World,bool Place);
        PFStartBlueprintTest(World,FParse::Param(FCommandLine::Get(),TEXT("PFCustomizationBlueprintPlaceTest")));
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("PFSideOutletTest")))
    {
        PFStartSideOutletTest(World);
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("PFMultiplayerServerTest")))
    {
        PFStartMultiplayerServerTest(World);
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("PFDenseBlueprintTest")) || FParse::Param(FCommandLine::Get(), TEXT("PFDenseBlueprintReloadTest")))
    {
        extern void PFStartDenseBlueprintTest(UWorld* World,bool Reload);
        PFStartDenseBlueprintTest(World,FParse::Param(FCommandLine::Get(),TEXT("PFDenseBlueprintReloadTest")));
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("PFExtendedTest")))
    {
        PFStartExtendedTest(World);
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("PFBlueprintMultiOverflowTest")) || FParse::Param(FCommandLine::Get(), TEXT("PFBlueprintMultiOverflowReloadTest")))
    {
        extern void PFStartBlueprintMultiOverflowTest(UWorld* World,bool Reload);
        PFStartBlueprintMultiOverflowTest(World,FParse::Param(FCommandLine::Get(),TEXT("PFBlueprintMultiOverflowReloadTest")));
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("PFBlueprintMultiPartialTest")) || FParse::Param(FCommandLine::Get(), TEXT("PFBlueprintMultiPartialReloadTest")))
    {
        extern void PFStartBlueprintMultiPartialTest(UWorld* World,bool Reload);
        PFStartBlueprintMultiPartialTest(World,FParse::Param(FCommandLine::Get(),TEXT("PFBlueprintMultiPartialReloadTest")));
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("PFBlueprintPartialTest")) || FParse::Param(FCommandLine::Get(), TEXT("PFBlueprintPartialReloadTest")))
    {
        extern void PFStartBlueprintPartialTest(UWorld* World,bool Reload);
        PFStartBlueprintPartialTest(World,FParse::Param(FCommandLine::Get(),TEXT("PFBlueprintPartialReloadTest")));
        return;
    }

    if (FParse::Param(FCommandLine::Get(), TEXT("PFBlueprintOverflowTest")) || FParse::Param(FCommandLine::Get(), TEXT("PFBlueprintOverflowReloadTest")))
    {
        extern void PFStartBlueprintOverflowTest(UWorld* World,bool Reload);
        PFStartBlueprintOverflowTest(World,FParse::Param(FCommandLine::Get(),TEXT("PFBlueprintOverflowReloadTest")));
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("PFBlueprintDismantleTest")))
    {
        extern void PFStartBlueprintDismantleTest(UWorld* World);
        PFStartBlueprintDismantleTest(World);
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("PFSnappedBlueprintTest")) || FParse::Param(FCommandLine::Get(), TEXT("PFSnappedBlueprintReloadTest")))
    {
        extern void PFStartSnappedBlueprintTest(UWorld* World, bool Reload);
        PFStartSnappedBlueprintTest(World, FParse::Param(FCommandLine::Get(), TEXT("PFSnappedBlueprintReloadTest")));
        return;
    }

    if (FParse::Param(FCommandLine::Get(), TEXT("PFLargeBlueprintTest")) || FParse::Param(FCommandLine::Get(), TEXT("PFLargeBlueprintReloadTest")))
    {
        extern void PFStartLargeBlueprintTest(UWorld* World, bool Reload);
        PFStartLargeBlueprintTest(World, FParse::Param(FCommandLine::Get(), TEXT("PFLargeBlueprintReloadTest")));
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("PFBlueprintDesignerTest")) || FParse::Param(FCommandLine::Get(), TEXT("PFBlueprintPlaceTest")))
    {
        extern void PFStartBlueprintTest(UWorld* World, bool Place);
        PFStartBlueprintTest(World, FParse::Param(FCommandLine::Get(), TEXT("PFBlueprintPlaceTest")));
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("PFPartialRefundTest")) || FParse::Param(FCommandLine::Get(), TEXT("PFPartialRefundReloadTest")))
    {
        extern void PFStartPartialPersistenceTest(UWorld* World, bool Reload);
        PFStartPartialPersistenceTest(World, FParse::Param(FCommandLine::Get(), TEXT("PFPartialRefundReloadTest")));
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("PFMaxOverflowTest")))
    {
        extern void PFStartMaxOverflowTest(UWorld* World);
        PFStartMaxOverflowTest(World);
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("PFMixedRefundTest")))
    {
        extern void PFStartMixedRefundTest(UWorld* World);
        PFStartMixedRefundTest(World);
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("PFVerticalZoopTest")))
    {
        extern void PFStartVerticalZoopTest(UWorld* World);
        PFStartVerticalZoopTest(World);
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("PFAlignmentTest")))
    {
        extern void PFStartAlignmentTest(UWorld* World);
        PFStartAlignmentTest(World);
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("PFPlacementTest")))
    {
        extern void PFStartPlacementTest(UWorld* World);
        PFStartPlacementTest(World);
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("PFReloadTest")))
    {
        FTimerHandle Handle;
        TWeakObjectPtr<UWorld> WeakWorld = World;
        World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakWorld]
        {
            if (!WeakWorld.IsValid()) return;
            TMap<int32, UPFPowerConnection*> Floor;
            UFGPowerConnectionComponent* Smelter = nullptr;
            for (AFGBuildable* Actor : AFGBuildableSubsystem::Get(WeakWorld.Get())->GetAllBuildablesRef())
            {
                const FVector P = Actor->GetActorLocation();
                if (P.Z < 49700 || P.Z > 50001 || FMath::Abs(P.Y) > 801) continue;
                if (auto* Node = Actor->FindComponentByClass<UPFPowerConnection>()) Floor.Add(FMath::RoundToInt(P.X), Node);
                else if (Actor->GetClass()->GetName() == TEXT("Build_SmelterMk1_C") && FMath::Abs(P.X - 1600) < 1) Smelter = Actor->FindComponentByClass<UFGPowerConnectionComponent>();
            }
            FString Report;
            int32 Errors = 0;
            auto Check = [&Report, &Errors](bool OK, const TCHAR* Text)
            {
                Report += FString::Printf(TEXT("%s: %s\n"), OK ? TEXT("PASS") : TEXT("FAIL"), Text);
                if (!OK) ++Errors;
            };
            Check(Floor.Num() == 4, TEXT("All four saved foundation actors and power components survived reload"));
            if (Floor.Contains(0) && Floor.Contains(800) && Floor.Contains(1600) && Floor.Contains(2400))
            {
                Check(Floor[0]->GetCircuitID() != INDEX_NONE && Floor[0]->GetCircuitID() == Floor[800]->GetCircuitID() && Floor[800]->GetCircuitID() == Floor[1600]->GetCircuitID(), TEXT("The connected floor circuit is rebuilt after load"));
                Check(Floor[1600]->GetCircuitID() != Floor[2400]->GetCircuitID(), TEXT("Diagonal isolation survives load"));
                Check(Smelter && Smelter->GetCircuitID() == Floor[1600]->GetCircuitID(), TEXT("Machine attachment survives load"));
            }
            Report += FString::Printf(TEXT("Failures: %d\n"), Errors);
            FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("TrapperPoweredFoundations-ReloadTest.txt")));
            UE_LOG(LogPFTest, Display, TEXT("Persistence reload test:\n%s"), *Report);
        }), 15.f, false);
        return;
    }
    if (!FParse::Param(FCommandLine::Get(), TEXT("PFIntegrationTest"))) return;
    auto Test = MakeShared<FPFTest>();
    Test->World = World;
    World->GetTimerManager().SetTimer(Test->Timer, FTimerDelegate::CreateLambda([Test] { Test->Advance(); }), 5.f, true, 15.f);
}
