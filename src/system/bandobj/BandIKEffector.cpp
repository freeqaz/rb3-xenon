// [NCCC f381] Retail inlines the owner-only ObjPtr ctor at all five sites in
// ??0BandIKEffector@@'s member-init list (mEffector, mGround, mMore, mElbow,
// unk64): no `bl ??0?$ObjPtr@...` calls, just three stores (mOwner/mObject/
// vtable) per member, each followed by a live `stw &mFoo, 0x50(r31)` EH state
// store -- the signature of the _EH variant's preserved cleanup region (see
// Object.h's RB3_OBJPTR_INLINE_OWNER_CTOR_EH comment; same pattern as
// rndobj/MultiMeshProxy.cpp). Must precede the first #include of obj/Object.h.
#define RB3_OBJPTR_INLINE_OWNER_CTOR_EH 1
#ifndef HX_NATIVE
// Suppresses Mtx.h's global Matrix3 Multiply for the MWCC paired-singles local
// version; on native there's no asm local, so use the global Multiply (Rot.cpp).
#define CHARHAIR_LOCAL_MULTIPLY
#endif
#include "bandobj/BandIKEffector.h"
#include "char/CharBones.h"
#include "char/CharClip.h"
#include "char/CharUtl.h"
#include "math/Mtx.h"
#include "math/Rot.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "utl/BinStream.h"
#include "utl/Symbols.h"
#include <string.h>

