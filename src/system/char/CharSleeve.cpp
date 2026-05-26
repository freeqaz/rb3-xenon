#include "char/CharSleeve.h"
#include "char/Character.h"
#include "math/Utl.h"
#include "math/Vec.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "rndobj/Rnd.h"
#include "rndobj/Utl.h"

void NormalizeScale(const Vector3 &, float, Vector3 &);

CharSleeve::CharSleeve()
    : mSleeve(this), mTopSleeve(this), mPos(0, 0, 0), mLastPos(0, 0, 0), mLastDT(0),
      mInertia(0.5f), mGravity(1.0f), mRange(0), mNegLength(0), mPosLength(0),
      mStiffness(0.02f) {}

CharSleeve::~CharSleeve() {}

BEGIN_PROPSYNCS(CharSleeve)
    SYNC_PROP(sleeve, mSleeve)
    SYNC_PROP(top_sleeve, mTopSleeve)
    SYNC_PROP(inertia, mInertia)
    SYNC_PROP(gravity, mGravity)
    SYNC_PROP(stiffness, mStiffness)
    SYNC_PROP(range, mRange)
    SYNC_PROP(neg_length, mNegLength)
    SYNC_PROP(pos_length, mPosLength)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(CharSleeve)
    SAVE_REVS(0, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mSleeve;
    bs << mTopSleeve;
    bs << mInertia;
    bs << mGravity;
    bs << mStiffness;
    bs << mRange;
    bs << mNegLength;
    bs << mPosLength;
END_SAVES

BEGIN_COPYS(CharSleeve)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(CharSleeve)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mSleeve)
        COPY_MEMBER(mTopSleeve)
        COPY_MEMBER(mInertia)
        COPY_MEMBER(mGravity)
        COPY_MEMBER(mStiffness)
        COPY_MEMBER(mRange)
        COPY_MEMBER(mNegLength)
        COPY_MEMBER(mPosLength)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(0, 0)

BEGIN_LOADS(CharSleeve)
    LOAD_REVS(bs)
    ASSERT_REVS(0, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    bs >> mSleeve >> mTopSleeve;
    bs >> mInertia >> mGravity >> mStiffness >> mRange >> mNegLength >> mPosLength;
END_LOADS

void CharSleeve::Poll() {
    auto _tmp1 = mSleeve->TransParent();
    if (mSleeve && _tmp1) {
        float deltasecs = TheTaskMgr.DeltaSeconds();
        float dvar12 = deltasecs * 60.0f;
        float gravity_z = (mGravity * (deltasecs * (dvar12 * -3.858268f)));
        auto _tmp0 = powf(1.0f - mStiffness, dvar12 * dvar12);
        RndTransformable *sleeveparent = mSleeve->TransParent();
        float absed = fabsf(mSleeve->LocalXfm().v.z);
        float powed = 1.0f - _tmp0;
        bool b2 = false;
        Character *me = Character::Current();
        if (me && me->Teleported()) {
            mPos = mSleeve->WorldXfm().v;
            Vector3 v9c(0.0f, 0.0f, -(absed + mPosLength));
            float dotted = Dot(v9c, sleeveparent->WorldXfm().m.x);
            ClampEq(dotted, -mRange, mRange);
            ScaleAddEq(v9c, sleeveparent->WorldXfm().m.x, dotted);
            mPos += v9c;
            Vector3 va8;
            ScaleAdd(sleeveparent->WorldXfm().v, sleeveparent->WorldXfm().m.x, dotted, va8);
            Subtract(mPos, va8, v9c);
            NormalizeScale(v9c, absed + mPosLength, v9c);
            Add(va8, v9c, mPos);
            mLastPos = mPos;
            b2 = true;
            mLastDT = 0;
        }
        Vector3 vb4(mPos);
        if (mLastDT > 0.0f && deltasecs > 0.0f) {
            Vector3 vc0;
            Subtract(mPos, mLastPos, vc0);
            ScaleAddEq(vb4, vc0, (mInertia / mLastDT) * deltasecs);
        }
        vb4.z += gravity_z;
        Vector3 vcc;
        Subtract(vb4, sleeveparent->WorldXfm().v, vcc);
        float dotted2 = Dot(vcc, sleeveparent->WorldXfm().m.x);
        float d4 = (1.0f - powed) * dotted2;
        ClampEq(d4, -mRange, mRange);
        ScaleAddEq(vcc, sleeveparent->WorldXfm().m.x, (d4 - dotted2));
        float len = Length(vcc);
        float interped = (absed - len) * powed + len;
        ClampEq(interped, absed - mNegLength, absed + mPosLength);
        NormalizeScale(vcc, interped, vcc);
        Add(sleeveparent->WorldXfm().v, vcc, vb4);
        Transform tf90;
        tf90.v = vb4;
        Scale(vcc, -1.0f, tf90.m.z);
        Cross(tf90.m.z, sleeveparent->WorldXfm().m.x, tf90.m.y);
        Normalize(tf90.m.z, tf90.m.z);
        Normalize(tf90.m.y, tf90.m.y);
        Cross(tf90.m.y, tf90.m.z, tf90.m.x);
        mSleeve->SetWorldXfm(tf90);
        mLastPos = mPos;
        mLastDT = deltasecs;
        mPos = vb4;
        if (b2)
            mLastPos = mPos;
        if (mTopSleeve) {
            float dotcc = Dot(vcc, sleeveparent->WorldXfm().m.x);
            ScaleAddEq(vcc, sleeveparent->WorldXfm().m.x, -dotcc);
            Add(sleeveparent->WorldXfm().v, vcc, tf90.v);
            Scale(vcc, -1.0f, tf90.m.z);
            Cross(tf90.m.z, sleeveparent->WorldXfm().m.x, tf90.m.y);
            Normalize(tf90.m.z, tf90.m.z);
            Normalize(tf90.m.y, tf90.m.y);
            Cross(tf90.m.y, tf90.m.z, tf90.m.x);
            mTopSleeve->SetWorldXfm(tf90);
        }
    }
}

void CharSleeve::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    if (mSleeve) {
        changedBy.push_back(mSleeve->TransParent());
        change.push_back(mSleeve);
        change.push_back(mTopSleeve);
    }
}

void CharSleeve::Highlight() {
    if (!mSleeve || !mSleeve->TransParent())
        return;
    UtilDrawAxes(mSleeve->WorldXfm(), 1.0f, Hmx::Color(0.0f, 1.0f, 0.0f));
    TheRnd.DrawLine(
        mSleeve->WorldXfm().v,
        mSleeve->TransParent()->WorldXfm().v,
        Hmx::Color(0.0f, 1.0f, 0.0f),
        false
    );
    if (mTopSleeve) {
        UtilDrawAxes(mTopSleeve->WorldXfm(), 1.0f, Hmx::Color(0.0f, 1.0f, 1.0f));
        TheRnd.DrawLine(
            mTopSleeve->WorldXfm().v,
            mTopSleeve->TransParent()->WorldXfm().v,
            Hmx::Color(0.0f, 1.0f, 1.0f),
            false
        );
    }
}

BEGIN_HANDLERS(CharSleeve)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS
