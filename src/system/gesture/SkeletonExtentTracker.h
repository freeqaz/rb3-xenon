#pragma once
#include "math/Geo.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "rndobj/Mesh.h"

class SkeletonExtentTracker : public Hmx::Object {
public:
    SkeletonExtentTracker();
    // Hmx::Object
    virtual DataNode Handle(DataArray *, bool);

    void Poll();
    void ApplyToMeshVerts(RndMesh *, bool) const;
    void StartTracking(int);

private:
    Hmx::Rect GetViewBox() const;

    float mMinX; // 0x28
    float mMinY; // 0x2c
    float mMaxX; // 0x30
    float mMaxY; // 0x34
    int mTrackingID; // 0x38
};
