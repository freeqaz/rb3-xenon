#include "rndobj/Shader.h"
#include "Rnd.h"
#include "os/System.h"
#include "rndobj/HiResScreen.h"
#include "rnddx9/RenderState.h"
#include "rndobj/Cam.h"
#include "rndobj/Env.h"
#include "rndobj/Mat_NG.h"
#include "rndobj/Env_NG.h"
#include "os/Debug.h"
#include "rndobj/Mat.h"
#include "rndobj/Rnd.h"
#include "rndobj/Rnd_NG.h"
#include "rndobj/ShaderMgr.h"
#include "rndobj/ShaderOptions.h"
#include "rndobj/ShaderProgram.h"
#include "rndobj/Shockwave.h"
#include "rndobj/Spline.h"
#include "rndobj/Stats_NG.h"
#include "math/Utl.h"
#include "utl/Loader.h"
#include "utl/Str.h"
#include <set>

#ifdef HX_NATIVE
bool RndShader::sCurrentUseAO;
bool RndShader::sMatShadersOK;
ModalCallbackFunc *RndShader::mModalCallback;
ShaderType RndShader::sCurrentShader;
bool RndShader::sCurrentSkinned;
RndShader *RndShader::sShaders[kMaxShaderTypes];
#endif

std::set<unsigned int> sWarnings;
RndShaderSimple gShaderSimple;
RndShaderParticles gShaderParticles;
RndShaderMultimesh gShaderMultimesh;
RndShaderStandard gShaderStandard;
RndShaderPostProc gShaderPostProc;
RndShaderDrawRect gShaderDrawRect;
RndShaderUnwrapUV gShaderUnwrapUV;
RndShaderVelocity gShaderVelocity;
RndShaderVelocityCamera gShaderVelocityCamera;
RndShaderDepthVolume gShaderDepthVolume;
RndShaderFur gShaderFur;
RndShaderSyncTrack gShaderSyncTrack;

unsigned int StrHash(const char *str) {
    unsigned int hash = 0;
    int constMult = 0xF8C9;
    for (const unsigned char *p = (const unsigned char *)str; *p != '\0'; p++) {
        hash = hash * constMult + *p;
        constMult *= 0x5C6B7;
    }
    return hash;
}

void CheckDistortionOpts(RndMat *, ShaderOptions &);
void CheckDistortion(RndMat *);
void SetColorWriteMask(const ShaderOptions &, RndMat *);
void CheckShadow();
void CheckExtrude();

void RndShader::Init() {
    sShaders[kBloomShader] = &gShaderSimple;
    sShaders[kDepthVolumeShader] = &gShaderDepthVolume;
    sShaders[kBloomGlareShader] = &gShaderSimple;
    sShaders[kBlurShader] = &gShaderSimple;
    sShaders[kDownsampleDepthShader] = &gShaderSimple;
    sShaders[kMultimeshShader] = &gShaderMultimesh;
    sShaders[kDownsample4xShader] = &gShaderSimple;
    sShaders[kDownsampleShader] = &gShaderSimple;
    sShaders[kDrawRectShader] = &gShaderDrawRect;
    sShaders[kFurShader] = &gShaderFur;
    sShaders[kErrorShader] = &gShaderSimple;
    sShaders[kMultimeshBBShader] = &gShaderMultimesh;
    sShaders[kLineNozShader] = &gShaderSimple;
    sShaders[kMovieShader] = &gShaderSimple;
    sShaders[kLineShader] = &gShaderSimple;
    sShaders[kPostprocessErrorShader] = &gShaderSimple;
    sShaders[kShadowmapShader] = &gShaderSimple;
    sShaders[kPlayerDepthVisShader] = &gShaderSimple;
    sShaders[kParticlesShader] = &gShaderParticles;
    sShaders[kStandardShader] = &gShaderStandard;
    sShaders[kPostprocessShader] = &gShaderPostProc;
    sShaders[kStandardBBShader] = &gShaderStandard;
    sShaders[kPlayerDepthShellShader] = &gShaderSimple;
    sShaders[kUnwrapUVShader] = &gShaderUnwrapUV;
    sShaders[kVelocityCameraShader] = &gShaderVelocityCamera;
    sShaders[kVelocityObjectShader] = &gShaderVelocity;
#ifdef HX_NATIVE
    sShaders[kSyncTrackShader] = &gShaderSyncTrack;
    sShaders[kPlayerDepthShell2Shader] = &gShaderSimple;
    sShaders[kDepthBuffer3DShader] = &gShaderSimple;
    sShaders[kYUVtoRGBShader] = &gShaderSimple;
    sShaders[kSyncTrackChargeEffectShader] = &gShaderSyncTrack;
    sShaders[kYUVtoBlackAndWhiteShader] = &gShaderSimple;
    sShaders[kPlayerGreenScreenShader] = &gShaderSimple;
    sShaders[kPlayerDepthGreenScreenShader] = &gShaderSimple;
    sShaders[kCrewPhotoShader] = &gShaderSimple;
    sShaders[kTwirlShader] = &gShaderSimple;
    sShaders[kKillAlphaShader] = &gShaderSimple;
    sShaders[kAllWhiteShader] = &gShaderStandard;
#endif
}

void RndShader::CheckForceCull(ShaderType s) {
    int cullOverride = TheShaderMgr.CullModeOverride();
    // Retail RB3 X360 gates on raw Rnd::Mode values 2 and 7 here; dc3's
    // newer source used kDrawShadowColor (3) and 8 for these two draw modes.
    if (TheRnd.DrawMode() == (Rnd::Mode)2 || cullOverride == 1) {
        TheRenderState.SetCullMode((RndRenderState::CullMode)0);
    } else if (s != kShadowmapShader && cullOverride != 3 && TheRnd.DrawMode() != (Rnd::Mode)7) {
        if (cullOverride == 2) {
            TheRenderState.SetCullMode((RndRenderState::CullMode)2);
        }
    } else {
        TheRenderState.SetCullMode((RndRenderState::CullMode)6);
    }
}

bool RndShader::RedundantState(
    const RndMat *mat, ShaderType s, bool skinned, bool useAO, bool b5
) {
    if (!b5 && mat && (NgMat *)mat == NgMat::Current() && !mat->Dirty()
        && s == sCurrentShader && skinned == sCurrentSkinned && useAO == sCurrentUseAO) {
        if (s == kStandardShader || s == kStandardBBShader || s == kParticlesShader
            || s == kMultimeshShader || s == kMultimeshBBShader
#ifdef HX_NATIVE
            || s == kSyncTrackShader || s == kSyncTrackChargeEffectShader
            || s == kAllWhiteShader
#endif
        ) {
            return true;
        }
    }
    sCurrentUseAO = useAO;
    sCurrentShader = s;
    sCurrentSkinned = skinned;
    return false;
}

void RndShader::ShaderWarn(const char *msg) {
    unsigned int hash = StrHash(msg);
    if (sWarnings.end() == sWarnings.find(hash)) {
        MILO_NOTIFY(msg);
        sWarnings.insert(hash);
    }
    if (TheLoadMgr.EditMode()) {
        bool fail = false;
        if (mModalCallback) {
            StackString<1024> str(msg);
            (*mModalCallback)(fail, (char *)str.c_str(), true);
        }
    }
}

void RndShader::WarnMatProp(const char *prop, NgMat *mat, NgEnviron *env, ShaderType s) {
    ShaderWarn(MakeString(
        "[%s] must have %s.  (%s, %s)",
        PathName(mat),
        prop,
        PathName(env),
        ShaderTypeName(s)
    ));
    sMatShadersOK = false;
}

