#include "char/CharIKFoot.h"
#include "CharIKHand.h"
#include "char/Character.h"
#include "math/Mtx.h"
#include "math/Vec.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "rndobj/Trans.h"

CharIKFoot::CharIKFoot() : mFootBone(this), mFootFsmState(0), mData(this), mDataIndex(0) {
    mFootBone = Hmx::Object::New<RndTransformable>();
    mFootBone->DirtyLocalXfm().Reset();
}

CharIKFoot::~CharIKFoot() { delete mFootBone; }

BEGIN_HANDLERS(CharIKFoot)
    HANDLE_SUPERCLASS(CharIKHand)
END_HANDLERS

BEGIN_PROPSYNCS(CharIKFoot)
    SYNC_PROP(data, mData)
    SYNC_PROP(data_index, mDataIndex)
    SYNC_SUPERCLASS(CharIKHand)
END_PROPSYNCS

BEGIN_SAVES(CharIKFoot)
    SAVE_REVS(6, 0)
    SAVE_SUPERCLASS(CharIKHand)
    bs << mData;
    bs << mDataIndex;
END_SAVES

BEGIN_COPYS(CharIKFoot)
    COPY_SUPERCLASS(CharIKHand)
    CREATE_COPY(CharIKFoot)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mData)
        COPY_MEMBER(mDataIndex)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(6, 0)

BEGIN_LOADS(CharIKFoot)
    LOAD_REVS(bs)
    ASSERT_REVS(6, 0)
    LOAD_SUPERCLASS(CharIKHand)
    if (d.rev < 6) {
        Symbol s;
        d >> s;
    }
    if (d.rev < 5) {
        int i;
        if (d.rev > 1)
            d >> i;
        if (d.rev > 2)
            d >> i;
        if (d.rev > 3)
            d >> i;
    } else {
        d >> mData;
        d >> mDataIndex;
    }
END_LOADS

void CharIKFoot::Enter() {
    mFootFsmState = 0;
    mFootBlendTime = 0.0f;
}

void CharIKFoot::PollDeps(std::list<Hmx::Object *> &l1, std::list<Hmx::Object *> &l2) {
    CharIKHand::PollDeps(l1, l2);
}

void CharIKFoot::Poll() {
#ifdef HX_NATIVE
    {
        static int sCharIKFootPollLog = 0;
        if (sCharIKFootPollLog < 5) {
            sCharIKFootPollLog++;
            fprintf(stderr,
                "DC3_IK_DIAG CharIKFootPoll[%d]: path=%s mFinger=%p mHand=%p "
                "mData=%p mFootBone=%p\n",
                sCharIKFootPollLog,
                PathName(this),
                (void*)mFinger.Ptr(), (void*)mHand.Ptr(),
                (void*)mData.Ptr(), (void*)mFootBone.Ptr());
        }
    }
#endif
    if (mFinger && mHand && mData) {
        mTargets.clear();
        mTargets.push_back(IKTarget(mFootBone, 0));
        DoFSM(Character::Current(), mFootBone->DirtyLocalXfm());
        CharIKHand::Poll();
        mTargets.clear();
    }
}

void CharIKFoot::DoFSM(Character *mMe, Transform &tf) {
    mFootTransform = mFinger->WorldXfm();
    if (mMe && mMe->Teleported())
        mFootFsmState = 0;
    float deltasecs = TheTaskMgr.DeltaSeconds();
    if (deltasecs < 0.0f)
        deltasecs = 0.0f;
    tf.m = mFinger->WorldXfm().m;
    tf.v.z = mFinger->WorldXfm().v.z;
    mFootPosition.z = tf.v.z;
    float f10;
    bool b2 = false;
    float vecat = mData->LocalXfm().v[mDataIndex];
    if (!(vecat < 1.0f)) {
        b2 = true;
    } else {
        if (vecat <= 0.0f) {
            ;
        } else {
            if (mFootFsmState == 1) {
                f10 = 0.6f;
            } else {
                f10 = 0.5f;
            }
            if (tf.v.z < f10) {
                b2 = true;
            }
        }
    }
    if (mFootFsmState == 0) {
        const Transform &wt = mFinger->WorldXfm();
        tf.v.x = wt.v.x;
        tf.v.y = wt.v.y;
        if (b2) {
            mFootPosition = tf.v;
            mFootFsmState = 1;
        }
    }
    if (mFootFsmState == 1) {
        if (!b2) {
            mFootFsmState = 2;
            mFootBlendTime = Distance(mFinger->WorldXfm().v, tf.v);
        } else {
            Vector3 v3c;
            Subtract(mFinger->WorldXfm().v, mFootPosition, v3c);
            float len = Length(v3c);
            if (len > 0.125f)
                v3c *= 0.125f / len;
            Add(mFootPosition, v3c, tf.v);
            return;
        }
    }
    if (mFootFsmState == 2) {
        Vector3 delta;
        Subtract(mFinger->WorldXfm().v, mFootPosition, delta);
        float len = Length(delta);
        mFootBlendTime = Min(-(deltasecs * 25.0f - mFootBlendTime), len);
        if (mFootBlendTime <= 0.0f)
            mFootFsmState = 0;
        else
            delta *= (len - mFootBlendTime) / len;
        Add(mFootPosition, delta, tf.v);
        if (b2) {
            mFootPosition = tf.v;
            mFootFsmState = 1;
        }
    }
}
