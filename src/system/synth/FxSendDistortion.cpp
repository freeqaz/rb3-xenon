#include "synth/FxSendDistortion.h"
#include "obj/Object.h"
#include "synth/FxSend.h"
#include "utl/BinStream.h"

FxSendDistortion::FxSendDistortion() : mDrive(0) {}

FxSendDistortion::~FxSendDistortion() {}

BEGIN_COPYS(FxSendDistortion)
    COPY_SUPERCLASS(FxSend)
    CREATE_COPY(FxSendDistortion)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mDrive)
    END_COPYING_MEMBERS
END_COPYS

void FxSendDistortion::Save(BinStream &bs) {
    bs << 1;
    SAVE_SUPERCLASS(FxSend)
    bs << mDrive;
}

INIT_REVS(1, 0)

// RB3 retail (0x82721370): the packed rev is split into two TU shorts -- here
// rev at +0 and alt at +4 (retail lbl_82E03CE4) -- with no version guard, and
// FxSend::Load gets the raw stream (rb3-Wii shape).
static struct {
    __declspec(align(4)) unsigned short rev;
    __declspec(align(4)) unsigned short altRev;
} gRevs_FxSendDistortion;

BEGIN_LOADS(FxSendDistortion)
    int revs;
    bs >> revs;
    gRevs_FxSendDistortion.rev = getHmxRev(revs);
    gRevs_FxSendDistortion.altRev = getAltRev(revs);
    FxSend::Load(bs);
    bs >> mDrive;
    OnParametersChanged();
END_LOADS

BEGIN_HANDLERS(FxSendDistortion)
    HANDLE_SUPERCLASS(FxSend)
END_HANDLERS

BEGIN_PROPSYNCS(FxSendDistortion)
    SYNC_PROP_MODIFY(drive, mDrive, OnParametersChanged())
    SYNC_SUPERCLASS(FxSend)
END_PROPSYNCS
