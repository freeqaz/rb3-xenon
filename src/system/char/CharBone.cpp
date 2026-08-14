#include "char/CharBone.h"
#include "char/CharBoneDir.h"
#include "char/CharBones.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Trans.h"
#include "utl/BinStream.h"

// RB3-360 retail rev storage. Retail's LOAD_REVS keeps NO BinStreamRev: it splits
// the packed rev into two mutable file-scope shorts, and ASSERT_REVS emits nothing.
// The two words must live in ONE aligned(4) aggregate (altRev +0, rev +4) -- MSVC
// does not lay .bss out in declaration order, so two separate statics get other
// globals interleaved between them and will not fold onto one base register.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_CharBone;
#define gAltRev gRevs_CharBone.altRev
#define gRev gRevs_CharBone.rev

CharBone::CharBone()
    : mPositionContext(0), mScaleContext(0), mRotation(CharBones::TYPE_END),
      mRotationContext(0), mTarget(this), mWeights(), mTrans(this),
      mBakeOutAsTopLevel(0) {}

void CharBone::ClearContext(int mask) {
    mPositionContext &= ~mask;
    mScaleContext &= ~mask;
    mRotationContext &= ~mask;
}

// NOTE(INSDEL-1): StuffBones' 20 charges are the stack-slot class in the
// NON-addressable direction -- RETAIL SHARES a slot across the three disjoint
// arms (`stw r29,0x54(r1)` in every arm) and WE over-allocate (0x54 then 0x50),
// which shifts the Bone/Symbol temps a uniform +8/+4.  Hoisting `Symbol name` /
// `CharBones::Bone bone` to function scope (retail's order, assignments left in
// the arms) measured 99.70 -> **76.16**, frame 268 -> 304, charges 20 -> 45:
// `Bone()` is a user-provided ctor (`weight(1.0f)`), so hoisting runs it
// unconditionally at the top -- the same hazard lane SRCARG-1 measured at
// -5.3 pp on FloatKeys, worse here because the object is bigger.
// ⇒ third consecutive failure of the "make retail-shared slots share" direction
// (after SampleData::Load and CharIKHand::Load): that direction is now 0-for-3.
void CharBone::StuffBones(std::list<CharBones::Bone> &bones, int mask) const {
    if (mPositionContext & mask) {
        Symbol name = CharBones::ChannelName(Name(), CharBones::TYPE_POS);
        CharBones::Bone bone;
        bone.name = name;
        bone.weight = GetWeight(mask);
        bones.push_back(bone);
    }
    if (mScaleContext & mask) {
        Symbol name = CharBones::ChannelName(Name(), CharBones::TYPE_SCALE);
        CharBones::Bone bone;
        bone.name = name;
        bone.weight = GetWeight(mask);
        bones.push_back(bone);
    }
    if (mRotation != CharBones::TYPE_END && mRotationContext & mask) {
        Symbol name = CharBones::ChannelName(Name(), mRotation);
        CharBones::Bone bone;
        bone.name = name;
        bone.weight = GetWeight(mask);
        bones.push_back(bone);
    }
}

const CharBone::WeightContext *CharBone::FindWeight(int ctx) const {
    FOREACH (it, mWeights) {
        if (it->mContext & ctx) {
            return &*it;
        }
    }
    return nullptr;
}

float CharBone::GetWeight(int mask) const {
    const WeightContext *ctx = FindWeight(mask);
    if (ctx) {
        return ctx->mWeight;
    } else {
        return 1.0f;
    }
}

DataNode CharBone::OnGetContextFlags(DataArray *da) {
    CharBoneDir *dir = dynamic_cast<CharBoneDir *>(Dir());
    if (dir)
        return dir->GetContextFlags();
    else {
        MILO_NOTIFY("CharBone: No CharBoneDir for context flags.");
        return DataArrayPtr();
    }
}

BEGIN_HANDLERS(CharBone)
    HANDLE_ACTION(clear_context, ClearContext(_msg->Int(2)))
    HANDLE(get_context_flags, OnGetContextFlags)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_CUSTOM_PROPSYNC(CharBone::WeightContext)
    SYNC_PROP(context, o.mContext)
    SYNC_PROP(weight, o.mWeight)
END_CUSTOM_PROPSYNC

BEGIN_PROPSYNCS(CharBone)
    SYNC_PROP(position_context, mPositionContext)
    SYNC_PROP(scale_context, mScaleContext)
    SYNC_PROP(rotation, (int &)mRotation)
    SYNC_PROP(rotation_context, mRotationContext)
    SYNC_PROP(target, mTarget)
    SYNC_PROP(weights, mWeights)
    SYNC_PROP(trans, mTrans)
    SYNC_PROP(bake_out_as_top_level, mBakeOutAsTopLevel)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BinStream &operator<<(BinStream &bs, const CharBone::WeightContext &ctx) {
    bs << ctx.mContext;
    bs << ctx.mWeight;
    return bs;
}

BinStream &operator>>(BinStream &d, CharBone::WeightContext &w) {
    d >> w.mContext >> w.mWeight;
    return d;
}

BEGIN_SAVES(CharBone)
    SAVE_REVS(10, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mPositionContext;
    bs << mScaleContext;
    bs << mRotation;
    bs << mRotationContext;
    bs << mTarget;
    bs << mWeights;
    bs << mTrans;
    bs << mBakeOutAsTopLevel;
END_SAVES

BEGIN_LOADS(CharBone)
    int rev;
    bs >> rev;
    gRev = getHmxRev(rev);
    gAltRev = getAltRev(rev);
    Hmx::Object::Load(bs);
    if (gRev < 9) {
        RndTransformableRemover t;
        t.Load(bs);
    }
    if (gRev > 6) {
        bs >> mPositionContext;
    } else {
        bool b;
        bs >> b;
        mPositionContext = b;
    }
    if (gRev > 6) {
        bs >> mScaleContext;
    } else if (gRev > 1) {
        bool b;
        bs >> b;
        mScaleContext = b;
    }
    bs >> (int &)mRotation;
    if (gRev < 5) {
        int x;
        bs >> x;
    }
    if (gRev < 2) {
        mScaleContext = 0;
        mRotation = (CharBones::Type)(mRotation + 1);
    }
    if (gRev < 5 && mRotation > CharBones::TYPE_END) {
        mRotation = CharBones::TYPE_END;
    }
    if (gRev > 6) {
        bs >> mRotationContext;
    } else {
        mRotationContext = mRotation != CharBones::TYPE_END;
    }
    if (gRev > 2 && gRev < 8) {
        int x;
        bs >> x;
    }
    if (gRev > 3) {
        bs >> mTarget;
    }
    if (gRev == 6) {
        int ctx;
        bs >> ctx;
        if (mPositionContext != 0) {
            mPositionContext = ctx;
        }
        if (mScaleContext != 0) {
            mScaleContext = ctx;
        }
        if (mRotationContext != 0) {
            mRotationContext = ctx;
        }
    }
    if (gRev > 7) {
        bs >> mWeights;
    }
    if (gRev > 8) {
        bs >> mTrans;
    }
    if (gRev > 9) {
        bs >> mBakeOutAsTopLevel;
    }
END_LOADS

BEGIN_COPYS(CharBone)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(CharBone)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mRotationContext)
        COPY_MEMBER(mScaleContext)
        COPY_MEMBER(mPositionContext)
        COPY_MEMBER(mRotation)
        COPY_MEMBER(mTarget)
        COPY_MEMBER(mWeights)
        COPY_MEMBER(mTrans)
        COPY_MEMBER(mBakeOutAsTopLevel)
    END_COPYING_MEMBERS
END_COPYS
