#include "synth/FxSendWah.h"
#include "obj/Object.h"
#include "synth/FxSend.h"
#include "utl/BinStream.h"

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

INIT_REVS(3, 0)

BEGIN_LOADS(FxSendWah)
    LOAD_REVS(bs)
    ASSERT_REVS(3, 0)
    LOAD_SUPERCLASS(FxSend)
    d >> mResonance;
    d >> mLowerFreq;
    d >> mUpperFreq;
    d >> mLfoFreq;
    d >> mMagic;
    if (d.rev >= 2) {
        d >> mTempoSync >> mTempo >> mSyncType;
    }
    if (d.rev >= 3) {
        d >> mDistAmount >> mAutoWah >> mFrequency;
    }
    OnParametersChanged();
END_LOADS

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
    SYNC_SUPERCLASS(FxSend)
END_PROPSYNCS
