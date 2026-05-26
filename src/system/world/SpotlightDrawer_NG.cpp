#include "world/SpotlightDrawer_NG.h"
#include "macros.h"
#include "math/Color.h"
#include "math/Mtx.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/Timer.h"
#include "rnddx9/RenderState.h"
#include "rnddx9/Rnd.h"
#include "rnddx9/Tex.h"
#include "rndobj/Cam.h"
#include "rndobj/HiResScreen.h"
#include "rndobj/Rnd.h"
#include "rndobj/Rnd_NG.h"
#include "rndobj/ShaderMgr.h"
#include "utl/Loader.h"
#include "world/Dir.h"
#include "world/Spotlight.h"
#include "world/SpotlightDrawer.h"
#include <math.h>

#ifdef HX_NATIVE
NgSpotlightDrawer::SpotlightResources *NgSpotlightDrawer::sSharedResources;
#endif

void GetLightPosition(Spotlight *s, Vector3 &v) {
    v = s->WorldXfm().v;
    Vector3 offset;
    Multiply(s->mBeam.mBeam->LocalXfm().v, s->WorldXfm().m, offset);
    float vx = v.x, vy = v.y, vz = v.z;
    v.x = vx + offset.x;
    v.y = vy + offset.y;
    v.z = vz + offset.z;
}

NgSpotlightDrawer::NgSpotlightDrawer()
    : mSpotCam(), mSavedCam(this), mFogDensityMap(0), unkb0(false) {
    mSpotCam = Hmx::Object::New<RndCam>();
}

NgSpotlightDrawer::~NgSpotlightDrawer() { RELEASE(mSpotCam); }

NgSpotlightDrawer::SpotlightResources::~SpotlightResources() {
    Clear();
}

void NgSpotlightDrawer::EndWorld() {
    if (SpotlightDrawer::sNeedDraw) {
        CheckCam();
    }
    SpotlightDrawer::EndWorld();
}

void NgSpotlightDrawer::DoPost() { RenderScene(); }

void NgSpotlightDrawer::SetAmbientColor(const Hmx::Color &color) {
    Hmx::Color c = color;
    sEnviron->SetAmbientColor(color);
    float r = c.red;
    float g = c.green;
    float b = c.blue;
    float a = c.alpha;
    TheShaderMgr.SetVConstant(kVS_AmbientColor, Vector4(r, g, b, a));
    TheShaderMgr.SetPConstant(kPS_AmbientColor, Vector4(r, g, b, a));
}

void NgSpotlightDrawer::ClearPostDraw() { sNeedDraw = false; }

void NgSpotlightDrawer::ClearPostProc() {
    sLights.resize(0);
    sShadowSpots.resize(0);
    sCans.resize(0);
}

void NgSpotlightDrawer::Init() {
    CheckSharedResources();
    REGISTER_OBJ_FACTORY(NgSpotlightDrawer);
    RELEASE(sDefault);
    sDefault = Hmx::Object::New<SpotlightDrawer>();
    ((SpotDrawParams &)sDefault->Params()).mLightingInfluence = 0;
    sDefault->Select();
}

int NgSpotlightDrawer::RTWidth() {
    if (TheNgRnd.PreProcessTexture()) {
        return TheNgRnd.PreProcessTexture()->Width() >> 1;
    } else {
        return TheNgRnd.Width() >> 1;
    }
}

int NgSpotlightDrawer::RTHeight() {
    if (TheNgRnd.PreProcessTexture()) {
        return TheNgRnd.PreProcessTexture()->Height() >> 1;
    } else {
        return TheNgRnd.Height() >> 1;
    }
}

void NgSpotlightDrawer::SpotlightResources::Clear() {
    if (unk4) {
        D3DResource_Release(unk4);
        unk4 = nullptr;
    }
    RELEASE(unk8);
    RELEASE(mDensityMap);
    unk18 = nullptr;
}


static float sBeamBrighten = 2.0f;
static float sSphereScale = 0.5f;
static float sSheetIntensity = 3.0f;
static float sSheetW = 0.0f;

#ifdef HX_NATIVE
void NgSpotlightDrawer::SetupFogDensityMap() {}
void NgSpotlightDrawer::RenderFogProxy() {}
void NgSpotlightDrawer::RenderSphere(Spotlight *) {}
void NgSpotlightDrawer::RenderSheet(Spotlight *) {}
bool NgSpotlightDrawer::CheckRTs(NgSpotlightDrawer::SpotlightResources *) { return false; }
void NgSpotlightDrawer::SetupXSection(Spotlight *, const Spotlight::BeamDef &) {}
void NgSpotlightDrawer::RenderConeDefs(Spotlight *, const Hmx::Color &) {}
void NgSpotlightDrawer::RenderCone(Spotlight *) {}
void NgSpotlightDrawer::RenderBeams(const Hmx::Matrix4 &) {}
bool NgSpotlightDrawer::CheckCam() { return true; }
void NgSpotlightDrawer::BlurRT(float, float) {}
void NgSpotlightDrawer::SetupForPostProcess() {}
void NgSpotlightDrawer::RenderScene() {}
#endif

