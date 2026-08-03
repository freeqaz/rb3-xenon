#include "rndobj/Fur.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "utl/BinStream.h"

RndFur::RndFur()
    : mLayers(12), mThickness(3), mCurvature(2), mShellOut(1), mAlphaFalloff(0.5),
      mStretch(1), mSlide(1), mGravity(1), mFluidity(0.25), mRootsTint(0, 0, 0),
      mEndsTint(1, 1, 1), mFurDetail(this, nullptr), mFurTiling(1),
      mWind(this, nullptr) {}

BEGIN_HANDLERS(RndFur)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(RndFur)
    SYNC_PROP(layers, mLayers)
    SYNC_PROP(thickness, mThickness)
    SYNC_PROP(curvature, mCurvature)
    SYNC_PROP(shell_out, mShellOut)
    SYNC_PROP(alpha_falloff, mAlphaFalloff)
    SYNC_PROP(stretch, mStretch)
    SYNC_PROP(slide, mSlide)
    SYNC_PROP(gravity, mGravity)
    SYNC_PROP(fluidity, mFluidity)
    SYNC_PROP(roots_tint, mRootsTint)
    SYNC_PROP(ends_tint, mEndsTint)
    SYNC_PROP(fur_detail, mFurDetail)
    SYNC_PROP(fur_tiling, mFurTiling)
    SYNC_PROP(wind, mWind)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(RndFur)
    SAVE_REVS(3, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mLayers << mThickness << mCurvature;
    bs << mShellOut << mAlphaFalloff;
    bs << mStretch << mSlide << mGravity << mFluidity;
    bs << mRootsTint << mEndsTint;
    bs << mFurDetail << mFurTiling;
    bs << mWind;
END_SAVES

BEGIN_COPYS(RndFur)
    CREATE_COPY_AS(RndFur, m)
    MILO_ASSERT(m, 0x7D);
    COPY_SUPERCLASS(Hmx::Object)
    COPY_MEMBER_FROM(m, mLayers)
    COPY_MEMBER_FROM(m, mThickness)
    COPY_MEMBER_FROM(m, mCurvature)
    COPY_MEMBER_FROM(m, mShellOut)
    COPY_MEMBER_FROM(m, mAlphaFalloff)
    COPY_MEMBER_FROM(m, mStretch)
    COPY_MEMBER_FROM(m, mSlide)
    COPY_MEMBER_FROM(m, mGravity)
    COPY_MEMBER_FROM(m, mFluidity)
    COPY_MEMBER_FROM(m, mRootsTint)
    COPY_MEMBER_FROM(m, mEndsTint)
    COPY_MEMBER_FROM(m, mFurDetail)
    COPY_MEMBER_FROM(m, mFurTiling)
    COPY_MEMBER_FROM(m, mWind)
END_COPYS

// RB3-360 retail rev storage (same pattern as rndobj/Env.cpp): retail's
// RndFur::Load asm never constructs a BinStreamRev temp (no
// ??_7BinStreamRev@@6B vtable store, no BinStream::BinStream(bool) ctor call)
// -- it inline-splits the packed rev int via getHmxRev/getAltRev straight
// into two mutable file-scope shorts and passes the raw `bs` on to
// Hmx::Object::Load. That's the obj/ObjMacros.h LOAD_REVS/LOAD_SUPERCLASS
// dialect (same shape as rb3-Wii's Fur.cpp), not the obj/Object.h
// BinStreamRev-object dialect this TU otherwise gets from its
// `#include "obj/Object.h"`. The two words must live in ONE aligned(4)
// aggregate (altRev +0, rev +4) -- MSVC does not lay .bss out in declaration
// order, so two separate statics get other globals interleaved between them
// and will not fold onto one base register.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_Fur;
#define gAltRev gRevs_Fur.altRev
#define gRev gRevs_Fur.rev

void RndFur::Load(BinStream &bs) {
    int rev;
    bs >> rev;
    gRev = getHmxRev(rev);
    gAltRev = getAltRev(rev);
    Hmx::Object::Load(bs);
    bs >> mLayers;
    bs >> mThickness;
    bs >> mCurvature;
    if (gRev > 1) {
        bs >> mShellOut;
        bs >> mAlphaFalloff;
    }
    bs >> mStretch;
    bs >> mSlide;
    bs >> mGravity;
    bs >> mFluidity;
    bs >> mRootsTint >> mEndsTint;
    bs >> mFurDetail;
    bs >> mFurTiling;
    if (gRev > 2) {
        bs >> mWind;
    }
}

bool RndFur::LoadOld(BinStreamRev &d) {
    bool ret;
    d >> ret;
    if (ret || d.rev < 0x20) {
        d >> mLayers;
        d >> mThickness;
        d >> mCurvature;
        d >> mStretch;
        d >> mSlide;
        d >> mGravity;
        d >> mFluidity;
        d.stream >> mRootsTint >> mEndsTint;
        if (d.rev > 0x1E) {
            d.stream >> mFurDetail >> mFurTiling;
        }
        if (d.rev < 0x24) {
            Vector3 v;
            float f;
            d.stream >> v >> f;
        }
    }
    return ret;
}
