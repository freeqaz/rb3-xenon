// Retail inlines the owner-only ObjPtr ctor for mBase/mDriver here (see
// run_diff_inspect: idx 48/59 show TGT `stw ..., 0x24/0x30, r30` vs our
// `bl ??0?$ObjPtr@VCharWeightable/VCharDriver@@...`). Take retail's exact
// owner-only ctor SHAPE (base ctor stores only owner, derived body assigns
// mObject after the derived vptr store) per the DEFER_OBJECT lever in
// obj/Object.h -- same combo CharClipSet.cpp uses for mPreviewChar.
#define RB3_OBJPTR_INLINE_OWNER_CTOR
#define RB3_TU_OBJPTR_OWNER_CTOR_DEFER_OBJECT
// Retail Load (0x823BA240) also inlines the two-argument ObjPtr ctor for its
// two ObjPtr<CharWeightSetter> locals (stores owner, vtable, null object; no
// bl ??0ObjPtr), which this per-TU switch gives.
#define RB3_TU_OBJPTR_FORCEINLINE_CTOR
#include "char/CharWeightSetter.h"
#include "char/CharWeightable.h"
#include "obj/Object.h"

CharWeightSetter::CharWeightSetter()
    : mBase(this), mDriver(this), mMinWeights(this), mMaxWeights(this), mFlags(0),
      mOffset(0), mScale(1), mBaseWeight(0), mBeatsPerWeight(0) {}

BEGIN_HANDLERS(CharWeightSetter)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(CharWeightSetter)
    SYNC_PROP(driver, mDriver)
    SYNC_PROP(flags, mFlags)
    SYNC_PROP(base, mBase)
    SYNC_PROP(offset, mOffset)
    SYNC_PROP(scale, mScale)
    SYNC_PROP(base_weight, mBaseWeight)
    SYNC_PROP(beats_per_weight, mBeatsPerWeight)
    SYNC_PROP(min_weights, mMinWeights)
    SYNC_PROP(max_weights, mMaxWeights)
    SYNC_SUPERCLASS(CharWeightable)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(CharWeightSetter)
    SAVE_REVS(9, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(CharWeightable)
    bs << mDriver;
    bs << mFlags;
    bs << mOffset;
    bs << mScale;
    bs << mBaseWeight;
    bs << mBeatsPerWeight;
    bs << mBase;
    bs << mMinWeights;
    bs << mMaxWeights;
END_SAVES

BEGIN_COPYS(CharWeightSetter)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(CharWeightable)
    CREATE_COPY(CharWeightSetter)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mDriver)
        COPY_MEMBER(mFlags)
        COPY_MEMBER(mBase)
        COPY_MEMBER(mOffset)
        COPY_MEMBER(mScale)
        COPY_MEMBER(mBaseWeight)
        COPY_MEMBER(mBeatsPerWeight)
        COPY_MEMBER(mMinWeights)
        COPY_MEMBER(mMaxWeights)
    END_COPYING_MEMBERS
END_COPYS

// Retail Load keeps no BinStreamRev: it splits the packed rev into one aligned
// file-scope aggregate (altRev +0, rev +4) and reads everything from the raw
// stream.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_CharWeightSetter;