#ifndef HX_NATIVE
bool NgSpotlightDrawer::CheckCam() {
    mSavedCam = RndCam::Current();
    RndCam *cam;
    if (TheLoadMgr.EditMode()) {
        cam = RndCam::Current();
    } else if (TheWorld && TheWorld->Cam()) {
        cam = TheWorld->Cam();
    } else {
        cam = RndCam::Current();
        if (!cam) {
            cam = TheRnd.GetDefaultCam();
        }
    }
    mSpotCam->Copy(cam, Hmx::Object::kCopyShallow);
    mSpotCam->SetTransParent(nullptr, false);
    return true;
}

static float sBeamIntensity = 1.0f;

void NgSpotlightDrawer::RenderCone(Spotlight *sl) {
    MILO_ASSERT(sl->HasBeam(), 0x45d);
    Spotlight *colorOwner = sl->mColorOwner;
    float scale = colorOwner->mIntensity * sBeamIntensity;
    Hmx::Color color(
        colorOwner->mColor.red * scale,
        colorOwner->mColor.green * scale,
        colorOwner->mColor.blue * scale,
        colorOwner->mColor.alpha * scale
    );
    if (!sl->mAnimateColorFromPreset && sl->mBeam.mMat) {
        color.red = sl->mBeam.mMat->GetColor().red * color.red;
        color.green = sl->mBeam.mMat->GetColor().green * color.green;
        color.blue = sl->mBeam.mMat->GetColor().blue * color.blue;
        color.alpha = sl->mBeam.mMat->GetColor().alpha * color.alpha;
    }
    RenderConeDefs(sl, color);
}

void NgSpotlightDrawer::RenderSphere(Spotlight *sl) {
    MILO_ASSERT(sl->HasBeam(), 0x470);
    float zero = 0.0f;
    Vector4 sphereParams(zero, zero, 0.625f, sl->mBeam.mTopRadius * sSphereScale);
    TheShaderMgr.SetPConstant((PShaderConstant)0x5b, sphereParams);

    Spotlight *colorOwner = sl->mColorOwner;
    float intensity = colorOwner->mIntensity * sl->mBeam.mBrighten * sBeamBrighten;
    float r = colorOwner->mColor.red * intensity;
    float g = colorOwner->mColor.green * intensity;
    float b = colorOwner->mColor.blue * intensity;
    float a = colorOwner->mColor.alpha * intensity;

    if (!sl->mAnimateColorFromPreset && sl->mBeam.mMat) {
        r = r * sl->mBeam.mMat->GetColor().red;
        g = sl->mBeam.mMat->GetColor().green * g;
        b = sl->mBeam.mMat->GetColor().blue * b;
        a = sl->mBeam.mMat->GetColor().alpha * a;
    }

    TheShaderMgr.mCullModeOverride = 1;
    Vector4 colorVec(r, g, b, a);
    TheShaderMgr.SetPConstant((PShaderConstant)0x5a, colorVec);

    SetXSectionTexture(sl->mBeam);
    sl->mBeam.mBeam->DrawShowing();
}

void NgSpotlightDrawer::RenderSheet(Spotlight *sl) {
    SetXSectionTexture(sl->mBeam);

    float brighten = sl->mBeam.mBrighten;
    const Transform &camXfm = mSpotCam->WorldXfm();

    Vector4 camPos(camXfm.v.x, camXfm.v.y, camXfm.v.z, 1.0f);
    TheShaderMgr.SetPConstant((PShaderConstant)0xa, camPos);

    const Transform &slXfm = sl->WorldXfm();
    Vector4 lightDir(slXfm.m.z.x, slXfm.m.z.y, slXfm.m.z.z, sSheetW);
    TheShaderMgr.SetPConstant((PShaderConstant)0x5b, lightDir);

    Spotlight *colorOwner = sl->mColorOwner;
    float intensity = colorOwner->mIntensity * sSheetIntensity;
    float r = colorOwner->mColor.red * intensity;
    float g = colorOwner->mColor.green * intensity;
    float b = colorOwner->mColor.blue * intensity;

    if (!sl->mAnimateColorFromPreset && sl->mBeam.mMat) {
        RndMat *mat = sl->mBeam.mMat;
        r = mat->GetColor().red * r;
        g = mat->GetColor().green * g;
        b = mat->GetColor().blue * b;
    }

    Vector4 colorVec(r * brighten, g * brighten, b * brighten, 1.0f);
    TheShaderMgr.SetPConstant((PShaderConstant)0x5a, colorVec);

    int prevUnlit = TheShaderMgr.CullModeOverride();
    TheShaderMgr.mCullModeOverride = 1;
    sl->mBeam.mBeam->DrawShowing();
    TheShaderMgr.mCullModeOverride = prevUnlit;
}

