#include "rndobj/PostProc_NG.h"
#include "Memory.h"
#include "Tex.h"
#include "hamobj/HamDirector.h"
#include "math/Color.h"
#include "math/Rand.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rnddx9/RenderState.h"
#include "rndobj/Mat.h"
#include "rndobj/Overlay.h"
#include "rndobj/PostProc.h"
#include "rndobj/Rnd.h"
#include "rndobj/Rnd_NG.h"
#include "rndobj/Tex.h"
#include "rndobj/VelocityBuffer.h"
#include "rndobj/HiResScreen.h"
#include "rndobj/ShaderMgr.h"
#include "utl/Loader.h"
#include "utl/MakeString.h"
#include <math.h>

extern void merged_ObjPtrListPopBack(void *);
void SetBloomBlurWeights(bool, float, float);
void SetBloomBlurWeightsStreak(bool, float, float, float, int, float);

Hmx::Color NgPostProc::s_prevBloomColor(-1, -1, -1, -1);
float NgPostProc::s_prevBloomIntensity = -1;
NgPostProc::BloomTextures<3> NgPostProc::sBloom;

static RndOverlay *sPostProcOverlay;

static int sBloomDebugCounter;

void Bloom_Downsample(ShaderType shader, RndTex *texSrc, RndTex *texDst) {
    MILO_ASSERT((shader == kBloomShader) || (shader == kDownsample4xShader), 0x17b);
    MILO_ASSERT(texDst->Width() > 0, 0x17c);
    MILO_ASSERT(texDst->Height() > 0, 0x17d);
    MILO_ASSERT(texDst->Width() < texSrc->Width(), 0x17e);
    MILO_ASSERT(texDst->Height() < texSrc->Height(), 0x17f);

    RndMat *workMat = TheShaderMgr.GetWork();
    workMat->SetBlend(BaseMaterial::kBlendSrc);
    workMat->SetZMode(kZModeDisable);
    workMat->SetTexWrap(kTexWrapClamp);
    workMat->MarkDirty(2);

    texDst->MakeDrawTarget();
    TheShaderMgr.SetPConstant((PShaderConstant)kPS_BloomParams, texSrc);
    workMat->SetDiffuseTex(texSrc);
    workMat->MarkDirty(2);

    Hmx::Rect rect(0, 0, (float)texDst->Width(), (float)texDst->Height());
    TheNgRnd.DrawRect(rect, workMat, shader, Hmx::Color(1, 1, 1), nullptr, nullptr);

    texDst->FinishDrawTarget();
}

void Bloom_Blur(RndTex *texDst, RndTex *texSrc, BloomBlurStyle style, BloomBlurDirection direction, unsigned int pass, float attenuation, float angle) {
    MILO_ASSERT(texDst->Width() > 0, 0x1b2);
    MILO_ASSERT(texDst->Height() > 0, 0x1b3);
    MILO_ASSERT(texDst->Width() == texSrc->Width(), 0x1b4);
    MILO_ASSERT(texDst->Height() == texSrc->Height(), 0x1b5);
    MILO_ASSERT(TheShaderMgr.NumTaps() == 1, 0x1b6);

    RndMat *workMat = TheShaderMgr.GetWork();
    workMat->SetZMode(kZModeDisable);
    workMat->SetTexWrap(kTexWrapClamp);
    workMat->SetBlend(BaseMaterial::kBlendSrc);
    workMat->MarkDirty(2);

    texDst->MakeDrawTarget();
    workMat->SetDiffuseTex(texSrc);

    ShaderType blurShader = kBlurShader;
    bool horizontal = (direction == kBloomBlurHorizontal);
    workMat->MarkDirty(2);

    {
        float w = (float)texDst->Width();
        float h = (float)texDst->Height();
        if ((unsigned int)style >= (unsigned int)kBloomBlurStreak) {
            if ((unsigned int)style != (unsigned int)kBloomBlurStreak) {
                if ((unsigned int)style < (unsigned int)(kBloomBlurGlare + 1)) {
                    blurShader = kBloomGlareShader;
                }
            } else {
                SetBloomBlurWeightsStreak(horizontal, w, h, attenuation, pass, angle);
            }
        } else {
            SetBloomBlurWeights(horizontal, w, h);
        }
    }

    Hmx::Color color;
    Hmx::Rect rect(0, 0, (float)texDst->Width(), (float)texDst->Height());
    TheNgRnd.DrawRect(rect, workMat, blurShader, color, nullptr, nullptr);

    TheShaderMgr.SetNumTaps(1);
    texDst->FinishDrawTarget();
}

