#pragma once
#include "Module/GameWorldModule.h"
#include "FGFactoryColoringTypes.h"
#include "PFGameWorldModule.generated.h"

USTRUCT()
struct FPFMaterialMapping
{
    GENERATED_BODY()
    UPROPERTY(EditDefaultsOnly) TSubclassOf<UFGFactoryCustomizationDescriptor_Material> Material;
    UPROPERTY(EditDefaultsOnly) TSubclassOf<AFGBuildable> Source;
    UPROPERTY(EditDefaultsOnly) TSubclassOf<UFGRecipe> Recipe;
};

/** Registers only our own class keys in the native Customizer material maps. */
UCLASS()
class TRAPPERPOWEREDFOUNDATIONS_API UPFGameWorldModule : public UGameWorldModule
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly) TArray<FPFMaterialMapping> MaterialMappings;
    virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;
};
