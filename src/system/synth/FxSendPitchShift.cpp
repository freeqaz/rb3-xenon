#include "synth/FxSendPitchShift.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "synth/FxSend.h"
#include "utl/BinStream.h"

FxSendPitchShift::FxSendPitchShift() : mRatio(1.0f) {}

BEGIN_COPYS(FxSendPitchShift)
    COPY_SUPERCLASS(FxSend)
    CREATE_COPY(FxSendPitchShift)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mRatio)
    END_COPYING_MEMBERS
END_COPYS

void FxSendPitchShift::Save(BinStream &bs) {
    MILO_NOTIFY("FxSendPitchShift objects should not be created in Milo!");
    bs << 1;
    SAVE_SUPERCLASS(FxSend)
    bs << mRatio;
}

void FxSendPitchShift::Load(BinStream &bs) {
    MILO_FAIL("FxSendPitchShift objects should not be created in Milo!\n");
    int rev;
    bs >> rev;
    if (rev > 1)
        MILO_NOTIFY("Can't load new FxSendPitchShift");
    else {
        FxSend::Load(bs);
        bs >> mRatio;
        OnParametersChanged();
    }
}

BEGIN_HANDLERS(FxSendPitchShift)
    HANDLE_SUPERCLASS(FxSend)
END_HANDLERS

BEGIN_PROPSYNCS(FxSendPitchShift)
    SYNC_PROP_MODIFY(pitch_ratio, mRatio, OnParametersChanged())
    SYNC_SUPERCLASS(FxSend)
END_PROPSYNCS
