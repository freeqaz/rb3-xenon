#include "rndobj/TexBlendController.h"
#include "math/Mtx.h"
#include "math/Utl.h"
#include "obj/Object.h"

// RB3-360 retail rev storage. Retail's LOAD_REVS keeps NO BinStreamRev: it splits
// the packed rev into two mutable file-scope shorts, and ASSERT_REVS emits nothing.
// The two words must live in ONE aligned(4) aggregate (altRev +0, rev +4) -- MSVC
// does not lay .bss out in declaration order, so two separate statics get other
// globals interleaved between them and will not fold onto one base register.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_TexBlendController;
#define gAltRev gRevs_TexBlendController.altRev
#define gRev gRevs_TexBlendController.rev

RndTexBlendController::RndTexBlendController()
    : mMesh(this), mObject1(this), mObject2(this), mReferenceDistance(0), mMinDistance(0),
      mMaxDistance(0), mTex(this) {}

BEGIN_HANDLERS(RndTexBlendController)
    HANDLE_ACTION(set_min_distance, UpdateMinDistance())
    HANDLE_ACTION(set_max_distance, UpdateMaxDistance())
    HANDLE_ACTION(set_base_distance, UpdateReferenceDistance())
    HANDLE_ACTION(set_all_distances, UpdateAllDistances())
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(RndTexBlendController)
    SYNC_PROP_MODIFY(reference_object_1, mObject1, UpdateAllDistances())
    SYNC_PROP_MODIFY(reference_object_2, mObject2, UpdateAllDistances())
    SYNC_PROP(mesh, mMesh)
    SYNC_PROP(base_distance, mReferenceDistance)
    SYNC_PROP(min_distance, mMinDistance)
    SYNC_PROP(max_distance, mMaxDistance)
    SYNC_PROP(override_map, mTex)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(RndTexBlendController)
    SAVE_REVS(2, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mMesh;
    bs << mObject1;
    bs << mObject2;
    bs << mReferenceDistance;
    bs << mMinDistance;
    bs << mMaxDistance;
    bs << mTex;
END_SAVES

BEGIN_COPYS(RndTexBlendController)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(RndTexBlendController)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mMesh)
        COPY_MEMBER(mObject1)
        COPY_MEMBER(mObject2)
        COPY_MEMBER(mReferenceDistance)
        COPY_MEMBER(mMinDistance)
        COPY_MEMBER(mMaxDistance)
        COPY_MEMBER(mTex)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_LOADS(RndTexBlendController)
    int rev;
    bs >> rev;
    gRev = getHmxRev(rev);
    gAltRev = getAltRev(rev);
    Hmx::Object::Load(bs);
    bs >> mMesh;
    bs >> mObject1;
    bs >> mObject2;
    bs >> mReferenceDistance;
    bs >> mMinDistance;
    bs >> mMaxDistance;
    if (gRev > 1) {
        bs >> mTex;
    }
END_LOADS

bool RndTexBlendController::IsValid() const {
    if (!mMesh) {
        return false;
    }
    if (!mTex) {
        float refDist = mReferenceDistance;
        bool distValid;
        if (!(mMinDistance > refDist) || !(mMaxDistance < refDist)) {
            distValid = true;
        } else {
            distValid = false;
        }
        return mObject1 && mObject2 && refDist > 0 && distValid;
    }
    return true;
}

bool RndTexBlendController::GetCurrentDistance(float &dist) const {
    if (mObject1 && mObject2) {
        const Transform &t = mObject1->WorldXfm();
        dist = Distance(t.v, mObject2->WorldXfm().v);
        return true;
    } else {
        dist = 0;
        return false;
    }
}

RndTexBlendController::BlendState
RndTexBlendController::GetBlendState(float &blend, float influence) const {
    BlendState state = kBlendNone;
    blend = 0.0f;

    if (IsValid()) {
        if (mTex) {
            blend = 1.0f;
            state = kBlendCustom;
        } else {
            float dist;
            if (GetCurrentDistance(dist) && (bool)(mReferenceDistance > 0.0f)) {
                if (dist < mReferenceDistance) {
                    float denom = mReferenceDistance - mMinDistance;
                    if (denom > 0.0f) {
                        state = kBlendNear;
                        blend = (mReferenceDistance - Max(dist, mMinDistance)) / denom;
                    }
                } else if (dist > mReferenceDistance) {
                    float denom = mMaxDistance - mReferenceDistance;
                    if (denom > 0.0f) {
                        state = kBlendFar;
                        blend = (Min(dist, mMaxDistance) - mReferenceDistance) / denom;
                    }
                }
            }
            float t2 = blend * blend;
            float t3 = blend * t2;
            blend = t2 * 2.0f + t3 * 3.0f;
        }
    }

    blend *= influence;
    blend = Clamp(0.0f, 1.0f, blend);
    blend = (float)((long long)(blend * 255.0f) & 0xFF) * (1.0f / 255.0f);
    if (blend < 1.0f / 255.0f) {
        state = kBlendNone;
    }

    return state;
}

void RndTexBlendController::UpdateReferenceDistance() {
    GetCurrentDistance(mReferenceDistance);
    mMinDistance = Min(mMinDistance, mReferenceDistance);
    mMaxDistance = Max(mMaxDistance, mReferenceDistance);
}

void RndTexBlendController::UpdateMinDistance() {
    GetCurrentDistance(mMinDistance);
    mMinDistance = Min(mMinDistance, mReferenceDistance);
}

void RndTexBlendController::UpdateMaxDistance() {
    GetCurrentDistance(mMaxDistance);
    mMaxDistance = Max(mMaxDistance, mReferenceDistance);
}

void RndTexBlendController::UpdateAllDistances() {
    UpdateReferenceDistance();
    mMinDistance = mReferenceDistance * 0.5f;
    mMaxDistance = mReferenceDistance * 1.5f;
}
