// PatchRenderer implementation, written from retail bytes 0x822AE130-0x822AF1C8.
// Compiled into BandSwatch's object (#included from BandSwatch.cpp): retail
// places this code inside BandSwatch's .text span.
#include "bandobj/PatchRenderer.h"
#include "obj/ObjMacros.h"
#include "os/System.h"
#include "rndobj/Rnd.h"
#include "Memory.h"

// Retail keeps the two patch dirs at 0x82CBCD78 (blank) / 0x82CBCD7C (test).
RndDir *PatchRenderer::sBlankPatch;
RndDir *PatchRenderer::sTestPatch;

// Load stores both rev halves through one base register at offsets 0/4 (the
// file-scope aggregate shape; see gSwatchRevs in BandSwatch.cpp).
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gPatchRevs;

void PatchRenderer::Init() {
    PhysMemTypeTracker tracker("D3D(phys):Global");
    SystemConfig("objects", "PatchRenderer");
    sBlankPatch = Hmx::Object::New<RndDir>();
    REGISTER_OBJ_FACTORY(PatchRenderer)
}

void PatchRenderer::Terminate() {
    RELEASE(sTestPatch);
    RELEASE(sBlankPatch);
}

PatchRenderer::PatchRenderer()
    : mBackMat(this, nullptr), mOverlayMat(this, nullptr), mTestMode("blank"),
      mPosition("front") {}

// The ctor never sets the test patch: SetPatch(nullptr) falls back to the
// blank patch, and the "test_mode" property chooses between the two.
void PatchRenderer::SetPatch(RndDir *dir) { SetDraw(dir ? dir : sBlankPatch); }

void PatchRenderer::DrawShowing() { RndTexRenderer::DrawShowing(); }

void PatchRenderer::DrawBefore() {
    mSavedEnv = RndEnviron::Current();
    if (mBackMat) {
        Hmx::Rect r(0, 0, mOutputTexture->Width(), mOutputTexture->Height());
        TheRnd.DrawRect(r, Hmx::Color(1, 1, 1, 1), mBackMat, nullptr, nullptr);
    }
}

void PatchRenderer::DrawAfter() {
    if (mOverlayMat) {
        Hmx::Rect r(0, 0, mOutputTexture->Width(), mOutputTexture->Height());
        TheRnd.DrawRect(r, Hmx::Color(1, 1, 1, 1), mOverlayMat, nullptr, nullptr);
    }
    if (mSavedEnv != RndEnviron::Current())
        mSavedEnv->Select(nullptr);
}

BEGIN_HANDLERS(PatchRenderer)
    HANDLE_SUPERCLASS(RndTexRenderer)
    HANDLE_CHECK(0x84)
END_HANDLERS

// Each property Symbol is a guarded function-local static built right before
// its compare (one guard word, bits 1/2/4/8 in this order).
BEGIN_PROPSYNCS(PatchRenderer)
    static Symbol test_mode("test_mode");
    SYNC_PROP_MODIFY_ALT(
        test_mode, mTestMode, SetPatch(mTestMode == "test" ? sTestPatch : sBlankPatch)
    )
    static Symbol position("position");
    SYNC_PROP(position, mPosition)
    static Symbol back_mat("back_mat");
    SYNC_PROP(back_mat, mBackMat)
    static Symbol overlay_mat("overlay_mat");
    SYNC_PROP(overlay_mat, mOverlayMat)
    SYNC_SUPERCLASS(RndTexRenderer)
END_PROPSYNCS

void PatchRenderer::Save(BinStream &bs) {
    bs << 1;
    RndTexRenderer::Save(bs);
    bs << mTestMode;
    bs << mPosition;
    bs << mBackMat;
    bs << mOverlayMat;
}

// Retail copies the members first and calls RndTexRenderer::Copy last, passing
// the cast pointer, with no null test on the cast.
void PatchRenderer::Copy(const Hmx::Object *o, Hmx::Object::CopyType ty) {
    const PatchRenderer *c = dynamic_cast<const PatchRenderer *>(o);
    mBackMat = c->mBackMat;
    mOverlayMat = c->mOverlayMat;
    mTestMode = c->mTestMode;
    mPosition = c->mPosition;
    RndTexRenderer::Copy(c, ty);
}

void PatchRenderer::Load(BinStream &bs) {
    int rev;
    bs >> rev;
    gPatchRevs.rev = getHmxRev(rev);
    gPatchRevs.altRev = getAltRev(rev);
    RndTexRenderer::Load(bs);
    bs >> mTestMode;
    bs >> mPosition;
    if (gPatchRevs.rev != 0) {
        bs >> mBackMat;
        bs >> mOverlayMat;
    }
}