bool RndShader::MatShaderFlagsOK(RndMat *mat, ShaderType s) {
    if (!mat || TheRnd.DefaultEnv() == RndEnviron::Current()
        || TheRnd.DrawMode() == Rnd::kDrawOcclusion) {
        return true;
    }
    NgEnviron *curEnv = (NgEnviron *)RndEnviron::Current();
    sMatShadersOK = true;
    RndShader *curShader = sShaders[s];
    bool b1824 = mat->UseEnviron() && RndEnviron::Current()->NumLights_Real() != 0;
    if (curShader->CheckError((MatFlagErrorType)0) && !mat->FadeOut()) {
        bool fadeoutCheck = curEnv->FadeOut() && curEnv->FadeEnd() != curEnv->FadeStart();
        if (fadeoutCheck) {
            WarnMatProp("fadeout checked", (NgMat *)mat, curEnv, s);
        }
    } else if (mat->FadeOut()) {
        bool fadeoutUncheck =
            curEnv->FadeOut() && curEnv->FadeEnd() != curEnv->FadeStart();
        if (!fadeoutUncheck) {
            WarnMatProp("fadeout unchecked", (NgMat *)mat, curEnv, s);
        }
    }
    if (curShader->CheckError((MatFlagErrorType)1) && b1824 && !mat->PointLights()
        && curEnv->NumLights_Point()) {
        WarnMatProp("point_lights checked", (NgMat *)mat, curEnv, s);
    }
    if (curShader->CheckError((MatFlagErrorType)2) && !mat->ColorAdjust()
        && curEnv->UseColorAdjust()) {
        WarnMatProp("color_adjust checked", (NgMat *)mat, curEnv, s);
    }
    return sMatShadersOK;
}

bool RndShader::DisplayMatShaderFlagsError(RndMat *mat, ShaderType s) {
    bool ret = false;
    if (TheShaderMgr.ShowShaderErrors()) {
        ret = !MatShaderFlagsOK(mat, s);
    }
    return ret;
}

// Retail (0x824a5740, 112 B) is only the shader-type override and a tail call
// into sShaders[type]->Select: no range assert, no EditMode/UsingCD error-shader
// branch, no null check. Retail compares DrawMode against 1 and 5 here (not the
// 2/6 of DC3); the native build keeps DC3's values and its diagnostic paths.
void RndShader::SelectConfig(RndMat *mat, ShaderType shader_type, bool b3) {
#ifdef HX_NATIVE
    RndShader *shader;
    if (TheRnd.DrawMode() == 2) {
        shader_type = kShadowmapShader;
    } else if (TheRnd.DrawMode() == 6) {
        shader_type = kVelocityObjectShader;
    } else if (TheShaderMgr.InDepthVolume()) {
        shader_type = kDepthVolumeShader;
    }
    // Native/web: skip shader diagnostic path. On Xbox retail UsingCD()==true
    // so this path is dead code. On native, UsingCD() may be false (no .ark),
    // which would activate editor-mode shader validation that crashes on WASM
    // (virtual calls into unimplemented NG shader subsystems).
    if (!b3 && TheLoadMgr.EditMode()) {
        DisplayMatShaderFlagsError(mat, shader_type);
        shader_type = shader_type == kPostprocessShader
            ? kPostprocessErrorShader
            : kErrorShader;
    }
    shader = sShaders[shader_type];
    if (!shader) {
        // Fallback: unregistered shader type — use error shader
        shader = sShaders[kErrorShader];
        if (!shader) return;
    }
    shader->Select(mat, shader_type, b3);
#else
    if (TheRnd.DrawMode() == 1) {
        shader_type = kShadowmapShader;
    } else if (TheRnd.DrawMode() == 5) {
        shader_type = kVelocityObjectShader;
    } else if (TheShaderMgr.InDepthVolume()) {
        shader_type = kDepthVolumeShader;
    }
    sShaders[shader_type]->Select(mat, shader_type, b3);
#endif
}

void RndShader::Cache(ShaderType s, ShaderOptions opts, RndMat *mat) {
    RndShaderProgram &program = TheShaderMgr.FindShader(s, opts);
    if (!program.Cached()) {
#ifdef HX_NATIVE
        if (!program.Cache(s, opts, nullptr, nullptr) && !TheShaderMgr.CacheShaders()) {
            MatShaderFlagsOK(mat, s);
        }
#else
        // Retail (0x824A58D8) ignores the result: no cache-shaders test and no
        // material flag check follow.
        program.Cache(s, opts, nullptr, nullptr);
#endif
    }
    // Retail compares the raw draw mode against 2 (RB3's shadow-colour mode).
    bool select = s == kShadowmapShader || TheRnd.DrawMode() == (Rnd::Mode)2;
    program.Select(select);
}

void RndShaderSimple::Select(RndMat *mat, ShaderType s, bool b) {
    if (!mat) {
        if (s == kLineNozShader) {
            mat = TheShaderMgr.DrawHighlightMat();
            mat->SetZMode(kZModeForce);
            s = kLineShader;
        } else {
            mat = TheRnd.DefaultMat();
        }
    }
    TheRenderState.SetFillMode((RndRenderState::FillMode)0);
    auto _tmp6 = TheShaderMgr.BoneCount();
    bool isSkinned = _tmp6 && (s == kErrorShader || s == kShadowmapShader);
    if (!RedundantState(mat, s, isSkinned, TheShaderMgr.UseAO(), b)) {
        ((NgMat *)mat)->SetupShader(TheShaderMgr.AllowPerPixel(), true);
        u64 optsVal = CalcShaderOpts((NgMat *)mat, s, b);
#ifdef HX_NATIVE
        TheNgStats->mMats++;
#endif
        SetColorWriteMask(ShaderOptions(optsVal), mat);
        CheckForceCull(s);
        Cache(s, ShaderOptions(optsVal), mat);
    }
}

bool RndShaderMultimesh::CheckError(MatFlagErrorType type) {
    return type == (MatFlagErrorType)0 || type == (MatFlagErrorType)1 || type == (MatFlagErrorType)2;
}

bool RndShaderParticles::CheckError(MatFlagErrorType type) {
        return !(type != (MatFlagErrorType)1 && type != (MatFlagErrorType)3) && TheRnd.DrawMode() != 3;
}

// Retail (0x824A5AF8) tests the option bit, then Offscreen(), then the
// material's alpha-write flag, and writes all four channels if any is set.
void SetColorWriteMask(const ShaderOptions &opts, RndMat *mat) {
    unsigned int mask;
    if ((opts.flags & 0x400000) != 0 || TheNgRnd.Offscreen() || mat->mAlphaWrite) {
        mask = 0xF;
    } else {
        mask = 7;
    }
    TheRenderState.SetColorWriteMask(mask);
}

void CheckDistortionOpts(RndMat *mat, ShaderOptions &opts) {
    RndSpline *spline = RndSpline::sGlobalDefaultSpline;
    if (spline
#ifdef RB3_DC3_MAT
        && !mat->mNeverFitToSpline
#endif
        && spline->mCtrlPoints.size() >= 2) {
        opts.flags |= (u64)1 << 55;
        opts.flags = ((u64)(spline->mPulseDrawing & 1) << 56)
            | (opts.flags & ~((u64)1 << 56));
    }
    RndShockwave *shockwave = RndShockwave::sSelected;
    if (shockwave) {
        bool ampBad = NearlyZero(shockwave->mAmplitude);
#ifdef RB3_DC3_MAT
        if (!ampBad && mat->mAllowDistortionEffects) {
            bool multBad = NearlyZero(mat->mShockwaveMult);
            if (!multBad) {
                opts.flags |= (u64)1 << 60;
            }
        }
#endif
    }
}

