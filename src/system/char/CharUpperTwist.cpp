#include "char/CharUpperTwist.h"
#include "math/Rot.h"
#include "obj/Object.h"

void NormalizeAboutX(Hmx::Matrix3 &m) {
    Cross(m.x, m.y, m.z);
    Normalize(m.z, m.z);
    Cross(m.z, m.x, m.y);
}

CharUpperTwist::CharUpperTwist() : mUpperArm(this), mTwist1(this), mTwist2(this) {}
CharUpperTwist::~CharUpperTwist() {}

BEGIN_HANDLERS(CharUpperTwist)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(CharUpperTwist)
    SYNC_PROP(upper_arm, mUpperArm)
    SYNC_PROP(twist1, mTwist1)
    SYNC_PROP(twist2, mTwist2)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(CharUpperTwist)
    SAVE_REVS(1, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mTwist2;
    bs << mUpperArm;
    bs << mTwist1;
END_SAVES

BEGIN_COPYS(CharUpperTwist)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(CharUpperTwist)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mTwist2)
        COPY_MEMBER(mUpperArm)
        COPY_MEMBER(mTwist1)
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
    if (!mTwist2 || !mTwist1 || !mUpperArm)
        return;
    const Transform &twist2parentworld = mTwist2->TransParent()->WorldXfm();
    const Transform &twist2world = mTwist2->WorldXfm();
    Hmx::Quat q;
    MakeRotQuat(twist2parentworld.m.x, twist2world.m.x, q);
    Vector3 v68;
    Multiply(twist2parentworld.m.y, q, v68);
    Transform tf48;
    tf48.m.x = twist2world.m.x;
    tf48.v = mUpperArm->WorldXfm().v;
    Interp(v68, twist2world.m.y, 0.333f, tf48.m.y);
    NormalizeAboutX(tf48.m);
    mUpperArm->SetWorldXfm(tf48);
#ifdef HX_NATIVE
    // Back-compute mLocalXfm so it survives dirty cascades (same as CharForeTwist fix).
    if (mUpperArm->TransParent()) {
        Transform invParent;
        Invert(mUpperArm->TransParent()->WorldXfm(), invParent);
        Multiply(tf48, invParent, mUpperArm->mLocalXfm);
    }
#endif
    tf48.v = mTwist1->WorldXfm().v;
    Interp(v68, twist2world.m.y, 0.666f, tf48.m.y);
    NormalizeAboutX(tf48.m);
    mTwist1->SetWorldXfm(tf48);
#ifdef HX_NATIVE
    if (mTwist1->TransParent()) {
        Transform invParent;
        Invert(mTwist1->TransParent()->WorldXfm(), invParent);
        Multiply(tf48, invParent, mTwist1->mLocalXfm);
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
