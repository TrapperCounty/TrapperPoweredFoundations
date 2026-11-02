#include "CoreMinimal.h"
#include "FGGameInstance.h"
#include "Server/FGDSGameplayTypes.h"
#include "JsonObjectConverter.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace
{
    void JoinLocalQAServer(UWorld* World)
    {
        if(!FParse::Param(FCommandLine::Get(),TEXT("PFMultiplayerClientTest")))return;
        const FString StatusPath=TEXT("S:/SatisfactoryModding/QA/NetworkHandshake/join-status.txt");
        auto Status=[&](const TCHAR* Text){FFileHelper::SaveStringToFile(Text,*StatusPath);};
        auto* Game=World?Cast<UFGGameInstance>(World->GetGameInstance()):nullptr;
        auto* PC=World?World->GetFirstPlayerController():nullptr;
        if(!Game||!PC){Status(TEXT("Missing local game instance or player controller"));return;}
        FString JSON;FFGServerJoinDataResponse JoinData;
        if(!FFileHelper::LoadFileToString(JSON,TEXT("S:/SatisfactoryModding/QA/ServerPrivate/client-join.json"))||
           !FJsonObjectConverter::JsonObjectStringToUStruct(JSON,&JoinData,0,0))
        {Status(TEXT("Unable to read authenticated local join data"));return;}
        if(JoinData.EncryptionToken.IsEmpty()||JoinData.EncryptionKey.IsEmpty()||JoinData.ServerEntryToken.PrivilegeLevel!=EPrivilegeLevel::Client)
        {Status(TEXT("Invalid or incorrectly privileged local join data"));return;}
        FFGPendingConnectionEncryptionData Pending;
        Pending.EncryptionToken=JoinData.EncryptionToken;
        Pending.EncryptionData.Identifier=JoinData.EncryptionToken;
        Pending.EncryptionData.Key.SetNum(JoinData.EncryptionKey.Len()/2);
        if(HexToBytes(JoinData.EncryptionKey,Pending.EncryptionData.Key.GetData())!=Pending.EncryptionData.Key.Num())
        {Status(TEXT("Invalid encryption key encoding"));return;}
        Game->SetPendingConnectionEncryptionData(Pending);
        // Tokens originate from the server's authenticated HTTPS API; native
        // game entry and transport encryption remain enabled and unchanged.
        const FString URL=TEXT("127.0.0.1:17777?EncryptionToken=")+JoinData.EncryptionToken+TEXT("?EntryTicket=")+JoinData.ServerEntryToken.ToString();
        Status(TEXT("Joining loopback QA server with authenticated encrypted transport"));
        PC->ClientTravel(URL,TRAVEL_Absolute);
    }
    FAutoConsoleCommandWithWorld LocalQAJoinCommand(
        TEXT("PFQA.JoinLocalServer"),TEXT("Opt-in local QA server join using authenticated API credentials."),
        FConsoleCommandWithWorldDelegate::CreateStatic(&JoinLocalQAServer));
}
