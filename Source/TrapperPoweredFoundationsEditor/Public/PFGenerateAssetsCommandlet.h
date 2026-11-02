#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "PFGenerateAssetsCommandlet.generated.h"

UCLASS()
class UPFGenerateAssetsCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UPFGenerateAssetsCommandlet();
    virtual int32 Main(const FString& Params) override;
};