#ifdef __MWERKS__
inline void Multiply(const Hmx::Matrix3 &a, const Hmx::Matrix3 &b, Hmx::Matrix3 &out) {
    typedef __vec2x32float__ psq;
    register const Hmx::Matrix3 *_a = &a;
    register const Hmx::Matrix3 *_b = &b;
    register Hmx::Matrix3 *_out = &out;
    float row2[3], row1[3], row0[3];
    register psq _f0, _f1, _f2, _f3, _f4, _f5, _f6, _f7, _f8, _f9, _f10, _f11, _f12;
    asm { cmplw _b, _out }
    asm volatile {
        beq alias_path
        // non-alias path
        psq_l  _f4, 0x4(_a),  0, 0
        psq_l  _f3, 0x18(_b), 0, 0
        psq_l  _f2, 0x20(_b), 1, 0
        ps_muls1 _f1, _f3, _f4
        psq_l  _f3, 0xc(_b),  0, 0
        ps_muls1 _f0, _f2, _f4
        psq_l  _f2, 0x14(_b), 1, 0
        psq_l  _f9, 0x10(_a), 0, 0
        psq_l  _f8, 0x18(_b), 0, 0
        psq_l  _f7, 0x20(_b), 1, 0
        ps_madds0 _f1, _f3, _f4, _f1
        ps_madds0 _f0, _f2, _f4, _f0
        psq_l  _f4, 0x0(_a),  0, 0
        ps_muls1 _f6, _f8, _f9
        psq_l  _f3, 0x0(_b),  0, 0
        ps_muls1 _f5, _f7, _f9
        ps_madds0 _f1, _f3, _f4, _f1
        psq_l  _f2, 0x8(_b),  1, 0
        psq_l  _f8, 0xc(_b),  0, 0
        ps_madds0 _f0, _f2, _f4, _f0
        psq_l  _f7, 0x14(_b), 1, 0
        ps_madds0 _f6, _f8, _f9, _f6
        psq_l  _f2, 0xc(_a),  0, 0
        ps_madds0 _f5, _f7, _f9, _f5
        psq_l  _f4, 0x1c(_a), 0, 0
        psq_l  _f7, 0x1c(_b), 0, 0
        psq_l  _f3, 0x18(_b), 0, 0
        ps_madds0 _f6, _f1, _f2, _f6
        ps_madds0 _f5, _f0, _f2, _f5
        psq_l  _f8, 0x20(_b), 1, 0
        ps_muls1 _f3, _f3, _f7
        psq_l  _f9, 0x18(_a), 0, 0
        ps_muls1 _f2, _f8, _f7
        psq_st _f1, 0x0(_out), 0, 0
        ps_madds0 _f6, _f3, _f9, _f6
        ps_madds0 _f5, _f2, _f9, _f5
        psq_st _f0, 0x8(_out), 1, 0
        ps_madds0 _f3, _f1, _f4, _f3
        psq_st _f6, 0xc(_out), 0, 0
        ps_madds0 _f2, _f0, _f4, _f2
        psq_st _f5, 0x14(_out), 1, 0
        psq_st _f3, 0x18(_out), 0, 0
        psq_st _f2, 0x20(_out), 1, 0
        b mult_end
    alias_path:
        psq_l  _f4, 0x4(_a),  0, 0
        la r7, row2
        psq_l  _f3, 0x18(_out), 0, 0
        la r6, row1
        psq_l  _f2, 0x20(_out), 1, 0
        la r5, row0
        ps_muls1 _f1, _f3, _f4
        psq_l  _f3, 0xc(_out), 0, 0
        ps_muls1 _f0, _f2, _f4
        psq_l  _f2, 0x14(_out), 1, 0
        psq_l  _f9, 0x10(_a),  0, 0
        psq_l  _f8, 0x18(_out), 0, 0
        psq_l  _f7, 0x20(_out), 1, 0
        ps_madds0 _f1, _f3, _f4, _f1
        ps_muls1 _f6, _f8, _f9
        psq_l  _f12, 0x1c(_a), 0, 0
        ps_mr  _f8, _f3
        psq_l  _f3, 0x18(_out), 0, 0
        ps_muls1 _f5, _f7, _f9
        ps_muls1 _f11, _f3, _f12
        ps_mr  _f7, _f2
        psq_l  _f3, 0x0(_out), 0, 0
        ps_madds0 _f0, _f2, _f4, _f0
        psq_l  _f2, 0x20(_out), 1, 0
        psq_l  _f4, 0x0(_a),  0, 0
        ps_muls1 _f10, _f2, _f12
        psq_l  _f2, 0x8(_out), 1, 0
        ps_madds0 _f1, _f3, _f4, _f1
        ps_madds0 _f6, _f8, _f9, _f6
        ps_madds0 _f0, _f2, _f4, _f0
        psq_l  _f4, 0x18(_a), 0, 0
        ps_madds0 _f5, _f7, _f9, _f5
        psq_l  _f9, 0xc(_a),  0, 0
        ps_madds0 _f11, _f8, _f12, _f11
        ps_madds0 _f10, _f7, _f12, _f10
        psq_st _f1, 0x0(r7), 0, 0
        ps_madds0 _f6, _f3, _f9, _f6
        ps_madds0 _f5, _f2, _f9, _f5
        ps_madds0 _f11, _f3, _f4, _f11
        lfs    _f8, 0x0(r7)
        ps_madds0 _f10, _f2, _f4, _f10
        psq_st _f6, 0x0(r6), 0, 0
        lfs    _f7, 0x4(r7)
        psq_st _f11, 0x0(r5), 0, 0
        lfs    _f4, 0x4(r6)
        psq_st _f5, 0x8(r6), 1, 0
        lfs    _f5, 0x0(r6)
        psq_st _f0, 0x8(r7), 1, 0
        lfs    _f3, 0x8(r6)
        psq_st _f10, 0x8(r5), 1, 0
        lfs    _f6, 0x8(r7)
        lfs    _f2, 0x0(r5)
        lfs    _f1, 0x4(r5)
        lfs    _f0, 0x8(r5)
        stfs   _f8, 0x0(_out)
        stfs   _f7, 0x4(_out)
        stfs   _f6, 0x8(_out)
        stfs   _f5, 0xc(_out)
        stfs   _f4, 0x10(_out)
        stfs   _f3, 0x14(_out)
        stfs   _f2, 0x18(_out)
        stfs   _f1, 0x1c(_out)
        stfs   _f0, 0x20(_out)
    mult_end:
    }
}
#endif