void CheckDistortion(RndMat *mat) {
    RndSpline *spline = RndSpline::sGlobalDefaultSpline;
    if (spline
#ifdef RB3_DC3_MAT
        && !mat->mNeverFitToSpline
#endif
        && !spline->mManual
        && spline->mCtrlPoints.size() >= 2) {
        spline->PrepareShader();
    }
    RndShockwave *shock = RndShockwave::sSelected;
    if (shock) {
        bool ampBad = NearlyZero(shock->mAmplitude);
#ifdef RB3_DC3_MAT
        if (!ampBad && mat->mAllowDistortionEffects) {
            bool multBad = NearlyZero(mat->mShockwaveMult);
            if (!multBad) {
                shock->PrepareShader(mat->mShockwaveMult);
            }
        }
#endif
    }
}

void CheckShadow() {
    RndCam *shadowCam = TheNgRnd.GetShadowCam();
    if (shadowCam) {
        Transform viewXfm;
        Hmx::Matrix4 projMtx;
        shadowCam->GetViewProjectXfms(viewXfm, projMtx);
        Hmx::Matrix4 viewProj = Hmx::operator*(viewXfm, projMtx);
        // Clip space to shadow-map texture space: scale x/y by 0.5 (y flipped),
        // bias both by 0.5 + 1/1024.
        static Hmx::Matrix4 sShadowTexMatrix(
            Vector4(0.5f, 0.0f, 0.0f, 0.0f),
            Vector4(0.0f, -0.5f, 0.0f, 0.0f),
            Vector4(0.0f, 0.0f, 1.0f, 0.0f),
            Vector4(0.5009765625f, 0.5009765625f, 0.0f, 1.0f)
        );
        viewProj = Hmx::operator*(viewProj, sShadowTexMatrix);
        TheShaderMgr.SetVConstant((VShaderConstant)0x28, viewProj);
    }
}

void CheckExtrude() {
    // Retail RB3 X360 gates extrude on raw Rnd::Mode value 2 (dc3 used 3).
    if (TheRnd.DrawMode() == (Rnd::Mode)2) {
        TheRenderState.SetDepthTestEnable(true);
        TheRenderState.SetDepthWriteEnable(true);
        TheRenderState.SetBlendEnable(true);
        TheRenderState.SetBlend(
            (RndRenderState::Blend)0, (RndRenderState::Blend)1,
            (RndRenderState::Blend)1, (RndRenderState::Blend)1
        );
        TheRenderState.SetDepthFunc((RndRenderState::TestFunc)1);
        TheRenderState.SetAlphaTestEnable(false);
        Transform viewXfm;
        Hmx::Matrix4 projMtx;
        RndCam::Current()->GetViewProjectXfms(viewXfm, projMtx);
        Hmx::Matrix4 viewProj = Hmx::operator*(viewXfm, projMtx);
        TheShaderMgr.SetVConstant(kVS_ViewProjMatrix, viewProj);
    }
}

u64 RndShaderVelocityCamera::CalcShaderOpts(NgMat *mat, ShaderType s, bool b) {
    return (u64)(TheHiResScreen.IsActive() & 1) << 52;
}

u64 RndShaderVelocity::CalcShaderOpts(NgMat *mat, ShaderType s, bool b) {
    // Retail (0x824A7440) has no HiResScreen term: only the skinned bit.
    return ((u64)(TheShaderMgr.BoneCount() > 0) & 1) << 12;
}

u64 RndShaderUnwrapUV::CalcShaderOpts(NgMat *mat, ShaderType s, bool b) {
    // Retail RB3 X360 has no HiResScreen term here (a later-engine addition).
    u64 opts = ((u64)(bool)mat->GetDiffuseTex() & 1) | 0x10;
    return opts << 4;
}

u64 RndShaderDepthVolume::CalcShaderOpts(NgMat *mat, ShaderType s, bool b) {
    // Retail RB3 X360 has no HiResScreen term (so the low-half mask clears only
    // bit 23, not dc3's bits 23+52) and gates on raw Rnd::Mode 2.
    u64 skinned = (u64)(bool)TheShaderMgr.BoneCount() & 1;
    u64 opts = (((u64)(uint)TheShaderMgr.unk1c & ~0xFFFFFFFCULL) | skinned << 11) << 1;
    u64 shadow = (u64)(TheRnd.DrawMode() == (Rnd::Mode)2) & 1;
    return (shadow << 23) | (opts & ~(1ULL << 23));
}

u64 RndShaderSimple::CalcShaderOpts(NgMat *mat, ShaderType s, bool b) {
    u64 opts = 0;
    switch (s) {
    case kBlurShader:
        opts = (u64)((TheShaderMgr.unk14 - 1) & 0xf) << 14;
        break;
    case kErrorShader: {
        int boneCount = TheShaderMgr.BoneCount();
        bool displayError = TheShaderMgr.GetShaderErrorDisplay();
        u64 bc = (u64)(bool)boneCount & 1;
        u64 de = (u64)displayError & 1;
        opts = (de << 23 | bc) << 12;
        break;
    }
    case kPostprocessErrorShader: {
        bool displayError = TheShaderMgr.GetShaderErrorDisplay();
        opts = (u64)(displayError & 1) << 35;
        break;
    }
    case kShadowmapShader: {
        int bc = TheShaderMgr.BoneCount();
        opts = (u64)(((bool)bc & 1) << 12);
        break;
    }
    default:
        break;
    }
    // Retail RB3 X360 gates on raw Rnd::Mode value 3 here (dc3's newer enum
    // shifted kDrawOcclusion to 4); same -1 divergence handled in CheckForceCull.
    if (TheRnd.DrawMode() == (Rnd::Mode)3) opts = 0;
    return opts;
}

u64 RndShaderDrawRect::CalcShaderOpts(NgMat *mat, ShaderType s, bool b) {
    // Retail RB3 X360: occlusion is raw Rnd::Mode 3 (dc3's enum shifted it to
    // 4), and there are no HiResScreen / ResourceCached terms, so the mask
    // clears only bit 22.
    if (TheRnd.DrawMode() == (Rnd::Mode)3) return 0;
    u64 opts = (((u64)(bool)mat->GetDiffuseTex() & 1)
        | (u64)(mat->Prelit() & 1) << 4) << 4;
    bool offscreen;
    if (b) {
        offscreen = TheShaderMgr.GetUnk41();
    } else {
        offscreen = TheNgRnd.Offscreen();
    }
    u64 pseudoHDR;
    if (!offscreen && mat->AllowHDR()) {
        pseudoHDR = 1;
    } else {
        pseudoHDR = 0;
    }
    return ((pseudoHDR & 1) << 22) | (opts & ~(1ULL << 22));
}

u64 RndShaderParticles::CalcShaderOpts(NgMat *mat, ShaderType s, bool b) {
    // Retail RB3 X360 differs from the dc3-era body in four ways, all read off
    // the target listing: no fog term at all, no HiResScreen/ResourceCached
    // bits, and Rnd::Mode is one lower than dc3's enum (velocity 6 not 7,
    // occlusion 3 not 4). The option word is also folded field-by-field as it
    // is computed rather than assembled in one expression at the end.
    u64 opts = (((u64)(bool)mat->GetDiffuseTex() & 1) | 0x10) << 4;
    int texGen = mat->GetTexGen();
    uint texGenVal;
    switch (texGen) {
    case kTexGenSphere:
        texGenVal = 1;
        break;
    case kTexGenProjected:
        texGenVal = 2;
        break;
    default:
        texGenVal = texGen == kTexGenEnviron ? 3 : 0;
        break;
    }
    // The target clears bits 16-17 alongside the 2-bit texgen field at 10-11
    // before inserting; bits 16-17 are provably zero here, so the wider clear
    // is a no-op. dc3's target does exactly the same, so it is a property of
    // the shared source, not of retail.
    opts = (opts & ~0x30C00ULL) | (((s64)(int)texGenVal & 3) << 10);
    RndEnviron *env = RndEnviron::Current();
    bool fadeOut;
    if (b) {
        fadeOut = mat->FadeOut();
    } else {
        fadeOut = env->FadeOut() && env->FadeEnd() != env->FadeStart();
    }
    u64 pseudoHDR;
    if (!fadeOut) {
        bool offscreen;
        if (b) {
            offscreen = TheShaderMgr.GetUnk41();
        } else {
            offscreen = TheNgRnd.Offscreen();
        }
        if (!offscreen && mat->AllowHDR()) {
            pseudoHDR = 1;
        } else {
            pseudoHDR = 0;
        }
    } else {
        pseudoHDR = 0;
    }
    opts = (opts & ~(1ULL << 0x16)) | ((pseudoHDR & 1) << 0x16);
    bool colorAdjust;
    if (b) {
        colorAdjust = mat->ColorAdjust();
    } else {
        colorAdjust = env->UseColorAdjust();
    }
    opts = (opts & ~((1ULL << 0x15) | (1ULL << 0x35)))
        | (((u64)(mat->GetIntensify() & 1) << 0x20 | (u64)(colorAdjust & 1)) << 0x15);
    if (fadeOut) {
        Vector4 fadeParams(mat->unk238, mat->unk23c, mat->unk240, mat->unk244);
        TheShaderMgr.SetPConstant((PShaderConstant)0x68, fadeParams);
        opts = (opts & ~(3ULL << 0x1a)) | (((s64)mat->unk234 & 3) << 0x1a);
    }
    if (mat->GetRefractEnabled(b) && mat->GetRefractNormalMap() != nullptr) {
        opts |= 0x400000000000;
    }
    if (TheRnd.DrawMode() == (Rnd::Mode)6) {
        opts |= 0x200000000000;
    }
    return (s64)(TheRnd.DrawMode() != (Rnd::Mode)3 ? -1 : 0) & opts;
}

// The two bodies below were previously scored against each other's retail
// address: the map had RndShaderMultimesh's name on 0x824A66E8, but the RTTI
// Complete Object Locator in front of the vtable that holds 0x824A66E8 names
// RndShaderStandard, and the one in front of the vtable holding 0x824A6070
// names RndShaderMultimesh.  Both are written as ShaderOptions bitfield
// assignments (see ShaderOptions.h), which is what produces retail's
// `and ~mask` + `rldimi` pairs.
u64 RndShaderMultimesh::CalcShaderOpts(NgMat *mat, ShaderType s, bool b) {
    // Retail RB3 X360: occlusion is raw Rnd::Mode 3.
    if (TheRnd.DrawMode() == (Rnd::Mode)3)
        return 0;
    NgEnviron *env = (NgEnviron *)RndEnviron::Current();
    ShaderOptions opts(0);
    bool hasDiffuse = mat->GetDiffuseTex() != nullptr;
    opts.mDiffuseMap = hasDiffuse;
    opts.mPrelit = mat->Prelit();
    opts.mRealLights = mat->UseEnviron() && env->NumLights_Real() > 0;
    opts.mApproxLights = mat->UseEnviron() && env->NumLights_Approx() > 0;
    if (opts.mRealLights || opts.mApproxLights) {
        opts.mSpecular = mat->GetSpecularRGB().Pack() != 0;
        if (TheShaderMgr.AllowPerPixel() && mat->GetPerPixelLit()) {
            int hasNormal = mat->NormalMap() != nullptr;
            opts.mNormalMap = hasNormal;
            opts.mPerPixelLighting = 1;
            bool normDetail = mat->GetNormDetailMap() != nullptr
                && mat->GetNormDetailStrength() > 0.0f;
            opts.mNormDetail = normDetail;
            // Retail RB3 X360 has no mFlipNormal here (dc3 adds it).
            opts.mSpecularMap = opts.mSpecular && mat->GetSpecularMap() != nullptr;
            opts.mRimLight = mat->GetRimRGB().Pack() != 0;
            opts.mRimLightUnder = opts.mRimLight && mat->GetRimLightUnder();
            opts.mRimLightMap = opts.mRimLight && mat->GetRimMap() != nullptr;
        }
        if (mat->GetEnvironMap() != nullptr) {
            opts.mEnvironMapFalloff = mat->GetEnvironMapFalloff();
            opts.mEnvironMap = 1;
            opts.mEnvironMapSpecMask = opts.mSpecularMap && mat->GetEnvironMapSpecMask();
        }
        opts.mNumPoint = env->NumLights_Point();
    }
    bool hasEmissive = mat->GetEmissiveMap() != nullptr;
    opts.mGlowMap = hasEmissive;
    opts.mIntensify = mat->GetIntensify();
    int texGenVal;
    switch (mat->GetTexGen()) {
    case kTexGenSphere:
        texGenVal = 1;
        break;
    case kTexGenProjected:
        texGenVal = 2;
        break;
    case kTexGenEnviron:
        texGenVal = 3;
        break;
    default:
        texGenVal = 0;
        break;
    }
    opts.mTexGen = texGenVal;
    bool fadeOut;
    if (b) {
        fadeOut = mat->FadeOut();
    } else {
        fadeOut = env->FadeOut() && env->FadeEnd() != env->FadeStart();
    }
    opts.mPseudoHDR = !fadeOut
        && !(b ? TheShaderMgr.GetUnk41() : TheNgRnd.Offscreen()) && mat->AllowHDR();
    // Retail RB3 X360: no fog term for multimesh.
    opts.mBillboard = s == kMultimeshBBShader;
    bool colorAdjust;
    if (b) {
        colorAdjust = mat->ColorAdjust();
    } else {
        colorAdjust = env->UseColorAdjust();
    }
    opts.mColorXfm = colorAdjust;
    opts.mColorMod = mat->GetColorModFlags();
    opts.mCustomVariation = mat->GetShaderVariation();
    if (!opts.mPrelit && TheShaderMgr.UseAO() && env->AOEnabled()
        && env->AOStrength() > 0.003f) {
        opts.mEnableAO = 1;
    }
    opts.mToneMapping = env->UseToneMapping();
    if (fadeOut) {
        Vector4 fadeParams(mat->unk238, mat->unk23c, mat->unk240, mat->unk244);
        TheShaderMgr.SetPConstant((PShaderConstant)0x68, fadeParams);
        opts.mFadeOut = mat->unk234;
    }
    // Retail carries no CheckDistortionOpts, projected-light count,
    // ShowShaderCost or HiResScreen bits here (dc3 adds them).
    return opts.flags;
}

