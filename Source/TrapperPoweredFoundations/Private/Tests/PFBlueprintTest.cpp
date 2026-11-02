// Opt-in Designer save and fresh-process blueprint placement regression test.
#include "PFNetworkSubsystem.h"
#include "PFPowerConnection.h"
#include "FGCharacterPlayer.h"
#include "FGPlayerController.h"
#include "FGInventoryComponent.h"
#include "FGBlueprintSubsystem.h"
#include "FGBlueprintProxy.h"
#include "FGBuildableSubsystem.h"
#include "FGPowerInfoComponent.h"
#include "FGSaveSession.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Buildables/FGBuildableBlueprintDesigner.h"
#include "Buildables/FGBuildableFoundation.h"
#include "Buildables/FGBuildableWire.h"
#include "Equipment/FGBuildGun.h"
#include "Hologram/FGBlueprintHologram.h"
#include "Components/BoxComponent.h"
#include "TimerManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogPFBlueprintTest,Log,All);
bool PFTestingCustomizationBlueprint()
{
    return FParse::Param(FCommandLine::Get(),TEXT("PFCustomizationBlueprintTest")) || FParse::Param(FCommandLine::Get(),TEXT("PFCustomizationBlueprintPlaceTest"));
}
namespace
{
    struct FPFBPGroup
    {
        TArray<TWeakObjectPtr<AFGBuildable>> Tiles;
        TWeakObjectPtr<AFGBuildable> Pole, Machine, Source, Wire;
        FString Label;
    };
    struct FPFBPTest : TSharedFromThis<FPFBPTest>
    {
        TWeakObjectPtr<UWorld> World;
        TWeakObjectPtr<AFGCharacterPlayer> Player;
        TWeakObjectPtr<AFGBuildableBlueprintDesigner> Designer;
        TArray<TWeakObjectPtr<AFGBuildable>> Owned;
        TArray<TWeakObjectPtr<AActor>> Proxies;
        TArray<FPFBPGroup> Groups;
        TArray<FInventoryStack> InventoryBefore;
        TMap<UClass*,int32> ExpectedCost;
        FString Name,Report;
        int32 Failures=0;
        bool Place=false;
        FString ManifestPath() const { return FPaths::ProjectSavedDir()/(PFTestingCustomizationBlueprint()?TEXT("TrapperPoweredFoundations-CustomizationBlueprintName.txt"):TEXT("TrapperPoweredFoundations-BlueprintTestName.txt")); }
        void Check(bool OK,const FString& Label)
        {
            Report+=FString::Printf(TEXT("%s: %s\n"),OK?TEXT("PASS"):TEXT("FAIL"),*Label);
            UE_LOG(LogPFBlueprintTest,Display,TEXT("%s: %s"),OK?TEXT("PASS"):TEXT("FAIL"),*Label);
            if(!OK) ++Failures;
        }
        void Later(void(FPFBPTest::*Method)())
        {
            FTimerHandle Timer; auto Self=AsShared();
            World->GetTimerManager().SetTimer(Timer,FTimerDelegate::CreateLambda([Self,Method]{(Self.Get().*Method)();}),3.f,false);
        }
        AFGBuildableHologram* Hologram(const FString& Recipe,const FTransform& TM)
        {
            const FString Short=FPaths::GetCleanFilename(Recipe);
            auto* R=LoadObject<UClass>(nullptr,*(Recipe+TEXT(".")+Short+TEXT("_C")));
            auto* H=R?Cast<AFGBuildableHologram>(AFGHologram::SpawnHologramFromRecipe(R,Player->GetBuildGun(),TM.GetLocation(),Player.Get())):nullptr;
            if(H){H->SetActorTickEnabled(false);H->SetSuppressBuildEffect(true);H->SetActorTransform(TM);}
            return H;
        }
        AFGBuildable* Build(const FString& Recipe,FTransform TM,bool Inside=false,bool AlignBottom=false)
        {
            auto* H=Hologram(Recipe,TM);
            if(!H){Check(false,TEXT("Creates hologram: ")+Recipe);return nullptr;}
            if(AlignBottom)
            {
                auto* CDO=Cast<AFGBuildable>(H->GetBuildClass()->GetDefaultObject());
                if(CDO){const auto B=CDO->GetCombinedClearanceBox();if(B.IsValid)TM.AddToTranslation(FVector(0,0,-B.Min.Z));}
                H->SetActorTransform(TM);
            }
            if(Inside)
            {
                Check(Designer->IsLocationInsideDesigner(TM.GetLocation()),TEXT("Fixture position lies inside Designer: ")+Recipe);
                H->SetInsideBlueprintDesigner(Designer.Get());
                for(const auto& I:H->GetCost(false))ExpectedCost.FindOrAdd(I.ItemClass)+=I.Amount;
            }
            TArray<AActor*> Children;
            auto* B=Cast<AFGBuildable>(H->Construct(Children,FNetConstructionID()));
            if(B)Owned.AddUnique(B);
            for(auto* A:Children)if(auto* C=Cast<AFGBuildable>(A))Owned.AddUnique(C);
            H->Destroy();return B;
        }
        UFGPowerConnectionComponent* Power(TWeakObjectPtr<AFGBuildable> B)const
        {return B.IsValid()?B->FindComponentByClass<UFGPowerConnectionComponent>():nullptr;}
        void Run()
        {
            if(PFTestingCustomizationBlueprint()) UFGSaveSession::Get(World.Get())->SetAutoSaveEnabled(false);
            auto* PC=World->GetFirstPlayerController();Player=PC?Cast<AFGCharacterPlayer>(PC->GetPawn()):nullptr;
            if(!Player.IsValid()||!Player->GetBuildGun()){Check(false,TEXT("Player/build gun available"));Finish();return;}
            Player->GetInventory()->GetInventoryStacks(InventoryBefore,true);
            if(Place){PlaceCopies();return;}
            Designer=Cast<AFGBuildableBlueprintDesigner>(Build(TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_BlueprintDesigner"),FTransform(FVector(200000,-200000,50000))));
            Check(Designer.IsValid(),TEXT("Constructs a real Mk.1 Blueprint Designer and its storage"));
            if(!Designer.IsValid()){Finish();return;}
            auto* Box=Designer->GetCollisionComponent();
            FVector Center=Box->GetComponentLocation();Center.Z-=Box->GetScaledBoxExtent().Z;Center.Z+=500.;
            UE_LOG(LogPFBlueprintTest,Display,TEXT("Designer center %s extent %s fixture top %s"),*Box->GetComponentLocation().ToString(),*Box->GetScaledBoxExtent().ToString(),*Center.ToString());
            FPFBPGroup Group;Group.Label=TEXT("Designer");
            const int32 Heights[3]={1,2,4};
            for(int32 I=0;I<3;++I)
            {
                const TCHAR* Finishes[]={TEXT("_Asphalt"),TEXT("_PolishedConcrete"),TEXT("_GripMetal")};
                const FString Recipe=FString::Printf(TEXT("/TrapperPoweredFoundations/Recipes/Recipe_PoweredFoundation_%dm%s"),Heights[I],PFTestingCustomizationBlueprint()?Finishes[I]:TEXT(""));
                auto* B=Build(Recipe,FTransform(Center+FVector((I-1)*800,0,-Heights[I]*50.)),true);
                Check(B&&B->GetBlueprintDesigner()==Designer.Get()&&Designer->GetBuildablesInBlueprintDesigner().Contains(B),FString::Printf(TEXT("%dm foundation registers in the real Designer"),Heights[I]));
                if(B)Group.Tiles.Add(B);
            }
            Group.Pole=Build(TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleMk1"),FTransform(Center+FVector(-800,0,0)),true,true);
            Group.Machine=Build(TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_SmelterMk1"),FTransform(Center+FVector(800,0,0)),true,true);
            Check(Designer->GetBuildablesInBlueprintDesigner().Num()==5,TEXT("Designer contains three powered floors, a pole and a foundry"));
            Groups.Add(Group);APFNetworkSubsystem::Get(World.Get())->MarkDirty();Later(&FPFBPTest::SaveDesigner);
        }
        void SaveDesigner()
        {
            if(!Designer.IsValid()){Check(false,TEXT("Designer remains available"));Finish();return;}
            const auto& G=Groups[0];
            bool Linked=G.Tiles.Num()==3;
            for(int32 I=0;I+1<G.Tiles.Num();++I)Linked &= Power(G.Tiles[I])&&Power(G.Tiles[I+1])&&Power(G.Tiles[I])->HasHiddenConnection(Power(G.Tiles[I+1]));
            Check(Linked,TEXT("Hidden side connections form inside the Designer"));
            Check(G.Tiles.Num()==3&&Power(G.Pole)&&Power(G.Pole)->HasHiddenConnection(Power(G.Tiles[0])),TEXT("Designer pole attaches to its supporting powered foundation"));
            bool MachineAttached=false;
            for(auto T:G.Tiles)
            {
                const bool LinkedToTile=Power(G.Machine)&&Power(G.Machine)->HasHiddenConnection(Power(T));
                MachineAttached |= LinkedToTile;
                UE_LOG(LogPFBlueprintTest,Display,TEXT("Designer machine info=%s tile=%s linked=%d machineLoc=%s clearance=%s"),*GetNameSafe(Power(G.Machine)?Power(G.Machine)->GetPowerInfo():nullptr),*GetNameSafe(T.Get()),LinkedToTile,*G.Machine->GetActorLocation().ToString(),*G.Machine->GetCombinedClearanceBox().ToString());
            }
            Check(MachineAttached,TEXT("Designer machine attaches through its footprint"));
            TArray<FItemAmount> Cost;Designer->CalculateBlueprintCost(Cost);
            TMap<UClass*,int32> Actual;for(const auto& I:Cost)Actual.FindOrAdd(I.ItemClass)+=I.Amount;
            bool Correct=Actual.Num()==ExpectedCost.Num();for(const auto& I:ExpectedCost)Correct &= Actual.FindRef(I.Key)==I.Value;
            Check(Correct,TEXT("Designer cost includes complete recipes and extra foundation wire"));
            Name=TEXT("Powered Foundations Test ")+FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
            FBlueprintRecord Record;Record.BlueprintName=Name;Record.BlueprintDescription=TEXT("Test: 1m/2m/4m powered foundations, power pole and foundry. Feed the pole to energize the floor.");Record.Color=FLinearColor(0,.6,.8);
            auto* BS=AFGBlueprintSubsystem::Get(World.Get());BS->AssignCategoryAndSubcategoryToBlueprintRecord(Record);
            Designer->SaveBlueprint(Record,Cast<AFGPlayerController>(Player->GetController()));
            Later(&FPFBPTest::VerifySaved);
        }
        void VerifySaved()
        {
            auto* BS=AFGBlueprintSubsystem::Get(World.Get());
            const FString Base=BS->GetSessionBlueprintPath()/Name;
            Check(IFileManager::Get().FileSize(*(Base+TEXT(".sbp")))>0,TEXT("Designer writes a nonempty blueprint file to disk"));
            Check(IFileManager::Get().FileSize(*(Base+TEXT(".sbpcfg")))>0,TEXT("Designer writes the blueprint metadata file"));
            Check(BS->ReadBlueprintFromDisc(Name),TEXT("Native subsystem can read the saved blueprint archive"));
            BS->RefreshBlueprintsAndDescriptors();
            auto* Desc=BS->GetBlueprintDescriptorByNameString(Name);
            Check(Desc&&Desc->GetRecipeRequirementsAreMet(),TEXT("Saved blueprint descriptor is available with recipes unlocked"));
            Check(FFileHelper::SaveStringToFile(Name,*ManifestPath()),TEXT("Records unique blueprint name for the fresh-process placement test"));
            Report+=TEXT("Blueprint: ")+Name+TEXT("\nFiles: ")+Base+TEXT(".sbp / .sbpcfg\n");Finish();
        }
        void PlaceCopies()
        {
            Check(FFileHelper::LoadFileToString(Name,*ManifestPath())&&!Name.IsEmpty(),TEXT("Reads the blueprint saved by the Designer test"));
            auto* BS=AFGBlueprintSubsystem::Get(World.Get());
            auto* Desc=BS->GetBlueprintDescriptorByNameString(Name);
            Check(Desc&&Desc->GetRecipeRequirementsAreMet(),TEXT("Fresh game discovers the saved blueprint and resolves its recipes"));
            if(!Desc){Finish();return;}
            Check(BS->ReadBlueprintFromDisc(Name),TEXT("Fresh process reads blueprint archive from disk"));
            auto* MachineTemplate=Hologram(TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_SmelterMk1"),FTransform(FVector(240000,-200000,50000)));
            UClass* ExpectedMachineClass=MachineTemplate?MachineTemplate->GetBuildClass().Get():nullptr;
            if(MachineTemplate)MachineTemplate->Destroy();
            Check(ExpectedMachineClass!=nullptr,TEXT("Resolves the saved machine's class from its actual recipe"));
            int32 Copy=0;
            for(float Yaw:{0.f,37.f})
            {
                BS->SetActiveBlueprintHologramDescriptor(Desc);
                const FTransform TM(FRotator(0,Yaw,0),FVector(240000,-200000+Copy++*20000,50000));
                auto* H=Cast<AFGBlueprintHologram>(Hologram(TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_Blueprint"),TM));
                Check(H!=nullptr,FString::Printf(TEXT("Yaw %.0f creates the native blueprint placement hologram"),Yaw));
                if(!H)continue;
                H->SetBlueprintDescriptor(Desc);H->LoadBlueprintToOtherWorld();H->SetActorTransform(TM);
                int32 PreviewFloors=0;for(auto* B:BS->GetBlueprintWorldBuildables())if(B->FindComponentByClass<UPFPowerConnection>())++PreviewFloors;
                UE_LOG(LogPFBlueprintTest,Display,TEXT("Preview world buildings=%d visual roots=%d floors=%d"),BS->GetBlueprintWorldBuildables().Num(),H->mBuildableToNewRoot.Num(),PreviewFloors);
                for(const auto& Entry:H->mBuildableToNewRoot)UE_LOG(LogPFBlueprintTest,Display,TEXT("Preview root class=%s valid=%d"),*GetNameSafe(Entry.Key?Entry.Key->GetClass():nullptr),IsValid(Entry.Key));
                Check(PreviewFloors==3,TEXT("Blueprint preview contains all three powered foundation components"));
                TArray<FItemAmount> StoredCost;Desc->GetBlueprintCost(StoredCost);const auto PreviewCost=H->GetCost(false);
                TMap<UClass*,int32> A,B;for(const auto& I:StoredCost)A.FindOrAdd(I.ItemClass)+=I.Amount;for(const auto& I:PreviewCost)B.FindOrAdd(I.ItemClass)+=I.Amount;
                bool CostOK=A.Num()==B.Num()&&!A.IsEmpty();for(const auto& I:A)CostOK &= B.FindRef(I.Key)==I.Value;
                Check(CostOK,TEXT("Blueprint placement cost matches the saved Designer cost"));
                TSet<AFGBuildable*> Before;auto* Builds=AFGBuildableSubsystem::Get(World.Get());for(auto* X:Builds->GetAllBuildablesRef())Before.Add(X);
                TArray<AActor*> Children;auto* Root=H->Construct(Children,FNetConstructionID());if(Root)Proxies.Add(Root);
                FPFBPGroup G;G.Label=FString::Printf(TEXT("Placed copy yaw %.0f"),Yaw);
                int32 Count=0;bool Context=true;
                for(auto* X:Builds->GetAllBuildablesRef())if(!Before.Contains(X))
                {
                    ++Count;Owned.AddUnique(X);Context &= !X->IsBuildableInsideBlueprintDesigner() && X->GetBlueprintDesigner()==nullptr && X->GetBlueprintProxy()!=nullptr;
                    UE_LOG(LogPFBlueprintTest,Display,TEXT("Placed building %s class=%s power=%s info=%s"),*X->GetName(),*X->GetClass()->GetName(),*GetNameSafe(Power(X)),*GetNameSafe(Power(X)?Power(X)->GetPowerInfo():nullptr));
                    if(X->FindComponentByClass<UPFPowerConnection>())G.Tiles.Add(X);
                    else if(X->GetClass()->GetName()==TEXT("Build_PowerPoleMk1_C"))G.Pole=X;
                    else if(X->GetClass()==ExpectedMachineClass)G.Machine=X;
                }
                G.Tiles.Sort([](const auto& X,const auto& Y){return CastChecked<AFGBuildableFoundation>(X.Get())->mHeight<CastChecked<AFGBuildableFoundation>(Y.Get())->mHeight;});
                UE_LOG(LogPFBlueprintTest,Display,TEXT("Placed count=%d floors=%d pole=%s machine=%s"),Count,G.Tiles.Num(),*GetNameSafe(G.Pole.Get()),*GetNameSafe(G.Machine.Get()));
                Check(Count==5&&G.Tiles.Num()==3&&G.Pole.IsValid()&&G.Machine.IsValid(),G.Label+TEXT(" constructs all five saved buildings"));
                Check(Context,G.Label+TEXT(" has a blueprint proxy and no stale Designer ownership"));
                bool Stable=G.Tiles.Num()==3;
                for(auto X:G.Tiles){auto* P=X->FindComponentByClass<UPFPowerConnection>();Stable &= P&&P->GetFName()==TEXT("PoweredFloorConnection")&&X->GetActorQuat().Equals(TM.GetRotation(),.001);}
                Check(Stable,G.Label+TEXT(" preserves stable power components and placement rotation"));
                if(G.Tiles.Num()==3)Check(PFGeometry::ShareSide(G.Tiles[0]->FindComponentByClass<UPFPowerConnection>()->GetTile(),G.Tiles[1]->FindComponentByClass<UPFPowerConnection>()->GetTile())&&PFGeometry::ShareSide(G.Tiles[1]->FindComponentByClass<UPFPowerConnection>()->GetTile(),G.Tiles[2]->FindComponentByClass<UPFPowerConnection>()->GetTile()),G.Label+TEXT(" preserves exact mixed-height side contact"));
                Groups.Add(G);H->Destroy();
            }
            BS->SetActiveBlueprintHologramDescriptor(nullptr);APFNetworkSubsystem::Get(World.Get())->MarkDirty();Later(&FPFBPTest::SupplyCopies);
        }
        void SupplyCopies()
        {
            for(auto& G:Groups)
            {
                if(!G.Pole.IsValid())continue;
                Check(G.Tiles.Num()==3&&Power(G.Tiles[0])&&!Power(G.Tiles[0])->HasPower(),G.Label+TEXT(" starts unpowered without an external supply"));
                const FVector P=G.Pole->GetActorLocation()+FVector(-5000,-5000,0);
                G.Source=Build(TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleMk1"),FTransform(P));
                auto* Source=Power(G.Source);
                if(!Source){Check(false,G.Label+TEXT(" creates test supply terminal"));continue;}
                auto* Info=NewObject<UFGPowerInfoComponent>(G.Source.Get());Info->RegisterComponent();Source->SetPowerInfo(Info);Info->SetBaseProduction(100);
                auto* WireClass=LoadObject<UClass>(nullptr,TEXT("/Game/FactoryGame/Buildable/Factory/PowerLine/Build_PowerLine.Build_PowerLine_C"));
                auto* Builds=AFGBuildableSubsystem::Get(World.Get());
                auto* Cable=WireClass?Builds->BeginSpawnBuildable(WireClass,FTransform(P)):nullptr;
                if(Cable){Cable->FinishSpawning(FTransform(P));if(!Builds->GetAllBuildablesRef().Contains(Cable))Builds->AddBuildable(Cable);Owned.Add(Cable);}
                G.Wire=Cable;
                auto* Wire=Cast<AFGBuildableWire>(G.Wire.Get());
                Check(Wire&&Wire->Connect(Source,Power(G.Pole)),G.Label+TEXT(" accepts a real cable into the blueprint pole"));
            }
            APFNetworkSubsystem::Get(World.Get())->MarkDirty();Later(&FPFBPTest::VerifyPower);
        }
        void VerifyPower()
        {
            for(auto& G:Groups)
            {
                auto* P=Power(G.Pole);bool Powered=P&&P->HasPower()&&G.Tiles.Num()==3;
                for(auto T:G.Tiles){auto* C=Power(T);Powered &= C&&P&&C->HasPower()&&C->GetCircuitID()==P->GetCircuitID();}
                Check(Powered,G.Label+TEXT(" supplies all three powered foundations on one native circuit"));
                auto* M=Power(G.Machine);Check(M&&P&&M->HasPower()&&M->GetCircuitID()==P->GetCircuitID(),G.Label+TEXT(" supplies the machine through the floor without a machine cable"));
                bool Glow=G.Tiles.Num()==3;for(auto T:G.Tiles)Glow &= T->FindComponentByClass<UPFPowerConnection>()->IsGridPowered();
                Check(Glow,G.Label+TEXT(" updates energized rim state on all floors"));
                if(PFTestingCustomizationBlueprint())
                {
                    const TCHAR* Finishes[]={TEXT("_Asphalt_C"),TEXT("_PolishedConcrete_C"),TEXT("_GripMetal_C")};
                    for(int32 I=0;I<G.Tiles.Num();++I)
                    {
                        Check(I<3&&G.Tiles[I]->GetClass()->GetName().EndsWith(Finishes[I]),G.Label+TEXT(" retains saved customized finish"));
                        bool Green=false;TInlineComponentArray<UStaticMeshComponent*> Meshes(G.Tiles[I].Get());
                        for(auto* Mesh:Meshes)if(Mesh->GetFName()==TEXT("PF_CornerLights"))if(auto* MID=Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)))Green=MID->K2_GetVectorParameterValue(TEXT("PowerColor")).Equals(FLinearColor(.015,1.6,.03),.001);
                        Check(Green,G.Label+TEXT(" retains green corner indicators after blueprint placement"));
                    }
                }
            }
            if(Groups.Num()==2)Check(Power(Groups[0].Pole)&&Power(Groups[1].Pole)&&Power(Groups[0].Pole)->GetCircuitID()!=Power(Groups[1].Pole)->GetCircuitID(),TEXT("Separate placed blueprint copies do not retain cross-copy power links"));
            for(auto& G:Groups)if(G.Wire.IsValid())G.Wire->TurnOffAndDestroy();
            Later(&FPFBPTest::VerifyDisconnected);
        }
        void VerifyDisconnected()
        {
            for(const auto& G:Groups)
            {
                bool Off=G.Tiles.Num()==3;for(auto T:G.Tiles)Off &= Power(T)&&!Power(T)->HasPower()&&!T->FindComponentByClass<UPFPowerConnection>()->IsGridPowered();
                Off &= Power(G.Machine)&&!Power(G.Machine)->HasPower();Check(Off,G.Label+TEXT(" loses floor and machine power after removing its external cable"));
            }
            Finish();
        }
        void Finish()
        {
            for(int32 I=Owned.Num()-1;I>=0;--I)if(Owned[I].IsValid()&&!Owned[I]->IsActorBeingDestroyed())Owned[I]->TurnOffAndDestroy();
            for(auto P:Proxies)if(P.IsValid()&&!P->IsActorBeingDestroyed())P->Destroy();
            if(Player.IsValid()&&!InventoryBefore.IsEmpty())
            {
                auto* Inv=Player->GetInventory();bool Same=Inv->GetSizeLinear()==InventoryBefore.Num();
                for(int32 I=0;I<InventoryBefore.Num();++I){FInventoryStack S;Inv->GetStackFromIndex(I,S);const auto& B=InventoryBefore[I];Same &= S.HasItems()==B.HasItems()&&(!B.HasItems()||(S.Item.GetItemClass()==B.Item.GetItemClass()&&S.NumItems==B.NumItems));}
                Check(Same,TEXT("Player inventory slots and material totals remain unchanged"));
            }
            Report+=TEXT("Blueprint: ")+Name+FString::Printf(TEXT("\nFailures: %d\n"),Failures);
            FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/(PFTestingCustomizationBlueprint()?(Place?TEXT("TrapperPoweredFoundations-CustomizationBlueprintPlaceTest.txt"):TEXT("TrapperPoweredFoundations-CustomizationBlueprintTest.txt")):(Place?TEXT("TrapperPoweredFoundations-BlueprintPlaceTest.txt"):TEXT("TrapperPoweredFoundations-BlueprintDesignerTest.txt")))));
            UE_LOG(LogPFBlueprintTest,Display,TEXT("Blueprint %s test complete: %d failures"),Place?TEXT("placement"):TEXT("Designer save"),Failures);
        }
    };
}
void PFStartBlueprintTest(UWorld* World,bool Place)
{
    auto Test=MakeShared<FPFBPTest>();Test->World=World;Test->Place=Place;
    FTimerHandle Timer;World->GetTimerManager().SetTimer(Timer,FTimerDelegate::CreateLambda([Test]{Test->Run();}),15.f,false);
}

#include "PFLargeBlueprintTest.inl"


#include "PFSnappedBlueprintTest.inl"
#include "PFBlueprintDismantleTest.inl"
#include "PFBlueprintOverflowTest.inl"

#include "PFBlueprintPartialTest.inl"

#include "PFBlueprintMultiPartialTest.inl"

#include "PFBlueprintMultiOverflowTest.inl"
#include "PFDenseBlueprintTest.inl"