// Retail addresses both rev statics off ONE base register (+0 gAltRev, +4 gRev),
// which needs an INTERNAL-linkage adjacent pair rather than DECLARE_REVS's class
// statics (same as LayerDir.cpp). Explicit `= 0` keeps them out of .bss.
static unsigned short gAltRev = 0;
static unsigned short gRev = 0;
CharClip *BandIKEffector::sDeformClip;

BandIKEffector::Constraint::Constraint(Hmx::Object *o)
    : mTarget(o, 0), mFinger(o, 0), mWeight(1.0f) {}

BandIKEffector::Constraint::Constraint(const BandIKEffector::Constraint &c)
    : mTarget(c.mTarget), mFinger(c.mFinger), mWeight(c.mWeight) {}

BandIKEffector::Constraint &
BandIKEffector::Constraint::operator=(const BandIKEffector::Constraint &c) {
    mTarget = c.mTarget;
    mFinger = c.mFinger;
    mWeight = c.mWeight;
    return *this;
}

BandIKEffector::BandIKEffector()
    : mEffector(this, 0), mGround(this, 0), mMore(this, 0), mElbow(this, 0),
      mConstraints(this), unk64(this, 0) {}

BandIKEffector::~BandIKEffector() {}

void BandIKEffector::SetName(const char *cc, ObjectDir *dir) {
    Hmx::Object::SetName(cc, dir);
    unk64 = dynamic_cast<BandCharacter *>(dir);
}

void BandIKEffector::SetDeformClip(Hmx::Object *o) {
    static Symbol bc("BandCharacter");
    Symbol name = o->ClassName();
    if (name == bc) {
        sDeformClip =
            BandCharDesc::GetDeformClip(dynamic_cast<BandCharacter *>(o)->mGender);
    } else
        sDeformClip = 0;
}

bool BandIKEffector::MeasureLengths(
    RndTransformable *&handBone,
    RndTransformable *&elbowBone,
    float &inv2ab,
    float &aaPlusbb,
    float &aPlusb
) {
    handBone = mEffector->TransParent();
    if (!handBone)
        return false;
    elbowBone = handBone->TransParent();
    if (!elbowBone)
        return false;
    float a = mEffector->mLocalXfm.v.x;
    float b = handBone->mLocalXfm.v.x;
    aPlusb = a + b;
    aaPlusbb = a * a + b * b;
    inv2ab = 1.0f / (2.0f * a * b);
    return true;
}

void BandIKEffector::NeutralLocalPos(RndTransformable *bone, Vector3 &pos) {
    if (sDeformClip) {
        bool pelvisMatch = (strcmp(bone->Name(), "bone_pelvis.mesh") == 0);
        if (!pelvisMatch) {
            Symbol sym = CharBones::ChannelName(bone->Name(), CharBones::TYPE_POS);
            void *chan = sDeformClip->GetChannel(sym);
            if (chan) {
                sDeformClip->EvaluateChannel(&pos, chan, 0.0f);
                return;
            }
        }
    }
    pos = bone->mLocalXfm.v;
}

void BandIKEffector::NeutralLocalXfm(RndTransformable *bone, Transform &tf) {
    tf = bone->mLocalXfm;
    if (sDeformClip) {
        bool pelvisMatch = (strcmp(bone->Name(), "bone_pelvis.mesh") == 0);
        if (!pelvisMatch) {
            Symbol channel = CharBones::ChannelName(bone->Name(), CharBones::TYPE_POS);
            void *posChan = sDeformClip->GetChannel(channel);
            if (posChan)
                sDeformClip->EvaluateChannel(&tf.v, posChan, 0.0f);
            channel = CharBones::ChannelName(bone->Name(), CharBones::TYPE_SCALE);
            void *scaleChan = sDeformClip->GetChannel(channel);
            if (scaleChan) {
                Vector3 targetScale;
                sDeformClip->EvaluateChannel(&targetScale, scaleChan, 0.0f);
                Vector3 currScale;
                MakeScale(tf.m, currScale);
                tf.m.x *= targetScale.x / currScale.x;
                tf.m.y *= targetScale.y / currScale.y;
                tf.m.z *= targetScale.z / currScale.z;
            }
        }
    }
}

