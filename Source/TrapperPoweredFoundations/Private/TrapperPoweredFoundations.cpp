#include "Modules/ModuleManager.h"
#include "UObject/CoreRedirects.h"
#include "Misc/CommandLine.h"
class FTrapperPoweredFoundationsModule : public IModuleInterface
{
public:
 void StartupModule() override
 {
  if (!FParse::Param(FCommandLine::Get(), TEXT("TPFMigratePrivateSave"))) return;
  TArray<FCoreRedirect> Redirects;
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/Script/PoweredFoundations"), TEXT("/Script/TrapperPoweredFoundations"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/RootGameWorld_PoweredFoundations"), TEXT("/TrapperPoweredFoundations/RootGameWorld_PoweredFoundations"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Foundations/Build_PoweredFoundation_1m"), TEXT("/TrapperPoweredFoundations/Foundations/Build_PoweredFoundation_1m"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Foundations/Build_PoweredFoundation_1m_Asphalt"), TEXT("/TrapperPoweredFoundations/Foundations/Build_PoweredFoundation_1m_Asphalt"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Foundations/Build_PoweredFoundation_1m_Concrete"), TEXT("/TrapperPoweredFoundations/Foundations/Build_PoweredFoundation_1m_Concrete"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Foundations/Build_PoweredFoundation_1m_GripMetal"), TEXT("/TrapperPoweredFoundations/Foundations/Build_PoweredFoundation_1m_GripMetal"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Foundations/Build_PoweredFoundation_1m_PolishedConcrete"), TEXT("/TrapperPoweredFoundations/Foundations/Build_PoweredFoundation_1m_PolishedConcrete"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Foundations/Build_PoweredFoundation_2m"), TEXT("/TrapperPoweredFoundations/Foundations/Build_PoweredFoundation_2m"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Foundations/Build_PoweredFoundation_2m_Asphalt"), TEXT("/TrapperPoweredFoundations/Foundations/Build_PoweredFoundation_2m_Asphalt"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Foundations/Build_PoweredFoundation_2m_Concrete"), TEXT("/TrapperPoweredFoundations/Foundations/Build_PoweredFoundation_2m_Concrete"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Foundations/Build_PoweredFoundation_2m_GripMetal"), TEXT("/TrapperPoweredFoundations/Foundations/Build_PoweredFoundation_2m_GripMetal"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Foundations/Build_PoweredFoundation_2m_PolishedConcrete"), TEXT("/TrapperPoweredFoundations/Foundations/Build_PoweredFoundation_2m_PolishedConcrete"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Foundations/Build_PoweredFoundation_4m"), TEXT("/TrapperPoweredFoundations/Foundations/Build_PoweredFoundation_4m"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Foundations/Build_PoweredFoundation_4m_Asphalt"), TEXT("/TrapperPoweredFoundations/Foundations/Build_PoweredFoundation_4m_Asphalt"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Foundations/Build_PoweredFoundation_4m_Concrete"), TEXT("/TrapperPoweredFoundations/Foundations/Build_PoweredFoundation_4m_Concrete"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Foundations/Build_PoweredFoundation_4m_GripMetal"), TEXT("/TrapperPoweredFoundations/Foundations/Build_PoweredFoundation_4m_GripMetal"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Foundations/Build_PoweredFoundation_4m_PolishedConcrete"), TEXT("/TrapperPoweredFoundations/Foundations/Build_PoweredFoundation_4m_PolishedConcrete"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Foundations/Desc_PoweredFoundation_1m"), TEXT("/TrapperPoweredFoundations/Foundations/Desc_PoweredFoundation_1m"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Foundations/Desc_PoweredFoundation_1m_Asphalt"), TEXT("/TrapperPoweredFoundations/Foundations/Desc_PoweredFoundation_1m_Asphalt"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Foundations/Desc_PoweredFoundation_1m_Concrete"), TEXT("/TrapperPoweredFoundations/Foundations/Desc_PoweredFoundation_1m_Concrete"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Foundations/Desc_PoweredFoundation_1m_GripMetal"), TEXT("/TrapperPoweredFoundations/Foundations/Desc_PoweredFoundation_1m_GripMetal"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Foundations/Desc_PoweredFoundation_1m_PolishedConcrete"), TEXT("/TrapperPoweredFoundations/Foundations/Desc_PoweredFoundation_1m_PolishedConcrete"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Foundations/Desc_PoweredFoundation_2m"), TEXT("/TrapperPoweredFoundations/Foundations/Desc_PoweredFoundation_2m"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Foundations/Desc_PoweredFoundation_2m_Asphalt"), TEXT("/TrapperPoweredFoundations/Foundations/Desc_PoweredFoundation_2m_Asphalt"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Foundations/Desc_PoweredFoundation_2m_Concrete"), TEXT("/TrapperPoweredFoundations/Foundations/Desc_PoweredFoundation_2m_Concrete"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Foundations/Desc_PoweredFoundation_2m_GripMetal"), TEXT("/TrapperPoweredFoundations/Foundations/Desc_PoweredFoundation_2m_GripMetal"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Foundations/Desc_PoweredFoundation_2m_PolishedConcrete"), TEXT("/TrapperPoweredFoundations/Foundations/Desc_PoweredFoundation_2m_PolishedConcrete"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Foundations/Desc_PoweredFoundation_4m"), TEXT("/TrapperPoweredFoundations/Foundations/Desc_PoweredFoundation_4m"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Foundations/Desc_PoweredFoundation_4m_Asphalt"), TEXT("/TrapperPoweredFoundations/Foundations/Desc_PoweredFoundation_4m_Asphalt"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Foundations/Desc_PoweredFoundation_4m_Concrete"), TEXT("/TrapperPoweredFoundations/Foundations/Desc_PoweredFoundation_4m_Concrete"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Foundations/Desc_PoweredFoundation_4m_GripMetal"), TEXT("/TrapperPoweredFoundations/Foundations/Desc_PoweredFoundation_4m_GripMetal"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Foundations/Desc_PoweredFoundation_4m_PolishedConcrete"), TEXT("/TrapperPoweredFoundations/Foundations/Desc_PoweredFoundation_4m_PolishedConcrete"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Materials/M_PoweredRim"), TEXT("/TrapperPoweredFoundations/Materials/M_PoweredRim"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Materials/M_PowerStatus"), TEXT("/TrapperPoweredFoundations/Materials/M_PowerStatus"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Materials/SM_PowerCorners"), TEXT("/TrapperPoweredFoundations/Materials/SM_PowerCorners"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Recipes/Recipe_PoweredFoundation_1m"), TEXT("/TrapperPoweredFoundations/Recipes/Recipe_PoweredFoundation_1m"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Recipes/Recipe_PoweredFoundation_1m_Asphalt"), TEXT("/TrapperPoweredFoundations/Recipes/Recipe_PoweredFoundation_1m_Asphalt"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Recipes/Recipe_PoweredFoundation_1m_Concrete"), TEXT("/TrapperPoweredFoundations/Recipes/Recipe_PoweredFoundation_1m_Concrete"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Recipes/Recipe_PoweredFoundation_1m_GripMetal"), TEXT("/TrapperPoweredFoundations/Recipes/Recipe_PoweredFoundation_1m_GripMetal"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Recipes/Recipe_PoweredFoundation_1m_PolishedConcrete"), TEXT("/TrapperPoweredFoundations/Recipes/Recipe_PoweredFoundation_1m_PolishedConcrete"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Recipes/Recipe_PoweredFoundation_2m"), TEXT("/TrapperPoweredFoundations/Recipes/Recipe_PoweredFoundation_2m"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Recipes/Recipe_PoweredFoundation_2m_Asphalt"), TEXT("/TrapperPoweredFoundations/Recipes/Recipe_PoweredFoundation_2m_Asphalt"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Recipes/Recipe_PoweredFoundation_2m_Concrete"), TEXT("/TrapperPoweredFoundations/Recipes/Recipe_PoweredFoundation_2m_Concrete"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Recipes/Recipe_PoweredFoundation_2m_GripMetal"), TEXT("/TrapperPoweredFoundations/Recipes/Recipe_PoweredFoundation_2m_GripMetal"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Recipes/Recipe_PoweredFoundation_2m_PolishedConcrete"), TEXT("/TrapperPoweredFoundations/Recipes/Recipe_PoweredFoundation_2m_PolishedConcrete"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Recipes/Recipe_PoweredFoundation_4m"), TEXT("/TrapperPoweredFoundations/Recipes/Recipe_PoweredFoundation_4m"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Recipes/Recipe_PoweredFoundation_4m_Asphalt"), TEXT("/TrapperPoweredFoundations/Recipes/Recipe_PoweredFoundation_4m_Asphalt"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Recipes/Recipe_PoweredFoundation_4m_Concrete"), TEXT("/TrapperPoweredFoundations/Recipes/Recipe_PoweredFoundation_4m_Concrete"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Recipes/Recipe_PoweredFoundation_4m_GripMetal"), TEXT("/TrapperPoweredFoundations/Recipes/Recipe_PoweredFoundation_4m_GripMetal"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Recipes/Recipe_PoweredFoundation_4m_PolishedConcrete"), TEXT("/TrapperPoweredFoundations/Recipes/Recipe_PoweredFoundation_4m_PolishedConcrete"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/PoweredFoundations/Schematics/Schematic_PoweredFoundations"), TEXT("/TrapperPoweredFoundations/Schematics/Schematic_PoweredFoundations"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Class, TEXT("/PoweredFoundations/RootGameWorld_PoweredFoundations.RootGameWorld_PoweredFoundations_C"), TEXT("/TrapperPoweredFoundations/RootGameWorld_PoweredFoundations.RootGameWorld_PoweredFoundations_C"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Object, TEXT("/PoweredFoundations/RootGameWorld_PoweredFoundations.RootGameWorld_PoweredFoundations"), TEXT("/TrapperPoweredFoundations/RootGameWorld_PoweredFoundations.RootGameWorld_PoweredFoundations"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Class, TEXT("/PoweredFoundations/Schematics/Schematic_PoweredFoundations.Schematic_PoweredFoundations_C"), TEXT("/TrapperPoweredFoundations/Schematics/Schematic_PoweredFoundations.Schematic_PoweredFoundations_C"));
  Redirects.Emplace(ECoreRedirectFlags::Type_Object, TEXT("/PoweredFoundations/Schematics/Schematic_PoweredFoundations.Schematic_PoweredFoundations"), TEXT("/TrapperPoweredFoundations/Schematics/Schematic_PoweredFoundations.Schematic_PoweredFoundations"));
  FCoreRedirects::AddRedirectList(Redirects, TEXT("TrapperPrivateSaveMigration"));
 }
};
IMPLEMENT_MODULE(FTrapperPoweredFoundationsModule, TrapperPoweredFoundations)
