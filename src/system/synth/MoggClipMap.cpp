#include "synth/MoggClipMap.h"
#include "utl/BinStream.h"

int MoggClipMap::sRev = 0;

void MoggClipMap::mySave(BinStream &bs) const {
    bs << mMoggClip;
    bs << mVolume;
    bs << mPan;
    bs << mPanWidth;
    bs << mIsStereo;
}

MoggClipMap::MoggClipMap(Hmx::Object *obj)
    : mMoggClip(obj), mPan(0.0f), mPanWidth(0.0f), mVolume(0.0f), mIsStereo(false) {}

// Retail copy-constructs the Hmx::Object base (0x822774F8, the body every other
// caller spells as Object's copy ctor), not its default ctor (0x8275CB88).
MoggClipMap::MoggClipMap(const MoggClipMap &mogg)
#if HX_NATIVE
    // Native has no Hmx::Object copy ctor (it would duplicate the ref list and
    // name-table entry); the map's state is its five members. W16-TM.
    : Hmx::Object(),
#else
    : Hmx::Object(mogg),
#endif
      mMoggClip(mogg.mMoggClip), mPan(mogg.mPan), mPanWidth(mogg.mPanWidth),
      mVolume(mogg.mVolume), mIsStereo(mogg.mIsStereo) {}

MoggClipMap &MoggClipMap::operator=(const MoggClipMap &mogg) {
    // RB3 retail assigns the Hmx::Object base first (out-of-line
    // `Hmx::Object::operator=` call before the members).
    Hmx::Object::operator=(mogg);
    mMoggClip = mogg.mMoggClip;
    mPan = mogg.mPan;
    mPanWidth = mogg.mPanWidth;
    mVolume = mogg.mVolume;
    mIsStereo = mogg.mIsStereo;
    return *this;
}

BinStream &operator<<(BinStream &bs, const MoggClipMap &mogg) {
    mogg.mySave(bs);
    return bs;
}

void MoggClipMap::myLoad(BinStream &bs) {
    bs >> mMoggClip;
    if (sRev >= 11) {
        bs >> mVolume;
        bs >> mPan;
        bs >> mPanWidth;
        bs >> mIsStereo;
    }
}

BinStream &operator>>(BinStream &bs, MoggClipMap &mogg) {
    mogg.myLoad(bs);
    return bs;
}