void BandIKEffector::NeutralWorldXfm(RndTransformable *trans, Transform &tf) {
    RndTransformable *parent = trans->TransParent();
    // retail tests the parent with a signed compare (cmpwi cr6,r3,0)
    if ((int)parent == 0) {
        SetDeformClip(trans);
        NeutralLocalXfm(trans, tf);
    } else {
        Transform tf38;
        NeutralWorldXfm(parent, tf);
        NeutralLocalXfm(trans, tf38);
        Multiply(tf38, tf, tf);
    }
}

void BandIKEffector::Highlight() {}

BinStream &operator>>(BinStream &bs, BandIKEffector::Constraint &c) {
    bs >> c.mTarget;
    bs >> c.mFinger;
    if (gRev > 2)
        bs >> c.mWeight;
    return bs;
}

BinStream &operator<<(BinStream &bs, const BandIKEffector::Constraint &c) {
    bs << c.mTarget;
    bs << c.mFinger;
    bs << c.mWeight;
    return bs;
}

BEGIN_SAVES(BandIKEffector)
    SAVE_REVS(4, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(CharWeightable)
    bs << mEffector;
    bs << mMore;
    bs << mElbow;
    bs << mConstraints;
    bs << mGround;
END_SAVES

BEGIN_LOADS(BandIKEffector)
    LOAD_REVS(bs)
    ASSERT_REVS(4, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(CharWeightable)
    bs >> mEffector;
    bs >> mMore;
    if (gRev > 1)
        bs >> mElbow;
    if (gRev < 1) {
        int i;
        bs >> i;
    }
    bs >> mConstraints;
    if (gRev > 3)
        bs >> mGround;
END_LOADS

BEGIN_COPYS(BandIKEffector)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(CharWeightable)
    CREATE_COPY(BandIKEffector)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mEffector)
        COPY_MEMBER(mMore)
        COPY_MEMBER(mElbow)
        COPY_MEMBER(mConstraints)
        COPY_MEMBER(mGround)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_HANDLERS(BandIKEffector)
    HANDLE_SUPERCLASS(CharWeightable)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x388)
END_HANDLERS

BEGIN_CUSTOM_PROPSYNC(BandIKEffector::Constraint)
    SYNC_PROP(target, o.mTarget)
    SYNC_PROP(finger, o.mFinger)
    SYNC_PROP(weight, o.mWeight)
END_CUSTOM_PROPSYNC

void BandIKEffector::ComputeElbowPullAndQuat(
    QuatXfm &outQuat, const Transform &shoulderXfm, const Vector3 &elbowTarget
) {
    Vector3 localElbow;
    MultiplyTranspose(shoulderXfm, elbowTarget, localElbow);

    const Vector3 &armVec = mEffector->TransParent()->mLocalXfm.v;
    MakeRotQuat(armVec, localElbow, outQuat.q);

    float dy = elbowTarget.y - shoulderXfm.v.y;
    float armLen = armVec.x;
    float dx = elbowTarget.x - shoulderXfm.v.x;
    outQuat.v.x = dx;
    float dz = elbowTarget.z - shoulderXfm.v.z;
    float len = (float)sqrt(dy * dy + outQuat.v.x * outQuat.v.x + dz * dz);
    float scale = 1.0f - armLen / len;
    outQuat.v.x *= scale;
    outQuat.v.y = dy * scale;
    outQuat.v.z = dz * scale;
}

