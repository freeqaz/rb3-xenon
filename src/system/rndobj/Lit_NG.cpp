#include "rndobj/Lit_NG.h"
#include "Lit.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/System.h"
#include "rndobj/Draw.h"
#include "Memory.h"
#include "rndobj/Lit.h"
#include "rndobj/Cam.h"
#include "rndobj/Mat.h"
#include "rndobj/Rnd.h"
#include "rndobj/Rnd_NG.h"
#include "rndobj/ShaderMgr.h"
#include "rnddx9/RenderState.h"
#include "math/Mtx.h"
#include <cstring>

bool NgLight::WantShadows() const {
    return GetGfxMode() == kNewGfx && mShadowOverride && !mShadowOverride->empty();
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

NgLight::~NgLight() {
    RELEASE(mShadowRT);
    RELEASE(unk188);
}

NgLight::NgLight() : mShadowRT(0), mShadowMapTex(0), unk188(0), unk18c(-1) {}

RndTex *NgLight::CreateShadowTex() {
    PhysMemTypeTracker tracker("D3D(phys):ShadowTex");
    RndTex *tex = Hmx::Object::New<RndTex>();
    tex->SetBitmap(0x100, 0x100, 16, RndTex::kRenderedNoZ, false, nullptr);
    return tex;
}

bool NgLight::SphereConeTest(const Vector3 &sphereCenter, float sphereRadius) {
    const Transform &xfm1 = WorldXfm();
    const Transform &xfm2 = WorldXfm();
    Vector3 sc = sphereCenter;

    float proj = xfm2.m.y.x * (sc.x - xfm1.v.x)
        + xfm2.m.y.z * (sc.z - xfm1.v.z)
        + xfm2.m.y.y * (sc.y - xfm1.v.y);

    if (proj < -sphereRadius) {
        return false;
    }

    float range = mRange;
    if (proj > range + sphereRadius) {
        return false;
    }

    Vector3 axis = xfm2.m.y;
    Vector3 origin = xfm1.v;

    Vector3 perp;
    perp.y = (sc.y - origin.y) - axis.y * proj;
    perp.x = (sc.x - origin.x) - axis.x * proj;
    perp.z = (sc.z - origin.z) - axis.z * proj;

    Normalize(perp, perp);

    float topR = mTopRadius;
    float botR = mBotRadius;

    Vector3 perpTop = perp;
    perpTop *= topR;

    Vector3 perpBot = perp;
    perpBot *= botR;

    Vector3 topPoint = origin;
    topPoint += perpTop;

    Vector3 toSphere = sphereCenter;
    toSphere -= topPoint;

    Vector3 botPoint = origin;
    botPoint.x += (float)((double)axis.x * range);
    botPoint.y += (float)((double)axis.y * range);
    botPoint.z += (float)((double)axis.z * range);
    botPoint += perpBot;

    Vector3 edgeDir = botPoint;
    edgeDir -= topPoint;

    float t = (1.0f / Dot(edgeDir, edgeDir)) * Dot(toSphere, edgeDir);

    Vector3 closest = toSphere;
    closest.x -= t * edgeDir.x;
    closest.y -= t * edgeDir.y;
    closest.z -= t * edgeDir.z;

    bool _result = true;
    if (Dot(perp, closest) >= 0.0f) {
        _result = Length(closest) < sphereRadius;
    }
    return _result;
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

    Hmx::Matrix4 projMat;
    projMat.x.x = 1.0f; projMat.y.x = 0.0f; projMat.z.x = 0.0f; projMat.w.x = 0.0f;
    projMat.x.y = 0.0f; projMat.y.y = 1.0f; projMat.z.y = 0.0f; projMat.w.y = 0.0f;
    projMat.x.z = 0.0f; projMat.y.z = 0.0f; projMat.z.z = invRange; projMat.w.z = 0.0f;
    projMat.x.w = 0.0f; projMat.y.w = 0.0f; projMat.z.w = (mBotRadius - mTopRadius) * invRange; projMat.w.w = mTopRadius;

    Hmx::Matrix4 shadowMat = lightToWorld * projMat;

    Transform invLight;
    Invert(lightToWorld, invLight);

    TheShaderMgr.SetVConstant(kVS_ViewProjMatrix, shadowMat);
    TheShaderMgr.SetVConstant((VShaderConstant)0x10, Hmx::Matrix4(invLight));
}

void NgLight::RenderShadows(std::vector<RndDrawable *> &shadowCasters) {
    MILO_ASSERT(mShadowRT && !shadowCasters.empty(), 0x112);
    MILO_ASSERT(WantShadows(), 0x113);
    RndCam *savedCam = RndCam::Current();
    mShadowRT->MakeDrawTarget();
    SetAndClearShadowViewport();
    SetShadowTransforms();
    Rnd::DrawMode savedDrawMode = TheRnd.GetDrawMode();
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
    BlurShadowRT();
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
                if (TheRnd.DrawCount() != unk18c) {
                    bool tempOverride = !mShadowOverride && mShadowObjects.size() != 0;
                    if (tempOverride) {
                        mShadowOverride = &mShadowObjects;
                    }
                    if (WantShadows()) {
                        if (!mShadowRT && !unk188) {
                            mShadowRT = CreateShadowTex();
                            unk188 = CreateShadowTex();
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
                    unk18c = TheRnd.DrawCount();
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

void NgLight::BlurShadowRT() {
    static const float kWeights[] = { 0.1f, 0.25f, 0.3f, 0.25f, 0.1f };
    float blurX = 1.0f;
    float blurDir = 0.0f;
    int pass = 0;
    do {
        RndTex *srcTex, *dstTex;
        if (pass == 0) {
            srcTex = mShadowRT;
            dstTex = unk188;
        } else {
            srcTex = unk188;
            dstTex = mShadowRT;
            float tmp = blurX;
            blurX = blurDir;
            blurDir = tmp;
        }

        int w = dstTex->Width();
        int h = dstTex->Height();

        Hmx::Rect rect(0.0f, 0.0f, (float)(long long)w, (float)(long long)h);
        TheShaderMgr.SetNumTaps(5);

        float invW = 1.0f / (float)(long long)w;
        float invH = 1.0f / (float)(long long)h;

        const float *pWeight = kWeights - 1;
        int i = -2;
        int taps = 5;
        do {
            Vector4 offset(
                (float)((float)((float)(long long)i * invW) * blurX),
                (float)((float)((float)(long long)i * invH) * blurDir),
                1.0f, 1.0f
            );
            TheShaderMgr.SetPConstant((PShaderConstant)(0x8c + i), offset);

            pWeight++;
            float wt = *pWeight;
            Vector4 weight(wt, wt, wt, wt);
            TheShaderMgr.SetPConstant((PShaderConstant)(0x9c + i), weight);
            taps--;
            i++;
        } while (taps != 0);

        TheRenderState.SetTextureFilter(0, (RndRenderState::FilterMode)1, false);
        TheRenderState.SetTextureFilter(6, (RndRenderState::FilterMode)1, false);

        dstTex->MakeDrawTarget();

        RndMat *workMat = TheShaderMgr.GetWork();
        workMat->SetDiffuseTex(srcTex);
        workMat->mBlend = BaseMaterial::kBlendSrc;
        workMat->mTexWrap = kTexWrapClamp;
        workMat->mZMode = kZModeDisable;
        workMat->MarkDirty(2);

        Hmx::Color color;
        TheNgRnd.DrawRect(rect, workMat, (ShaderType)1, color, nullptr, nullptr);

        dstTex->FinishDrawTarget();
        pass++;
        TheShaderMgr.SetNumTaps(1);
    } while (pass < 2);
}

void NgLight::Init() {
    REGISTER_OBJ_FACTORY(NgLight);
    PhysMemTypeTracker tracker("D3D(phys):NgLight");
}
