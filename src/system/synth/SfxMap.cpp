#include "synth/Sfx.h"
#include "synth/Stream.h"
#include "utl/BinStream.h"

int SfxMap::gRev = 0;

SfxMap::SfxMap(Hmx::Object *obj)
    : mSample(obj), mVolume(0), mPan(0), mTranspose(0), mFXCore(kFXCoreNone) {}

void SfxMap::Save(BinStream &bs) const {
    bs << mSample;
    bs << mVolume;
    bs << mPan;
    bs << mTranspose;
    bs << mFXCore;
    bs << mADSR;
}

void SfxMap::Load(BinStream &bs) {
    bs >> mSample;
    if (gRev > 2) {
        bs >> mVolume;
        bs >> mPan;
        bs >> mTranspose;
        int fx;
        bs >> fx;
        mFXCore = (FXCore)fx;
        if (gRev >= 4) {
            bs >> mADSR;
        }
    }
}

BinStream &operator<<(BinStream &bs, const SfxMap &s) {
    s.Save(bs);
    return bs;
}

BinStream &operator>>(BinStream &bs, SfxMap &s) {
    s.Load(bs);
    return bs;
}
