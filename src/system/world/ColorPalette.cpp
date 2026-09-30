#include "world/ColorPalette.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "utl/BinStream.h"

ColorPalette::ColorPalette() {}

BEGIN_HANDLERS(ColorPalette)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(ColorPalette)
    SYNC_PROP(colors, mColors)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(ColorPalette)
    SAVE_REVS(1, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mColors;
END_SAVES

BEGIN_COPYS(ColorPalette)
    CREATE_COPY(ColorPalette)
    MILO_ASSERT(c, 0x4A);
    COPY_SUPERCLASS_FROM(Hmx::Object, c)
    COPY_MEMBER(mColors)
END_COPYS

BinStream &operator>>(BinStream &bs, ColorSet &cs) {
    bs >> cs.mPrimary;
    bs >> cs.mSecondary;
    return bs;
}

BinStreamRev &operator>>(BinStreamRev &d, ColorSet &cs) {
    d.stream >> cs;
    return d;
}

INIT_REVS(1, 0)

// RB3 retail (0x824DFEB0): the packed rev is split into two TU shorts (alt +0,
// rev +4), no version guard, and both vector readers get the raw BinStream.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_ColorPalette;

BEGIN_LOADS(ColorPalette)
    int revs;
    bs >> revs;
    gRevs_ColorPalette.rev = getHmxRev(revs);
    gRevs_ColorPalette.altRev = getAltRev(revs);
    Hmx::Object::Load(bs);
    if (gRevs_ColorPalette.rev < 1) {
        std::vector<ColorSet> vec;
        bs >> vec;
        mColors.clear();
        FOREACH (it, vec) {
            mColors.push_back(it->mPrimary);
        }
    } else
        bs >> mColors;
END_LOADS
