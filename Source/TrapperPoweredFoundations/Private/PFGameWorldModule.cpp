#include "PFGameWorldModule.h"
#include "Buildables/FGBuildable.h"
#include "FGRecipe.h"

void UPFGameWorldModule::DispatchLifecycleEvent(ELifecyclePhase Phase)
{
    if (Phase == ELifecyclePhase::CONSTRUCTION)
        for (const auto& Mapping : MaterialMappings)
            if (Mapping.Material && Mapping.Source && Mapping.Recipe)
            {
                auto* Descriptor = Mapping.Material->GetDefaultObject<UFGFactoryCustomizationDescriptor_Material>();
                Descriptor->GetBuildableMap().Add(Mapping.Source, Mapping.Recipe);
                Descriptor->mValidBuildables.AddUnique(Mapping.Source);
            }
    Super::DispatchLifecycleEvent(Phase);
}