NgPostProc *NgPostProc::s_BloomSetter;

void NgPostProc::DoBloom() {
    float bloomIntensity = BloomIntensity();
    bool doBloom = (0.0f < bloomIntensity) || (0.0f < mBloomColor.alpha);
    bool doGlare = mBloomGlare && !TheHiResScreen.IsActive();

    if (!doBloom && s_BloomSetter) {
        RndOverlay *overlay = RndOverlay::Find("postproc", true);
        RndOverlay *prevOverlay = sPostProcOverlay;
        if (overlay->Showing()) {
            sPostProcOverlay = overlay;
            FormatString fmt("BLOOM : NONE\n");
            TheDebug << fmt.Str();
        }
        s_BloomSetter = nullptr;
        s_prevBloomIntensity = -1.0f;
        s_prevBloomColor = Hmx::Color(-1, -1, -1, -1);
        sPostProcOverlay = prevOverlay;
    }

    if (doBloom) {
        bloomIntensity = BloomIntensity();
        float redScaled = mBloomColor.red * sBloomLocFactor * bloomIntensity;
        float greenScaled = mBloomColor.green * sBloomLocFactor * bloomIntensity;
        float blueScaled = mBloomColor.blue * sBloomLocFactor * bloomIntensity;

        bool paramsChanged = !(mBloomColor == s_prevBloomColor)
            || BloomIntensity() != s_prevBloomIntensity
            || s_BloomSetter != this;

        if (paramsChanged) {
            s_prevBloomColor = mBloomColor;
            s_prevBloomIntensity = BloomIntensity();
            s_BloomSetter = this;

            RndOverlay *overlay = RndOverlay::Find("postproc", true);
            RndOverlay *prevOverlay = sPostProcOverlay;
            if (overlay->Showing()) {
                sPostProcOverlay = overlay;
                const char *worldName = PathName(TheHamDirector->GetActivePostProc());
                float intensity = BloomIntensity();
                int r = (int)(mBloomColor.red * 256.0f);
                int g = (int)(mBloomColor.green * 256.0f);
                int b = (int)(mBloomColor.blue * 256.0f);
                int counter = sBloomDebugCounter % 100;
                sBloomDebugCounter++;
                TheDebug << MakeString("%03d:BLOOM: C=<%3d,%3d,%3d> I=%5.2f : %s\n", counter, r, g, b, intensity, worldName);
            }
            sPostProcOverlay = prevOverlay;
        }

        Vector4 bloomColorVec(redScaled, greenScaled, blueScaled, 0.0f);
        TheShaderMgr.SetPConstant((PShaderConstant)6, bloomColorVec);
        TheShaderMgr.SetPConstant((PShaderConstant)kPS_BloomParams, TheRnd.GetDefaultTex(Rnd::kDefaultTex_Black));
        TheShaderMgr.SetPConstant((PShaderConstant)kPS_SpotlightTex, TheRnd.GetDefaultTex(Rnd::kDefaultTex_Black));
        TheShaderMgr.SetPConstant((PShaderConstant)kPS_NgMatCustom, TheRnd.GetDefaultTex(Rnd::kDefaultTex_Black));
        TheRenderState.SetTextureFilter(kPS_BloomParams, (RndRenderState::FilterMode)1, false);
        TheRenderState.SetTextureClamp(kPS_BloomParams, (RndRenderState::ClampMode)2);
        TheRenderState.SetTextureFilter(kPS_BloomParams, (RndRenderState::FilterMode)1, false);
        TheRenderState.SetTextureClamp(kPS_BloomParams, (RndRenderState::ClampMode)2);
        TheRenderState.SetTextureFilter(kPS_SpotlightTex, (RndRenderState::FilterMode)1, false);
        TheRenderState.SetTextureClamp(kPS_SpotlightTex, (RndRenderState::ClampMode)2);
        TheRenderState.SetTextureFilter(kPS_NgMatCustom, (RndRenderState::FilterMode)1, false);
        TheRenderState.SetTextureClamp(kPS_NgMatCustom, (RndRenderState::ClampMode)2);

        RndTex *preprocess = TheNgRnd.PreProcessTexture();
        if (preprocess) {
            RndTex *bloomTex;
            PShaderConstant finalSlot;
            if (doGlare) {
                Bloom_Downsample(kBloomShader, preprocess, sBloom.mTextures[0].mBloomTexture[1]);
                Bloom_Blur(sBloom.mTextures[0].mBloomTexture[1], sBloom.mTextures[0].mBloomTexture[0], kBloomBlurNormal, kBloomBlurHorizontal, 0, 0.0f, 0.0f);
                Bloom_Blur(sBloom.mTextures[0].mBloomTexture[0], sBloom.mTextures[0].mBloomTexture[1], kBloomBlurNormal, kBloomBlurVertical, 0, 0.0f, 0.0f);
                Bloom_Blur(sBloom.mTextures[0].mBloomTexture[1], sBloom.mTextures[0].mBloomTexture[0], kBloomBlurGlare, kBloomBlurHorizontal, 0, 0.0f, 0.0f);
                bloomTex = sBloom.mTextures[0].mBloomTexture[0];
                finalSlot = kPS_BloomParams;
            } else if (!mBloomStreak || mBloomGlare) {
                Bloom_Downsample(kBloomShader, preprocess, sBloom.mTextures[0].mBloomTexture[1]);
                Bloom_Blur(sBloom.mTextures[0].mBloomTexture[1], sBloom.mTextures[0].mBloomTexture[0], kBloomBlurNormal, kBloomBlurHorizontal, 0, 0.0f, 0.0f);
                Bloom_Blur(sBloom.mTextures[0].mBloomTexture[0], sBloom.mTextures[0].mBloomTexture[1], kBloomBlurNormal, kBloomBlurVertical, 0, 0.0f, 0.0f);
                Bloom_Downsample(kDownsample4xShader, sBloom.mTextures[0].mBloomTexture[1], sBloom.mTextures[1].mBloomTexture[0]);
                Bloom_Blur(sBloom.mTextures[1].mBloomTexture[0], sBloom.mTextures[1].mBloomTexture[1], kBloomBlurNormal, kBloomBlurHorizontal, 0, 0.0f, 0.0f);
                Bloom_Blur(sBloom.mTextures[1].mBloomTexture[1], sBloom.mTextures[1].mBloomTexture[0], kBloomBlurNormal, kBloomBlurVertical, 0, 0.0f, 0.0f);
                Bloom_Downsample(kDownsample4xShader, sBloom.mTextures[1].mBloomTexture[0], sBloom.mTextures[2].mBloomTexture[0]);
                Bloom_Blur(sBloom.mTextures[2].mBloomTexture[0], sBloom.mTextures[2].mBloomTexture[1], kBloomBlurNormal, kBloomBlurHorizontal, 0, 0.0f, 0.0f);
                Bloom_Blur(sBloom.mTextures[2].mBloomTexture[1], sBloom.mTextures[2].mBloomTexture[0], kBloomBlurNormal, kBloomBlurVertical, 0, 0.0f, 0.0f);
                TheShaderMgr.SetPConstant((PShaderConstant)kPS_BloomParams, sBloom.mTextures[0].mBloomTexture[1]);
                TheShaderMgr.SetPConstant((PShaderConstant)kPS_SpotlightTex, sBloom.mTextures[1].mBloomTexture[0]);
                bloomTex = sBloom.mTextures[2].mBloomTexture[0];
                finalSlot = kPS_NgMatCustom;
            } else {
                Bloom_Downsample(kBloomShader, preprocess, sBloom.mTextures[0].mBloomTexture[1]);
                Bloom_Blur(sBloom.mTextures[0].mBloomTexture[1], sBloom.mTextures[0].mBloomTexture[0], kBloomBlurStreak, kBloomBlurHorizontal, 0, mBloomStreakAttenuation, mBloomStreakAngle);
                Bloom_Blur(sBloom.mTextures[0].mBloomTexture[0], sBloom.mTextures[0].mBloomTexture[1], kBloomBlurStreak, kBloomBlurHorizontal, 1, mBloomStreakAttenuation, mBloomStreakAngle);
                Bloom_Blur(sBloom.mTextures[0].mBloomTexture[1], sBloom.mTextures[0].mBloomTexture[0], kBloomBlurStreak, kBloomBlurHorizontal, 2, mBloomStreakAttenuation, mBloomStreakAngle);
                Bloom_Downsample(kBloomShader, preprocess, sBloom.mTextures[1].mBloomTexture[1]);
                Bloom_Blur(sBloom.mTextures[1].mBloomTexture[1], sBloom.mTextures[1].mBloomTexture[0], kBloomBlurStreak, kBloomBlurVertical, 0, mBloomStreakAttenuation, mBloomStreakAngle);
                Bloom_Blur(sBloom.mTextures[1].mBloomTexture[0], sBloom.mTextures[1].mBloomTexture[1], kBloomBlurStreak, kBloomBlurVertical, 1, mBloomStreakAttenuation, mBloomStreakAngle);
                Bloom_Blur(sBloom.mTextures[1].mBloomTexture[1], sBloom.mTextures[1].mBloomTexture[0], kBloomBlurStreak, kBloomBlurVertical, 2, mBloomStreakAttenuation, mBloomStreakAngle);
                TheShaderMgr.SetPConstant((PShaderConstant)kPS_BloomParams, sBloom.mTextures[0].mBloomTexture[0]);
                bloomTex = sBloom.mTextures[1].mBloomTexture[0];
                finalSlot = kPS_SpotlightTex;
            }
            TheShaderMgr.SetPConstant(finalSlot, bloomTex);
        }
    } else {
        s_BloomSetter = nullptr;
        s_prevBloomIntensity = -1.0f;
        s_prevBloomColor = Hmx::Color(-1, -1, -1, -1);
    }

    if (doBloom) {
        if (doGlare) {
            TheShaderMgr.unk28 = true;
            TheShaderMgr.unk27 = false;
        } else {
            TheShaderMgr.unk28 = false;
            TheShaderMgr.unk27 = true;
        }
    } else {
        TheShaderMgr.unk27 = false;
        TheShaderMgr.unk28 = false;
    }
}

