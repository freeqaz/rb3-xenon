#include "synth/FxSendCompress.h"
#include "obj/Object.h"
#include "synth/FxSend.h"
#include "utl/BinStream.h"

// Retail RB3 uses the rb3-Wii (ObjMacros.h) rev dialect -- file-scope rev
// words written by Load -- not the DC3-derived obj/Object.h BinStreamRev
// local.  Both words fold onto ONE base register at offsets 0/4, which only
// happens for internal-linkage align(4) file-scope statics.
static struct {
    __declspec(align(4)) unsigned short rev;
    __declspec(align(4)) unsigned short altRev;
} gRevs;
#define gRev gRevs.rev
#define gAltRev gRevs.altRev

FxSendCompress::FxSendCompress()
    : mThresholdDB(-12.0f), mRatio(3.0f), mOutputLevel(0.0f), mAttack(0.005f),
      mRelease(0.12f), mExpRatio(1.0f), mExpAttack(0.12f), mExpRelease(0.005f) {}

FxSendCompress::~FxSendCompress() {}

BEGIN_COPYS(FxSendCompress)
    COPY_SUPERCLASS(FxSend)
    CREATE_COPY(FxSendCompress)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mThresholdDB)
        COPY_MEMBER(mRatio)
        COPY_MEMBER(mOutputLevel)
        COPY_MEMBER(mAttack)
        COPY_MEMBER(mRelease)
        COPY_MEMBER(mExpRatio)
        COPY_MEMBER(mExpAttack)
        COPY_MEMBER(mExpRelease)
        COPY_MEMBER(mGateThresholdDB)
    END_COPYING_MEMBERS
END_COPYS

void FxSendCompress::Save(BinStream &bs) {
    bs << 4;
    SAVE_SUPERCLASS(FxSend)
    bs << mThresholdDB;
    bs << mRatio;
    bs << mOutputLevel;
    bs << mAttack;
    bs << mRelease;
    bs << mExpRatio << mExpAttack << mExpRelease << mGateThresholdDB;
}

void FxSendCompress::Load(BinStream &bs) {
    int rev;
    bs >> rev;
    gRev = getHmxRev(rev);
    gAltRev = getAltRev(rev);
    FxSend::Load(bs);
    bs >> mThresholdDB >> mRatio >> mOutputLevel;
    if (gRev < 2)
        mOutputLevel = 0.0f;
    bs >> mAttack >> mRelease;
    int dummy;
    if (gRev < 2)
        bs >> dummy;
    if (gRev >= 3) {
        bs >> mExpRatio >> mExpAttack >> mExpRelease;
    }
    if (gRev >= 4)
        bs >> mGateThresholdDB;
    OnParametersChanged();
}

BEGIN_HANDLERS(FxSendCompress)
    HANDLE_SUPERCLASS(FxSend)
END_HANDLERS

BEGIN_PROPSYNCS(FxSendCompress)
    SYNC_PROP_MODIFY(threshold, mThresholdDB, OnParametersChanged())
    SYNC_PROP_MODIFY(comp_ratio, mRatio, OnParametersChanged())
    SYNC_PROP_MODIFY(output_level, mOutputLevel, OnParametersChanged())
    SYNC_PROP_MODIFY(attack, mAttack, OnParametersChanged())
    SYNC_PROP_MODIFY(release, mRelease, OnParametersChanged())
    SYNC_PROP_MODIFY(exp_ratio, mExpRatio, OnParametersChanged())
    SYNC_PROP_MODIFY(exp_attack, mExpAttack, OnParametersChanged())
    SYNC_PROP_MODIFY(exp_release, mExpRelease, OnParametersChanged())
    SYNC_PROP_MODIFY(gate_threshold, mGateThresholdDB, OnParametersChanged())
    SYNC_SUPERCLASS(FxSend)
END_PROPSYNCS