void NgSpotlightDrawer::RenderBeams(const Hmx::Matrix4 &viewProj) {
    TheShaderMgr.mInDepthVolume = 1;
    D3DDevice_SetDepthStencilSurface(TheDxRnd.Device(), 0);

    SpotlightResources &sr = SR();
    TheShaderMgr.SetPConstant((PShaderConstant)0xc, sr.unk18);

    SetupFogDensityState();

    SpotlightEntry *it = sLights.begin();
    SpotlightEntry *itEnd = sLights.end();
    if (it != itEnd) {
        float zero = 0.0f;
        do {
            Spotlight *sl = it->mSpotlight;
            if (sl->mBeam.mLength > zero) {
                unsigned int shape = sl->mBeam.mShape;
                int shaderShape;
                if (shape < 2) {
                    shaderShape = 0;
                } else if (shape == 2) {
                    shaderShape = 1;
                } else {
                    if (shape >= 5) {
                        MILO_ASSERT(false, 0x456);
                    }
                    shaderShape = 2;
                }
                TheShaderMgr.unk1c = shaderShape;

                int shapeVal = sl->mBeam.mShape;
                if (shapeVal == 2) {
                    RenderSheet(sl);
                } else if (shapeVal < 3 || shapeVal > 4) {
                    RenderCone(sl);
                } else {
                    RenderSphere(sl);
                }

                TheShaderMgr.SetPConstant((PShaderConstant)0xc, sr.unk18);
            }
            it++;
        } while (it != itEnd);
    }

    TheShaderMgr.mCullModeOverride = 0;
    SetupFogDensityMap();
    TheShaderMgr.SetVConstant((VShaderConstant)4, viewProj);
    TheShaderMgr.SetPConstant((PShaderConstant)0xc, (RndTex *)0);
    TheShaderMgr.mInDepthVolume = 0;
}

void NgSpotlightDrawer::RenderConeDefs(Spotlight *sl, const Hmx::Color &color) {
    TheShaderMgr.mCullModeOverride = 3;
    TheShaderMgr.unk24 = 0;

    RndMesh *beam = sl->mBeam.mBeam;
    if (beam && sl->mBeam.mLength > 0.0f) {
        float brighten = sl->mBeam.mBrighten;
        Vector4 colorVec(
            color.red * brighten, color.green * brighten, color.blue * brighten, 1.0f
        );
        TheShaderMgr.SetPConstant((PShaderConstant)0x5a, colorVec);

        SetupXSection(sl, sl->mBeam);

        const Transform &camXfm = mSpotCam->WorldXfm();
        const Transform &camXfm2 = mSpotCam->WorldXfm();

        float camPosX = camXfm.v.x;
        float camPosY = camXfm.v.y;
        float camPosZ = camXfm.v.z;

        float camUpX = camXfm2.m.y.x;
        float camUpY = camXfm2.m.y.y;
        float camUpZ = camXfm2.m.y.z;

        Vector4 camPos(camPosX, camPosY, camPosZ, 1.0f);
        TheShaderMgr.SetPConstant((PShaderConstant)0xa, camPos);

        float dotProduct = -(camUpX * camPosX + camUpY * camPosY + camUpZ * camPosZ);
        Vector4 camPlane(camUpX, camUpY, camUpZ, dotProduct);
        TheShaderMgr.SetPConstant((PShaderConstant)0x1e, camPlane);

        float farPlane = mSpotCam->FarPlane();
        float zero = 0.0f;
        float invFarPlane = zero;
        if (zero < farPlane) {
            invFarPlane = 1.0f / farPlane;
        }

        Vector4 fogParams(mParams.mHalfDistance, invFarPlane, zero, zero);
        TheShaderMgr.SetPConstant((PShaderConstant)0x5b, fogParams);

        Vector3 lightPos;
        GetLightPosition(sl, lightPos);

        const Transform &slXfm = sl->WorldXfm();
        float dirX = slXfm.m.y.x;
        float dirY = slXfm.m.y.y;
        float dirZ = slXfm.m.y.z;

        Vector2 radii = sl->mBeam.NGRadii();
        float topRad = radii.x;
        float botRad = radii.y;
        float minRad = (topRad - botRad) < 0.0f ? topRad : botRad;

        float offset = 0.0f;
        if (0.0f < botRad) {
            offset = (minRad * sl->mBeam.mLength) / (botRad - minRad);
        }

        float negOffset = -offset;
        float totalLength = offset + sl->mBeam.mLength;
        float invTotalLength = 1.0f / totalLength;

        float apexX = lightPos.x + dirX * negOffset;
        float apexY = lightPos.y + dirY * negOffset;
        float apexZ = lightPos.z + dirZ * negOffset;

        Vector4 apex(apexX, apexY, apexZ, invTotalLength);
        TheShaderMgr.SetPConstant((PShaderConstant)0x19, apex);

        const Transform &slXfm2 = sl->WorldXfm();
        Vector4 direction(slXfm2.m.y.x, slXfm2.m.y.y, slXfm2.m.y.z, totalLength);
        TheShaderMgr.SetPConstant((PShaderConstant)0x1a, direction);

        const Transform &camXfm3 = mSpotCam->WorldXfm();
        float vx = camXfm3.v.x;
        float vy = camXfm3.v.y;
        float vz = camXfm3.v.z;

        float relX = vx - apexX;
        float relY = vy - apexY;
        float relZ = vz - apexZ;

        Vector4 relCamPos(relX, relY, relZ, 1.0f);
        TheShaderMgr.SetPConstant((PShaderConstant)0x1b, relCamPos);

        Vector4 radiiVec(
            minRad, botRad,
            dirX * apexX + dirY * apexY + dirZ * apexZ,
            dirX * apexX + dirY * apexY + dirZ * apexZ + totalLength
        );
        TheShaderMgr.SetPConstant((PShaderConstant)0x1d, radiiVec);

        float radiusDiff = botRad - minRad;
        float dotRelDir = dirX * relX + dirY * relY + dirZ * relZ;
        float tanSlope = invTotalLength * radiusDiff;

        float shift = 0.0f;
        if (radiusDiff != 0.0f) {
            shift = (minRad / radiusDiff) * totalLength;
        }

        float extProj = shift + dotRelDir;
        float cosAngle = (float)cos((float)atan(invTotalLength * botRad));

        Vector4 coneParams(
            tanSlope * tanSlope + 1.0f,
            extProj * tanSlope * tanSlope + dotRelDir,
            -(extProj * extProj * tanSlope * tanSlope -
              -(dotRelDir * dotRelDir - (relX * relX + relY * relY + relZ * relZ))),
            cosAngle * cosAngle
        );
        TheShaderMgr.SetPConstant((PShaderConstant)0x1c, coneParams);

        beam->DrawShowing();
    }
}