u64 RndShaderStandard::CalcShaderOpts(NgMat *mat, ShaderType s, bool b) {
    NgEnviron *env = (NgEnviron *)RndEnviron::Current();
    bool skinned = TheShaderMgr.BoneCount() != 0;
    ShaderOptions opts(0);
    opts.mSkinned = skinned;
    // Retail RB3 X360: occlusion is raw Rnd::Mode 3, and there is no
    // shadow-depth early-out (dc3 adds one).
    if (TheRnd.DrawMode() == (Rnd::Mode)3)
        return opts.flags;
    bool fadeOut;
    if (b) {
        fadeOut = mat->FadeOut();
    } else {
        fadeOut = env->FadeOut() && env->FadeEnd() != env->FadeStart();
    }
    u64 pseudoHDR;
    if (mat->AllowHDR() && !fadeOut) {
        bool offscreen;
        if (b) {
            offscreen = TheShaderMgr.GetUnk41();
        } else {
            offscreen = TheNgRnd.Offscreen();
        }
        if (!offscreen) {
            pseudoHDR = 1;
        } else {
            pseudoHDR = 0;
        }
    } else {
        pseudoHDR = 0;
    }
    bool hasDiffuse = mat->GetDiffuseTex() != nullptr;
    opts.mDiffuseMap = hasDiffuse;
    opts.mPrelit = mat->Prelit();
    opts.mPseudoHDR = pseudoHDR;
    opts.mRealLights = mat->UseEnviron() && env->NumLights_Real() > 0;
    opts.mApproxLights = mat->UseEnviron() && env->NumLights_Approx() > 0;
    if (opts.mRealLights || opts.mApproxLights) {
        opts.mSpecular = mat->GetSpecularRGB().Pack() != 0;
        if (TheShaderMgr.AllowPerPixel() && mat->GetPerPixelLit()) {
            int hasNormal = mat->NormalMap() != nullptr;
            opts.mNormalMap = hasNormal;
            opts.mPerPixelLighting = 1;
            bool normDetail = mat->GetNormDetailMap() != nullptr
                && mat->GetNormDetailStrength() > 0.0f;
            opts.mNormDetail = normDetail;
            // Retail RB3 X360 has no mFlipNormal here (dc3 adds it).
            opts.mSpecularMap = opts.mSpecular && mat->GetSpecularMap() != nullptr;
            opts.mRimLight = mat->GetRimRGB().Pack() != 0;
            opts.mRimLightUnder = opts.mRimLight && mat->GetRimLightUnder();
            opts.mRimLightMap = opts.mRimLight && mat->GetRimMap() != nullptr;
            int hasShadowMap = TheRnd.GetShadowCam() != nullptr;
            opts.mShadowBuffer = hasShadowMap;
        }
        if (mat->GetEnvironMap() != nullptr) {
            opts.mEnvironMapFalloff = mat->GetEnvironMapFalloff();
            opts.mEnvironMap = 1;
            opts.mEnvironMapSpecMask = opts.mSpecularMap && mat->GetEnvironMapSpecMask();
        }
        bool recvProjLights = mat->GetRecvProjLights() && env->NumLights_Proj() > 0;
        bool pointCubeTex = mat->GetRecvPointCubeTex() && env->NumLights_Point() > 0
            && env->HasPointCubeTex();
        opts.mAnisotropic = mat->GetAnisotropy() > 0.0f;
        opts.mNumPoint = env->NumLights_Point();
        opts.mNumProj = recvProjLights ? env->NumLights_Proj() : 0;
        opts.mProjLightMultiply = recvProjLights && env->GetProjectedBlend() == 1;
        opts.mPointCubeTex = pointCubeTex;
    }
    if (mat->GetRefractEnabled(b) && mat->GetRefractNormalMap() != nullptr) {
        opts.mRefractWorld = 1;
    }
    bool hasEmissive = mat->GetEmissiveMap() != nullptr;
    opts.mGlowMap = hasEmissive;
    opts.mScreenAligned = mat->GetScreenAligned();
    opts.mIntensify = mat->GetIntensify();
    int texGenVal;
    switch (mat->GetTexGen()) {
    case kTexGenSphere:
        texGenVal = 1;
        break;
    case kTexGenProjected:
        texGenVal = 2;
        break;
    case kTexGenEnviron:
        texGenVal = 3;
        break;
    default:
        texGenVal = 0;
        break;
    }
    opts.mTexGen = texGenVal;
    // Retail RB3 X360: the fog source follows `b` (material vs environment),
    // as in the fur shader; dc3 reads the material flag unconditionally.
    opts.mFog = mat->AllowFog() && (b ? mat->GetFog() : env->FogEnable());
    opts.mBillboard = s == kStandardBBShader;
    bool colorAdjust;
    if (b) {
        colorAdjust = mat->ColorAdjust();
    } else {
        colorAdjust = env->UseColorAdjust();
    }
    opts.mColorXfm = colorAdjust;
    opts.mColorMod = mat->GetColorModFlags();
    opts.mCustomVariation = mat->GetShaderVariation();
    if (!opts.mPrelit && TheShaderMgr.UseAO() && env->AOEnabled()
        && env->AOStrength() > 0.003f) {
        opts.mEnableAO = 1;
    }
    opts.mToneMapping = env->UseToneMapping();
    if (fadeOut && !opts.mFog) {
        Vector4 fadeParams(mat->unk238, mat->unk23c, mat->unk240, mat->unk244);
        TheShaderMgr.SetPConstant((PShaderConstant)0x68, fadeParams);
        opts.mFadeOut = mat->unk234;
    }
    // Retail carries no CheckDistortionOpts, ShowShaderCost or HiResScreen
    // bits here (dc3 adds them).
    return opts.flags;
}

u64 RndShaderPostProc::CalcShaderOpts(NgMat *mat, ShaderType s, bool b) {
    bool v2e = TheShaderMgr.unk2e;
    bool v25 = TheShaderMgr.unk25;
    bool v3d = TheShaderMgr.unk3d;
    bool v39 = TheShaderMgr.unk39;
    bool v3f = TheShaderMgr.unk3f;
    bool v28 = TheShaderMgr.unk28;
    bool v3e = TheShaderMgr.unk3e;
    bool v2a = TheShaderMgr.unk2a;
    bool v3a = TheShaderMgr.unk3a;
    bool v2d = TheShaderMgr.unk2d;
    bool v26 = TheShaderMgr.unk26;
    bool v27 = TheShaderMgr.unk27;
    bool v2f = TheShaderMgr.unk2f;
    bool v30 = TheShaderMgr.unk30;
    bool v2c = TheShaderMgr.unk2c;
    bool v31 = TheShaderMgr.unk31;
    bool v29 = TheShaderMgr.unk29;
    bool v2b = TheShaderMgr.unk2b;
    uint v34 = TheShaderMgr.unk34;
    bool v38 = TheShaderMgr.unk38;
    bool v3b = TheShaderMgr.unk3b;
    bool v3c = TheShaderMgr.unk3c;
    TheShaderMgr.unk2a = false;
    TheShaderMgr.unk2d = false;
    TheShaderMgr.unk2e = false;
    TheShaderMgr.unk26 = false;
    TheShaderMgr.unk27 = false;
    TheShaderMgr.unk28 = false;
    TheShaderMgr.unk2f = false;
    TheShaderMgr.unk30 = false;
    TheShaderMgr.unk2c = false;
    TheShaderMgr.unk31 = false;
    TheShaderMgr.unk25 = false;
    TheShaderMgr.unk29 = false;
    TheShaderMgr.unk2b = false;
    TheShaderMgr.unk38 = false;
    TheShaderMgr.unk39 = false;
    TheShaderMgr.unk3a = false;
    TheShaderMgr.unk3b = false;
    TheShaderMgr.unk3c = false;
    TheShaderMgr.unk3d = false;
    TheShaderMgr.unk34 = 0;
    TheShaderMgr.unk3e = false;
    TheShaderMgr.unk3f = false;
    // Retail RB3 X360 carries no HiResScreen bit in this chain; every other
    // field keeps its relative position.
    // Retail RB3 X360: no HiResScreen bit, and unk2a/unk2d sit at different
    // places in the chain than dc3's later-engine version.
    return (((((((((((((((((((((((u64)(v25 & 1) << 4 | (u64)(v2e & 1)) << 2 | (u64)(v3f & 1)) << 2 | (u64)(v3d & 1)) << 1 | (u64)(v39 & 1)) << 5 | (u64)(v28 & 1)) << 1 | (u64)(v3e & 1)) << 11 | (u64)(v3a & 1)) << 1 | (u64)(v38 & 1)) << 2 | (u64)(v34 & 3)) << 1 | (u64)(v2d & 1)) << 1 | (u64)(v29 & 1)) << 5 | (u64)(v3c & 1)) << 1 | (u64)(v31 & 1)) << 6 | (u64)(v3b & 1)) << 1 | (u64)(v2c & 1)) << 1 | (u64)(v30 & 1)) << 1 | (u64)(v2f & 1)) << 1 | (u64)(v27 & 1)) << 1 | (u64)(v26 & 1)) << 1 | (u64)(v2a & 1)) << 1 | (u64)(v2b & 1)) << 1);
}

