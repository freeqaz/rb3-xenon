#include "rndobj/Lit_NG.h"
#include "Lit.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/System.h"
#include "rndobj/Draw.h"
#include "../../Memory.h"
#include "rndobj/Lit.h"
#include "rndobj/Cam.h"
#include "rndobj/Mat.h"
#include "rndobj/Rnd.h"
#include "rndobj/Rnd_NG.h"
#include "rndobj/ShaderMgr.h"
#include "rnddx9/RenderState.h"
#include "math/Mtx.h"
#include <cstring>

// No gfx-mode test: retail inlines this as mShadowOverride && size() != 0.
bool NgLight::WantShadows() const {
    return mShadowOverride && !mShadowOverride->empty();
}

bool NgLight::HaveShadows(std::vector<RndDrawable *> &draws) {
    MILO_ASSERT(mShadowOverride && !mShadowOverride->empty(), 0x3D);
    for (ObjPtrList<RndDrawable>::iterator it = mShadowOverride->begin();
         it != mShadowOverride->end();
         ++it) {
        RndDrawable *cur = *it;
        Sphere s;
        if (!cur->MakeWorldSphere(s, false) || SphereConeTest(s.center, s.radius)) {
            draws.push_back(cur);
        }
    }
    return !draws.empty();
}

BEGIN_COPYS(NgLight)
    COPY_SUPERCLASS(RndLight)
    CheckShadowMap();
END_COPYS

BEGIN_LOADS(NgLight)
    RndLight::Load(bs);
    CheckShadowMap();
END_LOADS

NgLight::~NgLight() { RELEASE(mShadowRT); }

NgLight::NgLight() : mShadowRT(0), mShadowMapTex(0), mShadowDrawCount(-1) {}

// Ported from DC3's newer body (dc3-decomp rndobj/Lit_NG.cpp, whose RESIDUAL
// note explains the rest): it replaces an older hand spelling here (single
// `proj` scalar, plain perp/topPoint arithmetic) and lifts the row 50.0 ->
// 63.2 canonical against retail 0x82B8A128.  Retail agrees with it on the
// prologue, both early-outs, the `sc -= xfm1.v` write-back to 0x50, the
// dirTop-then-dirBot copy order and the reciprocal `1.0f / Dot`
// (lbl_820009FC).  RESIDUAL is liveness: ours saves r24-r31 + f26-f31 in a
// 0x140 frame, retail only r27-r31 + f30/f31 in 0x100 -- after Normalize
// retail keeps every float in f0-f13.
// NEGATIVE RESULT (w17-lit, 2026-09-30): a 200-step random reorder of the
// eighteen post-Normalize statements reached 68.4, but only with orders the
// retail copy sequence contradicts (toSphere declared first although retail
// copies sphereCenter into 0x60 after botPoint; closest copied after edgeDir
// although retail copies 0x60 -> 0xb0 first).  Not kept: a score fit, not a
// source.
bool NgLight::SphereConeTest(const Vector3 &sphereCenter, float sphereRadius) {
    const Transform &xfm1 = WorldXfm();
    const Transform &xfm2 = WorldXfm();

    Vector3 sc = sphereCenter;
    sc -= xfm1.v;

    // MSVC materialises each of the three products once and re-derives the
    // sum at every use site; naming the products is what stops it contracting
    // them into fmadds.
    float py = xfm2.m.y.y * sc.y;
    float pz = xfm2.m.y.z * sc.z;
    float px = xfm2.m.y.x * sc.x;

    if (px + pz + py < -sphereRadius) {
        return false;
    }

    float range = mRange;
    if (px + pz + py > range + sphereRadius) {
        return false;
    }

    Vector3 axisProj = xfm2.m.y;
    axisProj *= pz + (px + py);

    Vector3 perp = sc;
    perp -= axisProj;

    Vector3 dir = perp;
    Normalize(dir, dir);

    float topR = mTopRadius;
    float botR = mBotRadius;

    Vector3 topPoint = xfm1.v;
    // dirTop is declared first because the image claims its slot first: the
    // two 16-byte copies out of `dir` go 0x70 -> 0xa0 (dirTop, the one later
    // scaled by mTopRadius at 0xa4/0xa8) and only then 0x70 -> 0xb0 (dirBot).
    // Worth one callee-saved FPR and 0x10 of frame: with this order the
    // prologue is __savefpr_26 and the frame Δ is +0x40, the other way round
    // it is __savefpr_25 and +0x50.
    Vector3 dirTop = dir;
    Vector3 dirBot = dir;
    Vector3 axisRange = xfm2.m.y;
    Vector3 botPoint = xfm1.v;
    Vector3 toSphere = sphereCenter;

    dirTop *= topR;
    topPoint += dirTop;

    axisRange *= range;
    botPoint += axisRange;

    toSphere -= topPoint;

    dirBot *= botR;
    Vector3 conePoint = botPoint;
    conePoint += dirBot;

    Vector3 closest = toSphere;

    Vector3 edgeDir = conePoint;
    edgeDir -= topPoint;

    // The image divides ONE into the squared length and multiplies; it does
    // NOT divide numerator by denominator.  0x826B9414 `lis r8,
    // __real@3f800000@ha` / 0x826B9424 `lfs f7, __real@3f800000@l(r8)`, then
    // `fdivs f12, f7, f12` and `fmuls f12, f12, f13`.  A plain `a / b` emits a
    // single `fdivs f12, f12, f13` here and the 1.0f literal never appears at
    // all -- /fp:fast does NOT introduce the reciprocal on its own (we are
    // built with it, and it did not), so the reciprocal is in the source.
    // Different arithmetic, not just different instructions.
    //
    // NEGATIVE RESULT (w7-as, 2026-09-14): this is a deliberate LOSS.  Faithful
    // reciprocal + faithful dirTop/dirBot order = 63.1 canonical; the unfaithful
    // `Dot(a,b) / Dot(b,b)` + reversed order scored 65.6.  Measured 4 ways:
    //   plain divide, dirBot first  65.6   (single fdivs, no 1.0f -- unfaithful)
    //   plain divide, dirTop first  62.3
    //   reciprocal,   dirBot first  61.9
    //   reciprocal,   dirTop first  63.1   <- kept
    // The reciprocal row itself MATCHES in the kept spelling; the 2.5pp is
    // paid in where MSVC schedules the `lis`/`lfs` pair and the regalloc that
    // follows it.
    float invEdgeLenSq = 1.0f / Dot(edgeDir, edgeDir);
    float t = Dot(toSphere, edgeDir) * invEdgeLenSq;

    Vector3 scaled = edgeDir;
    scaled *= t;
    closest -= scaled;

    if (Dot(dir, closest) < 0.0f) {
        return true;
    }
    return Length(closest) < sphereRadius;
}

namespace Hmx {
    Matrix4 operator*(const Transform &t, const Matrix4 &b) {
        Matrix4 out;

        { Vector3 ca = b.Col3(0); out.x.x = ca.z * t.m.x.z + ca.y * t.m.x.y + ca.x * t.m.x.x; }
        { Vector3 cb = b.Col3(1); out.x.y = cb.z * t.m.x.z + cb.y * t.m.x.y + cb.x * t.m.x.x; }
        { Vector3 ca = b.Col3(2); out.x.z = ca.z * t.m.x.z + ca.y * t.m.x.y + ca.x * t.m.x.x; }
        { Vector3 cb = b.Col3(3); out.x.w = cb.z * t.m.x.z + cb.y * t.m.x.y + cb.x * t.m.x.x; }

        { Vector3 ca = b.Col3(0); out.y.x = ca.z * t.m.y.z + ca.y * t.m.y.y + ca.x * t.m.y.x; }
        { Vector3 cb = b.Col3(1); out.y.y = cb.z * t.m.y.z + cb.y * t.m.y.y + cb.x * t.m.y.x; }
        { Vector3 ca = b.Col3(2); out.y.z = ca.z * t.m.y.z + ca.y * t.m.y.y + ca.x * t.m.y.x; }
        { Vector3 cb = b.Col3(3); out.y.w = cb.z * t.m.y.z + cb.y * t.m.y.y + cb.x * t.m.y.x; }

        { Vector3 ca = b.Col3(0); out.z.x = ca.z * t.m.z.z + ca.y * t.m.z.y + ca.x * t.m.z.x; }
        { Vector3 cb = b.Col3(1); out.z.y = cb.z * t.m.z.z + cb.y * t.m.z.y + cb.x * t.m.z.x; }
        { Vector3 ca = b.Col3(2); out.z.z = ca.z * t.m.z.z + ca.y * t.m.z.y + ca.x * t.m.z.x; }
        { Vector3 cb = b.Col3(3); out.z.w = cb.z * t.m.z.z + cb.y * t.m.z.y + cb.x * t.m.z.x; }

        { Vector3 ca = b.Col3(0); out.w.x = ca.z * t.v.z + ca.y * t.v.y + ca.x * t.v.x + b.w.x; }
        { Vector3 cb = b.Col3(1); out.w.y = cb.z * t.v.z + cb.y * t.v.y + cb.x * t.v.x + b.w.y; }
        { Vector3 ca = b.Col3(2); out.w.z = ca.z * t.v.z + ca.y * t.v.y + ca.x * t.v.x + b.w.z; }
        { Vector3 cb = b.Col3(3); out.w.w = cb.z * t.v.z + cb.y * t.v.y + cb.x * t.v.x + b.w.w; }

        return out;
    }
}

