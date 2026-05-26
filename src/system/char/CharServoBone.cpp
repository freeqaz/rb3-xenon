#include "char/CharServoBone.h"
#include "char/CharBoneDir.h"
#include "char/CharBonesMeshes.h"
#include "char/CharClipDriver.h"
#include "char/CharDriver.h"
#include "char/CharPollable.h"
#include "char/CharUtl.h"
#include "char/Character.h"
#include "math/Mtx.h"
#include "math/Rot.h"
#include "math/Trig.h"
#include "math/Utl.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "utl/Symbol.h"

void RotateAboutZ(const Vector3 &v, float f, Vector3 &res) {
    float c = Cosine(f);
    float s = Sine(f);
    res.Set(v.x * c - v.y * s, v.x * s + v.y * c, v.z);
}

CharServoBone::CharServoBone()
    : mPelvis(0), mFacingRotDelta(0), mFacingPosDelta(0), mFacingRot(0), mFacingPos(0),
      mMoveSelf(false), mDeltaChanged(false), mRegulate(this) {}

CharServoBone::~CharServoBone() {}

BEGIN_PROPSYNCS(CharServoBone)
    SYNC_PROP_SET(clip_type, mClipType, SetClipType(_val.Sym()))
    SYNC_PROP_SET(move_self, mMoveSelf, SetMoveSelf(_val.Int()))
    SYNC_PROP(delta_changed, mDeltaChanged)
    SYNC_PROP(regulate, mRegulate)
    SYNC_SUPERCLASS(CharBonesMeshes)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(CharServoBone)
    SAVE_REVS(2, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mClipType;
END_SAVES

BEGIN_COPYS(CharServoBone)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(CharServoBone)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mMoveSelf)
        SetClipType(c->mClipType);
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(2, 0)

