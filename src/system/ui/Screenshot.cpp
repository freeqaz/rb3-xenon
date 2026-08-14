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
    // NOTE (lane INSDEL-4): retail's Copy does NOT call Sync() -- dropping the
    // call closed the row (92.938 -> 100, +128 B). Retail's body ends straight
    // after the mTexPath String assign with `addi r1,r1,112; b __restgprlr_29`.
    // Two controls, both readable before editing:
    //  (1) SIZE INEQUALITY IN THE DIRECTION THAT EXCLUDES INLINING -- target
    //      128 B vs our 136 B, exactly the 8 bytes of `subi r3,r31,60` +
    //      `bl ?Sync@Screenshot@@AAAXXZ`. Sync() is large (two RELEASEs, two
    //      Hmx::Object::New, SetBitmap, SetZMode, SetDiffuseTex), so an INLINED
    //      Sync would make retail much BIGGER; retail is smaller by exactly the
    //      call sequence, so it is absent rather than inlined.
    //  (2) The note at the top of this file records a prior lane finding the
    //      SAME divergence at an adjacent site: retail's Screenshot::SyncProperty
    //      also has no Sync() call where dc3/rb3-Wii do.
    // Load() keeps its Sync() -- retail's Load is anonymous in the map so it
    // could not be used as the control, and it was left untouched.
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