u64 RndShaderFur::CalcShaderOpts(NgMat *mat, ShaderType s, bool b) {
    NgEnviron *env = (NgEnviron *)RndEnviron::Current();
    bool skinned = TheShaderMgr.BoneCount() != 0;
    ShaderOptions opts(0);
    opts.mSkinned = skinned;
    // Retail RB3 X360: occlusion is raw Rnd::Mode 3.
    if (TheRnd.DrawMode() == (Rnd::Mode)3)
        return opts.flags;
    bool hasDiffuse = mat->GetDiffuseTex() != nullptr;
    opts.mDiffuseMap = hasDiffuse;
    opts.mPrelit = mat->Prelit();
    opts.mRealLights = mat->UseEnviron() && env->NumLights_Real() > 0;
    opts.mApproxLights = mat->UseEnviron() && env->NumLights_Approx() > 0;
    if (opts.mRealLights || opts.mApproxLights) {
        if (TheShaderMgr.AllowPerPixel() && mat->GetPerPixelLit()) {
            // fur is never per-pixel lit; the target clears the bit here anyway
            opts.mPerPixelLighting = 0;
            int hasShadowMap = TheRnd.GetShadowCam() != nullptr;
            opts.mShadowBuffer = hasShadowMap;
        }
        bool recvProjLights = mat->GetRecvProjLights() && env->NumLights_Proj() > 0;
        bool pointCubeTex = mat->GetRecvPointCubeTex() && env->NumLights_Point() > 0
            && env->HasPointCubeTex();
        opts.mAnisotropic = mat->GetAnisotropy() > 0.0f;
        opts.mNumPoint = env->NumLights_Point();
        opts.mNumProj = recvProjLights ? env->NumLights_Proj() : 0;
        opts.mProjLightMultiply = recvProjLights && env->GetProjectedBlend() == 1;
        opts.mPointCubeTex = pointCubeTex;
    }
    opts.mScreenAligned = mat->GetScreenAligned();
    bool fog;
    if (b) {
        fog = mat->GetFog();
    } else {
        fog = env->FogEnable();
    }
    opts.mFog = fog && env->FogEnable();
    bool colorAdjust;
    if (b) {
        colorAdjust = mat->ColorAdjust();
    } else {
        colorAdjust = env->UseColorAdjust();
    }
    opts.mColorXfm = colorAdjust;
    bool furDetail;
    if (mat->GetFur() != nullptr && mat->GetFur()->GetFurDetail() != nullptr) {
        furDetail = true;
    } else {
        furDetail = false;
    }
    opts.mFurDetail = furDetail;
    bool fadeOut;
    if (b) {
        fadeOut = mat->FadeOut();
    } else {
        fadeOut = env->FadeOut() && env->FadeEnd() != env->FadeStart();
    }
    if (fadeOut && !opts.mFog) {
        Vector4 fadeParams(mat->unk238, mat->unk23c, mat->unk240, mat->unk244);
        TheShaderMgr.SetPConstant((PShaderConstant)0x68, fadeParams);
        opts.mFadeOut = mat->unk234;
    }
    // Retail carries no ShowShaderCost / HiResScreen bits (dc3 adds them).
    return opts.flags;
}

u64 RndShaderSyncTrack::CalcShaderOpts(NgMat *mat, ShaderType s, bool b) {
    NgEnviron *env = (NgEnviron *)RndEnviron::Current();
    if (TheRnd.DrawMode() == Rnd::kDrawOcclusion) return 0;
    bool fadeOut;
    if (!b) {
        if (!env->FadeOut() || env->FadeEnd() == env->FadeStart()) {
            fadeOut = false;
        } else {
            fadeOut = true;
        }
    } else {
        fadeOut = mat->FadeOut();
    }
    bool allowHDR = mat->AllowHDR();
    u64 pseudoHDR;
    if (allowHDR && !fadeOut) {
        bool offscreen;
        if (!b) {
            offscreen = TheNgRnd.Offscreen();
        } else {
            offscreen = TheShaderMgr.GetUnk41();
        }
        if (!offscreen) {
            pseudoHDR = 1;
        } else {
            pseudoHDR = 0;
        }
    } else {
        pseudoHDR = 0;
    }
    int hasDiffuse = mat->GetDiffuseTex() != nullptr;
    bool prelit = mat->Prelit();
    u64 hasRealLights;
    if (!mat->UseEnviron()) {
        hasRealLights = 0;
    } else {
        hasRealLights = (env->NumLights_Real() >= 1) ? 1 : 0;
    }
    u64 hasApproxLights;
    if (!mat->UseEnviron()) {
        hasApproxLights = 0;
    } else {
        hasApproxLights = (env->NumLights_Approx() >= 1) ? 1 : 0;
    }
    u64 opts = hasApproxLights << 0x11
        | hasRealLights << 0x10
        | ((pseudoHDR << 0xe | (u64)(prelit & 1)) << 4 | (u64)hasDiffuse) << 4;
    if (hasRealLights || hasApproxLights) {
        u64 hasSpecular = ((int)(mat->GetSpecularRGB().blue * 255.0f) & 0xff) != 0
            || ((int)(mat->GetSpecularRGB().green * 255.0f) & 0xff) != 0
            || ((int)(mat->GetSpecularRGB().red * 255.0f) & 0xff) != 0;
        opts |= hasSpecular << 2;
        double dZero = 0.0;
        if (TheShaderMgr.AllowPerPixel() && mat->GetPerPixelLit()) {
            int hasNormal = mat->NormalMap() != nullptr;
            u64 hasNormDetail;
            if (mat->GetNormDetailMap() == nullptr || mat->GetNormDetailStrength() <= 0.0f) {
                hasNormDetail = 0;
            } else {
                hasNormDetail = 1;
            }
            int cull = mat->GetCull();
            u64 hasSpecMap;
            if (!hasSpecular || mat->GetSpecularMap() == nullptr) {
                hasSpecMap = 0;
            } else {
                hasSpecMap = 1;
            }
            u64 hasRim = ((int)(mat->GetRimRGB().blue * 255.0f) & 0xff) != 0
                || ((int)(mat->GetRimRGB().green * 255.0f) & 0xff) != 0
                || ((int)(mat->GetRimRGB().red * 255.0f) & 0xff) != 0;
            u64 rimLightUnder;
            if (!hasRim || !mat->GetRimLightUnder()) {
                rimLightUnder = 0;
            } else {
                rimLightUnder = 1;
            }
            u64 hasRimMap;
            if (!hasRim || mat->GetRimMap() == nullptr) {
                hasRimMap = 0;
            } else {
                hasRimMap = 1;
            }
            u64 shadowMap = TheRnd.GetShadowMap() != nullptr;
            opts = ((s64)(int)(uint)(shadowMap != 0) << 4 | hasRimMap) << 0xf
                | rimLightUnder << 0xe
                | hasRim << 0x25
                | hasSpecMap << 1
                | (((u64)(cull == kCullBackwards) << 0x1e | hasNormDetail) << 0x18
                | (s64)(int)(uint)(hasNormal != 0) << 5 | opts
                | 1);
        }
        if (mat->GetEnvironMap() != nullptr) {
            u64 environSpecMask;
            if (!(opts & 2) || !mat->GetEnvironMapSpecMask()) {
                environSpecMask = 0;
            } else {
                environSpecMask = 1;
            }
            opts = environSpecMask << 0x31
                | ((u64)(mat->GetEnvironMapFalloff() & 1)) << 0x2b
                | opts | 8;
        }
        bool hasRecvProjLights;
        if (!mat->GetRecvProjLights()) {
            hasRecvProjLights = false;
        } else {
            hasRecvProjLights = (env->NumLights_Proj() >= 1);
        }
        u64 hasPointCubeTex;
        if (!mat->GetRecvPointCubeTex() || env->NumLights_Point() < 1) {
            hasPointCubeTex = 0;
        } else {
            hasPointCubeTex = env->HasPointCubeTex() ? 1 : 0;
        }
        float aniso = mat->GetAnisotropy();
        int numPointLights = env->NumLights_Point();
        int numProjLights;
        if (hasRecvProjLights) {
            numProjLights = env->NumLights_Proj();
        } else {
            numProjLights = 0;
        }
        u64 projBlend;
        if (hasRecvProjLights) {
            projBlend = (env->GetProjectedBlend() == 1) ? 1 : 0;
        } else {
            projBlend = 0;
        }
        opts = (hasPointCubeTex << 4 | projBlend) << 0x2c
            | ((s64)numProjLights & 3U) << 0x1c
            | (((s64)numPointLights & 3U) << 0x14 | (u64)(dZero < (double)aniso)) << 0x14 | opts;
    }
    if (mat->GetRefractEnabled(b) && mat->GetRefractNormalMap() != nullptr) {
        opts |= 0x400000000000;
    }
    int emissiveMap = mat->GetEmissiveMap() != nullptr;
    bool screenAligned = mat->GetScreenAligned();
    bool intensify = mat->GetIntensify();
    int texGen = mat->GetTexGen();
    uint texGenVal;
    if (texGen == kTexGenSphere) {
        texGenVal = 1;
    } else if (texGen == kTexGenProjected) {
        texGenVal = 2;
    } else {
        texGenVal = -(uint)(texGen == kTexGenEnviron) & 3;
    }
    bool fog;
    if (mat->AllowFog() && mat->GetFog()) {
        fog = true;
    } else {
        fog = false;
    }
    bool colorAdjust;
    if (!b) {
        colorAdjust = env->UseColorAdjust();
    } else {
        colorAdjust = mat->ColorAdjust();
    }
    u64 shaderOpts = ((((s64)mat->GetColorModFlags() & 3U) << 2
        | (u64)(uint)mat->GetShaderVariation() & 0xffffffff00000003) << 9
        | (u64)(colorAdjust & 1)) << 0x15
        | (u64)fog << 0x12
        | (s64)(int)texGenVal << 10
        | ((((u64)(intensify & 1) << 0x28 | (u64)(screenAligned & 1)) << 6 | (u64)(emissiveMap != 0))
        << 7 | opts);
    if (!(opts & 0x100) && TheShaderMgr.UseAO()
        && env->AOEnabled() && 0.003f < env->AOStrength()) {
        shaderOpts |= 0x4000000000;
    }
    shaderOpts |= ((u64)env->UseToneMapping() & 1) << 0x27;
    if (fadeOut && !(shaderOpts & 0x40000)) {
        Vector4 fadeParams(mat->unk238, mat->unk23c, mat->unk240, mat->unk244);
        TheShaderMgr.SetPConstant((PShaderConstant)0x68, fadeParams);
        shaderOpts |= ((s64)mat->unk234 & 3U) << 0x1a;
    }
    u64 result = (((u64)(TheHiResScreen.IsActive() & 1) << 2
        | (u64)(TheRnd.ResourceCached() & 1)) << 0x32) | shaderOpts;
    result |= 0x80000000000000;
    if (RndSpline::sGlobalDefaultSpline != nullptr) {
        result |= ((u64)(RndSpline::sGlobalDefaultSpline->mPulseDrawing & 1)) << 0x38;
    }
#ifdef HX_NATIVE
    return (u64)(s == kSyncTrackChargeEffectShader) << 0x3b | result;
#else
    return result;
#endif
}

