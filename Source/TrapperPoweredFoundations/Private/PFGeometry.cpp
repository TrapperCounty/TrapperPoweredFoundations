#include "PFGeometry.h"

FBox PFGeometry::FTile::WorldBounds() const
{
    return FBox(-HalfSize, HalfSize).TransformBy(Transform);
}

double PFGeometry::FTile::Top() const { return WorldBounds().Max.Z; }

bool PFGeometry::ShareSide(const FTile& A, const FTile& B)
{
    // Upright floor edges can join even when the whole factory is rotated.
    // Side adjacency requires overlapping height; top/bottom contact is separate.
    const FVector AX = A.Transform.GetUnitAxis(EAxis::X);
    const FVector AY = A.Transform.GetUnitAxis(EAxis::Y);
    const FVector BX = B.Transform.GetUnitAxis(EAxis::X);
    const FVector BY = B.Transform.GetUnitAxis(EAxis::Y);
    if (FMath::Abs(A.Transform.GetUnitAxis(EAxis::Z).Z) < .999 ||
        FMath::Abs(B.Transform.GetUnitAxis(EAxis::Z).Z) < .999) return false;
    if (FMath::Max(FMath::Abs(AX.Dot(BX)), FMath::Abs(AX.Dot(BY))) < .9999) return false;
    const FBox AB = A.WorldBounds(), BB = B.WorldBounds();
    if (FMath::Min(AB.Max.Z, BB.Max.Z) - FMath::Max(AB.Min.Z, BB.Min.Z) <= EdgeTolerance) return false;
    const FVector Delta = B.Transform.GetLocation() - A.Transform.GetLocation();
    const FVector AS = A.HalfSize * A.Transform.GetScale3D().GetAbs();
    const FVector BS = B.HalfSize * B.Transform.GetScale3D().GetAbs();
    const double XReach = AS.X + FMath::Abs(AX.Dot(BX)) * BS.X + FMath::Abs(AX.Dot(BY)) * BS.Y;
    const double YReach = AS.Y + FMath::Abs(AY.Dot(BX)) * BS.X + FMath::Abs(AY.Dot(BY)) * BS.Y;
    const double DX = FMath::Abs(Delta.Dot(AX)), DY = FMath::Abs(Delta.Dot(AY));
    return (FMath::Abs(DX - XReach) <= EdgeTolerance && YReach - DY > EdgeTolerance * 2) ||
           (FMath::Abs(DY - YReach) <= EdgeTolerance && XReach - DX > EdgeTolerance * 2);
}

bool PFGeometry::ShareFace(const FTile& A, const FTile& B)
{
    if (ShareSide(A, B)) return true;
    if (FMath::Abs(A.Transform.GetUnitAxis(EAxis::Z).Z) < .999 ||
        FMath::Abs(B.Transform.GetUnitAxis(EAxis::Z).Z) < .999) return false;
    const FBox AB = A.WorldBounds(), BB = B.WorldBounds();
    if (FMath::Abs(AB.Max.Z - BB.Min.Z) > EdgeTolerance &&
        FMath::Abs(BB.Max.Z - AB.Min.Z) > EdgeTolerance) return false;
    // Require actual face overlap, excluding edge/corner contact and open stories.
    return FootprintsOverlap(A, FBox(-B.HalfSize, B.HalfSize), B.Transform);
}

bool PFGeometry::FootprintsOverlap(const FTile& Tile, const FBox& Body, const FTransform& TM)
{
    if (!Body.IsValid) return false;
    TArray<FVector2D, TInlineAllocator<8>> A, B;
    TArray<FVector2D, TInlineAllocator<8>> Axes;
    for (int X : {-1, 1}) for (int Y : {-1, 1})
        A.Add(FVector2D(Tile.Transform.TransformPosition(FVector(X * Tile.HalfSize.X, Y * Tile.HalfSize.Y, 0))));
    for (int X : {0, 1}) for (int Y : {0, 1}) for (int Z : {0, 1})
        B.Add(FVector2D(TM.TransformPosition(FVector(X ? Body.Max.X : Body.Min.X, Y ? Body.Max.Y : Body.Min.Y, Z ? Body.Max.Z : Body.Min.Z))));
    // The projection of an oriented box is a convex polygon. Its edges are
    // parallel to projections of the original box axes, including tilted pumps.
    for (const FTransform* T : {&Tile.Transform, &TM})
        for (EAxis::Type Axis : {EAxis::X, EAxis::Y, EAxis::Z})
        {
            const FVector V = T->GetUnitAxis(Axis);
            FVector2D Normal(-V.Y, V.X);
            if (Normal.Normalize()) Axes.Add(Normal);
        }
    for (const FVector2D& Axis : Axes)
    {
        double AMin = DBL_MAX, AMax = -DBL_MAX, BMin = DBL_MAX, BMax = -DBL_MAX;
        for (const FVector2D& P : A) { const double D = P.Dot(Axis); AMin = FMath::Min(AMin, D); AMax = FMath::Max(AMax, D); }
        for (const FVector2D& P : B) { const double D = P.Dot(Axis); BMin = FMath::Min(BMin, D); BMax = FMath::Max(BMax, D); }
        if (FMath::Min(AMax, BMax) - FMath::Max(AMin, BMin) <= EdgeTolerance) return false;
    }
    return true;
}

bool PFGeometry::Supports(const FTile& Tile, const FBox& Body, const FTransform& TM, double ExtraHeight)
{
    if (!Body.IsValid) return false;
    const FBox World = Body.TransformBy(TM);
    const double Gap = World.Min.Z - Tile.Top();
    return Gap >= -ContactTolerance && Gap <= ContactTolerance + ExtraHeight && FootprintsOverlap(Tile, Body, TM);
}

TArray<FIntVector> PFGeometry::CellsFor(const FBox& Bounds)
{
    TArray<FIntVector> Result;
    if (!Bounds.IsValid) return Result;
    const FIntVector Min(FMath::FloorToInt(Bounds.Min.X / CellSize), FMath::FloorToInt(Bounds.Min.Y / CellSize), FMath::FloorToInt(Bounds.Min.Z / CellSize));
    const FIntVector Max(FMath::FloorToInt(Bounds.Max.X / CellSize), FMath::FloorToInt(Bounds.Max.Y / CellSize), FMath::FloorToInt(Bounds.Max.Z / CellSize));
    for (int X = Min.X; X <= Max.X; ++X) for (int Y = Min.Y; Y <= Max.Y; ++Y) for (int Z = Min.Z; Z <= Max.Z; ++Z)
        Result.Emplace(X, Y, Z);
    return Result;
}
