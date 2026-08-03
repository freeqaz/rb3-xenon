#include "rndobj/ScreenMask.h"
#include "math/Geo.h"
#include "os/Debug.h"
#include "rndobj/Cam.h"
#include "rndobj/Draw.h"
#include "rndobj/HiResScreen.h"
#include "rndobj/Rnd.h"
#include "utl/BinStream.h"

void RndScreenMask::Save(BinStream &bs) {
    bs << 2;
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndDrawable)
    bs << mMat << mColor << mRect << mUseCamRect;
}

BEGIN_HANDLERS(RndScreenMask)
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_COPYS(RndScreenMask)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndDrawable)
    CREATE_COPY(RndScreenMask)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mMat)
        COPY_MEMBER(mColor)
        COPY_MEMBER(mRect)
        COPY_MEMBER(mUseCamRect)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_PROPSYNCS(RndScreenMask)
    SYNC_PROP(mat, mMat)
    SYNC_PROP(color, mColor)
    SYNC_PROP(alpha, mColor.alpha)
    SYNC_PROP(screen_rect, mRect)
    SYNC_PROP(use_cam_rect, mUseCamRect)
    SYNC_SUPERCLASS(RndDrawable)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

RndScreenMask::RndScreenMask()
    : mMat(this), mColor(1, 1, 1, 1), mRect(0, 0, 1, 1), mUseCamRect(false) {}

// RB3-360 retail rev dialect (rb3-Wii/ObjMacros shape): the packed rev is split
// into two HALFWORDS stored four bytes apart onto ONE internal-linkage align(4)
// base, and the RAW incoming BinStream is forwarded to every read and to the
// superclass Load.  DC3's Object.h BinStreamRev stack decorator additionally
// emits ??0BinStream, a ??_7BinStreamRev@@6B@ vtable store and a ??1BinStream
// destructor that retail has none of, and dispatches each read on `&d`.
//
// Written longhand rather than by including obj/ObjMacros.h: that header also
// swaps the SYNC_PROP and HANDLE families, which are already byte-exact here.
// The pair MUST share one aggregate -- two separate file statics are laid out
// independently and will not fold onto a single base register.  No `#define
// gRev` alias: several of these TUs are scatter-INCLUDED into another unit
// (e.g. rndobj/Anim.cpp includes rndobj/MotionBlur.cpp) whose own gRev macro
// the alias would silently shadow for the rest of the amalgamated TU.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_ScreenMask;
BEGIN_LOADS(RndScreenMask)
    int rev;
    bs >> rev;
    gRevs_ScreenMask.rev = getHmxRev(rev);
    gRevs_ScreenMask.altRev = getAltRev(rev);
    Hmx::Object::Load(bs);
    RndDrawable::Load(bs);
    bs >> mMat;
    bs >> mColor;
    if (gRevs_ScreenMask.rev > 0) {
        bs >> mRect;
    }
    if (gRevs_ScreenMask.rev > 1) {
        bs >> mUseCamRect;
    }
END_LOADS

void RndScreenMask::DrawShowing() {
    if (TheRnd.GetDrawMode() != Rnd::kDrawNormal)
        return;

    float width = (float)TheRnd.Width();
    RndCam *cam = RndCam::Current();

    float height = (float)TheRnd.Height();
    RndTex *targetTex = cam->TargetTex();
    if ((int)targetTex) {
        height = (float)targetTex->Height();
        width = (float)targetTex->Width();
    }

    if (!mUseCamRect && (int)targetTex) {
        Hmx::Rect defaultRect(0.0f, 0.0f, 1.0f, 1.0f);
        if (!(cam->GetScreenRect() == defaultRect)) {
            MILO_NOTIFY_ONCE(
                "%s: Overriding camera screen_rect not supported with render texture",
                (char *)Name()
            );
        }
    }

    if (!mUseCamRect && !cam->TargetTex()) {
        TheRnd.GetDefaultCam()->Select();
        Hmx::Rect hiRes = TheHiResScreen.InvScreenRect();
        Hmx::Rect drawRect;
        drawRect.x = (mRect.x * hiRes.w + hiRes.x) * width;
        drawRect.y = (mRect.y * hiRes.h + hiRes.y) * height;
        drawRect.w = (mRect.w * hiRes.w) * width;
        drawRect.h = (mRect.h * hiRes.h) * height;
        TheRnd.DrawRect(drawRect, mColor, mMat, nullptr, nullptr);
        cam->Select();
    } else {
        Hmx::Rect hiRes = TheHiResScreen.InvScreenRect();
        Hmx::Rect drawRect;
        drawRect.x = (mRect.x * hiRes.w + hiRes.x) * width;
        drawRect.y = (mRect.y * hiRes.h + hiRes.y) * height;
        drawRect.w = (mRect.w * hiRes.w) * width;
        drawRect.h = (mRect.h * hiRes.h) * height;
        TheRnd.DrawRect(drawRect, mColor, mMat, nullptr, nullptr);
    }
}