BEGIN_LOADS(CharServoBone)
    LOAD_REVS(bs)
    ASSERT_REVS(2, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    Symbol s;
    if (d.rev > 1)
        bs >> s;
    SetClipType(s);
END_LOADS

void CharServoBone::Poll() {
    if (!mMeshes.empty()) {
        PoseMeshes();
        Character *me = Character::Current();
        if (mFacingPosDelta) {
            if (!mMoveSelf) {
                if (mDeltaChanged) {
                    Transform tf48 = me->LocalXfm();
                    MoveToDeltaFacing(tf48);
                    Transform tf78;
                    Multiply(mPelvis->LocalXfm(), tf48, tf78);
                    MoveToFacing(mPelvis->DirtyLocalXfm());
                    Transform tfa8;
                    FastInvert(mPelvis->DirtyLocalXfm(), tfa8);
                    Multiply(tfa8, tf78, me->DirtyLocalXfm());
                } else {
                    MoveToFacing(mPelvis->DirtyLocalXfm());
                }
                for (ObjDirItr<CharBone> it(
                         CharBoneDir::FindResourceFromClipType(mClipType), false
                     );
                     it != nullptr;
                     ++it) {
                    if (it->BakeOutAsTopLevel()) {
                        String str(it->Name());
                        if (str.find(".cb") != String::npos) {
                            str = str.substr(0, str.length() - 3);
                        }
                        RndTransformable *boneTrans =
                            CharUtlFindBoneTrans(str.c_str(), Dir());
                        if (mDeltaChanged) {
                            MoveToDeltaFacing(boneTrans->DirtyLocalXfm());
                            MoveToFacing(boneTrans->DirtyLocalXfm());
                        } else {
                            MoveToFacing(boneTrans->DirtyLocalXfm());
                        }
                    }
                }
            } else {
                if (mDeltaChanged) {
                    Transform tfd8(mPelvis->LocalXfm());
                    MoveToFacing(tfd8);
                    Multiply(tfd8, me->LocalXfm(), tfd8);
                    Transform tf108;
                    FastInvert(mPelvis->LocalXfm(), tf108);
                    Multiply(tf108, tfd8, me->DirtyLocalXfm());
                } else {
                    MoveToDeltaFacing(me->DirtyLocalXfm());
                }
                RegulateInternal(me);
            }
            mDeltaChanged = false;
        }
        ZeroDeltas();
    }
}

void CharServoBone::ReallocateInternal() {
    CharBonesMeshes::ReallocateInternal();
    mFacingRotDelta = 0;
    mFacingPosDelta = (Vector3 *)FindPtr("bone_facing_delta.pos");
    if (mFacingPosDelta) {
        mFacingPos = (Vector3 *)FindPtr("bone_facing.pos");
        mPelvis = CharUtlFindBoneTrans("bone_pelvis", Dir());
        if (!mFacingPos) {
            MILO_NOTIFY("CharServoBone: no Facing Pos in ReallocateInternal()");
        }
        if (!mPelvis) {
            MILO_NOTIFY("CharServoBone: no pelvis bone in this dir.");
        }
        mFacingRot = (float *)FindPtr("bone_facing.rotz");
        mFacingRotDelta = (float *)FindPtr("bone_facing_delta.rotz");
    }
}

void CharServoBone::Enter() {
    ZeroDeltas();
    SetRegulateWaypoint(nullptr);
    mDeltaChanged = false;
    mMoveSelf = mFacingPosDelta;
}

void CharServoBone::ZeroDeltas() {
    if (mFacingPosDelta)
        mFacingPosDelta->Zero();
    if (!mFacingRotDelta)
        return;
    *mFacingRotDelta = 0.0f;
}

void CharServoBone::SetClipType(Symbol sym) {
    if (sym != mClipType) {
        mClipType = sym;
        ClearBones();
        CharBoneDir::StuffBones(*this, mClipType);
    }
}

void CharServoBone::SetMoveSelf(bool b) {
    if (mMoveSelf == b)
        return;

    mMoveSelf = b;
    mDeltaChanged = true;
}

void CharServoBone::RegulateInternal(Character *me) {
    if (mRegulate) {
        CharClipDriver *driver = me->Driver()->Before(me->Driver()->Last());
        CharClipDriver *next = driver && driver->mRampIn > 0 ? driver->Next() : nullptr;
        if (next) {
            DoRegulate(me, mRegulate, next, driver->mRampIn, Max(2.0f, driver->mRampIn / 1.5f));
        }
        mRegulate->Constrain(me->DirtyLocalXfm());
    }
}

void CharServoBone::DoRegulate(
    Character *me, Waypoint *waypoint, CharClipDriver *driver, float f3, float f4
) {
    Transform &myxfm = me->DirtyLocalXfm();
    ClipPredict pred(driver->GetClip(), myxfm.v, GetZAngle(myxfm.m));
    pred.Predict(driver->mBeat, driver->mBeat + f3);
    Vector3 pos(pred.mLastPos);
    float ang = pred.mAng;
    float deltaBeat = TheTaskMgr.DeltaBeat() / f4;
    Vector3 shapedDelta;
    waypoint->ShapeDelta(pos, shapedDelta);
    ScaleAddEq(myxfm.v, shapedDelta, deltaBeat);
    float shapeDelta = waypoint->ShapeDelta(ang);
    RotateAboutZ(myxfm.m, shapeDelta * deltaBeat, myxfm.m);
}

void CharServoBone::MoveToDeltaFacing(Transform &tf) {
    Vector3 v18;
    Multiply(*mFacingPosDelta, tf.m, v18);
    tf.v += v18;
    if (mFacingRotDelta) {
        RotateAboutZ(tf.m, *mFacingRotDelta, tf.m);
        Normalize(tf.m, tf.m);
    }
}

void CharServoBone::MoveToFacing(Transform &tf) {
    if (mFacingRot) {
        RotateAboutZ(tf.m, *mFacingRot, tf.m);
        RotateAboutZ(tf.v, *mFacingRot, tf.v);
        Normalize(tf.m, tf.m);
    }
    tf.v += *mFacingPos;
}

void CharServoBone::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    change.push_back(this);
}

BEGIN_HANDLERS(CharServoBone)
    HANDLE_SUPERCLASS(CharPollable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS
