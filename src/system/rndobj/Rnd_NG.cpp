#include "rndobj/Rnd_NG.h"
#include "Env_NG.h"
#include "PostProc.h"
#include "math/Color.h"
#include "math/Vec.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Cam.h"
#include "rndobj/Flare.h"
#include "rndobj/Fur_NG.h"
#include "rndobj/Lit_NG.h"
#include "rndobj/Mat_NG.h"
#include "rndobj/Overlay.h"
#include "rndobj/ShaderMgr.h"
#include "rndobj/ShadowMap.h"
#include "rndobj/SoftParticleBuffer.h"
#include "rndobj/Stats_NG.h"
#include "rndobj/Tex.h"
// ⛔ The ??$MakeString@HH@@ row in this unit IS NOT SOURCE-CLOSABLE, and the
// splits pin behind it is WRONG (lane DS-4/C). Do not perturb the MakeString
// call sites below trying to move it.
//
// splits.txt pins a second .text block 0x82399348-0x823993A4 to Rnd_NG and the
// map names 0x82399348 `??$MakeString@HH@@YAPBDPBDHH@Z`. Retail's body there is
// NOT a MakeString: it is `if (this != &o) { reserve(o.size()); vector<
// HamIKEffector::Constraint>::operator=(...) }` -- element size 16 via
// `srawi r4,r11,4`, and it CALLS ??4?$vector@VConstraint@HamIKEffector@@.
// Map-independent proof: every MakeString must build a FormatString, whose
// ~2 KB buffer forces a huge frame. A scan of ALL 114 bl-sites targeting
// ??0FormatString@@QAA@PBD@Z (0x827c4380) finds frames of 0x870-0x8b0 WITHOUT
// EXCEPTION (our own base frame is 0x880). 0x82399348's frame is 0x70, so it
// cannot be any MakeString instantiation.
//
// Nor can it be repinned, because retail has NO distinct MakeString<int,int>:
// those 114 sites resolve to 34 map-named instantiations plus exactly ONE
// unnamed small caller, 0x8278f3b8 (0x5c/4 calls) -- and that one is REFUTED as
// <int,int>: its two streamed args go to DIFFERENT callees (0x827c40e8 int-class
// vs 0x827c4240 float-class), i.e. it is MakeString<int,float>. <int,int> needs
// both in one class. ⚠ Note 0x827c40e8 serves int, char, uchar AND char*, so it
// is an ICF FOLD CLASS, not a type -- it can only refute, never confirm.
// Consistent with that, build/45410914/icf_aliases.map carries
// ??$MakeString@PBDPBD@@ @0x8229D148 as a synthetic ICF-alias row and no
// ??$MakeString@HH@@ row at all: <int,int> was folded away, so there is no home
// address to pin. Fixing this is map/splits work (drop or re-home the block).
#include "utl/MakeString.h"
#include "rndobj/PostProc_NG.h"
#include "rndobj/DOFProc_NG.h"

extern "C" void OnlyReturns();

NgStats gNgStats[3];
NgStats *TheNgStats = &gNgStats[0];

NgRnd::NgRnd()
    : mViewport(), mLowRes(0), mShadowMap(0), mShadowCam(0), mOcclusionQueryMgr(0), mParticleBuffer(0),
      mInited(0) {}

NgRnd::~NgRnd() {}

void NgRnd::PreInit() {
    if (!mInited) {
        mInited = true;
        Rnd::PreInit();
        REGISTER_OBJ_FACTORY(NgEnviron)
        REGISTER_OBJ_FACTORY(NgMat)
        NgLight::Init();
        REGISTER_OBJ_FACTORY(NgFur)
        RndShadowMap::Init();
        REGISTER_OBJ_FACTORY(RndSoftParticleBuffer)
#ifndef HX_NATIVE
        // On native, skip re-creating defaults with Ng types — basic Rnd types suffice
        CreateDefaults();
#endif
    }
}

void NgRnd::Init() {
    PreInit();
    TheShaderMgr.Init();
    mPointTestQueries.reserve(0x200);
    Rnd::Init();
    mParticleBuffer = Hmx::Object::New<RndSoftParticleBuffer>();
}

void NgRnd::ReInit() { TheShaderMgr.InitShaders(); }

void NgRnd::Terminate() {
    RELEASE(mOcclusionQueryMgr);
    RELEASE(mParticleBuffer);
    TheShaderMgr.Terminate();
    NgPostProc::Terminate();
    RndShadowMap::Terminate();
    NgDOFProc::Terminate();
    Rnd::Terminate();
}

void NgRnd::SetShadowMap(RndTex *tex, RndCam *cam, const Hmx::Color *color) {
    mShadowMap = tex;
    mShadowCam = cam;
    if (cam) {
        const Vector3 &v3 = cam->WorldXfm().m.y;
        Vector4 v4(v3.x, v3.y, v3.z, 1);
        TheShaderMgr.SetPConstant(kPS_ShadowCamDir, v4);
    }
    if (color) {
        Vector4 v4 = Vector4(color->red, color->green, color->blue, color->alpha);
        TheShaderMgr.SetPConstant(kPS_ShadowColor, v4);
    }
    if (tex) {
        TheShaderMgr.SetPConstant(kPS_Texture, tex);
    }
}

void NgRnd::RemovePointTest(RndFlare *flare) {
    Rnd::RemovePointTest(flare);
    FOREACH (it, mPointTestQueries) {
        if (it->mFlare == flare) {
            mOcclusionQueryMgr->ReleaseQuery(it->mAreaQueryIdx);
            mOcclusionQueryMgr->ReleaseQuery(it->mPointQueryIdx);
            mPointTestQueries.erase(it);
            return;
        }
    }
}

void NgRnd::DoPostProcess() { Rnd::DoPostProcess(); }

void NgRnd::CreateLargeQuad(int, int, LargeQuadRenderData &) {
#ifndef HX_NATIVE
    MILO_FAIL("NgRnd::CreateLargeQuad not implemented!");
#endif
}

void NgRnd::DrawLargeQuad(
    const LargeQuadRenderData &, const Transform &, RndMat *, ShaderType
) {
#ifndef HX_NATIVE
    MILO_FAIL("NgRnd::DrawLargeQuad not implemented!");
#endif
}

void NgRnd::SetVertShaderTex(RndTex *, int) {
#ifndef HX_NATIVE
    MILO_FAIL("NgRnd::SetVertShaderTex not implemented!");
#endif
}

void NgRnd::ResetStats() {
    if (mProcCmds == kProcessWorld || mProcCmds == kProcessAll) {
        TheNgStats = &gNgStats[0];
    } else if (mProcCmds == kProcessPost) {
        TheNgStats = &gNgStats[1];
    } else {
        TheNgStats = &gNgStats[2];
    }
    memset(TheNgStats, 0, sizeof(NgStats));
#ifdef HX_NATIVE
    TheNgStats->mCams++;
#endif
}

float EstimateDraw(int idx) {
    return ((float)gNgStats[idx].mFlares * 0.017f + ((float)gNgStats[idx].mMotionBlurs * 0.003f + ((float)gNgStats[idx].mMultiMeshInsts * 0.001f + ((float)gNgStats[idx].mBones * 0.00126f + ((float)gNgStats[idx].mLightsReal * 0.001f + ((float)gNgStats[idx].mCams * 0.0068f + ((float)gNgStats[idx].mMats * 0.0097f + ((float)gNgStats[idx].mPartSys * 0.005f + ((float)gNgStats[idx].mMutMeshes * 0.0112f + ((float)gNgStats[idx].mRegMeshes * 0.0028f + ((float)gNgStats[idx].mParts * 0.00023333334f + (float)gNgStats[idx].mLightsApprox * 0.01f)))))))))));
}

float NgRnd::UpdateOverlay(RndOverlay *overlay, float y) {
    if (overlay == mStatsOverlay) {
        mStatsOverlay->Clear();
        if (mProcCmds == kProcessWorld || mProcCmds == kProcessPost) {
            *mStatsOverlay
                << MakeString("faces %d %d\n", gNgStats[0].mFaces, gNgStats[1].mFaces);
            *mStatsOverlay
                << MakeString("parts %d %d\n", gNgStats[0].mParts, gNgStats[1].mParts);
            *mStatsOverlay << MakeString(
                "part_sys %d %d\n", gNgStats[0].mPartSys, gNgStats[1].mPartSys
            );
            *mStatsOverlay << MakeString(
                "reg_meshes %d %d\n", gNgStats[0].mRegMeshes, gNgStats[1].mRegMeshes
            );
            *mStatsOverlay << MakeString(
                "mut_meshes %d %d\n", gNgStats[0].mMutMeshes, gNgStats[1].mMutMeshes
            );
            *mStatsOverlay
                << MakeString("bones %d %d\n", gNgStats[0].mBones, gNgStats[1].mBones);
            *mStatsOverlay
                << MakeString("mats %d %d\n", gNgStats[0].mMats, gNgStats[1].mMats);
            *mStatsOverlay
                << MakeString("cams %d %d\n", gNgStats[0].mCams, gNgStats[1].mCams);
            *mStatsOverlay << MakeString(
                "lights (real) %d %d\n", gNgStats[0].mLightsReal, gNgStats[1].mLightsReal
            );
            *mStatsOverlay << MakeString(
                "lights (approx) %d %d\n",
                gNgStats[0].mLightsApprox,
                gNgStats[1].mLightsApprox
            );
            *mStatsOverlay << MakeString(
                "multimesh instances %d %d\n",
                gNgStats[0].mMultiMeshInsts,
                gNgStats[1].mMultiMeshInsts
            );
            *mStatsOverlay << MakeString(
                "multimesh batches %d %d\n",
                gNgStats[0].mMultiMeshBatches,
                gNgStats[1].mMultiMeshBatches
            );
            *mStatsOverlay
                << MakeString("flares %d %d\n", gNgStats[0].mFlares, gNgStats[1].mFlares);
            *mStatsOverlay << MakeString(
                "motion blur %d %d\n", gNgStats[0].mMotionBlurs, gNgStats[1].mMotionBlurs
            );
            *mStatsOverlay
                << MakeString("est draw %.1f %.1f\n", EstimateDraw(0), EstimateDraw(1));
            TheNgStats = &gNgStats[2];
        } else {
            *mStatsOverlay << MakeString("faces %d\n", gNgStats[0].mFaces);
            *mStatsOverlay << MakeString("parts %d\n", gNgStats[0].mParts);
            *mStatsOverlay << MakeString("part_sys %d\n", gNgStats[0].mPartSys);
            *mStatsOverlay << MakeString("reg_meshes %d\n", gNgStats[0].mRegMeshes);
            *mStatsOverlay << MakeString("mut_meshes %d\n", gNgStats[0].mMutMeshes);
            *mStatsOverlay << MakeString("bones %d\n", gNgStats[0].mBones);
            *mStatsOverlay << MakeString("mats %d\n", gNgStats[0].mMats);
            *mStatsOverlay << MakeString("cams %d\n", gNgStats[0].mCams);
            *mStatsOverlay << MakeString("lights (real) %d\n", gNgStats[0].mLightsReal);
            *mStatsOverlay
                << MakeString("lights (approx) %d\n", gNgStats[0].mLightsApprox);
            *mStatsOverlay
                << MakeString("multimesh instances %d\n", gNgStats[0].mMultiMeshInsts);
            *mStatsOverlay
                << MakeString("multimesh batches %d\n", gNgStats[0].mMultiMeshBatches);
            *mStatsOverlay << MakeString("flares %d\n", gNgStats[0].mFlares);
            *mStatsOverlay << MakeString("motion blur %d\n", gNgStats[0].mMotionBlurs);
            *mStatsOverlay << MakeString("est draw %.1f\n", EstimateDraw(0));
            TheNgStats = &gNgStats[2];
        }
        return y;
    } else {
        return Rnd::UpdateOverlay(overlay, y);
    }
}
