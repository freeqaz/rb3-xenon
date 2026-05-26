#include "char/CharBlendBone.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "math/Rot.h"
#include "math/Utl.h"
#include <cstring>

#pragma region CharBlendBone

CharBlendBone::CharBlendBone()
    : mTargets(this), mSrc1(this), mSrc2(this), mTransX(false), mTransY(false),
      mTransZ(false), mRotation(false), mSetLocal(false) {}

BEGIN_PROPSYNCS(CharBlendBone)
    SYNC_PROP(target, mTargets)
    SYNC_PROP(src_one, mSrc1)
    SYNC_PROP(src_two, mSrc2)
    SYNC_PROP(trans_x, mTransX)
    SYNC_PROP(trans_y, mTransY)
    SYNC_PROP(trans_z, mTransZ)
    SYNC_PROP(rotation, mRotation)
    SYNC_PROP(set_local, mSetLocal)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(CharBlendBone)
    SAVE_REVS(4, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mTargets;
    bs << mSrc1;
    bs << mSrc2;
    bs << mTransX;
    bs << mTransY;
    bs << mTransZ;
    bs << mRotation;
    bs << mSetLocal;
END_SAVES

BEGIN_COPYS(CharBlendBone)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(CharBlendBone)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mTargets)
        COPY_MEMBER(mSrc1)
        COPY_MEMBER(mSrc2)
        COPY_MEMBER(mTransX)
        COPY_MEMBER(mTransY)
        COPY_MEMBER(mTransZ)
        COPY_MEMBER(mRotation)
        COPY_MEMBER(mSetLocal)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(4, 0)

BEGIN_LOADS(CharBlendBone)
    LOAD_REVS(bs)
    ASSERT_REVS(4, 0)
    MILO_ASSERT(d.rev > 2, 0x66);
    LOAD_SUPERCLASS(Hmx::Object)
    d >> mTargets;
    d >> mSrc1;
    d >> mSrc2;
    d >> mTransX;
    d >> mTransY;
    d >> mTransZ;
    d >> mRotation;
    if (3 < d.rev) {
        d >> mSetLocal;
    }
END_LOADS

void CharBlendBone::Poll() {
    for (ObjVector<ConstraintSystem>::iterator it = mTargets.begin(); it != mTargets.end();
         ++it) {
        RndTransformable *target = it->mTarget;
        if (target && mSrc1 && mSrc2) {
            const Transform &xfm1 = mSrc1->WorldXfm();
            const Transform &xfm2 = mSrc2->WorldXfm();
            Transform tf48(target->WorldXfm());
            if (mTransX || mTransY || mTransZ) {
                if (mTransX) {
                    Interp(xfm1.v.x, xfm2.v.x, it->mWeight, tf48.v.x);
                }
                if (mTransY) {
                    Interp(xfm1.v.y, xfm2.v.y, it->mWeight, tf48.v.y);
                }
                if (mTransZ) {
                    Interp(xfm1.v.z, xfm2.v.z, it->mWeight, tf48.v.z);
                }
            }
            if (mRotation) {
                Interp(xfm1.m, xfm2.m, it->mWeight, tf48.m);
            }
            if (mSetLocal) {
                RndTransformable *parent = target->TransParent();
                if (parent) {
                    Transform inverted;
                    Invert(parent->WorldXfm(), inverted);
                    Multiply(tf48, inverted, target->DirtyLocalXfm());
                } else {
                    target->SetLocalXfm(tf48);
                }
            } else {
                target->SetWorldXfm(tf48);
            }
        }
    }
}

void CharBlendBone::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    changedBy.push_back(mSrc1);
    changedBy.push_back(mSrc2);
    for (ObjVector<ConstraintSystem>::iterator it = mTargets.begin();
         it != mTargets.end();
         ++it) {
        change.push_back((*it).mTarget);
    }
}

BEGIN_HANDLERS(CharBlendBone)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

#pragma endregion CharBlendBone
#pragma region CharBlendBone::ConstraintSystem

CharBlendBone::ConstraintSystem::ConstraintSystem(Hmx::Object *o)
    : mTarget(o), mWeight(0.5f) {}

BinStream &operator>>(BinStream &bs, CharBlendBone::ConstraintSystem &cs) {
    bs >> cs.mTarget;
    bs >> cs.mWeight;
    return bs;
}

BinStream &operator>>(BinStreamRev &bsrev, ObjVector<CharBlendBone::ConstraintSystem> &vec) {
    BinStream &bs = bsrev.stream;
    int count;
    bs.ReadEndian(&count, 4);
    vec.resize(count);

    CharBlendBone::ConstraintSystem *cs = vec.begin();
    while (cs != vec.end()) {
        bs >> *cs;
        cs++;
    }

    return bs;
}

BEGIN_CUSTOM_PROPSYNC(CharBlendBone::ConstraintSystem)
    SYNC_PROP(target, o.mTarget)
    SYNC_PROP(weight, o.mWeight)
END_CUSTOM_PROPSYNC

#pragma endregion CharBlendBone::ConstraintSystem
