#include "hamobj/TransConstraint.h"
#include "math/Geo.h"
#include "math/Mtx.h"
#include "math/Rot.h"
#include "math/Utl.h"
#include "math/Vec.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/System.h"
#include "rndobj/Highlight.h"
#include "rndobj/Poll.h"
#include "rndobj/Trans.h"
#include "rndobj/Utl.h"

TransConstraint::TransConstraint()
    : mParent(this), mChild(this), mSpeed(10), mAffectScale(0), mUseUITime(0), mEnabled(1) {
    mStaticCube.Zero();
    for (int i = 0; i < 3; i++) {
        mTracks[i] = false;
    }
}

BEGIN_HANDLERS(TransConstraint)
    HANDLE_ACTION(snap_to_parent, SnapToParent())
    HANDLE_SUPERCLASS(RndHighlightable)
    HANDLE_SUPERCLASS(RndPollable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(TransConstraint)
    SYNC_PROP(parent, mParent)
    SYNC_PROP(child, mChild)
    SYNC_PROP(static_cube, mStaticCube)
    SYNC_PROP(speed, mSpeed)
    SYNC_PROP(affect_scale, mAffectScale)
    SYNC_PROP(use_ui_time, mUseUITime)
    SYNC_PROP(track_x, mTracks[0])
    SYNC_PROP(track_y, mTracks[1])
    SYNC_PROP(track_z, mTracks[2])
    SYNC_SUPERCLASS(RndHighlightable)
    SYNC_SUPERCLASS(RndPollable)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(TransConstraint)
    SAVE_REVS(4, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndPollable)
    SAVE_SUPERCLASS(RndHighlightable)
    bs << mParent;
    bs << mChild;
    bs << mStaticCube;
    for (int i = 0; i < 3; i++) {
        bs << mTracks[i];
    }
    bs << mSpeed;
    bs << mAffectScale;
    bs << mUseUITime;
END_SAVES

BEGIN_COPYS(TransConstraint)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndPollable)
    COPY_SUPERCLASS(RndHighlightable)
    CREATE_COPY(TransConstraint)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mParent)
        COPY_MEMBER(mChild)
        COPY_MEMBER(mStaticCube)
        for (int i = 0; i < 3; i++) {
            COPY_MEMBER(mTracks[i])
        }
        COPY_MEMBER(mSpeed)
        COPY_MEMBER(mAffectScale)
        COPY_MEMBER(mUseUITime)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(4, 0)

