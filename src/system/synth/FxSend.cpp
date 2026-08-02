#include "synth/FxSend.h"
#include "Sfx.h"
#include "math/Decibels.h"
#include "math/Utl.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "synth/Synth.h"
#include "utl/BinStream.h"
#include "utl/Loader.h"

FxSend::FxSend()
    : mNextSend(this), mStage(0), mBypass(0), mDryGain(kDbSilence), mWetGain(0),
      mInputGain(0), mReverbMixDb(kDbSilence), mReverbEnable(0), mEnableUpdates(1),
      mChannels(kSendAll) {}

void FxSend::Replace(ObjRef *from, Hmx::Object *to) {
    if (RefIs(from, mNextSend)) {
        mNextSend.SetObj(to);
        RebuildChain();
        return;
    } else
        Hmx::Object::Replace(from, to);
}

BEGIN_HANDLERS(FxSend)
    HANDLE_ACTION(test_with_mic, TestWithMic())
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(FxSend)
    SYNC_PROP_SET(next_send, NextSend(), SetNextSend(_val.Obj<FxSend>()))
    SYNC_PROP_SET(stage, Stage(), SetStage(_val.Int()))
    SYNC_PROP_MODIFY(dry_gain, mDryGain, UpdateMix())
    SYNC_PROP_MODIFY(wet_gain, mWetGain, UpdateMix())
    SYNC_PROP_MODIFY(input_gain, mInputGain, UpdateMix())
    SYNC_PROP_MODIFY(reverb_mix_db, mReverbMixDb, UpdateMix())
    SYNC_PROP_MODIFY(reverb_enable, mReverbEnable, RebuildChain())
    SYNC_PROP_MODIFY(channels, (int &)mChannels, RebuildChain())
    SYNC_PROP_MODIFY(bypass, mBypass, UpdateMix())
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(FxSend)
    SAVE_REVS(7, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mNextSend;
    bs << mStage;
    bs << mChannels;
    bs << mDryGain;
    bs << mWetGain;
    bs << mInputGain;
    bs << mBypass;
    bs << mReverbMixDb;
    bs << mReverbEnable;
END_SAVES

BEGIN_COPYS(FxSend)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(FxSend)
    BEGIN_COPYING_MEMBERS
        mNextSend.SetObj(c->mNextSend);
        COPY_MEMBER(mStage)
        COPY_MEMBER(mWetGain)
        COPY_MEMBER(mDryGain)
        COPY_MEMBER(mInputGain)
        COPY_MEMBER(mChannels)
        COPY_MEMBER(mBypass)
        COPY_MEMBER(mReverbMixDb)
        COPY_MEMBER(mReverbEnable)
    END_COPYING_MEMBERS
END_COPYS

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

void FxSend::Load(BinStream &bs) {
    int rev;
    bs >> rev;
    gRev = getHmxRev(rev);
    gAltRev = getAltRev(rev);
    Hmx::Object::Load(bs);
    // Retail rebuilds the chain only when the routing actually changed.
    FxSend *oldPtr = mNextSend;
    int oldStage = mStage;
    SendChannels oldchans = mChannels;
    bs >> mNextSend;
    bs >> mStage;
    // Rev 2-4: Used percentage-based wet/dry mix
    if (gRev < 5) {
        if (gRev >= 2) {
            float f;
            bs >> f;
            mDryGain = RatioToDb((100.0f - f) / 100.0f);
            mWetGain = RatioToDb(f / 100.0f);
        }
        if (gRev >= 3) {
            bs >> mBypass;
        }
    }
    // Rev 4: Added channel routing
    if (gRev >= 4) {
        int chans;
        bs >> chans;
        mChannels = (SendChannels)chans;
    }
    // Rev 5: Switched to dB-based gains, added input gain
    if (gRev >= 5) {
        bs >> mDryGain >> mWetGain >> mInputGain;
    }
    // Rev 6: Moved bypass here (was in rev 3 block for old versions)
    if (gRev >= 6) {
        bs >> mBypass;
    }
    // Rev 7: Added reverb send controls
    if (gRev >= 7) {
        bs >> mReverbMixDb >> mReverbEnable;
    }
    if (mNextSend != oldPtr || mStage != oldStage || mChannels != oldchans)
        RebuildChain();
    UpdateMix();
}

void FxSend::SetNextSend(FxSend *next) {
    if (next != mNextSend && CheckChain(next, mStage)) {
        mNextSend = next;
        RebuildChain();
    }
}

void FxSend::RebuildChain() {
    std::vector<FxSend *> vec;
    BuildChainVector(vec);
    Recreate(vec);
}

void FxSend::BuildChainVector(std::vector<FxSend *> &sends) {
    sends.push_back(this);
    FOREACH (it, Refs()) {
        FxSend *send = dynamic_cast<FxSend *>(RefPtrOf(it)->RefOwner());
        if (send && send->mNextSend == this) {
            send->BuildChainVector(sends);
        } else {
            Sfx *sfx = dynamic_cast<Sfx *>(RefPtrOf(it)->RefOwner());
            if (sfx) {
                sfx->Stop(false);
            }
        }
    }
}

void FxSend::SetChannels(SendChannels chans) {
    if (chans != mChannels) {
        mChannels = chans;
        RebuildChain();
    }
}

void FxSend::EnableUpdates(bool enable) {
    mEnableUpdates = enable;
    if (mEnableUpdates)
        OnParametersChanged();
}

bool FxSend::CheckChain(FxSend *send, int i) {
    // Check for cycles in the chain
    FxSend *cur;
    for (cur = send; cur && cur != this; cur = cur->mNextSend)
        ;
    if (cur == this) {
        MILO_NOTIFY("Error: can't have loops in your FX chain.");
        return false;
    } else if (send && send->Stage() <= i) {
        MILO_NOTIFY(
            "Error: output send must be set to a higher stage (%d <= %d).",
            send->Stage(),
            i
        );
        return false;
    } else {
        FOREACH (it, mRefs) {
            FxSend *rsend = dynamic_cast<FxSend *>(RefPtrOf(it)->RefOwner());
            if (rsend && rsend->NextSend() == this && rsend->Stage() >= i) {
                MILO_NOTIFY(
                    "Error: stage must be higher than all input sends' stages (see %s).",
                    rsend->Name()
                );
                return false;
            }
        }
        return true;
    }
}

void FxSend::SetStage(int stage) {
    if (stage != mStage && CheckChain(mNextSend, stage)) {
        mStage = stage;
        RebuildChain();
    }
}

void FxSend::TestWithMic() {
    MILO_ASSERT(TheLoadMgr.EditMode(), 0x10A);
    Mic *mic = TheSynth->GetMic(0);
    mic->Start();
    mic->StartPlayback();
    mic->SetFxSend(this);
}
