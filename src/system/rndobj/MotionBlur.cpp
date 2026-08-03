#include "rndobj/MotionBlur.h"
#include "obj/Object.h"
#include "rndobj/Dir.h"
#include "rndobj/Draw.h"
#include "rndobj/Group.h"
#include "rndobj/Mesh.h"
#include "rndobj/PostProc.h"
#include "utl/BinStream.h"

RndMotionBlur::RndMotionBlur() : mDrawList(this) {}

BEGIN_HANDLERS(RndMotionBlur)
    HANDLE(allowed_drawable, OnAllowedDrawable)
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(RndMotionBlur)
    SYNC_PROP(draw_list, mDrawList)
    SYNC_SUPERCLASS(RndDrawable)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(RndMotionBlur)
    SAVE_REVS(1, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndDrawable)
    bs << mDrawList;
END_SAVES

BEGIN_COPYS(RndMotionBlur)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndDrawable)
    CREATE_COPY(RndMotionBlur)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mDrawList)
    END_COPYING_MEMBERS
END_COPYS

// RB3-360 retail rev dialect (rb3-Wii/ObjMacros shape), not DC3's Object.h
// BinStreamRev stack decorator.  Adjudicated on retail bytes at 0x82493EA8
// (148 B): the body reads the packed rev, stores `revs >> 16` and `revs` as two
// HALFWORDS four bytes apart onto ONE internal-linkage base (lbl_82CC6A48 +0/+4)
// and then passes the RAW incoming BinStream to every read and to both
// superclass Loads.  Our BinStreamRev form emitted ??0BinStream, a
// ??_7BinStreamRev@@6B@ vtable store and a ??1BinStream destructor that retail
// has none of, and dispatched each read on `&d` instead of `bs`.
//
// Spelled out longhand rather than by including obj/ObjMacros.h: that header
// also swaps the SYNC_PROP and HANDLE families, which are already byte-exact
// here under the Object.h dialect.  Same established lever as rndobj/Flare.cpp.
// The pair MUST live in ONE aligned(4) aggregate -- two separate file statics
// are laid out independently and cannot fold onto a single base register.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_MotionBlur;
// No `#define gRev` alias here: rndobj/Anim.cpp scatter-INCLUDES this file and
// wraps the include in its own `#define gRev gRev_MotionBlur`, which such an
// alias would silently shadow for the rest of the amalgamated TU.

BEGIN_LOADS(RndMotionBlur)
    int rev;
    bs >> rev;
    gRevs_MotionBlur.rev = getHmxRev(rev);
    gRevs_MotionBlur.altRev = getAltRev(rev);
    Hmx::Object::Load(bs);
    RndDrawable::Load(bs);
    bs >> mDrawList;
END_LOADS

void RndMotionBlur::DrawShowing() {
    RndPostProc *cur = RndPostProc::Current();
    if (cur) {
        FOREACH (it, mDrawList) {
            cur->QueueMotionBlurObject(*it);
        }
    }
}

DataNode RndMotionBlur::OnAllowedDrawable(const DataArray *da) {
    int allowcount = 0;
    for (ObjDirItr<RndDrawable> it(Dir(), true); it != nullptr; ++it) {
        if (CanMotionBlur(it))
            allowcount++;
    }
    DataArrayPtr ptr(new DataArray(allowcount));
    allowcount = 0;
    for (ObjDirItr<RndDrawable> it(Dir(), true); it != nullptr; ++it) {
        if (CanMotionBlur(it)) {
            ptr->Node(allowcount++) = &*it;
        }
    }
    return ptr;
}

bool RndMotionBlur::CanMotionBlur(RndDrawable *d) {
    if (dynamic_cast<RndMesh *>(d) || dynamic_cast<RndDir *>(d)
        || dynamic_cast<RndGroup *>(d)) {
        return true;
    } else {
        return false;
    }
}
