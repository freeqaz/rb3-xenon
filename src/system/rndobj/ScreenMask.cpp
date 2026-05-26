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
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

RndScreenMask::RndScreenMask()
    : mMat(this), mColor(1, 1, 1, 1), mRect(0, 0, 1, 1), mUseCamRect(false) {}

INIT_REVS(2, 0)

BEGIN_LOADS(RndScreenMask)
    LOAD_REVS(bs)
    ASSERT_REVS(2, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(RndDrawable)
    d >> mMat;
    d >> mColor;
    if (d.rev > 0) {
        d >> mRect;
    }
    if (d.rev > 1) {
        d >> mUseCamRect;
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