void BandIKEffector::ComputeHandPullAndQuat(
    QuatXfm &outQuat,
    Transform &outElbowXfm,
    const Transform &shoulderXfm,
    const Vector3 &handTarget,
    float inv2ab,
    float aaPlusbb,
    float aPlusb
) {
    float dy = handTarget.y - shoulderXfm.v.y;
    float dx = handTarget.x - shoulderXfm.v.x;
    float maxReach = aPlusb * 0.99f;
    float dz = handTarget.z - shoulderXfm.v.z;
    outQuat.v.x = dx;
    outQuat.v.y = dy;
    outQuat.v.z = dz;
    float distSq = dz * dz + (dx * dx + dy * dy);
    float maxReachSq = maxReach * maxReach;

    if (distSq > maxReachSq && GetType() == 3) {
        float factor = 1.0f - maxReach / (float)sqrt(distSq);
        outQuat.v.x *= factor;
        outQuat.v.y *= factor;
        outQuat.v.z *= factor;
        distSq = maxReachSq;
    } else {
        outQuat.v.z = 0.0f;
        outQuat.v.y = 0.0f;
        outQuat.v.x = 0.0f;
    }

    float cosAngle = inv2ab * (distSq - aaPlusbb);
    if (cosAngle < -1.0f)
        cosAngle = -1.0f;
    else if (cosAngle > 1.0f)
        cosAngle = 1.0f;
    float cosSq = cosAngle * cosAngle;
    float sinAngle = -(float)sqrt(1.0f - cosSq);

    RndTransformable *parent = mEffector->TransParent();
    outElbowXfm.v = parent->mLocalXfm.v;
    outElbowXfm.m.Set(cosAngle, sinAngle, 0.0f, -sinAngle, cosAngle, 0.0f, 0.0f, 0.0f, 1.0f);

    Vector3 localDir;
    Multiply(mEffector->mLocalXfm.v, outElbowXfm, localDir);
    Vector3 localTarget;
    MultiplyTranspose(shoulderXfm, handTarget, localTarget);
    MakeRotQuat(localDir, localTarget, outQuat.q);
}

void BandIKEffector::DoFancyElbow(QuatXfm &hand, float handWeight) {
    RndTransformable *elbow;
    RndTransformable *shoulder;
    float inv2ab;
    float aaPlusbb;
    float aPlusb;
    if (!MeasureLengths(elbow, shoulder, inv2ab, aaPlusbb, aPlusb))
        return;
    Transform neutralElbow;
    NeutralWorldXfm(elbow, neutralElbow);
    Vector3 elbowDest;
    elbowDest.Zero();
    float elbowWeight = mElbow->ApplyPosConstraints(elbowDest, neutralElbow.v, this);
    float totalWeight = elbowWeight + handWeight;
    if (totalWeight == 0)
        return;
    QuatXfm accum;
    accum.v.Set(0, 0, 0);
    accum.q.Set(0, 0, 0, 0);
    // whatever weight the constraints leave unclaimed goes to the natural pose
    float naturalWeight = 0;
    if (totalWeight < 1) {
        naturalWeight = 1 - totalWeight;
        accum.q.w = naturalWeight;
        totalWeight = naturalWeight + totalWeight;
    }
    Transform worldShoulder = shoulder->WorldXfm();
    if (elbowWeight > 0) {
        elbowDest.Set(elbowDest.x / elbowWeight, elbowDest.y / elbowWeight, elbowDest.z / elbowWeight);
        QuatXfm elbowPull;
        ComputeElbowPullAndQuat(elbowPull, worldShoulder, elbowDest);
        ScaleAdd(accum.v, elbowPull.v, elbowWeight, accum.v);
        ScaleAddEq(accum.q, elbowPull.q, elbowWeight);
    }
    Transform handLocalElbow;
    if (handWeight > 0) {
        Vector3 handDest;
        Scale(hand.v, 1 / handWeight, handDest);
        QuatXfm handPull;
        ComputeHandPullAndQuat(
            handPull, handLocalElbow, worldShoulder, handDest, inv2ab, aaPlusbb, aPlusb
        );
        ScaleAdd(accum.v, handPull.v, handWeight, accum.v);
        ScaleAddEq(accum.q, handPull.q, handWeight);
    }
    Normalize(accum.q, accum.q);
    accum.v.Set(accum.v.x / totalWeight, accum.v.y / totalWeight, accum.v.z / totalWeight);
    Hmx::Matrix3 m;
    MakeRotMatrix(accum.q, m);
    Multiply(m, worldShoulder.m, worldShoulder.m);
    worldShoulder.v += accum.v;
    shoulder->SetWorldXfm(worldShoulder);
    if (handWeight > 0) {
        // blend the elbow (and then the hand) between their current
        // orientation and the one the hand pull solved for
        Hmx::Quat elbowQ;
        elbowQ.Set(elbow->LocalXfm().m);
        float keepWeight = naturalWeight + elbowWeight;
        elbowQ.Set(
            keepWeight * elbowQ.x,
            keepWeight * elbowQ.y,
            keepWeight * elbowQ.z,
            keepWeight * elbowQ.w
        );
        Hmx::Quat pulledQ;
        pulledQ.Set(handLocalElbow.m);
        ScaleAddEq(elbowQ, pulledQ, handWeight);
        Normalize(elbowQ, elbowQ);
        MakeRotMatrix(elbowQ, m);
        Transform elbowOut;
        elbowOut.v = elbow->WorldXfm().v;
        Multiply(m, worldShoulder.m, elbowOut.m);
        elbow->SetWorldXfm(elbowOut);
        const Transform &handWorld = mEffector->WorldXfm();
        Transform handOut;
        handOut.v = handWorld.v;
        Hmx::Quat handQ;
        handQ.Set(handWorld.m);
        ScaleAddEq(hand.q, handQ, keepWeight);
        Normalize(hand.q, hand.q);
        MakeRotMatrix(hand.q, handOut.m);
        mEffector->SetWorldXfm(handOut);
    }
}

