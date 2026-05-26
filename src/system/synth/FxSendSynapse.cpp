#include "synth/FxSendSynapse.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "synth/FxSend.h"
#include "synth/FxSendBitCrush.h"
#include "utl/BinStream.h"

FxSendSynapse::FxSendSynapse()
    : mAmount(0.0f), mProximityEffect(0.5f), mProximityFocus(0.5f), mNote1Hz(200.0f),
      mNote2Hz(0.0f), mNote3Hz(0.0f), mUnisonTrio(0), mAttackSmoothing(30.0f),
      mReleaseSmoothing(60.0f) {}

BEGIN_COPYS(FxSendSynapse)
    COPY_SUPERCLASS(FxSend)
    CREATE_COPY(FxSendSynapse)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mAmount)
        COPY_MEMBER(mProximityEffect)
        COPY_MEMBER(mProximityFocus)
        COPY_MEMBER(mNote1Hz)
        COPY_MEMBER(mNote2Hz)
        COPY_MEMBER(mNote3Hz)
        COPY_MEMBER(mUnisonTrio)
        COPY_MEMBER(mAttackSmoothing)
        COPY_MEMBER(mReleaseSmoothing)
    END_COPYING_MEMBERS
END_COPYS

void FxSendSynapse::Save(BinStream &bs) {
    bs << 5;
    SAVE_SUPERCLASS(FxSend)
    bs << mAmount;
    bs << mProximityEffect;
    bs << mNote1Hz;
    bs << mAttackSmoothing;
    bs << mProximityFocus;
    bs << mReleaseSmoothing;
    bs << mNote2Hz;
    bs << mNote3Hz;
    bs << mUnisonTrio;
}

void FxSendSynapse::Load(BinStream &bs) {
    int rev;
    bs >> rev;
    if (rev > 5)
        MILO_NOTIFY("Can't load new FxSendSynapse");
    else {
        FxSend::Load(bs);
        bs >> mAmount >> mProximityEffect >> mNote1Hz;
        if (rev > 1)
            bs >> mAttackSmoothing;
        if (rev > 2)
            bs >> mProximityFocus;
        if (rev > 3)
            bs >> mReleaseSmoothing;
        if (rev > 4)
            bs >> mNote2Hz >> mNote3Hz >> mUnisonTrio;
        OnParametersChanged();
    }
}

BEGIN_HANDLERS(FxSendBitCrush)
    HANDLE_SUPERCLASS(FxSend)
END_HANDLERS

BEGIN_HANDLERS(FxSendSynapse)
    HANDLE_SUPERCLASS(FxSend)
END_HANDLERS

BEGIN_PROPSYNCS(FxSendSynapse)
    SYNC_PROP_MODIFY(amount, mAmount, OnParametersChanged())
    SYNC_PROP_MODIFY(proximity_effect, mProximityEffect, OnParametersChanged())
    SYNC_PROP_MODIFY(proximity_focus, mProximityFocus, OnParametersChanged())
    SYNC_PROP_MODIFY(note1_hz, mNote1Hz, OnParametersChanged())
    SYNC_PROP_MODIFY(note2_hz, mNote2Hz, OnParametersChanged())
    SYNC_PROP_MODIFY(note3_hz, mNote3Hz, OnParametersChanged())
    SYNC_PROP_MODIFY(unison_trio, mUnisonTrio, OnParametersChanged())
    SYNC_PROP_MODIFY(attack_smoothing, mAttackSmoothing, OnParametersChanged())
    SYNC_PROP_MODIFY(release_smoothing, mReleaseSmoothing, OnParametersChanged())
    SYNC_SUPERCLASS(FxSend)
END_PROPSYNCS
