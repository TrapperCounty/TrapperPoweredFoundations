// Local-only opt-in QA. Handshake files coordinate assertions, never game state.
#include "Buildables/FGBuildableGeneratorFuel.h"
#include "Misc/Guid.h"
#include "FGSaveSystem.h"
#include "HAL/FileManager.h"
namespace
{
    struct FPFMultiplayerTest : FPFTest
    {
        TWeakObjectPtr<AFGCharacterPlayer> QAPlayer;
        TWeakObjectPtr<AFGBuildable> StandPlatform;
        FTransform OriginalPlayerTransform;
        TArray<FInventoryStack> OriginalInventory;
        FString Run=FGuid::NewGuid().ToString(EGuidFormats::Digits);
        FString Directory=TEXT("S:/SatisfactoryModding/QA/NetworkHandshake");
        FString Pending;
        double Start=FPlatformTime::Seconds(),ReadyAt=0,SentAt=0;
        int32 Phase=0;
        const FVector Base=FVector(100000,100000,0);
        void Status(const FString& Value)
        {FFileHelper::SaveStringToFile(Value,*(Directory/TEXT("server-status.txt")));}
        void FinishNetwork()
        {
            const auto KeepAlive=AsShared();
            if(QAPlayer.IsValid())
            {
                QAPlayer->SetActorTransform(OriginalPlayerTransform,false,nullptr,ETeleportType::TeleportPhysics);
                TArray<FInventoryStack> Now;QAPlayer->GetInventory()->GetInventoryStacks(Now,true);
                bool Same=Now.Num()==OriginalInventory.Num();
                for(int32 I=0;I<Now.Num()&&I<OriginalInventory.Num();++I)Same &= Now[I].HasItems()==OriginalInventory[I].HasItems()&&(!Now[I].HasItems()||(Now[I].NumItems==OriginalInventory[I].NumItems&&Now[I].Item.GetItemClass()==OriginalInventory[I].Item.GetItemClass()));
                Check(Same,TEXT("Server restores player position and preserves every inventory slot"));
            }
            for(int32 I=Actors.Num()-1;I>=0;--I)if(Actors[I].IsValid())Actors[I]->TurnOffAndDestroy();
            Report+=FString::Printf(TEXT("Failures: %d\n"),Failed);
            FFileHelper::SaveStringToFile(Report,*(Directory/TEXT("server-report.txt")));
            Status(TEXT("Complete\n")+Report);World->GetTimerManager().ClearTimer(Timer);
        }
        AFGBuildable* MakeMiddle(){return Spawn(TEXT("/TrapperPoweredFoundations/Foundations/Build_PoweredFoundation_2m"),Base+FVector(800,0,-100));}
        void TickNetwork()
        {
            const double Now=FPlatformTime::Seconds();
            if(Phase==0)
            {
                UFGSaveSession::Get(World.Get())->SetAutoSaveEnabled(false);
                if(auto* PC=World->GetFirstPlayerController())QAPlayer=Cast<AFGCharacterPlayer>(PC->GetPawn());
                if(!QAPlayer.IsValid())
                {Status(TEXT("Waiting for a local client player\nSave root: ")+UFGSaveSystem::GetSaveDirectoryPath());if(Now-Start>900){Check(false,TEXT("Client joins within 15 minutes"));FinishNetwork();}return;}
                Check(World->GetNetMode()==NM_DedicatedServer,TEXT("Tests execute on a real dedicated server"));
                OriginalPlayerTransform=QAPlayer->GetActorTransform();QAPlayer->GetInventory()->GetInventoryStacks(OriginalInventory,true);
                UFGSaveSession::Get(World.Get())->SetAutoSaveEnabled(false);
                A=Spawn(TEXT("/TrapperPoweredFoundations/Foundations/Build_PoweredFoundation_1m"),Base+FVector(0,0,-50));
                B=MakeMiddle();C=Spawn(TEXT("/TrapperPoweredFoundations/Foundations/Build_PoweredFoundation_4m"),Base+FVector(1600,0,-200));
                StandPlatform=Spawn(TEXT("/TrapperPoweredFoundations/Foundations/Build_PoweredFoundation_4m"),Base+FVector(-1600,0,-200));
                Pole=Spawn(TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleMk1/Build_PowerPoleMk1"),Base);
                Source=Spawn(TEXT("/Game/FactoryGame/Buildable/Factory/GeneratorBiomass/Build_GeneratorBiomass_Automated"),Base+FVector(-5000,-5000,0));
                auto* Gen=Cast<AFGBuildableGeneratorFuel>(Source.Get());
                auto* Fuel=LoadObject<UClass>(nullptr,TEXT("/Game/FactoryGame/Resource/Parts/BioFuel/Desc_Biofuel.Desc_Biofuel_C"));
                Check(Gen&&Fuel&&Gen->GetFuelInventory()->AddStackToIndex(Gen->GetFuelInventoryIndex(),FInventoryStack(100,Fuel),true)==100,TEXT("Server fuels an ordinary biomass generator"));
                Wire=Spawn(TEXT("/Game/FactoryGame/Buildable/Factory/PowerLine/Build_PowerLine"),Base);
                auto* Cable=Cast<AFGBuildableWire>(Wire.Get());
                Check(A.IsValid()&&B.IsValid()&&C.IsValid()&&StandPlatform.IsValid()&&Cable&&Connection(Pole)&&Connection(Source)&&Cable->Connect(Connection(Pole),Connection(Source)),TEXT("Server constructs mixed-height grid and native feed"));
                if(Failed){FinishNetwork();return;}
                QAPlayer->SetActorLocation(Origin+Base+FVector(-1600,0,220),false,nullptr,ETeleportType::TeleportPhysics);
                Phase=1;ReadyAt=Now+15;return;
            }
            if(Now<ReadyAt)return;
            if(Pending.IsEmpty())
            {
                bool State=true;
                const bool Fed=Phase!=4;
                for(int32 I=0;I<3;++I)
                {
                    auto Tile=I==0?A:(I==1?B:C);
                    if(Phase==6||(Phase==2&&I==1)){State &= !Tile.IsValid();continue;}
                    auto* P=Tile.IsValid()?Tile->FindComponentByClass<UPFPowerConnection>():nullptr;
                    const bool Expected=Fed&&!(Phase==2&&I==2);
                    State &= P&&P->HasPower()==Expected&&P->IsGridPowered()==Expected;
                }
                Check(State,FString::Printf(TEXT("Server phase %d authoritative grid state"),Phase));
                if(Failed){FinishNetwork();return;}
                Pending=Run+TEXT("|")+FString::FromInt(Phase);SentAt=Now;
                FFileHelper::SaveStringToFile(Pending,*(Directory/TEXT("phase.txt")));
                Status(TEXT("Awaiting client replication assertions for ")+Pending);return;
            }
            FString Ack;FFileHelper::LoadFileToString(Ack,*(Directory/TEXT("ack.txt")));
            if(Ack!=Pending)
            {if(Now-SentAt>90){Check(false,TEXT("Client acknowledges replicated state within 90 seconds: ")+Pending);FinishNetwork();}return;}
            Check(true,FString::Printf(TEXT("Client independently verifies replicated actors and rim colors in phase %d"),Phase));
            Pending.Reset();
            if(Phase==1)B->TurnOffAndDestroy();
            else if(Phase==2)B=MakeMiddle();
            else if(Phase==3)CastChecked<AFGBuildableWire>(Wire.Get())->Disconnect();
            else if(Phase==4)Check(CastChecked<AFGBuildableWire>(Wire.Get())->Connect(Connection(Pole),Connection(Source)),TEXT("Server restores native cable"));
            else if(Phase==5)
            {
                // Keep the client nearby so disappearance proves destruction,
                // rather than losing network relevance after a teleport.
                for(int32 I=Actors.Num()-1;I>=0;--I)if(Actors[I].IsValid()&&Actors[I]!=StandPlatform)Actors[I]->TurnOffAndDestroy();
            }
            else if(Phase==6){FinishNetwork();return;}
            ++Phase;ReadyAt=Now+8;
        }
    };
}
void PFStartMultiplayerServerTest(UWorld* World)
{
    auto T=MakeShared<FPFMultiplayerTest>();T->World=World;
    IFileManager::Get().MakeDirectory(*T->Directory,true);
    World->GetTimerManager().SetTimer(T->Timer,FTimerDelegate::CreateLambda([T]{auto KeepAlive=T;KeepAlive->TickNetwork();}),1.f,true,15.f);
}