float BandIKEffector::ApplyConstraints(
    QuatXfm &q, const Transform &tf, BandIKEffector *root
) {
    float totalWeight = 0.0f;
    for (int i = 0; i < mConstraints.size(); i++) {
        Constraint &c = mConstraints[i];
        if (c.mTarget) {
            if (c.mWeight <= 0.0f) {
                const Transform &world = c.mTarget->WorldXfm();
                q.v = world.v;
                q.q.Set(world.m);
                return 1.0f;
            }
            Transform neutral;
            NeutralWorldXfm(c.mTarget, neutral);
            Normalize(neutral.m, neutral.m);
            Transform tpose;
            Transpose(neutral, tpose);
            Transform local;
            Multiply(tf, tpose, local);
            float lensq = LengthSquared(local.v);
            float scaled = c.mWeight * 144.0f;
            float clamped = Max(0.001f, lensq);
            float w = scaled / clamped;
            totalWeight += w;
            Transform targetWorld = c.mTarget->WorldXfm();
            Normalize(targetWorld.m, targetWorld.m);
            Multiply(local, targetWorld, local);
            QuatXfm newQ(local);
            ScaleAdd(q.v, newQ.v, w, q.v);
            ScaleAddEq(q.q, newQ.q, w);
        }
    }
    if (mMore) {
        totalWeight += mMore->ApplyConstraints(q, tf, root);
    }
    return totalWeight;
}

float BandIKEffector::GetGroundHeight(RndTransformable *trans) {
    if (mGround) {
        return mGround->WorldXfm().v.z;
    } else if (mMore) {
        return mMore->GetGroundHeight(trans);
    } else {
        return trans->WorldXfm().v.z;
    }
}

