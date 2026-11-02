#pragma once
#include "CoreMinimal.h"
#include "Subsystem/ModSubsystem.h"
#include "PFNetworkSubsystem.generated.h"

class AFGBuildable;
class UFGCircuitConnectionComponent;
class UPFPowerConnection;

struct FPFLink
{
    TWeakObjectPtr<UFGCircuitConnectionComponent> A, B;
    FPFLink() = default;
    FPFLink(UFGCircuitConnectionComponent* X, UFGCircuitConnectionComponent* Y);
    bool operator==(const FPFLink& Other) const { return A == Other.A && B == Other.B; }
    friend uint32 GetTypeHash(const FPFLink& Link) { return HashCombine(GetTypeHash(Link.A), GetTypeHash(Link.B)); }
};

/** Only the authority changes circuit topology. Clients receive the rim state. */
UCLASS()
class TRAPPERPOWEREDFOUNDATIONS_API APFNetworkSubsystem : public AModSubsystem
{
    GENERATED_BODY()
public:
    APFNetworkSubsystem();
    static APFNetworkSubsystem* Get(UWorld* World);
    void MarkDirty() { bDirty = true; }
    // Opt-in QA timings; ordinary games do not collect measurements.
    uint64 GetQAPasses() const { return QAPasses; }
    double GetQATotalMs() const { return QATotalMs; }
    double GetQAMaxMs() const { return QAMaxMs; }
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UFUNCTION() void OnBuilt(AFGBuildable* Buildable);
    UFUNCTION() void OnRemoved(AFGBuildable* Buildable);
    UFUNCTION() void OnDesignerChanged();
    void Maintain();
    void Reconcile();
    bool bDirty = true;
    bool bScanned = false;
    int32 MaintenanceCounter = 0;
    FTimerHandle MaintenanceTimer;
    TSet<TWeakObjectPtr<AFGBuildable>> Candidates;
    TSet<TWeakObjectPtr<UPFPowerConnection>> InitializedTiles;
    TArray<TWeakObjectPtr<UPFPowerConnection>> Tiles;
    TSet<FPFLink> Links;
    uint64 QAPasses = 0;
    double QATotalMs = 0, QAMaxMs = 0;
};
