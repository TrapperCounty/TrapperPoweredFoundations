// One mesh/component per foundation: four L-shaped emitters wrap the top edge.
UMaterial* CornerMaterial()
{
    const TCHAR* Path = TEXT("/TrapperPoweredFoundations/Materials/M_PowerStatus");
    if (auto* Existing = LoadObject<UMaterial>(nullptr, TEXT("/TrapperPoweredFoundations/Materials/M_PowerStatus.M_PowerStatus"), nullptr, LOAD_NoWarn)) return Existing;
    auto* Material = NewObject<UMaterial>(CreatePackage(Path), TEXT("M_PowerStatus"), RF_Public | RF_Standalone);
    Material->SetShadingModel(MSM_Unlit);
    Material->TwoSided = true;
    auto* Color = NewObject<UMaterialExpressionVectorParameter>(Material);
    Color->ParameterName = TEXT("PowerColor");
    Color->DefaultValue = FLinearColor(1.8f, .025f, .015f);
    Material->GetExpressionCollection().AddExpression(Color);
    Material->GetEditorOnlyData()->EmissiveColor.Expression = Color;
    Material->PostEditChange(); FAssetRegistryModule::AssetCreated(Material);
    return Material;
}

UStaticMesh* CornerMesh(UMaterial* Material)
{
    const TCHAR* Path = TEXT("/TrapperPoweredFoundations/Materials/SM_PowerCorners");
    if (auto* Existing = LoadObject<UStaticMesh>(nullptr, TEXT("/TrapperPoweredFoundations/Materials/SM_PowerCorners.SM_PowerCorners"), nullptr, LOAD_NoWarn)) return Existing;
    auto* Mesh = NewObject<UStaticMesh>(CreatePackage(Path), TEXT("SM_PowerCorners"), RF_Public | RF_Standalone);
    Mesh->SetNumSourceModels(1);
    auto& Description = *Mesh->CreateMeshDescription(0);
    FStaticMeshAttributes Attributes(Description); Attributes.Register();
    Attributes.GetVertexInstanceUVs().SetNumChannels(1);
    auto Positions = Attributes.GetVertexPositions();
    auto Normals = Attributes.GetVertexInstanceNormals();
    auto UVs = Attributes.GetVertexInstanceUVs();
    const auto Group = Description.CreatePolygonGroup();
    Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("PowerStatus");
    auto Box = [&](FVector3f Center, FVector3f Half)
    {
        const FVector3f P[] = {
            {-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1},
            {-1,-1,1}, {1,-1,1}, {1,1,1}, {-1,1,1}};
        FVertexID V[8];
        for (int32 I=0; I<8; ++I) { V[I]=Description.CreateVertex(); Positions[V[I]]=Center+P[I]*Half; }
        const int32 Faces[6][4]={{0,3,2,1},{4,5,6,7},{0,1,5,4},{1,2,6,5},{2,3,7,6},{3,0,4,7}};
        for (const auto& Face : Faces)
        {
            TArray<FVertexInstanceID> Corners;
            const FVector3f Normal = FVector3f::CrossProduct(P[Face[1]]-P[Face[0]], P[Face[2]]-P[Face[0]]).GetSafeNormal();
            for(int32 I=0; I<4; ++I)
            {
                const auto VI=Description.CreateVertexInstance(V[Face[I]]);
                Normals[VI]=Normal; UVs.Set(VI,0,FVector2f(I==1||I==2?1:0,I>=2?1:0)); Corners.Add(VI);
            }
            Description.CreatePolygon(Group,Corners);
        }
    };
    for(float X : {-1.f,1.f}) for(float Y : {-1.f,1.f})
    {
        Box(FVector3f(X*386,Y*398,-2.5),FVector3f(14,3,3.5));
        Box(FVector3f(X*398,Y*386,-2.5),FVector3f(3,14,3.5));
    }
    Mesh->GetStaticMaterials().Add(FStaticMaterial(Material, TEXT("PowerStatus")));
    Mesh->GetSourceModel(0).BuildSettings.bRecomputeNormals = false;
    Mesh->GetSourceModel(0).BuildSettings.bGenerateLightmapUVs = false;
    Mesh->CommitMeshDescription(0); Mesh->Build(false);
    FAssetRegistryModule::AssetCreated(Mesh);
    return Mesh;
}