NgPostProc::BloomTextureSet::BloomTextureSet() {
    mBloomTexture[0] = (RndTex*)0;
    mBloomTexture[1] = (RndTex*)0;
}

NgPostProc::BloomTextureSet::~BloomTextureSet() { FreeTextures(); }

void NgPostProc::BloomTextureSet::AllocateTextures(unsigned int w, unsigned int h) {
    MILO_ASSERT(mBloomTexture[0] == NULL, 0x48);
    mBloomTexture[0] = Hmx::Object::New<RndTex>();
    mBloomTexture[0]->SetBitmap(w, h, TheRnd.Bpp(), RndTex::kRenderedNoZ, false, nullptr);
    mBloomTexture[1] = mBloomTexture[0];
}

void NgPostProc::BloomTextureSet::FreeTextures() { RELEASE(mBloomTexture[0]); }

NgPostProc::NgPostProc()
    : mRandomSeed1(RandomFloat()), mRandomSeed2(RandomFloat()), unk234(0), unk238(0),
      mMotionBlurDrawList(this), mMotionBlurEnabled(1) {}

NgPostProc::~NgPostProc() {}

void NgPostProc::Select() {
    RndPostProc::Select();
    mRandomSeed1 = RandomFloat();
    mRandomSeed2 = RandomFloat();
}