BEGIN_LOADS(TransConstraint)
    LOAD_REVS(bs)
    ASSERT_REVS(4, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(RndPollable)
    LOAD_SUPERCLASS(RndHighlightable)
    bs >> mParent;
    bs >> mChild;
    bs >> mStaticCube;
    for (int i = 0; i < 3; i++) {
        d >> mTracks[i];
    }
    if (d.rev > 0) {
        bs >> mSpeed;
    }
    if (d.rev > 1) {
        if (d.rev <= 3) {
            bool b;
            d >> b;
        }
        d >> mAffectScale;
    }
    if (d.rev > 2) {
        d >> mUseUITime;
    }
END_LOADS

void TransConstraint::Enter() {
    RndPollable::Enter();
    SnapToParent();
}

void TransConstraint::SetScaleVectorOnTransform(RndTransformable *trans, Vector3 &v) {
    MILO_ASSERT(trans, 0x80);
    Vector3 va0;
    Hmx::Matrix3 m90;
    Transform tf60 = trans->WorldXfm();
    MakeEuler(tf60.m, va0);
    MakeRotMatrix(va0, m90, true);
    Scale(m90, v, m90);
    tf60.m = m90;
    trans->SetWorldXfm(tf60);
}

void TransConstraint::SnapToParent() {
    if (mParent && mChild) {
        Vector3 v50 = mParent->WorldXfm().v;
        Vector3 v70 = mChild->WorldXfm().v;
        for (int i = 0; i < 3; i++) {
            if (mTracks[i]) {
                v70[i] = v50[i];
            }
        }
        mChild->SetWorldPos(v70);
        if (mAffectScale) {
            Vector3 v40;
            MakeScale(mParent->WorldXfm().m, v40);
            Vector3 v60;
            MakeScale(mChild->WorldXfm().m, v60);
            for (int i = 0; i < 3; i++) {
                if (mTracks[i]) {
                    v60[i] = v40[i];
                }
            }
            SetScaleVectorOnTransform(mChild, v60);
        }
    }
}

void TransConstraint::Highlight() {
    if (mParent && mChild) {
        Transform xfm = mParent->WorldXfm();
        xfm.m.Identity();

        Box box;
        for (int i = 0; i < 3; i++) {
            float halfExtent = mStaticCube[i] * 0.5f;
            box.mMin[i] = -halfExtent;
            box.mMax[i] = halfExtent;
        }

        Hmx::Color white(1, 1, 1, 1);
        UtilDrawAxes(mParent->WorldXfm(), 10.0f, white);

        Hmx::Color white2(1, 1, 1, 1);
        UtilDrawAxes(mChild->WorldXfm(), 10.0f, white2);

        Hmx::Color yellow(1, 1, 0, 1);
        UtilDrawBox(xfm, box, yellow, true);
    }
}

void TransConstraint::Poll() {
    if (!mParent || !mChild || !mEnabled)
        return;

    float dt;
    if (mUseUITime) {
        dt = TheTaskMgr.DeltaUISeconds();
    } else {
        dt = TheTaskMgr.DeltaSeconds();
    }

    Vector3 parentPos = mParent->WorldXfm().v;
    Vector3 childPos = mChild->WorldXfm().v;

    Vector3 delta;
    delta.x = parentPos.x - childPos.x;
    delta.y = parentPos.y - childPos.y;
    delta.z = parentPos.z - childPos.z;

    for (int i = 0; i < 3; i++) {
        if (!mTracks[i]) {
            delta[i] = 0.0f;
        }
    }

    if (Length(delta) > mSpeed * 3.0f) {
        SnapToParent();
    } else {
        Vector3 dir;
        Normalize(delta, dir);

        for (int i = 0; i < 3; i++) {
            if (mTracks[i]) {
                float halfCube = mStaticCube[i] * 0.5f;
                float lo = parentPos[i] - halfCube;
                float hi = parentPos[i] + halfCube;

                if (childPos[i] < lo || childPos[i] > hi) {
                    float step = dir[i] * mSpeed;

                    if (childPos[i] < lo) {
                        childPos[i] += step * dt;
                        float &cp = childPos[i];
                        cp = Min(cp, lo);
                    } else if (childPos[i] > hi) {
                        childPos[i] += step * dt;
                        float &cp = childPos[i];
                        cp = Max(cp, hi);
                    }
                }
            }
        }

        mChild->SetWorldPos(childPos);

        if (mAffectScale) {
            Vector3 parentScale;
            Vector3 childWorldScale;
            MakeScale(mParent->WorldXfm().m, parentScale);
            MakeScale(mChild->WorldXfm().m, childWorldScale);
            Vector3 childLocalScale;
            MakeScale(mChild->LocalXfm().m, childLocalScale);

            Vector3 scaleDelta;
            scaleDelta.x = parentScale.x - childWorldScale.x;
            scaleDelta.y = parentScale.y - childWorldScale.y;
            scaleDelta.z = parentScale.z - childWorldScale.z;

            for (int i = 0; i < 3; i++) {
                if (!mTracks[i]) {
                    scaleDelta[i] = 0.0f;
                }
            }

            float scaleRatio = 1.0f;
            float posDist = Length(delta);
            if (0.0f < posDist) {
                scaleRatio = Length(scaleDelta) / posDist;
            }
            Vector3 scaleDir;
            Normalize(scaleDelta, scaleDir);

            float scaleSpeed = scaleRatio * mSpeed;

            for (int i = 0; i < 3; i++) {
                if (mTracks[i]) {
                    float ps = parentScale[i];
                    if (childWorldScale[i] < ps
                        || childWorldScale[i] > ps) {
                        float step = scaleDir[i] * scaleSpeed;

                        if (childWorldScale[i] < ps) {
                            childWorldScale[i] += step * dt;
                            float &cws = childWorldScale[i];
                            cws = Min(cws, ps);
                        } else if (childWorldScale[i] > ps) {
                            childWorldScale[i] += step * dt;
                            float &cws = childWorldScale[i];
                            cws = Max(cws, ps);
                        }
                    }
                } else {
                    childWorldScale[i] = childLocalScale[i] + parentScale[i];
                }
            }

            SetScaleVectorOnTransform(mChild, childWorldScale);
        }
    }
}
