#include "char/CharEyeDartRuleset.h"
#include "obj/Object.h"

// RB3-360 retail rev storage. Retail's LOAD_REVS keeps NO BinStreamRev: it splits
// the packed rev into two mutable file-scope shorts, and ASSERT_REVS emits nothing.
// The two words must live in ONE aligned(4) aggregate (altRev +0, rev +4) -- MSVC
// does not lay .bss out in declaration order, so two separate statics get other
// globals interleaved between them and will not fold onto one base register.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_CharEyeDartRuleset;
#define gAltRev gRevs_CharEyeDartRuleset.altRev
#define gRev gRevs_CharEyeDartRuleset.rev

CharEyeDartRuleset::CharEyeDartRuleset() {}

void CharEyeDartRuleset::EyeDartRulesetData::ClearToDefaults() {
    mMinRadius = 0.5;
    mMaxRadius = 3;
    mOnTargetAngleThresh = 5;
    mMinDartsPerSequence = 2;
    mMaxDartsPerSequence = 5;
    mMinSecsBetweenDarts = 0.25;
    mMaxSecsBetweenDarts = 0.65;
    mMinSecsBetweenSequences = 1;
    mMaxSecsBetweenSequences = 2;
    mScaleWithDistance = true;
    mReferenceDistance = 70;
}

BEGIN_HANDLERS(CharEyeDartRuleset)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(CharEyeDartRuleset)
    SYNC_PROP(min_radius, mData.mMinRadius)
    SYNC_PROP(max_radius, mData.mMaxRadius)
    SYNC_PROP(on_target_angle_thresh, mData.mOnTargetAngleThresh)
    SYNC_PROP(min_darts_per_sequence, mData.mMinDartsPerSequence)
    SYNC_PROP(max_darts_per_sequence, mData.mMaxDartsPerSequence)
    SYNC_PROP(min_secs_between_darts, mData.mMinSecsBetweenDarts)
    SYNC_PROP(max_secs_between_darts, mData.mMaxSecsBetweenDarts)
    SYNC_PROP(min_secs_between_sequences, mData.mMinSecsBetweenSequences)
    SYNC_PROP(max_secs_between_sequences, mData.mMaxSecsBetweenSequences)
    SYNC_PROP(scale_with_distance, mData.mScaleWithDistance)
    SYNC_PROP(reference_distance, mData.mReferenceDistance)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(CharEyeDartRuleset)
    SAVE_REVS(1, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mData.mMinRadius;
    bs << mData.mMaxRadius;
    bs << mData.mOnTargetAngleThresh;
    bs << mData.mMinDartsPerSequence;
    bs << mData.mMaxDartsPerSequence;
    bs << mData.mMinSecsBetweenDarts;
    bs << mData.mMaxSecsBetweenDarts;
    bs << mData.mMinSecsBetweenSequences;
    bs << mData.mMaxSecsBetweenSequences;
    bs << mData.mScaleWithDistance;
    bs << mData.mReferenceDistance;
END_SAVES

BEGIN_COPYS(CharEyeDartRuleset)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(CharEyeDartRuleset)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mData.mMinRadius)
        COPY_MEMBER(mData.mMaxRadius)
        COPY_MEMBER(mData.mOnTargetAngleThresh)
        COPY_MEMBER(mData.mMinDartsPerSequence)
        COPY_MEMBER(mData.mMaxDartsPerSequence)
        COPY_MEMBER(mData.mMinSecsBetweenDarts)
        COPY_MEMBER(mData.mMaxSecsBetweenDarts)
        COPY_MEMBER(mData.mMinSecsBetweenSequences)
        COPY_MEMBER(mData.mMaxSecsBetweenSequences)
        COPY_MEMBER(mData.mScaleWithDistance)
        COPY_MEMBER(mData.mReferenceDistance)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_LOADS(CharEyeDartRuleset)
    int rev;
    bs >> rev;
    gRev = getHmxRev(rev);
    gAltRev = getAltRev(rev);
    Hmx::Object::Load(bs);
    bs >> mData.mMinRadius >> mData.mMaxRadius >> mData.mOnTargetAngleThresh
        >> mData.mMinDartsPerSequence >> mData.mMaxDartsPerSequence
        >> mData.mMinSecsBetweenDarts >> mData.mMaxSecsBetweenDarts
        >> mData.mMinSecsBetweenSequences >> mData.mMaxSecsBetweenSequences;
    bs >> mData.mScaleWithDistance;
    bs >> mData.mReferenceDistance;
END_LOADS