void RndShaderParticles::Select(RndMat *mat, ShaderType s, bool b) {
    if (!mat) mat = TheRnd.DefaultMat();
    TheRenderState.SetFillMode((RndRenderState::FillMode)0);
    if (!RedundantState(mat, s, false, false, b)) {
#ifdef HX_NATIVE
        TheNgStats->mMats++;
#endif
        ((NgMat *)mat)->SetupShader(false, true);
        u64 optsVal = CalcShaderOpts((NgMat *)mat, s, b);
        SetColorWriteMask(ShaderOptions(optsVal), mat);
        Cache(s, ShaderOptions(optsVal), mat);
    }
}

void RndShaderMultimesh::Select(RndMat *mat, ShaderType s, bool b) {
    if (!mat) mat = TheRnd.DefaultMat();
    TheRenderState.SetFillMode((RndRenderState::FillMode)0);
    if (!RedundantState(mat, s, false, TheShaderMgr.UseAO(), b)) {
#ifdef HX_NATIVE
        TheNgStats->mMats++;
#endif
        ((NgMat *)mat)->SetupShader(TheShaderMgr.AllowPerPixel(), true);
        u64 optsVal = CalcShaderOpts((NgMat *)mat, s, b);
        SetColorWriteMask(ShaderOptions(optsVal), mat);
        // Retail RB3 X360: no CheckDistortion here (dc3 adds it).
        CheckForceCull(kMultimeshShader);
        Cache(kMultimeshShader, ShaderOptions(optsVal), mat);
    }
}

// Retail RB3 X360 (0x824A8080, identified as RndShaderStandard by the RTTI
// locator in front of its vtable): the shader type is hard-wired to
// kStandardShader and there is no CheckDistortion.  The native build keeps
// dc3's type-preserving body, which the AllWhite shader relies on.
void RndShaderStandard::Select(RndMat *mat, ShaderType shader_type, bool b) {
    if (!mat) mat = TheRnd.DefaultMat();
    TheRenderState.SetFillMode((RndRenderState::FillMode)0);
    bool skinned = TheShaderMgr.BoneCount() != 0;
    if (!RedundantState(mat, shader_type, skinned, TheShaderMgr.UseAO(), b)) {
#ifdef HX_NATIVE
        TheNgStats->mMats++;
        ((NgMat *)mat)->SetupShader(TheShaderMgr.AllowPerPixel(), true);
        CheckShadow();
        ShaderOptions opts(CalcShaderOpts((NgMat *)mat, shader_type, b));
        MILO_ASSERT((shader_type == kStandardShader || shader_type == kStandardBBShader || shader_type == kAllWhiteShader), 0x4BB);
        if (shader_type == kStandardBBShader) {
            shader_type = kStandardShader;
        }
        SetColorWriteMask(opts, mat);
        CheckExtrude();
        CheckForceCull(shader_type);
        CheckDistortion(mat);
        Cache(shader_type, opts, mat);
#else
        ((NgMat *)mat)->SetupShader(TheShaderMgr.AllowPerPixel(), true);
        CheckShadow();
        u64 optsVal = CalcShaderOpts((NgMat *)mat, shader_type, b);
        SetColorWriteMask(ShaderOptions(optsVal), mat);
        CheckExtrude();
        CheckForceCull(kStandardShader);
        Cache(kStandardShader, ShaderOptions(optsVal), mat);
#endif
    }
}

void RndShaderPostProc::Select(RndMat *mat, ShaderType s, bool b) {
    if (!mat) mat = TheRnd.DefaultMat();
    TheRenderState.SetFillMode((RndRenderState::FillMode)0);
    if (!RedundantState(mat, s, false, false, b)) {
        ((NgMat *)mat)->SetupShader(TheShaderMgr.AllowPerPixel(), false);
        u64 optsVal = CalcShaderOpts((NgMat *)mat, s, b);
#ifdef HX_NATIVE
        TheNgStats->mMats++;
#endif
        TheRenderState.SetColorWriteMask(0xF);
        auto _tmp2 = ShaderOptions(optsVal);
        Cache(s, _tmp2, mat);
    }
}

