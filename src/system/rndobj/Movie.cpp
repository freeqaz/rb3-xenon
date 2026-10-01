#include "rndobj/Movie.h"
#include "obj/Object.h"
#include "os/File.h"
#include "rndobj/Anim.h"
#include "rndobj/Tex.h"
#include "utl/BinStream.h"
#include "utl/FilePath.h"

RndMovie::RndMovie() : mStream(false), mLoop(true), mTex(this) {}

// Retail (0x82478128) has NO Hmx::Object::Replace fallback -- the body's only
// calls are __RTDynamicCast and SetOwnerObj, then blr.  Hmx::Object::Replace is
// EMPTY in the match build (its whole body is #ifdef HX_NATIVE, Object.cpp:228),
// but with no LTCG an empty out-of-line callee still costs a real `bl`, and
// there is no such `bl` in the retail body.  Kept under HX_NATIVE, where the
// base really does forward to mSinks.
// Retail compares the held pointer RAW here (lwz r10,-8(r11); cmplw r10,r4)
// with no vbtable adjust, and calls SetTex through the vtable slot at +40 --
// both consistent with our source once RefIs() upcasts (the upcast is a no-op
// when Hmx::Object sits at offset 0 in the pointee, as it does for RndTex).
void RndMovie::Replace(ObjRef *from, Hmx::Object *to) {
    if (RefIs(from, mTex)) {
        SetTex(dynamic_cast<RndTex *>(to));
        return;
    }
#ifdef HX_NATIVE
    Hmx::Object::Replace(from, to);
#endif
}

BEGIN_HANDLERS(RndMovie)
    HANDLE_SUPERCLASS(RndAnimatable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(RndMovie)
    SYNC_PROP_SET(
        movie_file,
        FileRelativePath(FilePath::Root().c_str(), mFile.c_str()),
        SetFile(_val.Str(), mStream)
    )
    SYNC_PROP_SET(stream, mStream, SetFile(mFile, _val.Int()))
    SYNC_PROP(loop, mLoop)
    SYNC_PROP_SET(tex, mTex.Ptr(), SetTex(_val.Obj<RndTex>()))
    SYNC_SUPERCLASS(RndAnimatable)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

static int gSaveRev_RndMovie = 8;
BEGIN_SAVES(RndMovie)
    bs << gSaveRev_RndMovie;
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndAnimatable)
    bs << mFile << mTex << mStream << mLoop;
END_SAVES

BEGIN_COPYS(RndMovie)
    CREATE_COPY_AS(RndMovie, t);
    MILO_ASSERT(t, 0x55);
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndAnimatable)
    COPY_MEMBER_FROM(t, mLoop)
    mTex = t->mTex.Ptr();
    SetFile(t->mFile, t->mStream);
END_COPYS

BEGIN_LOADS(RndMovie)
    PreLoad(bs);
    PostLoad(bs);
END_LOADS

INIT_REVS(8, 0)

// RB3 retail (0x82479018 / 0x824780B0) keeps the load revision in a file
// static read with `bs >> gLoadRev` and consulted again by PostLoad; there is no
// BinStreamRev push/pop and no altRev.
static int gLoadRev;

void RndMovie::PreLoad(BinStream &bs) {
    bs >> gLoadRev;
    if (gLoadRev > 6) {
        Hmx::Object::Load(bs);
    }
    RndAnimatable::Load(bs);
    bs >> mFile;
    if (gLoadRev > 3) {
        bs >> mTex;
    }
    if (gLoadRev > 4) {
        bs >> mStream;
    }
    if (gLoadRev > 7 && !mStream) {
        TheLoadMgr.AddLoader(mFile, kLoadFront);
    }
}

void RndMovie::PostLoad(BinStream &bs) {
    if (gLoadRev > 5) {
        bs >> mLoop;
    } else if (gLoadRev == 5) {
        mLoop = !mStream;
    }
    SetFile(mFile, mStream);
}

void RndMovie::SetTex(RndTex *tex) { mTex = tex; }
