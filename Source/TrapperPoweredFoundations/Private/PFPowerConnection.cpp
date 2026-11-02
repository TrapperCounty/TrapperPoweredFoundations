#include "PFPowerConnection.h"
#include "PFNetworkSubsystem.h"
#include "Buildables/FGBuildableFoundation.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "FGPowerCircuit.h"

void PFStartMultiplayerClientObserver(UWorld* World);

UPFPowerConnection::UPFPowerConnection()
{
    SetIsHidden(true);
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}

PFGeometry::FTile UPFPowerConnection::GetTile() const
{
    PFGeometry::FTile Tile;
    if (const AFGBuildableFoundation* Foundation = Cast<AFGBuildableFoundation>(GetOwner()))
    {
        Tile.Transform = Foundation->GetActorTransform();
        Tile.HalfSize = FVector(Foundation->mWidth, Foundation->mDepth, Foundation->mHeight) * .5;
    }
    return Tile;
}

void UPFPowerConnection::BeginPlay()
{
    Super::BeginPlay();
    SetIsHidden(true);
    if (GetNetMode() != NM_DedicatedServer && IndicatorMesh && IndicatorMaterial)
    {
        Indicator = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("PF_CornerLights"));
        Indicator->SetupAttachment(GetOwner()->GetRootComponent());
        Indicator->SetStaticMesh(IndicatorMesh);
        Indicator->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Indicator->SetGenerateOverlapEvents(false);
        Indicator->SetCastShadow(false);
        Indicator->SetCanEverAffectNavigation(false);
        Indicator->SetCullDistance(20000);
        const FVector Half = GetTile().HalfSize;
        Indicator->SetRelativeLocation(FVector(0, 0, Half.Z));
        Indicator->SetRelativeScale3D(FVector(Half.X / 400., Half.Y / 400., 1));
        IndicatorMID = UMaterialInstanceDynamic::Create(IndicatorMaterial, this);
        Indicator->SetMaterial(0, IndicatorMID);
        Indicator->RegisterComponent();
        OnRep_GridPowered();
    }
    if (APFNetworkSubsystem* Manager = APFNetworkSubsystem::Get(GetWorld())) Manager->MarkDirty();
    PFStartMultiplayerClientObserver(GetWorld());
}

void UPFPowerConnection::EndPlay(const EEndPlayReason::Type Reason)
{
    if (GetOwner()->HasAuthority() && Reason == EEndPlayReason::Destroyed) ClearHiddenConnections();
    if (APFNetworkSubsystem* Manager = APFNetworkSubsystem::Get(GetWorld())) Manager->MarkDirty();
    if (Indicator) Indicator->DestroyComponent();
    Super::EndPlay(Reason);
}

void UPFPowerConnection::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UPFPowerConnection, bGridPowered);
}

void UPFPowerConnection::RefreshPowerState()
{
    if (!GetOwner()->HasAuthority()) return;
    UFGPowerCircuit* Circuit = GetPowerCircuit();
    const bool NewState = Circuit && Circuit->HasPower() && !Circuit->IsFuseTriggered();
    if (NewState != bGridPowered)
    {
        bGridPowered = NewState;
        GetOwner()->FlushNetDormancy();
        GetOwner()->ForceNetUpdate();
        OnRep_GridPowered();
    }
}

void UPFPowerConnection::OnRep_GridPowered()
{
    if (IndicatorMID) IndicatorMID->SetVectorParameterValue(TEXT("PowerColor"), bGridPowered ? FLinearColor(.015f, 1.6f, .03f) : FLinearColor(1.8f, .025f, .015f));
}
