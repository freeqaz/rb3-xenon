#include "char/CharUpperTwist.h"
#include "math/Rot.h"
#include "obj/Object.h"

void NormalizeAboutX(Hmx::Matrix3 &m) {
    Cross(m.x, m.y, m.z);
    Normalize(m.z, m.z);
    Cross(m.z, m.x, m.y);
}

CharUpperTwist::CharUpperTwist() : mTwist1(this), mTwist2(this), mUpperArm(this) {}
CharUpperTwist::~CharUpperTwist() {}

BEGIN_HANDLERS(CharUpperTwist)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(CharUpperTwist)
    SYNC_PROP(upper_arm, mUpperArm)
    SYNC_PROP(twist1, mTwist1)
    SYNC_PROP(twist2, mTwist2)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(CharUpperTwist)
    SAVE_REVS(1, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mUpperArm;
    bs << mTwist1;
    bs << mTwist2;
END_SAVES

BEGIN_COPYS(CharUpperTwist)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(CharUpperTwist)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mUpperArm)
        COPY_MEMBER(mTwist1)
        COPY_MEMBER(mTwist2)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(1, 0)

BEGIN_LOADS(CharUpperTwist)
    LOAD_REVS(bs)
    ASSERT_REVS(1, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    d >> mUpperArm;
    d >> mTwist1;
    d >> mTwist2;
END_LOADS

void CharUpperTwist::Poll() {
    if (!mUpperArm || !mTwist2 || !mTwist1)
        return;
    const Transform &upperparentworld = mUpperArm->TransParent()->WorldXfm();
    const Transform &upperworld = mUpperArm->WorldXfm();
    Hmx::Quat q;
    MakeRotQuat(upperparentworld.m.x, upperworld.m.x, q);
    Vector3 v68;
    Multiply(upperparentworld.m.y, q, v68);
    Transform tf48;
    tf48.m.x = upperworld.m.x;
    tf48.v = mTwist1->WorldXfm().v;
    Interp(v68, upperworld.m.y, 0.333f, tf48.m.y);
    NormalizeAboutX(tf48.m);
    mTwist1->SetWorldXfm(tf48);
#ifdef HX_NATIVE
    // Back-compute mLocalXfm so it survives dirty cascades (same as CharForeTwist fix).
    if (mTwist1->TransParent()) {
        Transform invParent;
        Invert(mTwist1->TransParent()->WorldXfm(), invParent);
        Multiply(tf48, invParent, mTwist1->mLocalXfm);
    }
#endif
    tf48.v = mTwist2->WorldXfm().v;
    Interp(v68, upperworld.m.y, 0.666f, tf48.m.y);
    NormalizeAboutX(tf48.m);
    mTwist2->SetWorldXfm(tf48);
#ifdef HX_NATIVE
    if (mTwist2->TransParent()) {
        Transform invParent;
        Invert(mTwist2->TransParent()->WorldXfm(), invParent);
        Multiply(tf48, invParent, mTwist2->mLocalXfm);
    }
#endif
}

void CharUpperTwist::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    changedBy.push_back(mUpperArm);
    change.push_back(mTwist1);
    change.push_back(mTwist2);
}