static Transform sIdentityXfm;
static int sIdentityXfmInited;

void NgLight::SetShadowTransforms() {
    if (!(sIdentityXfmInited & 1)) {
        sIdentityXfmInited |= 1;
        Vector3 identityV;
        identityV.Set(0.0f, 0.0f, 0.0f);
        Hmx::Matrix3 identityM;
        identityM.x.Set(1.0f, 0.0f, 0.0f);
        identityM.y.Set(0.0f, 0.0f, 1.0f);
        identityM.z.Set(0.0f, 1.0f, 0.0f);
        sIdentityXfm.m = identityM;
        sIdentityXfm.v = identityV;
    }

    Transform invXfm;
    Invert(WorldXfm(), invXfm);

    Transform lightToWorld;
    Multiply(invXfm, sIdentityXfm, lightToWorld);

    float invRange = 1.0f / mRange;

    Transform invLight;
    {
        Hmx::Matrix4 projMat;
        projMat.x.x = 1.0f; projMat.y.x = 0.0f; projMat.z.x = 0.0f; projMat.w.x = 0.0f;
        projMat.x.y = 0.0f; projMat.y.y = 1.0f; projMat.z.y = 0.0f; projMat.w.y = 0.0f;
        projMat.x.z = 0.0f; projMat.y.z = 0.0f; projMat.z.z = invRange; projMat.w.z = 0.0f;
        projMat.x.w = 0.0f; projMat.y.w = 0.0f; projMat.z.w = (mBotRadius - mTopRadius) * invRange; projMat.w.w = mTopRadius;

        Hmx::Matrix4 shadowMat = lightToWorld * projMat;

        Invert(lightToWorld, invLight);

        TheShaderMgr.SetVConstant(kVS_ViewProjMatrix, shadowMat);
    }
    TheShaderMgr.SetVConstant((VShaderConstant)0x10, Hmx::Matrix4(invLight));
}

void NgLight::RenderShadows(std::vector<RndDrawable *> &shadowCasters) {
    MILO_ASSERT(mShadowRT && !shadowCasters.empty(), 0x112);
    MILO_ASSERT(WantShadows(), 0x113);
    RndCam *savedCam = RndCam::Current();
    mShadowRT->MakeDrawTarget();
    SetAndClearShadowViewport();
    SetShadowTransforms();
    Rnd::Mode savedDrawMode = TheRnd.DrawMode();
    TheRnd.SetDrawMode(Rnd::kDrawOcclusion);
    for (std::vector<RndDrawable *>::iterator it = shadowCasters.begin(), end = shadowCasters.end();
         it != end;
         ++it) {
        RndDrawable *draw = *it;
        if (draw->Showing()) {
            draw->DrawShowing();
        }
    }
    TheRnd.SetDrawMode(savedDrawMode);
    mShadowRT->FinishDrawTarget();
    BlurShadowRT(1.0f, 0.0f);
    BlurShadowRT(0.0f, 1.0f);
    if (savedCam) {
        savedCam->Select();
    } else {
        TheRnd.GetDefaultCam()->Select();
    }
}

void NgLight::SetAndClearShadowViewport() {
    int width = mShadowRT->Width();
    int height = mShadowRT->Height();
    NgRnd::Viewport vp;
    vp.X = 0;
    vp.Y = 0;
    vp.Width = width;
    vp.Height = height;
    vp.MinZ = 0.0f;
    vp.MaxZ = 1.0f;
    TheNgRnd.SetViewport(vp);
    Hmx::Color clearColor(0.0f, 0.0f, 0.0f, 0.0f);
    TheNgRnd.Clear(1, clearColor);
}

