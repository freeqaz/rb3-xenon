#include "utl/HxGuid.h"
#include "os/Debug.h"
#include "utl/BinStream.h"
#include "xdk/XNET.h"

bool HxGuid::operator==(const HxGuid &hx) const {
    return (
        mData[0] == hx.mData[0] && mData[1] == hx.mData[1] && mData[2] == hx.mData[2]
        && mData[3] == hx.mData[3]
    );
}

bool HxGuid::operator<(const HxGuid &hx) const {
    return mData[0] < hx.mData[0]
        || (mData[0] == hx.mData[0]
            && (mData[1] < hx.mData[1]
                || (mData[1] == hx.mData[1]
                    && (mData[2] < hx.mData[2]
                        || (mData[2] == hx.mData[2] && (mData[3] < hx.mData[3]))))));
}

HxGuid::HxGuid() { Clear(); }

void HxGuid::Clear() { mData[0] = mData[1] = mData[2] = mData[3] = 0; }

void HxGuid::Generate() {
    while (true) {
        Clear();
        XNetRandom((unsigned char *)mData, sizeof(mData));
        if (IsNull()) {
            MILO_NOTIFY("Generated HxGuid is Null.  Will try again...");
        } else
            break;
    }
}

bool HxGuid::IsNull() const {
    return (mData[0] == 0 && mData[1] == 0 && mData[2] == 0 && mData[3] == 0);
}

const char *HxGuid::ToString() const {
    return MakeString("%08x%08x%08x%08x", mData[0], mData[1], mData[2], mData[3]);
}

const int *HxGuid::Data() const { return mData; }

#define kGuidRev 1

BinStream &operator<<(BinStream &bs, const HxGuid &hx) {
    bs << kGuidRev;
    bs << hx.mData[0] << hx.mData[1] << hx.mData[2] << hx.mData[3];
    return bs;
}

BinStream &operator>>(BinStream &bs, HxGuid &hx) {
    int rev;
    bs >> rev;
    MILO_ASSERT(rev <= kGuidRev, 0xBC);
    bs >> hx.mData[0] >> hx.mData[1] >> hx.mData[2] >> hx.mData[3];
    return bs;
}

#if HX_NATIVE
// HxGuid::SaveSize() has no out-of-line definition in the retail X360 tree (it's
// a trivial `return 0x14` that /O1 /Ob2 inlines at every call site, so no COMDAT
// is emitted — hence its absence here). The native FixedSizeSaveable round-trip
// links StandIn/SavedSetlist/BandProfile which reference it out-of-line, so
// provide the body natively. Value 0x14 (= kGuidRev int + 4 data ints = 20 bytes
// written by operator<<) is confirmed against the rb3-Wii oracle. X360-inert:
// the whole block is preprocessed out (HX_NATIVE undefined for the MSVC build).
int HxGuid::SaveSize() { return 0x14; }
#endif
