#include "Equipment/FGBuildGun.h"
#include "Equipment/FGBuildGunPaint.h"
#include "FGCustomizationRecipe.h"
#include "FGRecipeManager.h"
#include "FGRecipe.h"
#include "PFFoundationMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Buildables/FGBuildableFoundation.h"
#include "Hologram/FGBuildableHologram.h"
#include "GameFramework/CharacterMovementComponent.h"
namespace
{
    const TCHAR* PFFinishes[] = {TEXT("Default"),TEXT("Concrete"),TEXT("PolishedConcrete"),TEXT("Asphalt"),TEXT("GripMetal")};
    FString PFFinishSuffix(int32 H,int32 F) { return FString::Printf(TEXT("%dm"),H)+(F?FString(TEXT("_"))+PFFinishes[F]:TEXT("")); }
    UClass* PFLoadClass(const FString& Path) { return LoadObject<UClass>(nullptr,*(Path+TEXT(".")+FPackageName::GetShortName(Path)+TEXT("_C"))); }
    struct FPFCustomizeCase
    {
        TWeakObjectPtr<AFGBuildable> Floor, Neighbor, Load, Feed, Cable;
        FVector Position;
        int32 Height=1, From=0, To=0;
        FString Label;
    };
    struct FPFCustomizationTest : FPFExtendedTest
    {
        TArray<FPFCustomizeCase> Cases;
        int32 Step=0;
        bool Reload=false;
        TWeakObjectPtr<UFGBuildGunStatePaint> Paint;
        TSubclassOf<UFGCustomizationRecipe> PreviousRecipe;
        FTransform PreviousTransform;
        FRotator PreviousRotation;
        EMovementMode PreviousMovement=MOVE_Walking;
        bool MovingPlayer=false, Painting=false, AwaitingAim=false;
        int32 PaintIndex=0;
        void Repeat(float Interval)
        {
            auto Self=StaticCastSharedRef<FPFCustomizationTest>(AsShared());
            World->GetTimerManager().SetTimer(Timer,FTimerDelegate::CreateLambda([Self]{Self->TickCustom();}),Interval,true);
        }
        void AimCase()
        {
            auto& G=Cases[PaintIndex];
            const FString Base=TEXT("/Game/FactoryGame/Buildable/-Shared/Customization/Materials/");
            auto* Recipe=PFLoadClass(Base+TEXT("Recipe_Material_Foundation_")+PFFinishes[G.To]);
            auto* Material=PFLoadClass(Base+TEXT("MaterialDesc_Foundation_")+PFFinishes[G.To]);
            Check(Material&&Material->GetDefaultObject<UFGFactoryCustomizationDescriptor_Material>()->GetBuildableMap().Contains(G.Floor->GetClass()),G.Label+TEXT(" native material map contains powered source"));
            Player->SetActorLocation(G.Position+FVector(-550,0,G.Height*50+350));
            World->GetFirstPlayerController()->SetControlRotation(FRotator(-45,0,0));
            Player->GetBuildGun()->GotoPaintState(Recipe,true);
            AwaitingAim=true;
        }
        void RestorePlayer()
        {
            if(!MovingPlayer||!Player.IsValid())return;
            Player->UnequipBuildGun();
            Player->SetActorTransform(PreviousTransform);
            World->GetFirstPlayerController()->SetControlRotation(PreviousRotation);
            Player->GetCharacterMovement()->SetMovementMode(PreviousMovement);
            MovingPlayer=false;
        }
        FString Manifest() const { return FPaths::ProjectSavedDir()/TEXT("TrapperPoweredFoundations-CustomizationManifest.txt"); }
        AFGBuildable* FindAt(FVector P)
        {
            for(auto* X:AFGBuildableSubsystem::Get(World.Get())->GetAllBuildablesRef())
                if(IsValid(X)&&!X->IsActorBeingDestroyed()&&X->FindComponentByClass<UPFPowerConnection>()&&X->GetActorLocation().Equals(P,.1))return X;
            return nullptr;
        }
        bool Lights(AFGBuildable* X,bool Powered)
        {
            if(!X)return false;
            TInlineComponentArray<UStaticMeshComponent*> Meshes(X);
            for(auto* M:Meshes)if(M->GetFName()==TEXT("PF_CornerLights"))
                if(auto* MID=Cast<UMaterialInstanceDynamic>(M->GetMaterial(0)))
                    return M->GetStaticMesh()&&M->GetStaticMesh()->GetName()==TEXT("SM_PowerCorners")&&MID->K2_GetVectorParameterValue(TEXT("PowerColor")).Equals(Powered?FLinearColor(.015,1.6,.03):FLinearColor(1.8,.025,.015),.001);
            return false;
        }
        void RestoreInventory()
        {
            if(!Player.IsValid())return;
            auto* Inv=Player->GetInventory();Inv->Empty();
            for(int32 I=0;I<InventoryBefore.Num();++I)if(InventoryBefore[I].HasItems())Inv->AddStackToIndex(I,InventoryBefore[I]);
            if(Paint.IsValid())Paint->SetActiveRecipe(PreviousRecipe);
        }
        void SetupCustom()
        {
            UFGSaveSession::Get(World.Get())->SetAutoSaveEnabled(false);
            Player=Cast<AFGCharacterPlayer>(World->GetFirstPlayerController()->GetPawn());
            if(!Player.IsValid()||!Player->GetBuildGun()){Check(false,TEXT("Player/build gun available"));FinishCustom();return;}
            Player->GetInventory()->GetInventoryStacks(InventoryBefore,true);
            Paint=Cast<UFGBuildGunStatePaint>(Player->GetBuildGun()->GetBuildGunStateFor(EBuildGunState::BGS_PAINT));
            if(!Paint.IsValid()){Check(false,TEXT("Native paint state available"));FinishCustom();return;}
            PreviousRecipe=Paint->GetActiveRecipe();
            if(!Reload)
            {
                auto* Inv=Player->GetInventory();Inv->Empty();
                for(const TCHAR* Item:{TEXT("Cement"),TEXT("Plastic"),TEXT("IronPlate"),TEXT("SteelPlate"),TEXT("Wire")})
                {
                    auto* Type=PFLoadClass(FString(TEXT("/Game/FactoryGame/Resource/Parts/"))+Item+TEXT("/Desc_")+Item);
                    if(Type)Inv->AddStack(FInventoryStack(1000,Type),true);
                }
            }
            for(int32 From=0;From<5;++From)for(int32 To=0;To<5;++To)if(From!=To)for(int32 H:{1,2,4})
            {
                const int32 I=Cases.Num();FPFCustomizeCase G;G.From=From;G.To=To;G.Height=H;
                G.Position=Origin+FVector(100000+(I%10)*4000,-100000+(I/10)*4000,-H*50);
                G.Label=FString::Printf(TEXT("%s to %s %dm"),PFFinishes[From],PFFinishes[To],H);
                if(Reload)
                {
                    G.Floor=FindAt(G.Position);G.Neighbor=FindAt(G.Position+FVector(800,0,0));
                    for(auto* X:AFGBuildableSubsystem::Get(World.Get())->GetAllBuildablesRef())
                        if(X->GetActorLocation().Equals(G.Position+FVector(800,0,H*50),.1)&&X->GetClass()->GetName()==TEXT("Build_SmelterMk1_C"))G.Load=X;
                    if(G.Floor.IsValid())Actors.Add(G.Floor);if(G.Neighbor.IsValid())Actors.Add(G.Neighbor);if(G.Load.IsValid())Actors.Add(G.Load);
                    Check(G.Floor.IsValid()&&G.Floor->GetClass()->GetName()==TEXT("Build_PoweredFoundation_")+PFFinishSuffix(H,To)+TEXT("_C"),G.Label+TEXT(" customized class survives fresh-process reload"));
                    Check(Lights(G.Floor.Get(),false),G.Label+TEXT(" corner lights recreate red after reload without supply"));
                }
                else
                {
                    const FString S=PFFinishSuffix(H,From);
                    auto* Recipe=PFLoadClass(TEXT("/TrapperPoweredFoundations/Recipes/Recipe_PoweredFoundation_")+S);
                    auto* Holo=Cast<AFGBuildableHologram>(AFGHologram::SpawnHologramFromRecipe(Recipe,Player->GetBuildGun(),G.Position,Player.Get()));
                    if(Holo){Holo->SetActorTickEnabled(false);Holo->SetSuppressBuildEffect(true);Holo->SetActorLocation(G.Position);TArray<AActor*> Children;G.Floor=Cast<AFGBuildable>(Holo->Construct(Children,FNetConstructionID()));Holo->Destroy();}
                    Check(G.Floor.IsValid(),G.Label+TEXT(" native foundation construction"));
                    if(G.Floor.IsValid())Actors.Add(G.Floor);
                    G.Neighbor=TileAt(G.Position-Origin+FVector(800,0,H*50),H);
                    G.Load=Spawn(TEXT("/Game/FactoryGame/Buildable/Factory/SmelterMk1/Build_SmelterMk1"),G.Position-Origin+FVector(800,0,H*50),true);
                    Check(Lights(G.Floor.Get(),false),G.Label+TEXT(" independent red corner indicators before power"));
                }
                Supply(G.Load,G.Position-Origin+FVector(-1600,-1600,H*50),&G.Feed,&G.Cable);
                Cases.Add(G);
            }
        }
        void VerifyCase(FPFCustomizeCase& G,bool Powered)
        {
            G.Floor=FindAt(G.Position);if(G.Floor.IsValid())Actors.AddUnique(G.Floor);
            const bool Correct=G.Floor.IsValid()&&G.Floor->GetClass()->GetName()==TEXT("Build_PoweredFoundation_")+PFFinishSuffix(G.Height,G.To)+TEXT("_C");
            Check(Correct,G.Label+TEXT(" native Customizer result is the intended powered finish"));
            Check(Connection(G.Floor)&&Connection(G.Floor)->HasPower()==Powered&&Same(G.Floor,G.Neighbor)&&Same(G.Floor,G.Load),G.Label+TEXT(" electrical circuit preserved across finish replacement"));
            Check(Lights(G.Floor.Get(),Powered),G.Label+(Powered?TEXT(" green corners survive customization"):TEXT(" red corners survive customization")));
            if(G.Floor.IsValid())
            {
                auto* Mesh=G.Floor->FindComponentByClass<UPFFoundationMesh>();
                auto* Parent=G.Floor->GetClass()->GetSuperClass()->GetDefaultObject();
                auto* Prop=FindFProperty<FObjectPropertyBase>(Parent->GetClass(),TEXT("mInstanceDataCDO"));
                auto* Data=Cast<UAbstractInstanceDataObject>(Prop->GetObjectPropertyValue_InContainer(Parent));
                bool Appearance=Mesh&&Data;
                if(Appearance)
                {
                    const auto D=Data->GetInstanceData()[0];Appearance=Mesh->GetStaticMesh()==D.StaticMesh&&Mesh->GetRelativeTransform().Equals(D.RelativeTransform,.01);
                    for(int32 M=0;M<D.OverridenMaterials.Num();++M)Appearance &= Mesh->GetMaterial(M)==D.OverridenMaterials[M];
                }
                Check(Appearance,G.Label+TEXT(" rendered mesh, offset and finish materials match vanilla"));
                Check(CastChecked<AFGBuildableFoundation>(G.Floor.Get())->mHeight==G.Height*100&&PFGeometry::ShareSide(G.Floor->FindComponentByClass<UPFPowerConnection>()->GetTile(),G.Neighbor->FindComponentByClass<UPFPowerConnection>()->GetTile()),G.Label+TEXT(" height and exact side contact preserved"));
            }
        }
        void TickCustom()
        {
            if(!World.IsValid())return;
            if(Painting)
            {
                if(AwaitingAim)
                {
                    Check(Paint->IsMaterialHitValid(),Cases[PaintIndex].Label+TEXT(" real aimed Customizer target is valid"));
                    Paint->PrimaryFire_Implementation();Paint->PrimaryFireRelease_Implementation();
                    ++PaintIndex;AwaitingAim=false;return;
                }
                if(PaintIndex<Cases.Num()){AimCase();return;}
                Painting=false;RestorePlayer();Repeat(10.f);return;
            }
            if(Step++==0){SetupCustom();return;}
            if(Step==2)
            {
                for(auto& G:Cases)
                {
                    Check(Connection(G.Floor)&&Connection(G.Floor)->HasPower()&&Lights(G.Floor.Get(),true),G.Label+TEXT(" initial floor powers with green corners"));
                }
                if(!Reload)
                {
                    PreviousTransform=Player->GetActorTransform();PreviousRotation=World->GetFirstPlayerController()->GetControlRotation();PreviousMovement=Player->GetCharacterMovement()->MovementMode;
                    MovingPlayer=true;Player->GetCharacterMovement()->DisableMovement();Player->EquipEquipment(Player->GetBuildGun());
                    Painting=true;AimCase();Repeat(.75f);
                }
                return;
            }
            if(Step==3)
            {
                for(auto& G:Cases){VerifyCase(G,true);if(auto* W=Cast<AFGBuildableWire>(G.Cable.Get()))W->Disconnect();}
                return;
            }
            for(auto& G:Cases){VerifyCase(G,false);if(G.Cable.IsValid())G.Cable->TurnOffAndDestroy();if(G.Feed.IsValid())G.Feed->TurnOffAndDestroy();}
            RestorePlayer();RestoreInventory();
            if(!Reload&&Failed==0)
            {
                const FString Name=TEXT("PF_Customization_")+FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
                FFileHelper::SaveStringToFile(Name,*Manifest());
                Report+=TEXT("Evidence save: ")+Name+TEXT("\n");
                const auto Self=StaticCastSharedRef<FPFCustomizationTest>(AsShared());
                World->GetTimerManager().ClearTimer(Timer);
                UFGSaveSession::Get(World.Get())->SaveGame(Name,FOnSaveGameComplete::CreateLambda([Self](bool OK,const FText& Error,void*){Self->Check(OK,TEXT("Customized variants saved: ")+Error.ToString());Self->FinishCustom();}),nullptr);
            }
            else FinishCustom();
        }
        void FinishCustom()
        {
            const auto KeepAlive=AsShared();RestorePlayer();RestoreInventory();
            for(int32 I=Actors.Num()-1;I>=0;--I)if(Actors[I].IsValid())Actors[I]->TurnOffAndDestroy();
            if(Player.IsValid())
            {
                TArray<FInventoryStack> After;Player->GetInventory()->GetInventoryStacks(After,true);bool Equal=After.Num()==InventoryBefore.Num();
                for(int32 I=0;I<After.Num()&&I<InventoryBefore.Num();++I)Equal &= After[I].HasItems()==InventoryBefore[I].HasItems()&&(!After[I].HasItems()||(After[I].NumItems==InventoryBefore[I].NumItems&&After[I].Item.GetItemClass()==InventoryBefore[I].Item.GetItemClass()));
                Check(Equal,TEXT("Original inventory restored exactly in every slot"));
            }
            Report+=FString::Printf(TEXT("Failures: %d\n"),Failed);
            FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/(Reload?TEXT("TrapperPoweredFoundations-CustomizationReloadTest.txt"):TEXT("TrapperPoweredFoundations-CustomizationTest.txt"))));
            UE_LOG(LogPFTest,Display,TEXT("Customization regression complete reload=%d failures=%d"),Reload,Failed);
            World->GetTimerManager().ClearTimer(Timer);
        }
    };
}
void PFStartCustomizationTest(UWorld* World,bool Reload)
{
    auto T=MakeShared<FPFCustomizationTest>();T->World=World;T->Reload=Reload;
    World->GetTimerManager().SetTimer(T->Timer,FTimerDelegate::CreateLambda([T]{T->TickCustom();}),10.f,true,15.f);
}
