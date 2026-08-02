#include "synth/FxSendChorus.h"
#include "obj/Object.h"
#include "synth/FxSend.h"
#include "utl/BinStream.h"

// Rev dialect: retail RB3 stores the loaded revision into file-scope rev words
// (the obj/ObjMacros.h INIT_REVS/LOAD_REVS shape); it does NOT construct a
// BinStreamRev local the way the DC3-derived obj/Object.h dialect does.
// Retail folds both rev words onto ONE base register with offsets 0/4, which
// only happens for internal-linkage, align(4) file-scope statics (here rev+0,
// altRev+4) -- not for DECLARE_REVS/INIT_REVS class statics.  Same recipe as
// bandobj/BandWardrobe.cpp, which needs the opposite member order.
// Spelled out longhand rather than by including ObjMacros.h, which would also
// swap the SYNC_PROP and HANDLE families -- those already match here.
static struct {
    __declspec(align(4)) unsigned short rev;
    __declspec(align(4)) unsigned short altRev;
} gRevs;
#define gRev gRevs.rev
#define gAltRev gRevs.altRev

BEGIN_COPYS(FxSendChorus)
    COPY_SUPERCLASS(FxSend)
    CREATE_COPY(FxSendChorus)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mDelayMs)
        COPY_MEMBER(mRate)
        COPY_MEMBER(mDepth)
        COPY_MEMBER(mFeedbackPct)
        COPY_MEMBER(mOffsetPct)
        COPY_MEMBER(mTempoSync)
        COPY_MEMBER(mSyncType)
        COPY_MEMBER(mTempo)
    END_COPYING_MEMBERS
END_COPYS

FxSendChorus::FxSendChorus()
    : mDelayMs(50.0f), mRate(1.0f), mDepth(10.0f), mFeedbackPct(30), mOffsetPct(20),
      mTempoSync(0), mSyncType(), mTempo(120.0f) {
    static Symbol quarter("quarter");
    mSyncType = quarter;
    mDryGain = -3.0f;
    mWetGain = -3.0f;
}

void FxSendChorus::Save(BinStream &bs) {
    // RB3 retail is rev 3; DC3 (newer) bumped this to 4.  Target
    // fn_827201F0 emits `li r11, 0x3`.
    bs << 3;
    SAVE_SUPERCLASS(FxSend)
    bs << mDelayMs;
    bs << mRate;
    bs << mDepth;
    bs << mFeedbackPct;
    bs << mOffsetPct;
    bs << mTempoSync;
    bs << mSyncType;
    bs << mTempo;
}

void FxSendChorus::Load(BinStream &bs) {
    int rev;
    bs >> rev;
    gRev = getHmxRev(rev);
    gAltRev = getAltRev(rev);
    FxSend::Load(bs);
    if (gRev == 1) {
        mDryGain = -3.0f;
        mWetGain = -3.0f;
        UpdateMix();
    }
    bs >> mDelayMs >> mRate >> mDepth >> mFeedbackPct >> mOffsetPct;
    if (gRev >= 3) {
        // Separate statements, not `bs >> mTempoSync >> mSyncType`: retail
        // re-materialises bs from its callee-saved register for the Symbol
        // read instead of consuming the stream returned by the bool read.
        bs >> mTempoSync;
        bs >> mSyncType;
        bs >> mTempo;
    }
    OnParametersChanged();
}

BEGIN_HANDLERS(FxSendChorus)
    HANDLE_SUPERCLASS(FxSend)
END_HANDLERS

BEGIN_PROPSYNCS(FxSendChorus)
    SYNC_PROP_MODIFY(delay_ms, mDelayMs, OnParametersChanged())
    SYNC_PROP_MODIFY(rate, mRate, OnParametersChanged())
    SYNC_PROP_MODIFY(depth, mDepth, OnParametersChanged())
    SYNC_PROP_MODIFY(feedback_pct, mFeedbackPct, OnParametersChanged())
    SYNC_PROP_MODIFY(offset_pct, mOffsetPct, OnParametersChanged())
    SYNC_PROP_MODIFY(tempo_sync, mTempoSync, OnParametersChanged())
    SYNC_PROP_MODIFY(sync_type, mSyncType, OnParametersChanged())
    SYNC_PROP_MODIFY(tempo, mTempo, OnParametersChanged())
    SYNC_SUPERCLASS(FxSend)
END_PROPSYNCS