void NgPostProc::Init() {
    REGISTER_OBJ_FACTORY(NgPostProc);
    PhysMemTypeTracker tracker("D3D(phys):NgPostProc");
    RebuildTex();
}

void NgPostProc::RebuildTex() {
    ReleaseTex();
    int w = 0x80;
    int h = 0x80;
    if (TheLoadMgr.GetPlatform() != kPlatformNone) {
        MILO_ASSERT(TheNgRnd.PreProcessTexture(), 0x3AB );
        w = TheNgRnd.PreProcessTexture()->Width();
        h = TheNgRnd.PreProcessTexture()->Height();
    }
    RndVelocityBuffer::Singleton().AllocateData(w, h, TheRnd.Bpp());
    sBloom.AllocateTextures(w * 4, h * 4);
}

#ifdef HX_NATIVE
void NgPostProc::DoVelocity() {
    // Post-processing uses hardcoded ILP32 struct offsets throughout — stub on native
    // Will be reimplemented when the WebGPU renderer is built
}
#else
void NgPostProc::DoVelocity() {
    typedef void (*ShaderFunc)(void*, int, float*);
    *(s8*)((u8*)&TheShaderMgr + 0x39) = 0;
    if ((mMotionBlurVelocity) && (*(u8*)((u8*)&TheHiResScreen + 0x4) == 0) &&
        (RndVelocityBuffer::Singleton().Draw(*(RndCam**)((u8*)&TheRnd + 0xE4), mMotionBlurDrawList) != 0)) {
        *(s8*)((u8*)&TheShaderMgr + 0x39) = 1;
        float sp50 = *(float*)((u8*)&RndVelocityBuffer::Singleton() + 0x36BE8);
        void* shaderMgrVTable = *(void**)&TheShaderMgr;
        ShaderFunc func = *(ShaderFunc*)((u8*)shaderMgrVTable + 0x40);
        func(&TheShaderMgr, 0x7A, &sp50);
    }
    int head = *(int*)((u8*)this + 0x240);
    if (head != 0) {
        void* pList = (u8*)this + 0x23C;
        do {
            merged_ObjPtrListPopBack(pList);
            head = *(int*)((u8*)pList + 4);
        } while (head != 0);
    }
}
#endif

void NgPostProc::SetBloomColor() {
    float diff = 1.0f - mBloomThreshold;
    float divisor = (diff >= 0.0f) ? 1.0f : mBloomThreshold;
    float scale = 1.0f / divisor;
    float invScale = 1.0f / scale;

    Vector4 bloomParams(scale * 0.3f, scale * 0.59f, scale * 0.11f, invScale);
    TheShaderMgr.SetPConstant(kPS_BloomParams, bloomParams);
}

void NgPostProc::QueueMotionBlurObject(RndDrawable *draw) {
    if (!mMotionBlurDrawList.find(draw)) {
        mMotionBlurDrawList.insert(mMotionBlurDrawList.end(), draw);
    }
}

void NgPostProc::CheckBlendPrevious() {
    Vector4 blendParams(mBlendVec.x, mBlendVec.y, mBlendVec.z, 0.0f);
    TheShaderMgr.SetPConstant(kPS_BlendPrevious, blendParams);
}

void NgPostProc::CheckVignette() {
    if (DoVignette()) {
        Vector4 vignetteParams(mVignetteColor.red, mVignetteColor.green, mVignetteColor.blue, mVignetteIntensity);
        TheShaderMgr.SetPConstant(kPS_Vignette, vignetteParams);
        TheShaderMgr.unk3e = true;
    }
}

void NgPostProc::CheckMotionBlur() {
    if (DoMotionBlur()) {
        float blend = mMotionBlurBlend;
        Vector4 motionParams(
            mMotionBlurWeight.red * blend,
            mMotionBlurWeight.green * blend,
            mMotionBlurWeight.blue * blend,
            mMotionBlurWeight.alpha
        );
        TheShaderMgr.SetPConstant(kPS_MotionBlur, motionParams);
        TheShaderMgr.unk38 = true;
    }
}

