#include "gesture/SkeletonExtentTracker.h"
#include "gesture/BaseSkeleton.h"
#include "gesture/GestureMgr.h"
#include "math/Geo.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Mesh.h"
#include <float.h>

SkeletonExtentTracker::SkeletonExtentTracker() : mTrackingID(-1) {
    SetName("skeleton_extent_tracker", ObjectDir::Main());
}

BEGIN_HANDLERS(SkeletonExtentTracker)
    HANDLE_ACTION(start_tracking, StartTracking(_msg->Int(2)))
    HANDLE_ACTION(stop_tracking, mTrackingID = -1)
    HANDLE_ACTION(
        apply_to_mesh_verts, ApplyToMeshVerts(_msg->Obj<RndMesh>(2), _msg->Int(3))
    )
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

void SkeletonExtentTracker::StartTracking(int i1) {
    mTrackingID = i1;
    mMaxX = FLT_MIN;
    mMaxY = FLT_MIN;
    mMinX = FLT_MAX;
    mMinY = FLT_MAX;
}

void SkeletonExtentTracker::Poll() {
    if (mTrackingID != -1) {
        Skeleton *skeleton = TheGestureMgr->GetSkeletonByTrackingID(mTrackingID);
        if (skeleton) {
            for (int i = 0; i < kNumJoints; i++) {
                Vector2 pos;
                skeleton->ScreenPos((SkeletonJoint)i, pos);
                mMinX = Min(mMinX, pos.x);
                mMinY = Min(mMinY, pos.y - 0.10f);
                mMaxX = Max(mMaxX, pos.x);
                mMaxY = Max(mMaxY, pos.y);
            }
            mMinX = Max(0.0f, mMinX);
            mMinY = Max(0.0f, mMinY);
            mMaxX = Min(1.0f, mMaxX);
            mMaxY = Min(1.0f, mMaxY);
        }
    }
}

Hmx::Rect SkeletonExtentTracker::GetViewBox() const {
    Hmx::Rect ret;
    if (mMinX != FLT_MIN && mMinX != FLT_MAX && mMinY != FLT_MIN && mMinY != FLT_MAX) {
        float val = Min(mMaxY - mMinY, 1.0f);
        ret.Set(((mMaxX + mMinX) / 2.0f) - (val / 2.0f), mMaxY, val, val);
    } else {
        ret.Set(0, 0, 1, 1);
    }
    return ret;
}

void SkeletonExtentTracker::ApplyToMeshVerts(RndMesh *mesh, bool mirrored) const {
    Hmx::Rect box = GetViewBox();
    MILO_ASSERT(mesh->Verts().size() == 16, 0x43);

    int direction = (-(unsigned int)mirrored & 0xFFFFFFFEu) + 1;

    int vertIdx = 0;
    unsigned int i = 0;
    float prevYFrac = 0.0f;
    do {
        float yFrac = 0.0f;
        if ((i != 0 && (yFrac = 0.2f, i != 1)) && (yFrac = 0.8f, i > 2) && (yFrac = prevYFrac, i == 3))
            yFrac = 1.0f;

        unsigned int j = 0;
        int baseStride = vertIdx * 0x60;
        float prevXFrac = prevYFrac;
        do {
            float xFrac = 0.0f;
            if ((j != 0 && (xFrac = 0.2f, j != 1)) && (xFrac = 0.8f, j > 2) && (xFrac = prevXFrac, j == 3))
                xFrac = 1.0f;

            j++;
            vertIdx++;
            mesh->Verts()[vertIdx - 1].tex.x = box.w * xFrac + box.x;
            mesh->Verts()[vertIdx - 1].tex.y = (box.h * yFrac + box.y) * (float)(long long)direction;
            baseStride += 0x60;
            prevXFrac = xFrac;
        } while ((int)j < 4);

        i++;
        prevYFrac = yFrac;
    } while ((int)i < 4);
}