BEGIN_LOADS(CharWeightSetter)
    int rev;
    bs >> rev;
    gRevs_CharWeightSetter.rev = getHmxRev(rev);
    gRevs_CharWeightSetter.altRev = getAltRev(rev);
    Hmx::Object::Load(bs);
    if (gRevs_CharWeightSetter.rev > 1)
        CharWeightable::Load(bs);
    bs >> mDriver;
    bs >> mFlags;
    if (gRevs_CharWeightSetter.rev < 3) {
        mScale = 1.0f;
        mOffset = 0.0f;
    } else if (gRevs_CharWeightSetter.rev < 4) {
        bool b;
        bs >> b;
        if (b) {
            mScale = -1.0f;
            mOffset = 1.0f;
        } else {
            mScale = 1.0f;
            mOffset = 0.0f;
        }
    } else
        bs >> mOffset >> mScale;
    if (gRevs_CharWeightSetter.rev < 2) {
        ObjPtrList<CharWeightable, ObjectDir> pList(this, kObjListNoNull);
        bs >> pList;
        for (ObjPtrList<CharWeightable, ObjectDir>::iterator it = pList.begin();
             it != pList.end();
             ++it) {
            (*it)->SetWeightOwner(this);
        }
    }
    if (gRevs_CharWeightSetter.rev > 4) {
        bs >> mBaseWeight;
        bs >> mBeatsPerWeight;
    } else {
        mBaseWeight = mWeight;
        mBeatsPerWeight = 0.0f;
    }
    if (gRevs_CharWeightSetter.rev > 5)
        bs >> mBase;
    if (gRevs_CharWeightSetter.rev > 8) {
        bs >> mMinWeights;
        bs >> mMaxWeights;
    } else {
        if (gRevs_CharWeightSetter.rev > 6) {
            ObjPtr<CharWeightSetter> ptrWS(this, 0);
            bs >> ptrWS;
            if (ptrWS)
                mMinWeights.push_back(ptrWS);
        }
        if (gRevs_CharWeightSetter.rev > 7) {
            ObjPtr<CharWeightSetter> ptrWS(this, 0);
            bs >> ptrWS;
            if (ptrWS)
                mMaxWeights.push_back(ptrWS);
        }
    }
END_LOADS

void CharWeightSetter::SetWeight(float weight) {
    mBaseWeight = weight;
    mWeight = weight;
}

void CharWeightSetter::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    changedBy.push_back(mDriver);
    changedBy.push_back(mBase);
    for (ObjPtrList<CharWeightSetter>::iterator it = mMinWeights.begin();
         it != mMinWeights.end();
         ++it) {
        changedBy.push_back(*it);
    }
    for (ObjPtrList<CharWeightSetter>::iterator it = mMaxWeights.begin();
         it != mMaxWeights.end();
         ++it) {
        changedBy.push_back(*it);
    }
    FOREACH (it, Refs()) {
#ifdef HX_NATIVE
        CharWeightable *weightowner =
            dynamic_cast<CharWeightable *>(it->RefOwner());
#else
        // X360: ring entries are pool nodes; the ring-ref carries RefOwner().
        CharWeightable *weightowner =
            dynamic_cast<CharWeightable *>(RefPtrOf(it)->RefOwner());
#endif
        if (weightowner && weightowner->WeightOwner() == this)
            change.push_back(weightowner);
    }
}

void CharWeightSetter::Poll() {
    if (mDriver) {
        mBaseWeight = mScale * mDriver->EvaluateFlags(mFlags) + mOffset;
    } else if (mBase) {
        mBaseWeight = mScale * mBase->Weight() + mOffset;
    }

    if (mMinWeights.size() > 0) {
        float newminweight = mBaseWeight;
        for (ObjPtrList<CharWeightSetter>::iterator it = mMinWeights.begin();
             it != mMinWeights.end();
             ++it) {
            MinEq(newminweight, (*it)->Weight());
        }
        mBaseWeight = newminweight;
    }

    if (mMaxWeights.size() > 0) {
        float newmaxweight = mBaseWeight;
        for (ObjPtrList<CharWeightSetter>::iterator it = mMaxWeights.begin();
             it != mMaxWeights.end();
             ++it) {
            MaxEq(newmaxweight, (*it)->Weight());
        }
        mBaseWeight = newmaxweight;
    }

    if (mBaseWeight != mWeight) {
        if (mBeatsPerWeight <= 0.0f)
            mWeight = mBaseWeight;
        else {
            float secs = TheTaskMgr.DeltaBeat() / mBeatsPerWeight;
            if (secs > 0.0f) {
                float clamped = Clamp(-secs, secs, mBaseWeight - mWeight);
                mWeight += clamped;
            }
        }
    }
}
