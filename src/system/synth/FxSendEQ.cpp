#include "synth/FxSendEQ.h"
#include "FxSendEQ.h"
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

FxSendEQ::FxSendEQ()
    : mHighFreqCutoff(12000.0f), mHighFreqGain(0.0f), mMidFreqCutoff(8000.0f),
      mMidFreqBandwidth(1000.0f), mMidFreqGain(0.0f), mLowFreqCutoff(2000.0f),
      mLowFreqGain(0.0f), mLowPassCutoff(20000.0f), mLowPassReso(0.0f),
      mHighPassCutoff(20.0f), mHighPassReso(0.0f) {}

FxSendEQ::~FxSendEQ() {}

BEGIN_COPYS(FxSendEQ)
    COPY_SUPERCLASS(FxSend)
    CREATE_COPY(FxSendEQ)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mHighFreqCutoff)
        COPY_MEMBER(mHighFreqGain)
        COPY_MEMBER(mMidFreqCutoff)
        COPY_MEMBER(mMidFreqBandwidth)
        COPY_MEMBER(mMidFreqGain)
        COPY_MEMBER(mLowFreqCutoff)
        COPY_MEMBER(mLowFreqGain)
        COPY_MEMBER(mLowPassCutoff)
        COPY_MEMBER(mLowPassReso)
        COPY_MEMBER(mHighPassCutoff)
        COPY_MEMBER(mHighPassReso)
    END_COPYING_MEMBERS
END_COPYS

void FxSendEQ::Save(BinStream &bs) {
    bs << 2;
    SAVE_SUPERCLASS(FxSend)
    bs << mHighFreqCutoff;
    bs << mHighFreqGain;
    bs << mMidFreqCutoff;
    bs << mMidFreqBandwidth;
    bs << mMidFreqGain;
    bs << mLowFreqCutoff;
    bs << mLowFreqGain;
    bs << mLowPassCutoff;
    bs << mLowPassReso;
    bs << mHighPassCutoff;
    bs << mHighPassReso;
}

void FxSendEQ::Load(BinStream &bs) {
    int rev;
    bs >> rev;
    gRev = getHmxRev(rev);
    gAltRev = getAltRev(rev);
    FxSend::Load(bs);
    bs >> mHighFreqCutoff >> mHighFreqGain >> mMidFreqCutoff >> mMidFreqBandwidth
        >> mMidFreqGain >> mLowFreqCutoff >> mLowFreqGain;
    if (gRev >= 2) {
        bs >> mLowPassCutoff >> mLowPassReso >> mHighPassCutoff >> mHighPassReso;
    }
    OnParametersChanged();
}

BEGIN_HANDLERS(FxSendEQ)
    HANDLE_SUPERCLASS(FxSend)
END_HANDLERS

BEGIN_PROPSYNCS(FxSendEQ)
    SYNC_PROP_MODIFY(high_frequency, mHighFreqCutoff, OnParametersChanged())
    SYNC_PROP_MODIFY(high_gain, mHighFreqGain, OnParametersChanged())
    SYNC_PROP_MODIFY(mid_frequency, mMidFreqCutoff, OnParametersChanged())
    SYNC_PROP_MODIFY(mid_bandwidth, mMidFreqBandwidth, OnParametersChanged())
    SYNC_PROP_MODIFY(mid_gain, mMidFreqGain, OnParametersChanged())
    SYNC_PROP_MODIFY(low_frequency, mLowFreqCutoff, OnParametersChanged())
    SYNC_PROP_MODIFY(low_gain, mLowFreqGain, OnParametersChanged())
    SYNC_PROP_MODIFY(low_pass_cutoff, mLowPassCutoff, OnParametersChanged())
    SYNC_PROP_MODIFY(low_pass_reso, mLowPassReso, OnParametersChanged())
    SYNC_PROP_MODIFY(high_pass_cutoff, mHighPassCutoff, OnParametersChanged())
    SYNC_PROP_MODIFY(high_pass_reso, mHighPassReso, OnParametersChanged())
    SYNC_SUPERCLASS(FxSend)
END_PROPSYNCS