void NgPostProc::CheckChromaticAberration() {
    if (DoChromaticAberration()) {
        Vector4 chromaParams(
            mChromaticAberrationOffset / (float)TheRnd.Width(),
            mChromaticAberrationOffset / (float)TheRnd.Height(),
            0.0f,
            0.0f
        );
        TheShaderMgr.SetPConstant(kPS_ChromaticAberration, chromaParams);
        if (mChromaticSharpen) {
            TheShaderMgr.unk3d = true;
        } else {
            TheShaderMgr.unk3c = true;
        }
    }
}

void NgPostProc::CheckHallOfTime() {
    if (HallOfTime() && !mMotionBlurEnabled) {
        Vector4 hallParams(mHallOfTimeRate, mHallOfTimeMix, 0.0f, 0.0f);
        TheShaderMgr.SetPConstant(kPS_HallOfTimeParams, hallParams);
        Vector4 hallColor(mHallOfTimeColor.red, mHallOfTimeColor.green, mHallOfTimeColor.blue, 1.0f);
        TheShaderMgr.SetPConstant(kPS_HallOfTimeColor, hallColor);
        TheShaderMgr.unk34 = mHallOfTimeType + 1;
    }
}

void NgPostProc::CheckPosterizeAndKaleidoscope() {
    Vector4 posterParams(0.0f, 0.0f, 0.0f, 0.0f);
    Vector4 kaleidoParams(0.0f, 0.0f, 0.0f, 0.0f);
    if (0.0f < mPosterLevels * mPosterMin) {
        TheShaderMgr.unk2b = true;
        posterParams.x = mPosterLevels;
        posterParams.y = mPosterLevels * mPosterMin;
    }
    if (0.0f < mKaleidoscopeComplexity) {
        TheShaderMgr.unk2c = true;
        kaleidoParams.w = mKaleidoscopeRadius;
        kaleidoParams.y = mKaleidoscopeSize;
        float angle = mKaleidoscopeAngle * 0.017453292f;
        kaleidoParams.x = 6.2831855f / mKaleidoscopeComplexity;
        kaleidoParams.z = angle;
        if (mKaleidoscopeFlipUVs) {
            posterParams.z = 2.0f;
        } else {
            posterParams.z = 1.0f;
        }
    }
    TheShaderMgr.SetPConstant(kPS_Posterize, posterParams);
    TheShaderMgr.SetPConstant(kPS_Kaleidoscope, kaleidoParams);
}

void NgPostProc::CheckGradientMap() {
    if (DoGradientMap()) {
        Vector4 gradParams(mGradientMapStart, mGradientMapEnd, mGradientMapIndex, mGradientMapOpacity);
        TheShaderMgr.SetPConstant(kPS_EmissiveTex, mGradientMap.Ptr());
        TheShaderMgr.SetPConstant(kPS_GradientMap, gradParams);
        TheShaderMgr.unk3a = (mGradientMap.Ptr() != NULL);
    }
}

void NgPostProc::CheckHueConverge() {
    if (ColorXfmEnabled()) {
        Vector4 hueParams(mHueTarget * (1.0f / 360.0f) + 0.5f, mHueFocus, mBlendAmount, mBrightnessPower);
        TheShaderMgr.SetPConstant(kPS_HueConverge, hueParams);
        TheShaderMgr.unk2a = (0.0f < mBlendAmount);
    }
}

void NgPostProc::CheckRefract() {
    if (DoRefraction()) {
        float angleRad = mRefractAngle * 0.017453292f;
        float sinAngle = (float)sin((double)angleRad);
        float cosAngle = (float)cos((double)angleRad);

        unk234 += mRefractVelocity.x * mDeltaSecs;
        unk238 += mRefractVelocity.y * mDeltaSecs;

        Vector4 refractParams(angleRad, sinAngle, cosAngle, mRefractDist);

        unk234 = (float)fmod((double)unk234, 1.0);
        unk238 = (float)fmod((double)unk238, 1.0);

        Vector4 panningParams(mRefractScale.x, mRefractScale.y,
                              mRefractPanning.x + unk234, mRefractPanning.y + unk238);
        TheShaderMgr.SetPConstant(kPS_RefractStrength, refractParams);
        TheShaderMgr.SetPConstant(kPS_RefractPanning, panningParams);

        TheShaderMgr.SetPConstant((PShaderConstant)1, mRefractMap.Ptr());
        TheRenderState.SetTextureFilter(1, (RndRenderState::FilterMode)1, false);
        TheRenderState.SetTextureClamp(1, (RndRenderState::ClampMode)0);
        TheShaderMgr.unk3b = true;
    } else {
        unk234 = 0.0f;
        unk238 = 0.0f;
    }
}

void NgPostProc::CheckNoise() {
    bool doNoise = mNoiseIntensity != 0.0f && mNoiseMap;
    if (doNoise) {
        if (!mNoiseStationary) {
            Vector4 seeds(RandomFloat(), RandomFloat(), RandomFloat(), RandomFloat());
            TheShaderMgr.SetPConstant(kPS_NoiseSeeds, seeds);
            Vector4 params(mNoiseBaseScale.x, mNoiseBaseScale.y, mNoiseTopScale, mNoiseIntensity);
            TheShaderMgr.SetPConstant(kPS_NoiseParams, params);
        } else {
            Vector4 seeds(mRandomSeed1, mRandomSeed2, mRandomSeed1, mRandomSeed2);
            TheShaderMgr.SetPConstant(kPS_NoiseSeeds, seeds);
            Vector4 params(mNoiseBaseScale.x, mNoiseBaseScale.y, 1.0f, mNoiseIntensity);
            TheShaderMgr.SetPConstant(kPS_NoiseParams, params);
        }
        TheShaderMgr.SetPConstant(kPS_Anisotropy, mNoiseMap.Ptr());
        TheRenderState.SetTextureFilter(0xd, (RndRenderState::FilterMode)1, false);
        TheRenderState.SetTextureClamp(0xd, (RndRenderState::ClampMode)0);
    }
    TheShaderMgr.unk2d = doNoise;
    TheShaderMgr.unk2e = doNoise ? mNoiseMidtone : false;
}

void NgPostProc::ReleaseTex() {
    for (int i = 0; (unsigned int)i < 3; i++) {
        sBloom.mTextures[i].FreeTextures();
    }
    RndVelocityBuffer::Singleton().FreeData();
}

void NgPostProc::Terminate() { ReleaseTex(); }

void NgPostProc::EndWorld() {
    RndVelocityBuffer::Singleton().CacheCameraSettings(TheRnd.mWorldCamCopy);
}

void NgPostProc::OnSelect() {
    RndPostProc::OnSelect();
    mMotionBlurDrawList.clear();
}

void NgPostProc::ModulateColorXfm() {
    float mod = mColorModulation;
    Transform xfm = mColorXfm.mColorXfm;
    if (mod != 1.0f) {
        xfm.m.x.x *= mod;
        xfm.m.x.y *= mod;
        xfm.m.x.z *= mod;
        xfm.m.y.x *= mod;
        xfm.m.y.y *= mod;
        xfm.m.y.z *= mod;
        xfm.m.z.x *= mod;
        xfm.m.z.y *= mod;
        xfm.m.z.z *= mod;
    }
    RndShaderMgr &mgr = TheShaderMgr;
    mgr.SetPConstant4x3((PShaderConstant)kVS_WorldTransform, Hmx::Matrix4(xfm));
}

void NgPostProc::DoPost() {
    RndPostProc::DoPost();
    DoVelocity();
    DoBloom();
    ModulateColorXfm();
    CheckNoise();
    CheckBlendPrevious();
    CheckHallOfTime();
    CheckMotionBlur();
    CheckGradientMap();
    CheckHueConverge();
    CheckRefract();
    CheckChromaticAberration();
    CheckPosterizeAndKaleidoscope();
    CheckVignette();
    bool xfm = ColorXfmEnabled();
    TheShaderMgr.unk29 = xfm;
    TheShaderMgr.unk2f = BlendPrevious();
    mMotionBlurEnabled = false;
}

void NgPostProc::OnUnselect() {
    RndPostProc::OnUnselect();
    mMotionBlurDrawList.clear();
}
