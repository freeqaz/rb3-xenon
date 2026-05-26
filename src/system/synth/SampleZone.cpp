#include "synth/SampleZone.h"
#include "obj/Object.h"
#include "synth/Stream.h"
#include "utl/BinStream.h"

SampleZone::SampleZone(Hmx::Object *owner)
    : mSample(owner), mVolume(0.0f), mPan(0.0f), mCenterNote(0x24), mMinNote(0),
      mMaxNote(0x7f), mMinVel(0), mMaxVel(0x7f), mFXCore(kFXCoreNone) {}

void SampleZone::Save(BinStream &bs) const {
    bs << mSample;
    bs << mVolume;
    bs << mPan;
    bs << mCenterNote;
    bs << mMinNote;
    bs << mMaxNote;
    bs << mFXCore;
    bs << mADSR;
    bs << mMinVel;
    bs << mMaxVel;
}

void SampleZone::Load(BinStreamRev &d) {
    d >> mSample;
    d >> mVolume;
    d >> mPan;
    d >> mCenterNote;
    d >> mMinNote;
    d >> mMaxNote;
    int fx;
    d >> fx;
    mFXCore = (FXCore)fx;
    d >> mADSR;
    if (d.rev >= 2) {
        d >> mMinVel;
        d >> mMaxVel;
    }
}

bool SampleZone::Includes(unsigned char note, unsigned char vel) {
    return mMinNote <= note && note <= mMaxNote && mMinVel <= vel && vel <= mMaxVel;
}

BinStream &operator<<(BinStream &bs, const SampleZone &sz) {
    sz.Save(bs);
    return bs;
}

BinStreamRev &operator>>(BinStreamRev &d, SampleZone &sz) {
    sz.Load(d);
    return d;
}