void RndShaderDrawRect::Select(RndMat *mat, ShaderType s, bool b) {
    if (!mat) mat = TheShaderMgr.DrawRectMat();
    TheRenderState.SetFillMode((RndRenderState::FillMode)0);
    if (!RedundantState(mat, s, false, false, b)) {
        ((NgMat *)mat)->SetupShader(TheShaderMgr.AllowPerPixel(), true);
        u64 optsVal = CalcShaderOpts((NgMat *)mat, s, b);
#ifdef HX_NATIVE
        TheNgStats->mMats++;
#endif
        SetColorWriteMask(ShaderOptions(optsVal), mat);
        TheShaderMgr.SetVConstant(kVS_AmbientColor, Vector4(1.0f, 1.0f, 1.0f, 1.0f));
        TheShaderMgr.SetPConstant(kPS_AmbientColor, Vector4(1.0f, 1.0f, 1.0f, 1.0f));
        CheckForceCull(kStandardShader);
        Cache(kStandardShader, ShaderOptions(optsVal), mat);
    }
}

void RndShaderUnwrapUV::Select(RndMat *mat, ShaderType s, bool b) {
    if (!mat) mat = TheRnd.DefaultMat();
    TheRenderState.SetFillMode((RndRenderState::FillMode)0);
    if (!RedundantState(mat, s, false, false, b)) {
        ((NgMat *)mat)->SetupShader(TheShaderMgr.AllowPerPixel(), true);
        u64 optsVal = CalcShaderOpts((NgMat *)mat, s, b);
#ifdef HX_NATIVE
        TheNgStats->mMats++;
#endif
        TheRenderState.SetColorWriteMask(7);
        const Hmx::Color &color = mat->GetColor();
        auto _tmp0 = Vector4(color.red, color.green, color.blue, color.alpha);
        TheShaderMgr.SetVConstant(kVS_AmbientColor, _tmp0);
        TheShaderMgr.SetPConstant(kPS_AmbientColor, Vector4(color.red, color.green, color.blue, color.alpha));
        CheckForceCull(s);
        Cache(s, ShaderOptions(optsVal), mat);
    }
}

// 0x824a7468 (RndShaderVelocity vtable slot 2). Unlike VelocityCamera's
// Select (0x824a7548), the redundancy check is keyed on skinning: retail
// passes TheShaderMgr.BoneCount() != 0 as the third argument.
void RndShaderVelocity::Select(RndMat *mat, ShaderType s, bool b) {
    if (!mat) mat = TheRnd.DefaultMat();
    TheRenderState.SetFillMode((RndRenderState::FillMode)0);
    if (!RedundantState(mat, s, TheShaderMgr.BoneCount() != 0, false, b)) {
#ifdef HX_NATIVE
        TheNgStats->mMats++;
#endif
        ((NgMat *)mat)->SetupShader(false, false);
        u64 optsVal = CalcShaderOpts((NgMat *)mat, s, b);
        SetColorWriteMask(ShaderOptions(optsVal), mat);
        CheckForceCull(s);
        Cache(s, ShaderOptions(optsVal), mat);
    }
}

void RndShaderVelocityCamera::Select(RndMat *mat, ShaderType s, bool b) {
    if (!mat) mat = TheRnd.DefaultMat();
    TheRenderState.SetFillMode((RndRenderState::FillMode)0);
    if (!RedundantState(mat, s, false, false, b)) {
#ifdef HX_NATIVE
        TheNgStats->mMats++;
#endif
        ((NgMat *)mat)->SetupShader(false, false);
        u64 optsVal = CalcShaderOpts((NgMat *)mat, s, b);
        SetColorWriteMask(ShaderOptions(optsVal), mat);
        CheckForceCull(s);
        Cache(s, ShaderOptions(optsVal), mat);
    }
}

void RndShaderDepthVolume::Select(RndMat *mat, ShaderType s, bool b) {
    if (!mat) mat = TheRnd.DefaultMat();
    TheRenderState.SetFillMode((RndRenderState::FillMode)0);
    bool skinned = TheShaderMgr.BoneCount() != 0;
    if (!RedundantState(mat, s, skinned, false, b)) {
#ifdef HX_NATIVE
        TheNgStats->mMats++;
#endif
        ((NgMat *)mat)->SetupShader(TheShaderMgr.AllowPerPixel(), true);
        u64 optsVal = CalcShaderOpts((NgMat *)mat, s, b);
        SetColorWriteMask(ShaderOptions(optsVal), mat);
        if (TheShaderMgr.InDepthVolume()) {
            if (TheShaderMgr.unk24) {
                TheRenderState.SetBlendOp((RndRenderState::BlendOp)4);
            } else {
                TheRenderState.SetBlendOp((RndRenderState::BlendOp)0);
            }
            TheRenderState.SetBlendEnable(true);
            TheRenderState.SetBlend(
                (RndRenderState::Blend)1, (RndRenderState::Blend)1,
                (RndRenderState::Blend)1, (RndRenderState::Blend)1
            );
            TheRenderState.SetDepthTestEnable(false);
            TheRenderState.SetDepthWriteEnable(false);
        }
        CheckExtrude();
        TheShaderMgr.SetVConstant(kVS_AmbientColor, Vector4(1.0f, 1.0f, 1.0f, 1.0f));
        TheShaderMgr.SetPConstant(kPS_AmbientColor, Vector4(1.0f, 1.0f, 1.0f, 1.0f));
        CheckForceCull(s);
        Cache(s, ShaderOptions(optsVal), mat);
    }
}

void RndShaderFur::Select(RndMat *mat, ShaderType s, bool b) {
    if (!mat) mat = TheRnd.DefaultMat();
    TheRenderState.SetFillMode((RndRenderState::FillMode)0);
    bool skinned = TheShaderMgr.BoneCount() != 0;
    if (!RedundantState(mat, s, skinned, false, b)) {
#ifdef HX_NATIVE
        TheNgStats->mMats++;
#endif
        ((NgMat *)mat)->SetupShader(false, true);
        CheckShadow();
        u64 optsVal = CalcShaderOpts((NgMat *)mat, s, b);
        SetColorWriteMask(ShaderOptions(optsVal), mat);
        CheckForceCull(s);
        Cache(s, ShaderOptions(optsVal), mat);
    }
}

void RndShaderSyncTrack::Select(RndMat *mat, ShaderType shader_type, bool b) {
    if (!mat) mat = TheRnd.DefaultMat();
    TheRenderState.SetFillMode((RndRenderState::FillMode)0);
    bool skinned = TheShaderMgr.BoneCount() != 0;
    if (!RedundantState(mat, shader_type, skinned, TheShaderMgr.UseAO(), b)) {
#ifdef HX_NATIVE
        TheNgStats->mMats++;
#endif
        ((NgMat *)mat)->SetupShader(TheShaderMgr.AllowPerPixel(), true);
        CheckShadow();
        u64 optsVal = CalcShaderOpts((NgMat *)mat, shader_type, b);
#ifdef HX_NATIVE
        MILO_ASSERT((shader_type == kSyncTrackShader || shader_type == kSyncTrackChargeEffectShader), 0x749);
        if (shader_type == kSyncTrackChargeEffectShader) {
            shader_type = kSyncTrackShader;
        }
#endif
        SetColorWriteMask(ShaderOptions(optsVal), mat);
        CheckExtrude();
        CheckForceCull(shader_type);
        Cache(shader_type, ShaderOptions(optsVal), mat);
    }
}