void BandIKEffector::Poll() {
    int type = GetType();
    if (type == 4)
        return;
    float weight = Weight();
    RndTransformable *effector = mEffector;
    if (!(int)effector)
        return;
    if (weight != 0.0f) {
        Transform neutral;
        NeutralWorldXfm(effector, neutral);
        Normalize(neutral.m, neutral.m);

        QuatXfm neutralQ(neutral);

        QuatXfm q;
        q.v.x = 0.0f;
        q.v.y = 0.0f;
        q.v.z = 0.0f;
        q.q.x = 0.0f;
        q.q.y = 0.0f;
        q.q.z = 0.0f;
        q.q.w = 0.0f;
        float totalWeight = ApplyConstraints(q, neutral, this);

        if (type == 3 && mElbow) {
            DoFancyElbow(q, totalWeight);
            return;
        }

        if (weight != 1.0f) {
            MILO_ASSERT(weight == 1, 0x139);
        }

        Transform finalXfm;
        if (totalWeight < 1.0f) {
            if (!(totalWeight != 0.0f || type != 0)) {
                return;
            } else {
                const Transform &effWorld = mEffector->WorldXfm();
                QuatXfm effQ(effWorld);
                if (type == 2 || type == 1) {
                    RndTransformable *ground = unk64;
                    float groundHeight = GetGroundHeight(ground);
                    if (type == 1) {
                        RndTransformable *knee =
                            CharUtlFindBoneTrans("bone_L-knee", Dir());
                        RndTransformable *ankle =
                            CharUtlFindBoneTrans("bone_L-ankle", Dir());
                        if (knee && ankle) {
                            SetDeformClip(unk64);
                            Vector3 localPos;
                            NeutralLocalPos(ankle, localPos);
                            float ankleLen = localPos.x;
                            NeutralLocalPos(knee, localPos);
                            float kneeLen = localPos.x;
                            float lowerBound = kneeLen * 0.3f + ankleLen;
                            float worldHeight =
                                ankle->mLocalXfm.v.x + knee->mLocalXfm.v.x;
                            float heightDelta = effQ.v.z - groundHeight;
                            float ratio =
                                worldHeight / (kneeLen + ankleLen);
                            float blend = (heightDelta - lowerBound)
                                / ((kneeLen * 0.8f + ankleLen) - lowerBound);
                            if (blend < 0.0f)
                                blend = 0.0f;
                            else if (blend > 1.0f)
                                blend = 1.0f;
                            effQ.v.z = heightDelta
                                    * (blend * (ratio - 1.0f) + 1.0f)
                                + groundHeight;
                        }
                    } else if (type == 2) {
                        float blend =
                            ((neutralQ.v.z - groundHeight) - 5.0f) / 11.0f;
                        if (blend < 0.0f)
                            blend = 0.0f;
                        else if (blend > 1.0f)
                            blend = 1.0f;
                        Interp(neutralQ.v, effQ.v, blend, effQ.v);
                        Interp(neutralQ.q, effQ.q, blend, effQ.q);
                    }
                }
                float remaining = 1.0f - totalWeight;
                q.v.x += effQ.v.x * remaining;
                q.v.y += effQ.v.y * remaining;
                q.v.z += effQ.v.z * remaining;
                ScaleAddEq(q.q, effQ.q, remaining);
                totalWeight += remaining;
            }
        }

        float invWeight = 1.0f / totalWeight;
        q.v.x *= invWeight;
        q.v.y *= invWeight;
        q.v.z *= invWeight;
        Normalize(q.q, q.q);

        if (type == 2 || type == 3) {
            IKElbow(q.v);
        }

        finalXfm.v = q.v;
        MakeRotMatrix(q.q, finalXfm.m);
        mEffector->SetWorldXfm(finalXfm);
    }
}

int BandIKEffector::GetType() {
    if (!mEffector) {
        MILO_NOTIFY_ONCE("%s trying to get type with NULL effector", PathName(this));
        return 0;
    }
    // Retail re-reads the effector's name for every comparison.
    if (strncmp(mEffector->Name(), "bone_pelvis", 11) == 0)
        return 1;
    if (strncmp(mEffector->Name(), "bone_L-ankle", 12) == 0
        || strncmp(mEffector->Name(), "bone_R-ankle", 12) == 0)
        return 2;
    if (strncmp(mEffector->Name(), "bone_L-hand", 11) == 0
        || strncmp(mEffector->Name(), "bone_R-hand", 11) == 0)
        return 3;
    if (strncmp(mEffector->Name(), "bone_L-foreArm", 11) == 0
        || strncmp(mEffector->Name(), "bone_R-foreArm", 11) == 0)
        return 4;
    if (strncmp(mEffector->Name(), "bone_head", 9) == 0)
        return 5;
    return 0;
}

