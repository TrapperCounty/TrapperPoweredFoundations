#include "PFPowerConnection.h"
#include "Buildables/FGBuildable.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"

// Called when a replicated foundation component arrives. The ordinary game
// never enables this observer and no replicated/game state is changed here.
void PFStartMultiplayerClientObserver(UWorld* World)
{
    if(!World||World->GetNetMode()!=NM_Client||!FParse::Param(FCommandLine::Get(),TEXT("PFMultiplayerClientTest")))return;
    static TWeakObjectPtr<UWorld> StartedWorld;if(StartedWorld==World)return;StartedWorld=World;
    struct FObserver
    {
        TWeakObjectPtr<UWorld> W;FTimerHandle Timer;FString Seen,Report;
        FString Dir=TEXT("S:/SatisfactoryModding/QA/NetworkHandshake");
        void Tick()
        {
            if(!W.IsValid())return;FString PhaseText;if(!FFileHelper::LoadFileToString(PhaseText,*(Dir/TEXT("phase.txt")))||PhaseText==Seen)return;
            FString Run,PhaseString;if(!PhaseText.Split(TEXT("|"),&Run,&PhaseString))return;const int32 Phase=FCString::Atoi(*PhaseString);if(Phase<1||Phase>6)return;
            bool OK=true;int32 Present=0;TArray<FString> Details;
            for(int32 I=0;I<3;++I)
            {
                const int32 Height=I==0?1:(I==1?2:4);const FVector Position(100000+I*800,100000,50000-Height*50);
                AFGBuildable* Tile=nullptr;
                for(TActorIterator<AFGBuildable> It(W.Get());It;++It)if(IsValid(*It)&&It->GetActorLocation().Equals(Position,.2)&&It->FindComponentByClass<UPFPowerConnection>()){Tile=*It;break;}
                const bool Absent=Phase==6||(Phase==2&&I==1);
                if(Absent){OK &= Tile==nullptr;continue;}
                if(!Tile){OK=false;continue;}++Present;
                auto* Connection=Tile->FindComponentByClass<UPFPowerConnection>();const bool Expected=Phase!=4&&!(Phase==2&&I==2);
                bool RimOK=false;TArray<UStaticMeshComponent*> Meshes;Tile->GetComponents(Meshes);
                for(auto* M:Meshes)if(M->GetFName()==TEXT("PF_CornerLights"))if(auto* MID=Cast<UMaterialInstanceDynamic>(M->GetMaterial(0)))
                {const auto Color=MID->K2_GetVectorParameterValue(TEXT("PowerColor"));RimOK=Color.Equals(Expected?FLinearColor(.015f,1.6f,.03f):FLinearColor(1.8f,.025f,.015f),.01f);}
                const bool Good=!Tile->HasAuthority()&&Connection->GetFName()==TEXT("PoweredFloorConnection")&&Connection->IsGridPowered()==Expected&&RimOK;
                OK &= Good;Details.Add(FString::Printf(TEXT("tile%d authority=%d powered=%d expected=%d rim=%d"),I,Tile->HasAuthority(),Connection->IsGridPowered(),Expected,RimOK));
            }
            FFileHelper::SaveStringToFile(FString::Printf(TEXT("Phase=%d present=%d ready=%d\n"),Phase,Present,OK)+FString::Join(Details,TEXT("\n")),*(Dir/TEXT("client-status.txt")));
            if(!OK)return;
            Report+=FString::Printf(TEXT("PASS: Native client phase %d replicated presence, stable components, authority and rim colors\n"),Phase);
            FFileHelper::SaveStringToFile(Report,*(Dir/TEXT("client-report.txt")));
            Seen=PhaseText;FFileHelper::SaveStringToFile(Seen,*(Dir/TEXT("ack.txt")));
            // The timer owns this object; keeping it alive during the call makes
            // the final clear safe when no other delegate retains it.
            if(Phase==6)W->GetTimerManager().ClearTimer(Timer);
        }
    };
    auto Observer=MakeShared<FObserver>();Observer->W=World;
    World->GetTimerManager().SetTimer(Observer->Timer,FTimerDelegate::CreateLambda([Observer]{auto KeepAlive=Observer;KeepAlive->Tick();}),1.f,true,5.f);
}
