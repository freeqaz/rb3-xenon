#include "synth/FxSendWah.h"
#include "obj/Object.h"
#include "synth/FxSend.h"
#include "utl/BinStream.h"

// Retail RB3 uses the rb3-Wii (ObjMacros.h) rev dialect -- file-scope rev
// words written by Load -- not the DC3-derived obj/Object.h BinStreamRev
// local.  Both words fold onto ONE base register at offsets 0/4, which only
// happens for internal-linkage align(4) file-scope statics.
// Named per-class and used directly (no `#define gRev`) because this file is
// also scatter-included into FxSendMeterEffect.cpp, which has its own.
static struct {
    __declspec(align(4)) unsigned short rev;
    __declspec(align(4)) unsigned short altRev;
} gRevsWah;

FxSendWah::FxSendWah()
    : mResonance(7.0f), mUpperFreq(5000.0f), mLowerFreq(1000.0f), mLfoFreq(1.35f),
      mMagic(0.3f), mDistAmount(0.5f), mAutoWah(0), mFrequency(0.5f), mTempoSync(0),
      mSyncType("quarter"), mTempo(120.0f), mBeatFrac(0.0f) {}

BEGIN_COPYS(FxSendWah)
    COPY_SUPERCLASS(FxSend)
    CREATE_COPY(FxSendWah)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mResonance)
        COPY_MEMBER(mLowerFreq)
        COPY_MEMBER(mUpperFreq)
        COPY_MEMBER(mLfoFreq)
        COPY_MEMBER(mMagic)
        COPY_MEMBER(mTempoSync)
        COPY_MEMBER(mTempo)
        COPY_MEMBER(mSyncType)
        COPY_MEMBER(mDistAmount)
        COPY_MEMBER(mAutoWah)
        COPY_MEMBER(mFrequency)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_SAVES(FxSendWah)
    SAVE_REVS(3, 0)
    SAVE_SUPERCLASS(FxSend)
    bs << mResonance << mLowerFreq << mUpperFreq << mLfoFreq << mMagic;
    bs << mTempoSync << mTempo << mSyncType;
    bs << mDistAmount << mAutoWah << mFrequency;
END_SAVES

void FxSendWah::Load(BinStream &bs) {
    int rev;
    bs >> rev;
    gRevsWah.rev = getHmxRev(rev);
    gRevsWah.altRev = getAltRev(rev);
    FxSend::Load(bs);
    bs >> mResonance >> mLowerFreq >> mUpperFreq >> mLfoFreq >> mMagic;
    if (gRevsWah.rev >= 2) {
        bs >> mTempoSync >> mTempo >> mSyncType;
    }
    if (gRevsWah.rev >= 3) {
        bs >> mDistAmount >> mAutoWah >> mFrequency;
    }
    OnParametersChanged();
}

BEGIN_HANDLERS(FxSendWah)
    HANDLE_SUPERCLASS(FxSend)
END_HANDLERS

BEGIN_PROPSYNCS(FxSendWah)
    SYNC_PROP_MODIFY(resonance, mResonance, OnParametersChanged())
    SYNC_PROP_MODIFY(upper_freq, mUpperFreq, OnParametersChanged())
    SYNC_PROP_MODIFY(lower_freq, mLowerFreq, OnParametersChanged())
    SYNC_PROP_MODIFY(lfo_freq, mLfoFreq, OnParametersChanged())
    SYNC_PROP_MODIFY(magic, mMagic, OnParametersChanged())
    SYNC_PROP_MODIFY(tempo_sync, mTempoSync, OnParametersChanged())
    SYNC_PROP_MODIFY(sync_type, mSyncType, OnParametersChanged())
    SYNC_PROP_MODIFY(tempo, mTempo, OnParametersChanged())
    SYNC_PROP_MODIFY(beat_frac, mBeatFrac, OnParametersChanged())
    SYNC_PROP_MODIFY(dist_amount, mDistAmount, OnParametersChanged())
    SYNC_PROP_MODIFY(auto_wah, mAutoWah, OnParametersChanged())
    SYNC_PROP_MODIFY(frequency, mFrequency, OnParametersChanged())
    SYNC_PROP_MODIFY(dump, mDump, OnParametersChanged())
    SYNC_SUPERCLASS(FxSend)
END_PROPSYNCS
