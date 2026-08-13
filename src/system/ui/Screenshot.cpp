#include "ui/Screenshot.h"
#include "obj/Object.h"
#include "rndobj/Draw.h"
#include "rndobj/Utl.h"
#include "utl/Loader.h"
#include "rndobj/Rnd.h"

Screenshot::~Screenshot() {
    RELEASE(mTex);
    RELEASE(mMat);
}

Screenshot::Screenshot() : mTex(nullptr), mMat(nullptr) {}

BEGIN_PROPSYNCS(Screenshot)
    // RB3-360 retail's SyncProperty COMDAT tail-returns straight from
    // PropSync(mTexPath,...) for tex_path -- no `andi. op,0x11` gate, no
    // Sync() call (confirmed against target_symbol_map: the branch calls
    // fn_82769330==PropSync(FilePath&,...) then converts the bool result
    // directly, unlike dc3/rb3-Wii's SYNC_PROP_MODIFY[_ALT](..., Sync())).
    {
        static Symbol _s("tex_path");
        if (sym == _s) {
            if (PropSync(mTexPath, _val, _prop, _i + 1, _op))
                return true;
            else
                return false;
        }
    }
    SYNC_SUPERCLASS(RndDrawable)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

static int gSaveRev_Screenshot = 1;
BEGIN_SAVES(Screenshot)
    bs << gSaveRev_Screenshot;
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndDrawable)
    bs << mTexPath;
END_SAVES

BEGIN_COPYS(Screenshot)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndDrawable)
    CREATE_COPY(Screenshot)
    BEGIN_COPYING_MEMBERS
        if (ty != kCopyFromMax)
            COPY_MEMBER(mTexPath)
    END_COPYING_MEMBERS
    Sync();
END_COPYS

INIT_REVS(1, 0)

BEGIN_LOADS(Screenshot)
    LOAD_REVS(bs);
    ASSERT_REVS(1, 0);
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(RndDrawable)
    bs >> mTexPath;
    Sync();
END_LOADS

void Screenshot::DrawShowing() {
    if (!TheRnd.GetDrawMode() && TheLoadMgr.EditMode() && mMat) {
        TheRnd.DrawRect(
            Hmx::Rect(0, 0, TheRnd.Width(), TheRnd.Height()),
            Hmx::Color(0, 0, 0),
            mMat,
            0,
            0
        );
    }
}

void Screenshot::Sync() {
    if (TheLoadMgr.EditMode()) {
        RELEASE(mTex);
        RELEASE(mMat);
        mTex = Hmx::Object::New<RndTex>();
        mTex->SetBitmap(mTexPath);
        mMat = Hmx::Object::New<RndMat>();
        mMat->SetZMode(kZModeDisable);
        mMat->SetDiffuseTex(mTex);
    }
}

BEGIN_HANDLERS(Screenshot)
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS
