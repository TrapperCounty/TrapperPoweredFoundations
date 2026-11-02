#pragma once
#include "CoreMinimal.h"
#include "FGPowerConnectionComponent.h"
#include "PFGeometry.h"
#include "PFPowerConnection.generated.h"

UCLASS(ClassGroup=(TrapperPoweredFoundations), meta=(BlueprintSpawnableComponent))
class TRAPPERPOWEREDFOUNDATIONS_API UPFPowerConnection : public UFGPowerConnectionComponent
{
    GENERATED_BODY()
public:
    UPFPowerConnection();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    PFGeometry::FTile GetTile() const;
    void RefreshPowerState();
    bool IsGridPowered() const { return bGridPowered; }

    UPROPERTY(EditDefaultsOnly, Category="Powered Foundations")
    TObjectPtr<class UStaticMesh> IndicatorMesh;
    UPROPERTY(EditDefaultsOnly, Category="Powered Foundations")
    TObjectPtr<class UMaterialInterface> IndicatorMaterial;
private:
    UPROPERTY(ReplicatedUsing=OnRep_GridPowered)
    bool bGridPowered = false;
    UPROPERTY(Transient)
    TObjectPtr<class UMaterialInstanceDynamic> IndicatorMID;
    UPROPERTY(Transient)
    TObjectPtr<class UStaticMeshComponent> Indicator;
    UFUNCTION()
    void OnRep_GridPowered();
};