float BandIKEffector::ApplyPosConstraints(
    Vector3 &dst, const Vector3 &src, BandIKEffector *root
) {
    float totalWeight = 0.0f;
    for (int i = 0; i < mConstraints.size(); i++) {
        Constraint &c = mConstraints[i];
        if (c.mTarget) {
            Transform neutral;
            NeutralWorldXfm(c.mTarget, neutral);
            Normalize(neutral.m, neutral.m);
            Transform tpose;
            Transpose(neutral, tpose);
            Vector3 local;
            Multiply(src, tpose, local);
            float lensq = LengthSquared(local);
            Multiply(local, c.mTarget->WorldXfm(), local);
            float w = (c.mWeight * 144.0f) / Max(0.001f, lensq);
            ScaleAdd(dst, local, w, dst);
            totalWeight += w;
        }
    }
    if (mMore) {
        totalWeight += mMore->ApplyPosConstraints(dst, src, root);
    }
    return totalWeight;
}

void BandIKEffector::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    change.push_back(mEffector);
    changedBy.push_back(mEffector);
    for (ObjVector<Constraint>::iterator it = mConstraints.begin();
         it != mConstraints.end();
         ++it) {
        change.push_back(it->mFinger);
        changedBy.push_back(it->mFinger);
        changedBy.push_back(it->mTarget);
    }
    if (mMore) {
        for (ObjVector<Constraint>::iterator it = mMore->mConstraints.begin();
             it != mMore->mConstraints.end();
             ++it) {
            change.push_back(it->mFinger);
            changedBy.push_back(it->mFinger);
            changedBy.push_back(it->mTarget);
        }
    }
    int type = GetType();
    if (type == 2 || type == 3) {
        RndTransformable *parent = mEffector->TransParent();
        if (parent) {
            change.push_back(parent);
            changedBy.push_back(parent);
            RndTransformable *grandparent = parent->TransParent();
            if (grandparent) {
                change.push_back(grandparent);
                changedBy.push_back(grandparent);
            }
        }
    }
}

void BandIKEffector::IKElbow(const Vector3 &hand) {
    RndTransformable *elbow;
    RndTransformable *shoulder;
    float aaPlusbb;
    float inv2ab;
    float aPlusb;
    if (!MeasureLengths(elbow, shoulder, inv2ab, aaPlusbb, aPlusb))
        return;

    Transform shoulderXfm;
    shoulderXfm = shoulder->WorldXfm();

    QuatXfm quat;
    Transform elbowXfm;
    ComputeHandPullAndQuat(
        quat, elbowXfm, shoulderXfm, hand, inv2ab, aaPlusbb, aPlusb
    );

    Hmx::Matrix3 m;
    MakeRotMatrix(quat.q, m);
    Multiply(m, shoulderXfm.m, shoulderXfm.m);
    Add(shoulderXfm.v, quat.v, shoulderXfm.v);
    shoulder->SetWorldXfm(shoulderXfm);

    Transform elbowOut;
    Multiply(elbowXfm, shoulderXfm, elbowOut);
    elbow->SetWorldXfm(elbowOut);
}

BEGIN_PROPSYNCS(BandIKEffector)
    SYNC_PROP(effector, mEffector)
    SYNC_PROP(ground, mGround)
    SYNC_PROP(more, mMore)
    SYNC_PROP(elbow, mElbow)
    SYNC_PROP(constraints, mConstraints)
    SYNC_SUPERCLASS(CharWeightable)
END_PROPSYNCS