void NgLight::CheckShadowMap() {
    if (TheRnd.Drawing()) {
        if (TheShaderMgr.AllowPerPixel()) {
            if (mType == kFakeSpot) {
                if (TheRnd.DrawCount() != mShadowDrawCount) {
                    bool tempOverride = !mShadowOverride && mShadowObjects.size() != 0;
                    if (tempOverride) {
                        mShadowOverride = &mShadowObjects;
                    }
                    if (WantShadows()) {
                        if (!mShadowRT) {
                            PhysMemTypeTracker tracker("D3D(phys): Shadow Map");
                            mShadowRT = Hmx::Object::New<RndTex>();
                            mShadowRT->SetBitmap(
                                0x100, 0x100, 32, RndTex::kRenderedNoZ, false, nullptr
                            );
                        }
                        std::vector<RndDrawable *> draws;
                        if (HaveShadows(draws)) {
                            MILO_ASSERT(mShadowRT, 0x81);
                            RenderShadows(draws);
                            mShadowMapTex = mShadowRT;
                        } else {
                            mShadowMapTex = TheRnd.GetDefaultTex(Rnd::kDefaultTex_FlatNormal);
                            MILO_ASSERT(mShadowMapTex, 0x8a);
                        }
                    } else {
                        mShadowMapTex = TheRnd.GetDefaultTex(Rnd::kDefaultTex_FlatNormal);
                        MILO_ASSERT(mShadowMapTex, 0x91);
                    }
                    mShadowDrawCount = TheRnd.DrawCount();
                    if (tempOverride) {
                        mShadowOverride = nullptr;
                    }
                }
            } else {
                RELEASE(mShadowMapTex);
            }
        }
    }
}

// One directional pass over mShadowRT, sampling it in place; RenderShadows
// calls it once horizontally (1, 0) and once vertically (0, 1).
void NgLight::BlurShadowRT(float dirX, float dirY) {
    static const float kWeights[] = { 0.1f, 0.25f, 0.3f, 0.25f, 0.1f };
    RndTex *tex = mShadowRT;
    int w = tex->Width();
    int h = tex->Height();

    Hmx::Rect rect(0.0f, 0.0f, (float)(long long)w, (float)(long long)h);
    TheShaderMgr.SetNumTaps(5);

    float invW = 1.0f / (float)(long long)w;
    float invH = 1.0f / (float)(long long)h;

    const float *pWeight = kWeights - 1;
    int i = -2;
    int taps = 5;
    do {
        Vector4 offset(
            (float)((float)((float)(long long)i * invW) * dirX),
            (float)((float)((float)(long long)i * invH) * dirY),
            1.0f, 1.0f
        );
        TheShaderMgr.SetPConstant((PShaderConstant)(0x21 + i), offset);

        pWeight++;
        float wt = *pWeight;
        Vector4 weight(wt, wt, wt, wt);
        TheShaderMgr.SetPConstant((PShaderConstant)(0x31 + i), weight);
        taps--;
        i++;
    } while (taps != 0);

    TheRenderState.SetTextureFilter(0, (RndRenderState::FilterMode)1, false);

    tex->MakeDrawTarget();

    RndMat *workMat = TheShaderMgr.GetWork();
    workMat->SetDiffuseTex(tex);
    workMat->mZMode = kZModeDisable;
    workMat->mTexWrap = kTexWrapClamp;
    workMat->mBlend = RndMat::kBlendSrc;
    workMat->MarkDirty(2);

    Hmx::Color color;
    TheNgRnd.DrawRect(rect, workMat, (ShaderType)1, color, nullptr, nullptr);

    tex->FinishDrawTarget();
    TheShaderMgr.SetNumTaps(1);
}

void NgLight::Init() {
    REGISTER_OBJ_FACTORY(NgLight);
    PhysMemTypeTracker tracker("D3D(phys):Global");
}

// sw2 scatter-include (default/Lit_NG <- meta/StorePreviewMgr.cpp)
#define gRev gRev_StorePreviewMgr
#define gAltRev gAltRev_StorePreviewMgr
#include "meta/StorePreviewMgr.cpp"
#undef gRev
#undef gAltRev

// ---------------------------------------------------------------------------
// lane-AE batch-3 (sw3) scatter-include of an UNWIRED owner TU: retail placed
//   ?Shell@NgFur@@UBA_NHPAVRndMesh@@PAVRndMat@@@Z  (816 B)
//   ?Prep@NgFur@@UBA_NPAVRndMesh@@PAVRndMat@@@Z     (88 B)
// inside the .text span pinned to default/Lit_NG. rndobj/Fur_NG.cpp has real
// bodies for both but is not listed in config/45410914/objects.json, so nothing
// in the build ever compiled them. No SAVE_REVS in either TU.
#include "rndobj/Fur_NG.cpp"
