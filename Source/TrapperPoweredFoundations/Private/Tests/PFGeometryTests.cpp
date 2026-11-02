#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PFGeometry.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPFAdjacencyTest, "TrapperPoweredFoundations.Geometry.Adjacency", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPFAdjacencyTest::RunTest(const FString&)
{
    PFGeometry::FTile A, B;
    B.Transform.SetLocation(FVector(800, 0, 0));
    TestTrue(TEXT("East edge conducts"), PFGeometry::ShareSide(A, B));
    TestTrue(TEXT("Adjacency is symmetric"), PFGeometry::ShareSide(B, A));
    B.Transform.SetLocation(FVector(0, -800, 0));
    TestTrue(TEXT("South edge conducts"), PFGeometry::ShareSide(A, B));
    B.Transform.SetLocation(FVector(800, 800, 0));
    TestFalse(TEXT("Diagonal corners do not conduct"), PFGeometry::ShareSide(A, B));
    B.Transform.SetLocation(FVector(802, 0, 0));
    TestFalse(TEXT("Visible gaps do not conduct"), PFGeometry::ShareSide(A, B));
    B.Transform.SetLocation(FVector(0, 0, 100));
    TestFalse(TEXT("Stacked floors do not share a side"), PFGeometry::ShareSide(A, B));
    TestTrue(TEXT("Stacked top/bottom faces conduct"), PFGeometry::ShareFace(A, B));
    B.Transform.SetLocation(FVector(0, 0, 0));
    TestFalse(TEXT("Overlaid tiles do not conduct"), PFGeometry::ShareSide(A, B));
    B.HalfSize.Z = 200;
    B.Transform.SetLocation(FVector(800, 0, -150));
    TestTrue(TEXT("Different thickness at same top conducts"), PFGeometry::ShareSide(A, B));
    A.Transform.SetRotation(FRotator(0, 37, 0).Quaternion());
    B.Transform.SetRotation(A.Transform.GetRotation());
    B.Transform.SetLocation(A.Transform.TransformVector(FVector(800, 0, -150)));
    TestTrue(TEXT("Rotated factories conduct at shared edges"), PFGeometry::ShareSide(A, B));
    B.Transform.SetLocation(A.Transform.TransformVector(FVector(800, 800, -150)));
    TestFalse(TEXT("Rotated corners remain isolated"), PFGeometry::ShareSide(A, B));
    B.Transform.SetRotation(FRotator(0, 82, 0).Quaternion());
    TestFalse(TEXT("Crossed grids do not connect by bounding box"), PFGeometry::ShareSide(A, B));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPFStackFaceTest, "TrapperPoweredFoundations.Geometry.StackFaces", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPFStackFaceTest::RunTest(const FString&)
{
    for (double AH : {50., 100., 200.}) for (double BH : {50., 100., 200.})
    for (double Yaw : {0., 37., 90.})
    {
        PFGeometry::FTile A, B;
        A.HalfSize.Z = AH; B.HalfSize.Z = BH;
        A.Transform.SetRotation(FRotator(0, Yaw, 0).Quaternion());
        B.Transform.SetRotation(A.Transform.GetRotation());
        B.Transform.SetLocation(FVector(0, 0, AH + BH));
        TestTrue(TEXT("Every mixed height shares top/bottom faces"), PFGeometry::ShareFace(A, B));
        TestTrue(TEXT("Vertical conduction is symmetric"), PFGeometry::ShareFace(B, A));
        B.Transform.AddToTranslation(FVector(0, 0, 2));
        TestFalse(TEXT("Two centimeter vertical gap is isolated"), PFGeometry::ShareFace(A, B));
        B.Transform.SetLocation(A.Transform.TransformVector(FVector(800, 0, AH + BH)));
        TestFalse(TEXT("Vertical edge contact without area is isolated"), PFGeometry::ShareFace(A, B));
        B.Transform.SetLocation(A.Transform.TransformVector(FVector(800, 800, AH + BH)));
        TestFalse(TEXT("Vertical corner contact is isolated"), PFGeometry::ShareFace(A, B));
        B.Transform.SetLocation(A.Transform.TransformVector(FVector(600, 0, AH + BH)));
        TestTrue(TEXT("Offset stack with shared face area conducts"), PFGeometry::ShareFace(A, B));
        B.Transform.SetRotation(FRotator(0, Yaw + 45, 0).Quaternion());
        TestTrue(TEXT("Differently rotated face contact conducts"), PFGeometry::ShareFace(A, B));
        B.Transform.SetLocation(A.Transform.TransformVector(FVector(950, 950, AH + BH)));
        TestFalse(TEXT("Rotated AABB overlap alone cannot bridge top faces"), PFGeometry::ShareFace(A, B));
        B.Transform.SetLocation(FVector(0, 0, 0));
        TestFalse(TEXT("Intersecting volumes do not count as face contact"), PFGeometry::ShareFace(A, B));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPFContactTest, "TrapperPoweredFoundations.Geometry.MachineContact", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPFContactTest::RunTest(const FString&)
{
    PFGeometry::FTile Tile;
    FBox Body(FVector(-100, -100, 0), FVector(100, 100, 500));
    FTransform TM(FVector(0, 0, 50));
    TestTrue(TEXT("Machine resting on top attaches"), PFGeometry::Supports(Tile, Body, TM));
    TM.SetLocation(FVector(0, 0, 100));
    TestFalse(TEXT("Floating machine remains disconnected"), PFGeometry::Supports(Tile, Body, TM));
    TM.SetLocation(FVector(0, 0, -50));
    TestFalse(TEXT("Machine on lower story remains disconnected"), PFGeometry::Supports(Tile, Body, TM));
    TM.SetLocation(FVector(500, 500, 50));
    TestFalse(TEXT("Corner-only contact does not attach"), PFGeometry::Supports(Tile, Body, TM));
    TM.SetLocation(FVector(450, 0, 50));
    TestTrue(TEXT("Partial footprint resting on a tile attaches"), PFGeometry::Supports(Tile, Body, TM));
    FBox LargeBody(FVector(-1200, -1200, 0), FVector(1200, 1200, 3000));
    TM.SetLocation(FVector(1300, 0, 50));
    TestTrue(TEXT("Large machine needs only one supporting tile"), PFGeometry::Supports(Tile, LargeBody, TM));
    TM.SetLocation(FVector(600, 600, 50));
    TM.SetRotation(FRotator(0, 45, 0).Quaternion());
    FBox LongThinBody(FVector(-400, -10, 0), FVector(400, 10, 200));
    TestTrue(TEXT("Rotated footprint reaching floor attaches"), PFGeometry::Supports(Tile, LongThinBody, TM));
    TM.SetRotation(FRotator(0, -45, 0).Quaternion());
    TestFalse(TEXT("AABB overlap alone cannot attach a rotated machine"), PFGeometry::Supports(Tile, LongThinBody, TM));
    const auto Cells = PFGeometry::CellsFor(FBox(FVector(-10, -10, -10), FVector(10, 10, 10)));
    TestEqual(TEXT("Hash handles negative coordinates across cell boundaries"), Cells.Num(), 8);
    return true;
}
#endif
