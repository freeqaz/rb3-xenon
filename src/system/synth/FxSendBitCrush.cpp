#include "synth/FxSendBitCrush.h"
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

FxSendBitCrush::FxSendBitCrush() : mAmount(0) {}

FxSendBitCrush::~FxSendBitCrush() {}

BEGIN_COPYS(FxSendBitCrush)
    COPY_SUPERCLASS(FxSend)
    CREATE_COPY(FxSendBitCrush)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mAmount)
    END_COPYING_MEMBERS
END_COPYS

void FxSendBitCrush::Save(BinStream &bs) {
    bs << 1;
    SAVE_SUPERCLASS(FxSend)
    bs << mAmount;
}

void FxSendBitCrush::Load(BinStream &bs) {
    int rev;
    bs >> rev;
    gRev = getHmxRev(rev);
    gAltRev = getAltRev(rev);
    FxSend::Load(bs);
    bs >> mAmount;
    OnParametersChanged();
}

BEGIN_PROPSYNCS(FxSendBitCrush)
    SYNC_PROP_MODIFY(amount, mAmount, OnParametersChanged())
    SYNC_SUPERCLASS(FxSend)
END_PROPSYNCS
