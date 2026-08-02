#include "synth/FxSendDelay.h"
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

BEGIN_COPYS(FxSendDelay)
    COPY_SUPERCLASS(FxSend)
    CREATE_COPY(FxSendDelay)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mDelayTime)
        COPY_MEMBER(mGain)
        COPY_MEMBER(mTempoSync)
        COPY_MEMBER(mSyncType)
        COPY_MEMBER(mTempo)
        COPY_MEMBER(mPingPongPct)
    END_COPYING_MEMBERS
END_COPYS

FxSendDelay::FxSendDelay()
    : mDelayTime(0.2f), mGain(-6.0f), mPingPongPct(0.0f), mTempoSync(0), mSyncType(),
      mTempo(120.0f) {
    static Symbol eighth("eighth");
    mSyncType = eighth;
}

FxSendDelay::~FxSendDelay() {}

void FxSendDelay::Save(BinStream &bs) {
    bs << 3;
    SAVE_SUPERCLASS(FxSend)
    bs << mDelayTime;
    bs << mGain;
    bs << mTempoSync;
    bs << mSyncType;
    bs << mTempo;
    bs << mPingPongPct;
}

void FxSendDelay::Load(BinStream &bs) {
    int rev;
    bs >> rev;
    gRev = getHmxRev(rev);
    gAltRev = getAltRev(rev);
    FxSend::Load(bs);
    bs >> mDelayTime >> mGain;
    if (gRev >= 2) {
        // Separate statements, not a chain: retail re-materialises bs for the
        // Symbol read rather than consuming the bool read's returned stream.
        bs >> mTempoSync;
        bs >> mSyncType;
        bs >> mTempo;
    }
    if (gRev >= 3) {
        bs >> mPingPongPct;
    }
    OnParametersChanged();
}

BEGIN_HANDLERS(FxSendDelay)
    HANDLE_SUPERCLASS(FxSend)
END_HANDLERS

BEGIN_PROPSYNCS(FxSendDelay)
    SYNC_PROP_MODIFY(delay_time, mDelayTime, OnParametersChanged())
    SYNC_PROP_MODIFY(feedback, mGain, OnParametersChanged())
    SYNC_PROP_MODIFY(tempo_sync, mTempoSync, OnParametersChanged())
    SYNC_PROP_MODIFY(sync_type, mSyncType, OnParametersChanged())
    SYNC_PROP_MODIFY(tempo, mTempo, OnParametersChanged())
    SYNC_PROP_MODIFY(ping_pong_pct, mPingPongPct, OnParametersChanged())
    SYNC_SUPERCLASS(FxSend)
END_PROPSYNCS
