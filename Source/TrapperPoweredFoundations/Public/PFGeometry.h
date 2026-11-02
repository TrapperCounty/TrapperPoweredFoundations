#pragma once
#include "CoreMinimal.h"

// Centimeters, as used by Unreal and Satisfactory.
namespace PFGeometry
{
    constexpr double EdgeTolerance = 1.0;
    constexpr double ContactTolerance = 10.0;
    constexpr double CellSize = 1000.0;

    struct TRAPPERPOWEREDFOUNDATIONS_API FTile
    {
        FTransform Transform;
        FVector HalfSize = FVector(400, 400, 50);
        FBox WorldBounds() const;
        double Top() const;
    };

    TRAPPERPOWEREDFOUNDATIONS_API bool ShareSide(const FTile& A, const FTile& B);
    TRAPPERPOWEREDFOUNDATIONS_API bool ShareFace(const FTile& A, const FTile& B);
    TRAPPERPOWEREDFOUNDATIONS_API bool Supports(const FTile& Tile, const FBox& LocalBody, const FTransform& BodyTransform, double ExtraHeight = 0);
    TRAPPERPOWEREDFOUNDATIONS_API bool FootprintsOverlap(const FTile& Tile, const FBox& LocalBody, const FTransform& BodyTransform);
    TRAPPERPOWEREDFOUNDATIONS_API TArray<FIntVector> CellsFor(const FBox& Bounds);
}
