#include "PFPowerConnection.h"
#include "FGBuildableSubsystem.h"
#include "Buildables/FGBuildable.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "FGSaveSession.h"
#include "FGSchematicManager.h"
#include "Json.h"
#include "TimerManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "GameFramework/PlayerController.h"

void TPFStartPrivateMigrationTest(UWorld* World, bool Reload)
{
    FTimerHandle Timer;
    World->GetTimerManager().SetTimer(Timer,FTimerDelegate::CreateLambda([World,Reload]
    {
        UFGSaveSession::Get(World)->SetAutoSaveEnabled(false);
        FString Report,Text;int32 Failures=0;
        auto Check=[&](bool OK,const FString& Label){Report+=(OK?TEXT("PASS: "):TEXT("FAIL: "))+Label+TEXT("\n");if(!OK)++Failures;};
        const FString Dir=FPaths::ProjectSavedDir();
        auto* Shop=LoadObject<UClass>(nullptr,TEXT("/TrapperPoweredFoundations/Schematics/Schematic_PoweredFoundations.Schematic_PoweredFoundations_C"));
        Check(Shop&&AFGSchematicManager::Get(World)->IsSchematicPurchased(Shop,World->GetFirstPlayerController()),TEXT("Previously purchased shop unlock survives migration"));
        TSharedPtr<FJsonObject> Expected;
        const bool Read=FFileHelper::LoadFileToString(Text,*(Dir/TEXT("TrapperPrivateMigrationExpected.json")))&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Expected);
        Check(Read,TEXT("Reads original save actor manifest"));
        TMap<FString,AFGBuildable*> Tiles;
        for(auto* B:AFGBuildableSubsystem::Get(World)->GetAllBuildablesRef())if(B&&B->FindComponentByClass<UPFPowerConnection>())Tiles.Add(B->GetName(),B);
        if(Read)
        {
            const auto& Entries=Expected->GetArrayField(TEXT("tiles"));
            Check(Tiles.Num()==Entries.Num(),FString::Printf(TEXT("All original foundations preserved: actual=%d expected=%d"),Tiles.Num(),Entries.Num()));
            for(const auto& Value:Entries)
            {
                const auto E=Value->AsObject();const FString Name=E->GetStringField(TEXT("name"));auto* B=Tiles.FindRef(Name);
                Check(B&&B->GetClass()->GetPathName()==E->GetStringField(TEXT("cls")),Name+TEXT(" retains identity and intended finish under new mod ID"));
                if(!B)continue;
                const FVector P(E->GetNumberField(TEXT("x")),E->GetNumberField(TEXT("y")),E->GetNumberField(TEXT("z")));
                const FQuat Q(E->GetNumberField(TEXT("qx")),E->GetNumberField(TEXT("qy")),E->GetNumberField(TEXT("qz")),E->GetNumberField(TEXT("qw")));
                const FVector S(E->GetNumberField(TEXT("sx")),E->GetNumberField(TEXT("sy")),E->GetNumberField(TEXT("sz")));
                Check(B->GetActorLocation().Equals(P,.1)&&B->GetActorQuat().Equals(Q,.001)&&B->GetActorScale3D().Equals(S,.001),Name+TEXT(" retains exact original transform"));
            }
        }
        FString Inventory;auto* Player=Cast<AFGCharacterPlayer>(World->GetFirstPlayerController()->GetPawn());
        Check(Player!=nullptr,TEXT("Original player is available"));
        if(Player){TArray<FInventoryStack> Stacks;Player->GetInventory()->GetInventoryStacks(Stacks,true);for(int32 I=0;I<Stacks.Num();++I){const auto& S=Stacks[I];Inventory+=FString::Printf(TEXT("%d|%d|%s\n"),I,S.NumItems,S.HasItems()?*S.Item.GetItemClass()->GetPathName():TEXT("empty"));}}
        if(Reload){FString Before;Check(FFileHelper::LoadFileToString(Before,*(Dir/TEXT("TrapperPrivateMigrationInventory.txt")))&&Before==Inventory,TEXT("Every inventory slot and quantity survives new-ID save/reload"));}
        else FFileHelper::SaveStringToFile(Inventory,*(Dir/TEXT("TrapperPrivateMigrationInventory.txt")));
        if(!Reload&&Failures==0)
        {
            const FString Name=TEXT("Trapper_Public_")+FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
            FFileHelper::SaveStringToFile(Name,*(Dir/TEXT("TrapperPrivateMigrationSave.txt")));
            UFGSaveSession::Get(World)->SaveGame(Name,FOnSaveGameComplete::CreateLambda([Report,Dir](bool OK,const FText& Error,void*)mutable{Report+=(OK?TEXT("PASS: "):TEXT("FAIL: "))+FString(TEXT("Writes migrated save: "))+Error.ToString()+FString::Printf(TEXT("\nFailures: %d\n"),OK?0:1);FFileHelper::SaveStringToFile(Report,*(Dir/TEXT("TrapperPrivateMigrationTest.txt")));}),nullptr);
        }
        else {Report+=FString::Printf(TEXT("Failures: %d\n"),Failures);FFileHelper::SaveStringToFile(Report,*(Dir/(Reload?TEXT("TrapperPrivateMigrationReloadTest.txt"):TEXT("TrapperPrivateMigrationTest.txt"))));}
    }),15.f,false);
}