void NgSpotlightDrawer::SetupFogDensityMap() {
    float base = mParams.mBaseIntensity * 0.01f;
    float smoke = mParams.mSmokeIntensity * 0.01f;
    smoke *= 1.0f - base;
    Vector4 fogParams(base, smoke, 0.0f, 0.0f);
    TheShaderMgr.SetPConstant((PShaderConstant)0x7F, fogParams);
}

void NgSpotlightDrawer::SetupForPostProcess() {
    Vector4 zero(0.0f, 0.0f, 0.0f, 0.0f);
    TheShaderMgr.SetPConstant((PShaderConstant)0x5A, zero);
    BlurRT();
    float farPlane = mSpotCam->FarPlane();
    float recipFarPlane;
    if (farPlane > 0.0f) {
        recipFarPlane = 1.0f / farPlane;
    } else {
        recipFarPlane = 0.0f;
    }
    Vector4 intensityParams(mParams.mIntensity * 32.0f, 0.0f, 0.0f, recipFarPlane);
    TheShaderMgr.SetPConstant((PShaderConstant)0x5B, intensityParams);
    Hmx::Color c = mParams.mColor;
    Vector4 colorVec(c.red, c.green, c.blue, c.alpha);
    TheShaderMgr.SetPConstant((PShaderConstant)0x81, colorVec);
    TheShaderMgr.SetPConstant((PShaderConstant)0xC, SR().unk8);
    TheRenderState.SetTextureFilter(0xC, (RndRenderState::FilterMode)1, false);
    TheRenderState.SetTextureClamp(0xC, (RndRenderState::ClampMode)2);
    TheRenderState.SetTextureFilter(5, (RndRenderState::FilterMode)1, false);
    TheRenderState.SetTextureClamp(5, (RndRenderState::ClampMode)2);
    ClearPostProc();
    sActiveFrame = true;
}

static float kFogScale = 256.0f;

void NgSpotlightDrawer::RenderFogProxy() {
    RndDrawable *proxy = mParams.mProxy;
    if (proxy) {
        MILO_ASSERT(mFogDensityMap == SR().mDensityMap, 0x400);
        TheShaderMgr.SetPConstant((PShaderConstant)5, (RndTex *)0);
        float nearPlane = mSpotCam->NearPlane();
        Vector4 vsParams(nearPlane, -1.0f / nearPlane, 1.0f, 0.0f);
        TheShaderMgr.SetVConstant((VShaderConstant)0x37, vsParams);
        float farPlane = mSpotCam->FarPlane();
        float recipFarPlane;
        if (farPlane > 0.0f) {
            recipFarPlane = 1.0f / farPlane;
        } else {
            recipFarPlane = 0.0f;
        }
        Vector4 psParams(1.0f / kFogScale, 1.0f, 0.0f, recipFarPlane);
        TheShaderMgr.SetPConstant((PShaderConstant)0x5B, psParams);
        mSpotCam->SetTargetTex(mFogDensityMap);
        mSpotCam->Select();
        proxy->Draw();
        RestoreCam();
        if (mFogDensityMap) {
            TheShaderMgr.SetPConstant((PShaderConstant)5, mFogDensityMap);
            TheRenderState.SetTextureClamp(5, (RndRenderState::ClampMode)2);
        }
    }
}
#endif

void NgSpotlightDrawer::SetupFogDensityState() {
    if (mFogDensityMap) {
        TheShaderMgr.SetPConstant((PShaderConstant)5, mFogDensityMap);
        TheRenderState.SetTextureClamp(5, (RndRenderState::ClampMode)2);
    }

    Hmx::Matrix4 viewProj;
    RndCam::Current()->GetInfiniteViewProj(viewProj);
    TheShaderMgr.SetVConstant((VShaderConstant)4, viewProj);

    float fogDensity;
    float farPlane = mSpotCam->FarPlane();
    if (farPlane > 0.0f) {
        fogDensity = 1.0f / farPlane;
    } else {
        fogDensity = 0.0f;
    }

    Vector4 fogParams(0.0f, fogDensity, 0.0f, 0.0f);
    TheShaderMgr.SetPConstant((PShaderConstant)0x7F, fogParams);
}

static float sBlurAmount = 0.5f;
static bool sSeparateBlurPasses = false;

void NgSpotlightDrawer::BlurRT() {
    D3DDevice_SetDepthStencilSurface(TheDxRnd.Device(), 0);
    if (sSeparateBlurPasses) {
        BlurRT(sBlurAmount, 0.0f);
        BlurRT(0.0f, sBlurAmount);
    } else {
        BlurRT(sBlurAmount, sBlurAmount);
    }
}

static float sFogScale = 1.0f;

#ifndef HX_NATIVE
void NgSpotlightDrawer::BlurRT(float amountX, float amountY) {
    float fw = (float)RTWidth();
    float fh = (float)RTHeight();
    Hmx::Rect rect;
    rect.x = 0.0f;
    rect.y = 0.0f;
    rect.w = fw;
    rect.h = fh;

    TheShaderMgr.unk14 = 5;

    float invW = 1.0f / fw;
    float invH = 1.0f / fh;

    float kWeights[] = { 0.1f, 0.25f, 0.3f, 0.25f, 0.1f };

    int i = -2;
    const float *pWeight = kWeights - 1;
    do {
        float fi = (float)(int)i;
        Vector4 offset(fi * invW * amountX, fi * invH * amountY, 1.0f, 1.0f);
        TheShaderMgr.SetPConstant((PShaderConstant)(0x8c + i), offset);

        pWeight++;
        float wt = *pWeight;
        Vector4 weight(wt, wt, wt, wt);
        TheShaderMgr.SetPConstant((PShaderConstant)(0x9c + i), weight);

        i++;
    } while (i <= 2);

    TheRenderState.SetTextureClamp(0, (RndRenderState::ClampMode)2);
    TheRenderState.SetTextureFilter(0, (RndRenderState::FilterMode)1, false);

    SpotlightResources &sr = SR();
    DxTex *tex = (DxTex *)sr.unk8;
    D3DDevice *dev = TheDxRnd.Device();
    D3DSurface *rt = tex->GetRT();
    D3DDevice_SetRenderTarget_External(dev, 0, rt);

    RndMat *workMat = TheShaderMgr.GetWork();
    RndTex *srTex = SR().unk8;
    workMat->SetDiffuseTex(srTex);
    workMat->SetZMode(kZModeDisable);
    workMat->SetTexWrap(kTexWrapClamp);
    workMat->SetBlend(BaseMaterial::kBlendSrc);

    TheNgRnd.DrawRect(rect, workMat, (ShaderType)1, Hmx::Color(), 0, 0);

    D3DDevice_Resolve(TheDxRnd.Device(), 0, 0, tex->Tex(), 0, 0, 0, 0, 1.0f, 0, 0);
    TheShaderMgr.unk14 = 1;
}

void NgSpotlightDrawer::RenderScene() {
    START_AUTO_TIMER("world_draw");

    sActiveFrame = false;

    int numLights = sLights.end() - sLights.begin();
    if (numLights != 0 && Showing() && CheckSharedResources() && CheckFogTexture()) {
        MILO_ASSERT(sEnviron->GetUseApprox() == false, 0x595);

        sEnviron->Select(0);
        TheShaderMgr.unk25 = 1;
        TheHiResScreen.mOverride = true;

        TheRenderState.SetTextureFilter(9, (RndRenderState::FilterMode)0, false);
        TheRenderState.SetTextureClamp(9, (RndRenderState::ClampMode)2);

        float farPlane = mSpotCam->mFarPlane;
        float nearPlane = mSpotCam->mNearPlane;

        int h = RTHeight();
        float invH = 1.0f / (float)h;
        int w = RTWidth();
        float invW = 1.0f / (float)w;

        Vector4 camParams(nearPlane, farPlane, invW, invH);
        TheShaderMgr.SetPConstant((PShaderConstant)0x82, camParams);

        Vector4 depthRange;
        mSpotCam->GetDepthRangeValues(depthRange);
        TheShaderMgr.SetPConstant((PShaderConstant)0x59, depthRange);

        Vector4 fogParam(sFogScale, sFogScale, sFogScale, sFogScale);
        TheShaderMgr.SetPConstant((PShaderConstant)9, fogParam);

        RenderFogProxy();

        mSpotCam->SetTargetTex(SR().unk8);
        mSpotCam->Select();

        RenderBeams(RndCam::sCurrent->mViewProjMatrix);

        TheRenderState.SetTextureFilter(9, (RndRenderState::FilterMode)0, false);
        TheRenderState.SetTextureClamp(9, (RndRenderState::ClampMode)2);

        RestoreCam();
        TheHiResScreen.mOverride = false;
        SetupForPostProcess();
    } else {
        ClearPostDraw();
    }
}

void NgSpotlightDrawer::SetupXSection(Spotlight *sl, const Spotlight::BeamDef &def) {
    Vector3 lightPos;
    GetLightPosition(sl, lightPos);

    const Transform &camXfm = mSpotCam->WorldXfm();
    mSpotCam->WorldXfm();

    const Transform &slXfm = sl->WorldXfm();

    // Full 3D direction from camera to light
    Vector3 toCam = lightPos;
    toCam -= camXfm.v;

    // Save un-normalized copy
    Vector3 toCamOrig = toCam;
    Normalize(toCam, toCam);

    // Spotlight direction (m.y row of spotlight world transform)
    Vector3 slDir = slXfm.m.y;

    // Cross product components: slDir x (lightPos.x, toCamOrig.y, toCamOrig.z)
    float cmpY = slDir.y * toCamOrig.z;
    Vector3 perp;
    perp.x = slDir.x * toCamOrig.z - slDir.z * lightPos.x;
    float cmpX = slDir.z * toCamOrig.y;
    perp.y = slDir.y * lightPos.x - slDir.x * toCamOrig.y;
    Normalize(perp, perp);

    // Beam radii and length
    Vector2 radii = def.NGRadii();
    float topR = radii.x;
    float botR = radii.y;

    float cc = cmpX - cmpY;
    float ccTop = cc * topR;
    float ccBot = cc * botR;
    float len = def.mLength;

    // Copy lightPos to stack
    Vector3 lp = lightPos;
    // Copy perp to stack
    Vector3 perpC = perp;

    // Top-right corner (y,z in world)
    float trY = lightPos.y + perp.x * topR;
    float trZ = lightPos.z + perp.y * topR;

    // Bottom center
    float bcX = lightPos.x + slDir.x * len;
    float bcY = lightPos.y + slDir.y * len;
    float bcZ = lightPos.z + slDir.z * len;

    // Copy slDir to stack
    Vector3 slDirC = slDir;

    // Top-left corner (y,z)
    float tlY = lightPos.y - perp.x * topR;
    float tlZ = lightPos.z - perp.y * topR;

    // Camera world pos
    float cvx = camXfm.v.x;
    float cvy = camXfm.v.y;
    float cvz = camXfm.v.z;

    float trX = lightPos.x + ccTop;
    float trRx = trX - cvx;
    float tlX = lightPos.x - ccTop;

    // Bottom-right
    float brRx = (ccBot + bcX) - cvx;
    float brY = bcY + perp.x * botR;
    float brZ = bcZ + perp.y * botR;

    // Bottom-left
    float blX = bcX - ccBot;
    float blY = bcY - perp.x * botR;
    float blZ = bcZ - perp.y * botR;

    // Save topRight y,z
    float trYs = trY;
    float trZs = trZ;

    // Plane 1 edges relative to camera
    float e1y = trY - cvy;
    float e1z = trZ - cvz;
    float e2y = brY - cvy;
    float e2z = brZ - cvz;

    // Plane 1 normal (x,y components of cross product)
    Vector3 pn1;
    pn1.x = e1z * brRx - e2z * trRx;
    pn1.y = e2y * trRx - e1y * brRx;
    Normalize(pn1, pn1);

    // Plane 2 edges relative to camera
    float e3y = tlY - cvy;
    float e3z = tlZ - cvz;

    // Copy bottom-left
    Vector3 blC;
    blC.x = blY;
    blC.y = blZ;

    float e4z = blZ - cvz;
    float e4y = blY - cvy;

    float tlRx = tlX - cvx;
    float blRx = blX - cvx;

    // Plane 2 normal
    Vector3 pn2;
    pn2.x = e3z * blRx - e4z * tlRx;
    pn2.y = e4y * tlRx - e3y * blRx;
    Normalize(pn2, pn2);

    // Cross z-components
    float n1x = pn1.x;
    float n1y = pn1.y;
    float n1z = e1y * e2z - e1z * e2y;
    float d1 = cvx * n1z + (cvy * n1x + cvz * n1y);

    float n2x = pn2.x;
    float n2y = pn2.y;
    float n2z = e3y * e4z - e3z * e4y;
    float d2 = cvx * n2z + (cvy * n2x + cvz * n2y);

    // Combined normal
    Vector3 comb;
    comb.x = n1x + n2x;
    comb.y = n1y + n2y;

    Vector3 combC;
    combC.x = comb.x;
    combC.y = comb.y;
    Normalize(combC, pn1);

    // Inverse distances
    float dist1 = trYs * n1x + (n1z * lightPos.x + trZs * n1y);
    float inv1 = 0.0f;
    if (dist1 != 0.0f) {
        inv1 = 1.0f / dist1;
    }

    float dist2 = trYs * n2x + (n2z * lightPos.x + trZs * n2y);
    float inv2 = 0.0f;
    if (dist2 != 0.0f) {
        inv2 = 1.0f / dist2;
    }

    float pw = inv2 * d2;
    float pz = inv1 * d1;
    float px1 = n1z * inv1;
    float px2 = n2z * inv2;

    float minR = botR;
    if ((topR - botR) < 0.0f) {
        minR = topR;
    }

    Vector3 pn1s;
    pn1s.x = pn1.x;
    pn1s.y = pn1.y;

    Vector3 pn2s;
    pn2s.x = pn2.x;
    pn2s.y = pn2.y;

    float py1 = pn1.x * inv1;
    float pyy1 = pn1.y * inv1;
    float py2 = pn2.x * inv2;
    float pyy2 = pn2.y * inv2;

    float visOff = 0.0f;
    if (0.0f < botR) {
        visOff = (len * minR) / (botR - minR);
    }

    float halfA = (botR * 0.5f) / (len + visOff);
    float dotD = slDir.z * toCamOrig.z
        + (slDir.y * toCamOrig.y + slDir.x * lightPos.x);
    float sq = halfA * halfA;
    float fade = (1.0f - sq) / (sq + 1.0f);

    if (dotD <= 0.0f) {
        dotD = -dotD;
    }

    float vis = 0.0f;
    if (dotD < fade) {
        float diff = fade - dotD;
        vis = 1.0f;
        if (diff < 0.02f) {
            float t = -(diff * 50.0f - 1.0f);
            vis = -(t * t * t * t - 1.0f);
        }
    }

    Vector4 c56(vis, 0.0f, 0.0f, 0.0f);
    TheShaderMgr.SetPConstant((PShaderConstant)0x56, c56);

    Vector4 c57(px1, py1, pyy1, pz);
    TheShaderMgr.SetPConstant((PShaderConstant)0x57, c57);

    Vector4 c58(px2, py2, pyy2, pw);
    TheShaderMgr.SetPConstant((PShaderConstant)0x58, c58);

    SetXSectionTexture(def);
}
#endif

void NgSpotlightDrawer::SetXSectionTexture(const Spotlight::BeamDef &def) {
    RndTex *tex = def.mXSection;
    if (!tex) {
        tex = SR().unk14;
    }
    TheShaderMgr.SetPConstant(kPS_SpotlightTex, tex);
    TheRenderState.SetTextureClamp(0xB, (RndRenderState::ClampMode)2);
    TheRenderState.SetTextureFilter(0xB, (RndRenderState::FilterMode)1, false);
}

bool NgSpotlightDrawer::RestoreCam() {
    if (mSavedCam) {
        mSavedCam->Select();
    } else {
        TheRnd.GetDefaultCam()->Select();
    }
    return true;
}

bool NgSpotlightDrawer::CheckFogTexture() {
    if (mParams.mProxy) {
        mFogDensityMap = SR().mDensityMap;
    } else if (mParams.mTexture) {
        mFogDensityMap = mParams.mTexture;
    } else {
        mFogDensityMap = SR().unk10;
    }
    return mFogDensityMap;
}

#ifndef HX_NATIVE
bool NgSpotlightDrawer::CheckRTs(NgSpotlightDrawer::SpotlightResources *sr) {
    PhysMemTypeTracker tracker(Symbol("D3D(phys):NgSpotlightDrawer"));
    DxTex::SetEDRamChecksEnabled(false);
    int w = RTWidth();
    int h = RTHeight();
    if (!sr->unk8) {
        RndTex *tex = Hmx::Object::New<RndTex>();
        tex->SetBitmap(w, h, 32, RndTex::kDepthVolumeMap, false, nullptr);
        sr->unk8 = tex;
    }
    DxTex::SetEDRamChecksEnabled(true);
    if (!sr->unk4) {
        D3DSURFACE_DESC desc;
        ((DxTex *)sr->unk8)->GetDepthRT()->GetDesc(&desc);
        D3DFORMAT fmt = desc.Format;
        int createH = RTHeight();
        int createW = RTWidth();
        sr->unk4 = (D3DResource *)D3DDevice_CreateTexture(
            createW, createH, 1, 1, 0, fmt, 0, D3DRTYPE_TEXTURE
        );
        DX_ASSERT(sr->unk4, 0x12C);
    }
    if (!sr->unk10) {
        sr->unk10 = TheRnd.GetDefaultTex(Rnd::kDefaultTex_Black);
    }
    if (!sr->unk14) {
        sr->unk14 = TheRnd.GetDefaultTex(Rnd::kDefaultTex_WhiteTransparent);
    }
    sr->unk18 = sr->unk10;
    if (!sr->mDensityMap) {
        sr->mDensityMap = Hmx::Object::New<RndTex>();
        int dh = RTHeight() >> 1;
        int dw = RTWidth() >> 1;
        sr->mDensityMap->SetBitmap(dw, dh, 32, RndTex::kDensityMap, false, nullptr);
    }
    return true;
}
#endif

bool NgSpotlightDrawer::CheckSharedResources() {
    if (sSharedResources) {
        if (sSharedResources->unk8 && sSharedResources->unk8->Width() != RTWidth()) {
            RELEASE(sSharedResources);
        }
    }
    if (!sSharedResources) {
        sSharedResources = new SpotlightResources();
        return CheckRTs(sSharedResources);
    }
    return true;
}

#ifndef HX_NATIVE
// Manual vector implementations to match target code generation
typedef std::vector<SpotlightDrawer::SpotMeshEntry> SpotMeshEntryVector;
typedef SpotlightDrawer::SpotMeshEntry SpotMeshEntry;

namespace stlpmtx_std {

// Manual specialization for SpotMeshEntry vector to match target codegen
// The target binary uses manual memcpy loops instead of STL helpers
template <>
void vector<SpotMeshEntry, StlNodeAlloc<SpotMeshEntry>>::_M_fill_insert_aux(
    SpotMeshEntry* __pos,
    unsigned int __n,
    const SpotMeshEntry& __x,
    const __false_type&
) {
    // Self-reference check required for non-movable types
    if (_M_is_inside(__x)) {
        SpotMeshEntry __x_copy = __x;
        _M_fill_insert_aux(__pos, __n, __x_copy, __false_type());
        return;
    }

    pointer __old_finish = this->_M_finish;
    const size_type __elems_after = __old_finish - __pos;

    if (__elems_after > __n) {
        // Move tail elements forward
        __uninitialized_copy(__old_finish - __n, __old_finish, __old_finish, _TrivialUCpy());
        this->_M_finish += __n;

        // Manual backward copy loop to match target codegen
        pointer src = __old_finish - __n;
        pointer dst = __old_finish;
        for (int count = (src - __pos) / sizeof(SpotMeshEntry); count > 0; count--) {
            dst--;
            src--;
            memcpy(dst, src, sizeof(SpotMeshEntry));
        }

        // Manual fill loop to match target codegen
        pointer end = __pos + __n;
        for (pointer p = __pos; p != end; p++) {
            memcpy(p, &__x, sizeof(SpotMeshEntry));
        }
    } else {
        // Fill new elements beyond old finish
        this->_M_finish = __uninitialized_fill_n(this->_M_finish, __n - __elems_after, __x, _PODType());
        // Copy remaining elements
        __uninitialized_copy(__pos, __old_finish, this->_M_finish, _TrivialUCpy());
        this->_M_finish += __elems_after;
        // Fill elements within old range
        for (pointer p = __pos; p != __old_finish; p++) {
            memcpy(p, &__x, sizeof(SpotMeshEntry));
        }
    }
}

template <>
SpotMeshEntry* vector<SpotMeshEntry, StlNodeAlloc<SpotMeshEntry>>::_M_erase(
    SpotMeshEntry* __first,
    SpotMeshEntry* __last,
    const __false_type&
) {
    SpotMeshEntry* __pos = __first;
    SpotMeshEntry* __src = __last;
    int __count = (this->_M_finish - __src) / 0x50;

    if (__count > 0) {
        do {
            memcpy(__pos, __src, 0x50);
            __count--;
            __pos += 1;
            __src += 1;
        } while (__count != 0);
    }

    this->_M_finish = __pos;
    return __first;
}

template <>
SpotMeshEntry* vector<SpotMeshEntry, StlNodeAlloc<SpotMeshEntry>>::_M_erase(
    SpotMeshEntry* __pos,
    const __false_type&
) {
    SpotMeshEntry* __next = __pos + 1;
    if (__next != this->_M_finish) {
        int __bytes = (this->_M_finish - __next) * sizeof(SpotMeshEntry);
        memcpy(__pos, __next, __bytes);
    }
    --this->_M_finish;
    return __pos;
}

}  // namespace stlpmtx_std
#endif // HX_NATIVE
