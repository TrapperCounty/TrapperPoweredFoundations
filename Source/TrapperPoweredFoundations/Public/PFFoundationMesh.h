#pragma once
#include "CoreMinimal.h"
#include "FGColoredInstanceMeshProxy.h"
#include "PFFoundationMesh.generated.h"

/** Native colored proxy with a null-world guard for commandlet templates. */
UCLASS(meta=(BlueprintSpawnableComponent))
class TRAPPERPOWEREDFOUNDATIONS_API UPFFoundationMesh : public UFGColoredInstanceMeshProxy
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateRenderState() const override
    {
        return GetWorld() != nullptr && Super::ShouldCreateRenderState();
    }
};
