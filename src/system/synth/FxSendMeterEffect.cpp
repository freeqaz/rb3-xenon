#include "synth/FxSendMeterEffect.h"
#include "obj/Object.h"
#include "synth/FxSend.h"
#include "utl/BinStream.h"

// Retail RB3 uses the rb3-Wii (ObjMacros.h) rev dialect -- file-scope rev
// words written by Load -- not the DC3-derived obj/Object.h BinStreamRev
// local.  Both words fold onto ONE base register at offsets 0/4, which only
// happens for internal-linkage align(4) file-scope statics.
// Named per-class and used directly (no `#define gRev`) because this TU
// scatter-includes FxSendWah.cpp below, which carries its own rev words.
static struct {
    __declspec(align(4)) unsigned short rev;
    __declspec(align(4)) unsigned short altRev;
} gRevsMeterEffect;

BEGIN_COPYS(FxSendMeterEffect)
    COPY_SUPERCLASS(FxSend)
    CREATE_COPY(FxSendMeterEffect)
END_COPYS

void FxSendMeterEffect::Save(BinStream &bs) {
    bs << 1;
    SAVE_SUPERCLASS(FxSend)
}

void FxSendMeterEffect::Load(BinStream &bs) {
    int rev;
    bs >> rev;
    gRevsMeterEffect.rev = getHmxRev(rev);
    gRevsMeterEffect.altRev = getAltRev(rev);
    FxSend::Load(bs);
    OnParametersChanged();
}

float FxSendMeterEffect::ChannelData(int idx) {
    if (mChannels.size() == 0)
        return 0.0f;
    int lastIdx = mChannels.size() - 1;
    if (lastIdx < idx)
        idx = lastIdx;
    return mChannels[idx].mPeak;
}

BEGIN_HANDLERS(FxSendMeterEffect)
    HANDLE_EXPR(channel1, ChannelData(0))
    HANDLE_EXPR(channel2, ChannelData(1))
    HANDLE_SUPERCLASS(FxSend)
END_HANDLERS

BEGIN_PROPSYNCS(FxSendMeterEffect)
    SYNC_PROP_MODIFY(reset_peaks, mResetPeaks, OnParametersChanged())
#ifdef HX_NATIVE
    // DC3-era additions; RB3-360 retail syncs only reset_peaks here.  Arbitrated
    // on retail asm (lane CP-2), not on oracle agreement: objdiff's call diff for
    // the 264 B retail body reports ??0Symbol@@ target 1 / base 3, and
    // ?ChannelData@FxSendMeterEffect@@ base-only x2 -- retail never calls the
    // channel accessors at all.  The 70-instruction insert cluster is exactly
    // these two blocks.  rb3-Wii independently agrees (reset_peaks only).
    SYNC_PROP_SET(channel1, ChannelData(0), )
    SYNC_PROP_SET(channel2, ChannelData(1), )
#endif
    SYNC_SUPERCLASS(FxSend)
END_PROPSYNCS

FxSendMeterEffect::FxSendMeterEffect() : mResetPeaks(0) {}

// sw2 scatter-include (default/FxSendMeterEffect <- synth/FxSendWah.cpp)
#define gRev gRev_FxSendWah
#define gAltRev gAltRev_FxSendWah
#include "synth/FxSendWah.cpp"
#undef gRev
#undef gAltRev
