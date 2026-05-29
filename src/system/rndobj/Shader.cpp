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
    sShaders[kBlurShader] = &gShaderSimple;
    sShaders[kBloomShader] = &gShaderSimple;
    sShaders[kDepthVolumeShader] = &gShaderDepthVolume;
    sShaders[kBloomGlareShader] = &gShaderSimple;
    sShaders[kDrawRectShader] = &gShaderDrawRect;
    sShaders[kDownsampleShader] = &gShaderSimple;
    sShaders[kDownsampleDepthShader] = &gShaderSimple;
    sShaders[kDownsample4xShader] = &gShaderSimple;
    sShaders[kMultimeshShader] = &gShaderMultimesh;
    sShaders[kFurShader] = &gShaderFur;
    sShaders[kErrorShader] = &gShaderSimple;
    sShaders[kLineNozShader] = &gShaderSimple;
    sShaders[kMovieShader] = &gShaderSimple;
    sShaders[kMultimeshBBShader] = &gShaderMultimesh;
    sShaders[kLineShader] = &gShaderSimple;
    sShaders[kShadowmapShader] = &gShaderSimple;
    sShaders[kPostprocessErrorShader] = &gShaderSimple;
    sShaders[kPlayerDepthVisShader] = &gShaderSimple;
    sShaders[kParticlesShader] = &gShaderParticles;
    sShaders[kPlayerDepthShellShader] = &gShaderSimple;
    sShaders[kSyncTrackShader] = &gShaderSyncTrack;
    sShaders[kStandardShader] = &gShaderStandard;
    sShaders[kStandardBBShader] = &gShaderStandard;
    sShaders[kPostprocessShader] = &gShaderPostProc;
    sShaders[kPlayerDepthShell2Shader] = &gShaderSimple;
    sShaders[kDepthBuffer3DShader] = &gShaderSimple;
    sShaders[kYUVtoRGBShader] = &gShaderSimple;
    sShaders[kSyncTrackChargeEffectShader] = &gShaderSyncTrack;
    sShaders[kVelocityCameraShader] = &gShaderVelocityCamera;
    sShaders[kUnwrapUVShader] = &gShaderUnwrapUV;
    sShaders[kVelocityObjectShader] = &gShaderVelocity;
    sShaders[kYUVtoBlackAndWhiteShader] = &gShaderSimple;
    sShaders[kPlayerGreenScreenShader] = &gShaderSimple;
    sShaders[kPlayerDepthGreenScreenShader] = &gShaderSimple;
    sShaders[kCrewPhotoShader] = &gShaderSimple;
    sShaders[kTwirlShader] = &gShaderSimple;
    sShaders[kKillAlphaShader] = &gShaderSimple;
    sShaders[kAllWhiteShader] = &gShaderStandard;
}

void RndShader::CheckForceCull(ShaderType s) {
    int cullOverride = TheShaderMgr.CullModeOverride();
    if (TheRnd.GetDrawMode() == Rnd::kDrawShadowColor || cullOverride == 1) {
        TheRenderState.SetCullMode((RndRenderState::CullMode)0);
    } else if (s != kShadowmapShader && cullOverride != 3 && TheRnd.GetDrawMode() != 8) {
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
            || s == kMultimeshShader || s == kMultimeshBBShader || s == kSyncTrackShader
            || s == kSyncTrackChargeEffectShader || s == kAllWhiteShader) {
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
        Debug::ModalType ty = Debug::kModalNotify;
        if (mModalCallback) {
            StackString<1024> str(msg);
            (*mModalCallback)(ty, str, true);
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
        || TheRnd.GetDrawMode() == Rnd::kDrawOcclusion) {
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

void RndShader::SelectConfig(RndMat *mat, ShaderType shader_type, bool b3) {
    RndShader *shader;
    MILO_ASSERT(shader_type >= ShaderType(0) && shader_type < kMaxShaderTypes, 0x1BB);
    if (TheRnd.GetDrawMode() == 2) {
        shader_type = kShadowmapShader;
    } else if (TheRnd.GetDrawMode() == 6) {
        shader_type = kVelocityObjectShader;
    } else if (TheShaderMgr.InDepthVolume()) {
        shader_type = kDepthVolumeShader;
    }
#ifdef HX_NATIVE
    // Native/web: skip shader diagnostic path. On Xbox retail UsingCD()==true
    // so this path is dead code. On native, UsingCD() may be false (no .ark),
    // which would activate editor-mode shader validation that crashes on WASM
    // (virtual calls into unimplemented NG shader subsystems).
    if (!b3 && TheLoadMgr.EditMode()) {
#else
    if (!b3 && (TheLoadMgr.EditMode() || !UsingCD())) {
#endif
        if (!DisplayMatShaderFlagsError(mat, shader_type)) {
            bool doError = true;
            void *metaMat;
            if (mat && TheShaderMgr.ShowMetaMatErrors()) {
                metaMat = mat->GetMetaMaterial();
                doError = doError && (metaMat == nullptr);
            }
            if (!doError) {
                goto done;
            }
        }
        shader_type = shader_type == kPostprocessShader
            ? kPostprocessErrorShader
            : kErrorShader;
    }
done:
    shader = sShaders[shader_type];
#ifdef HX_NATIVE
    if (!shader) {
        // Fallback: unregistered shader type — use error shader
        shader = sShaders[kErrorShader];
        if (!shader) return;
    }
#else
    MILO_ASSERT(shader, 0x1D3);
#endif
    shader->Select(mat, shader_type, b3);
}

void RndShader::Cache(ShaderType s, ShaderOptions opts, RndMat *mat) {
    RndShaderProgram &program = TheShaderMgr.FindShader(s, opts);
    if (!program.Cached()) {
        if (!program.Cache(s, opts, nullptr, nullptr)
#ifdef HX_NATIVE
            && !TheShaderMgr.CacheShaders()
#else
            && (UsingCD() || !TheShaderMgr.CacheShaders())
#endif
        ) {
            MatShaderFlagsOK(mat, s);
        }
    }
    bool select = s == kShadowmapShader || TheRnd.GetDrawMode() == Rnd::kDrawShadowColor;
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
        TheNgStats->mMats++;
        SetColorWriteMask(ShaderOptions(optsVal), mat);
        CheckForceCull(s);
        Cache(s, ShaderOptions(optsVal), mat);
    }
}

bool RndShaderMultimesh::CheckError(MatFlagErrorType type) {
    return type == (MatFlagErrorType)0 || type == (MatFlagErrorType)1 || type == (MatFlagErrorType)2;
}

bool RndShaderParticles::CheckError(MatFlagErrorType type) {
        return !(type != (MatFlagErrorType)0 && type != (MatFlagErrorType)2) && TheRnd.GetDrawMode() != 4;
}

void SetColorWriteMask(const ShaderOptions &opts, RndMat *mat) {
    bool writeAlpha = mat->mAlphaWrite;
    if (!mat->mForceAlphaWrite
        && ((opts.flags & 0x400000) != 0 || TheNgRnd.Offscreen() || writeAlpha)) {
        writeAlpha = true;
    }
    TheRenderState.SetColorWriteMask((-(unsigned int)writeAlpha & 8) + 7);
}

void CheckDistortionOpts(RndMat *mat, ShaderOptions &opts) {
    RndSpline *spline = RndSpline::sGlobalDefaultSpline;
    if (spline && !mat->mNeverFitToSpline && spline->mCtrlPoints.size() >= 2) {
        opts.flags |= (u64)1 << 55;
        opts.flags = ((u64)(spline->mPulseDrawing & 1) << 56)
            | (opts.flags & ~((u64)1 << 56));
    }
    RndShockwave *shockwave = RndShockwave::sSelected;
    if (shockwave) {
        bool ampBad = Abs(shockwave->mAmplitude) < 0.0001f;
        if (!ampBad && mat->mAllowDistortionEffects) {
            bool multBad = Abs(mat->mShockwaveMult) < 0.0001f;
            if (!multBad) {
                opts.flags |= (u64)1 << 60;
            }
        }
    }
}

void CheckDistortion(RndMat *mat) {
    RndSpline *spline = RndSpline::sGlobalDefaultSpline;
    if (spline
        && !mat->mNeverFitToSpline
        && !spline->mManual
        && spline->mCtrlPoints.size() >= 2) {
        spline->PrepareShader();
    }
    RndShockwave *shock = RndShockwave::sSelected;
    if (shock) {
        bool ampBad = Abs(shock->mAmplitude) < 0.0001f;
        if (!ampBad && mat->mAllowDistortionEffects) {
            bool multBad = Abs(mat->mShockwaveMult) < 0.0001f;
            if (!multBad) {
                shock->PrepareShader(mat->mShockwaveMult);
            }
        }
    }
}

void CheckShadow() {
    RndCam *shadowCam = TheNgRnd.GetShadowCam();
    if (shadowCam) {
        Transform viewXfm;
        Hmx::Matrix4 projMtx;
        shadowCam->GetViewProjectXfms(viewXfm, projMtx);
        Hmx::Matrix4 viewProj = Hmx::operator*(viewXfm, projMtx);
        static Hmx::Matrix4 sShadowTexMatrix;
        static bool sInit;
        if (!sInit) {
            sShadowTexMatrix.x = Vector4(0.0f, 0.0f, 0.0f, 0.0f);
            sShadowTexMatrix.y = Vector4(0.0f, -0.5f, 0.0f, 0.0f);
            sShadowTexMatrix.z = Vector4(0.0f, 0.0f, 1.0f, 0.0f);
            sShadowTexMatrix.w = Vector4(0.0f, 0.501953125f, 0.0f, 1.0f);
            sInit = true;
        }
        Hmx::Matrix4 result = Hmx::operator*(viewProj, sShadowTexMatrix);
        TheShaderMgr.SetVConstant((VShaderConstant)0x28, result);
    }
}

void CheckExtrude() {
    if (TheRnd.GetDrawMode() == Rnd::kDrawShadowColor) {
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
    return ((u64)(TheHiResScreen.IsActive() & 1) << 40
        | (u64)(TheShaderMgr.BoneCount() > 0)) << 12;
}

u64 RndShaderUnwrapUV::CalcShaderOpts(NgMat *mat, ShaderType s, bool b) {
    return ((u64)(mat->GetDiffuseTex() != nullptr)
        | 0x10
        | ((u64)(TheHiResScreen.IsActive() & 1) << 48)) << 4;
}

u64 RndShaderDepthVolume::CalcShaderOpts(NgMat *mat, ShaderType s, bool b) {
    return (((u64)(TheHiResScreen.IsActive() & 1) << 29
        | (u64)(TheRnd.GetDrawMode() == Rnd::kDrawShadowColor)) << 23)
        | (((u64)(TheShaderMgr.BoneCount() != 0) << 11
        | (u64)(TheShaderMgr.unk1c & 3)) << 1);
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
        u64 de = (u64)displayError;
        opts = (de << 23 | bc) << 12;
        break;
    }
    case kMovieShader: {
        u64 specBit = (u64)(mat->GetSpecularMap() == nullptr) & 1;
        u64 normBit = (u64)(bool)mat->NormalMap() & 1;
        opts = (specBit | normBit << 4) << 1;
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
    opts = (opts & ~((u64)1 << 52)) | ((u64)(TheHiResScreen.IsActive() & 1) << 52);
    int drawDiff = TheRnd.GetDrawMode() - Rnd::kDrawOcclusion;
    return -(u64)(bool)drawDiff & opts;
}

u64 RndShaderDrawRect::CalcShaderOpts(NgMat *mat, ShaderType s, bool b) {
    if (TheRnd.GetDrawMode() == Rnd::kDrawOcclusion) return 0;
    int hasDiffuse = mat->GetDiffuseTex() != nullptr;
    bool prelit = mat->Prelit();
    bool offscreen;
    if (!b) {
        offscreen = TheNgRnd.Offscreen();
    } else {
        offscreen = TheShaderMgr.GetUnk41();
    }
    u64 pseudoHDR = 0;
    if (!offscreen && mat->AllowHDR()) {
        pseudoHDR = 1;
    }
    return ((((u64)(TheHiResScreen.IsActive() & 1) << 2
        | (u64)(TheRnd.ResourceCached() & 1)) << 28
        | pseudoHDR) << 22)
        | (((u64)(prelit & 1) << 4 | (u64)hasDiffuse) << 4);
}

u64 RndShaderParticles::CalcShaderOpts(NgMat *mat, ShaderType s, bool b) {
    RndEnviron *env = RndEnviron::Current();
    int hasDiffuse = mat->GetDiffuseTex() != nullptr;
    int texGen = mat->GetTexGen();
    uint texGenVal;
    if (texGen == kTexGenSphere) {
        texGenVal = 1;
    } else if (texGen == kTexGenProjected) {
        texGenVal = 2;
    } else {
        texGenVal = -(uint)(texGen == kTexGenEnviron) & 3;
    }
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
    u64 pseudoHDR;
    if (!fadeOut) {
        bool offscreen;
        if (!b) {
            offscreen = TheNgRnd.Offscreen();
        } else {
            offscreen = TheShaderMgr.GetUnk41();
        }
        if (!offscreen && mat->AllowHDR()) {
            pseudoHDR = 1;
        } else {
            pseudoHDR = 0;
        }
    } else {
        pseudoHDR = 0;
    }
    bool colorAdjust;
    if (!b) {
        colorAdjust = env->UseColorAdjust();
    } else {
        colorAdjust = mat->ColorAdjust();
    }
    u64 opts = (((u64)(mat->GetIntensify() & 1) << 0x20 | (u64)(colorAdjust & 1)) << 0x15
        | pseudoHDR << 0x16
        | (s64)(int)texGenVal << 10
        | (u64)(hasDiffuse != 0) << 4
        | 0x100);
    if (fadeOut) {
        Vector4 fadeParams(mat->unk2d8, mat->unk2dc, mat->unk2e0, mat->unk2e4);
        TheShaderMgr.SetPConstant((PShaderConstant)0x68, fadeParams);
        opts |= ((s64)mat->unk2d4 & 3U) << 0x1a;
    }
    if (mat->GetRefractEnabled(b) && mat->GetRefractNormalMap() != nullptr) {
        opts |= 0x400000000000;
    }
    bool fog;
    if (mat->AllowFog() && mat->GetFog()) {
        fog = true;
    } else {
        fog = false;
    }
    opts |= (u64)fog << 0x12;
    if (TheRnd.GetDrawMode() == (Rnd::DrawMode)7) {
        opts |= 0x200000000000;
    }
    return (((u64)(TheHiResScreen.IsActive() & 1) << 2
        | (u64)(TheRnd.ResourceCached() & 1)) << 0x32)
        | (-(u64)(TheRnd.GetDrawMode() != Rnd::kDrawOcclusion) & opts);
}

u64 RndShaderMultimesh::CalcShaderOpts(NgMat *mat, ShaderType s, bool b) {
    NgEnviron *env = (NgEnviron *)RndEnviron::Current();
    if (TheRnd.GetDrawMode() == Rnd::kDrawOcclusion) return 0;
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
        | (((u64)(prelit & 1) << 4 | (u64)hasDiffuse) << 4);
    if (hasRealLights || hasApproxLights) {
        u64 hasSpecular = ((int)(mat->GetSpecularRGB().blue * 255.0f) & 0xff) != 0
            || ((int)(mat->GetSpecularRGB().green * 255.0f) & 0xff) != 0
            || ((int)(mat->GetSpecularRGB().red * 255.0f) & 0xff) != 0;
        opts |= hasSpecular << 2;
        if (TheShaderMgr.AllowPerPixel() && mat->GetPerPixelLit()) {
            u64 hasNormDetail;
            if (mat->GetNormDetailMap() == nullptr || mat->GetNormDetailStrength() <= 0.0f) {
                hasNormDetail = 0;
            } else {
                hasNormDetail = 1;
            }
            u64 hasSpecMap;
            if (!hasSpecular || mat->GetSpecularMap() == nullptr) {
                hasSpecMap = 0;
            } else {
                hasSpecMap = 1;
            }
            int cull = mat->GetCull();
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
            int hasNormal = mat->NormalMap() != nullptr;
            opts = hasRimMap << 0xf
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
        int numPointLights = env->NumLights_Point();
        opts |= ((s64)numPointLights & 3U) << 0x28;
    }
    int emissiveMap = mat->GetEmissiveMap() != nullptr;
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
    u64 pseudoHDR;
    if (!fadeOut) {
        bool offscreen;
        if (!b) {
            offscreen = TheNgRnd.Offscreen();
        } else {
            offscreen = TheShaderMgr.GetUnk41();
        }
        if (!offscreen && mat->AllowHDR()) {
            pseudoHDR = 1;
        } else {
            pseudoHDR = 0;
        }
    } else {
        pseudoHDR = 0;
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
        | (u64)(s == kMultimeshBBShader) << 0x19
        | (u64)fog << 0x12
        | pseudoHDR << 0x16
        | (s64)(int)texGenVal << 10
        | (((u64)(intensify & 1) << 0x2e | (u64)(emissiveMap != 0)) << 7 | opts);
    if (!(opts & 0x100) && TheShaderMgr.UseAO()
        && env->AOEnabled() && 0.003f < env->AOStrength()) {
        shaderOpts |= 0x4000000000;
    }
    shaderOpts |= ((u64)env->UseToneMapping() & 1) << 0x27;
    if (fadeOut) {
        Vector4 fadeParams(mat->unk2d8, mat->unk2dc, mat->unk2e0, mat->unk2e4);
        TheShaderMgr.SetPConstant((PShaderConstant)0x68, fadeParams);
        shaderOpts |= ((s64)mat->unk2d4 & 3U) << 0x1a;
    }
    CheckDistortionOpts((RndMat *)mat, (ShaderOptions &)shaderOpts);
    bool hasRecvProjLights;
    if (mat->GetRecvProjLights()) {
        hasRecvProjLights = (env->NumLights_Proj() >= 1);
    } else {
        hasRecvProjLights = false;
    }
    u64 projBlend;
    if (hasRecvProjLights) {
        projBlend = env->NumLights_Proj();
    } else {
        projBlend = 0;
    }
    return ((((u64)(TheHiResScreen.IsActive() & 1) << 2
        | (u64)(TheRnd.ResourceCached() & 1)) << 0x16 | projBlend & 3) << 0x1c)
        | (shaderOpts & 0xffebffffcfffffff);
}

u64 RndShaderStandard::CalcShaderOpts(NgMat *mat, ShaderType s, bool b) {
    NgEnviron *env = (NgEnviron *)RndEnviron::Current();
    u64 skinned = (u64)(TheShaderMgr.BoneCount() != 0) << 0xc;
    if (TheRnd.GetDrawMode() == Rnd::kDrawOcclusion) return skinned;
    if (TheRnd.GetDrawMode() == Rnd::kDrawShadowDepth) {
        u64 base = (((u64)(mat->Prelit() & 1) << 4
            | (u64)(mat->GetDiffuseTex() != nullptr)) << 4) | skinned;
        if (!mat->UseEnviron()) return base;
        return base | 0x2000000000000000;
    }
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
        | ((pseudoHDR << 0xe | (u64)(prelit & 1)) << 4 | (u64)hasDiffuse) << 4 | skinned;
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
        | (u64)(s == kStandardBBShader) << 0x19
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
        Vector4 fadeParams(mat->unk2d8, mat->unk2dc, mat->unk2e0, mat->unk2e4);
        TheShaderMgr.SetPConstant((PShaderConstant)0x68, fadeParams);
        shaderOpts |= ((s64)mat->unk2d4 & 3U) << 0x1a;
    }
    CheckDistortionOpts((RndMat *)mat, (ShaderOptions &)shaderOpts);
    return (((u64)(TheHiResScreen.IsActive() & 1) << 2
        | (u64)(TheRnd.ResourceCached() & 1)) << 0x32)
        | (shaderOpts & 0xffebffffffffffff);
}

u64 RndShaderPostProc::CalcShaderOpts(NgMat *mat, ShaderType s, bool b) {
    bool v2a = TheShaderMgr.unk2a;
    bool v2e = TheShaderMgr.unk2e;
    bool v25 = TheShaderMgr.unk25;
    bool v39 = TheShaderMgr.unk39;
    bool v3d = TheShaderMgr.unk3d;
    bool v3f = TheShaderMgr.unk3f;
    bool v29 = TheShaderMgr.unk29;
    bool v2d = TheShaderMgr.unk2d;
    bool v26 = TheShaderMgr.unk26;
    bool v27 = TheShaderMgr.unk27;
    bool v28 = TheShaderMgr.unk28;
    bool v2f = TheShaderMgr.unk2f;
    bool v30 = TheShaderMgr.unk30;
    bool v2c = TheShaderMgr.unk2c;
    bool v31 = TheShaderMgr.unk31;
    bool v2b = TheShaderMgr.unk2b;
    uint v34 = TheShaderMgr.unk34;
    bool v38 = TheShaderMgr.unk38;
    bool v3a = TheShaderMgr.unk3a;
    bool v3b = TheShaderMgr.unk3b;
    bool v3c = TheShaderMgr.unk3c;
    bool v3e = TheShaderMgr.unk3e;
    TheShaderMgr.unk29 = false;
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
    TheShaderMgr.unk2b = false;
    TheShaderMgr.unk38 = false;
    TheShaderMgr.unk39 = false;
    TheShaderMgr.unk3a = false;
    TheShaderMgr.unk2a = false;
    TheShaderMgr.unk3b = false;
    TheShaderMgr.unk3c = false;
    TheShaderMgr.unk3d = false;
    TheShaderMgr.unk34 = 0;
    TheShaderMgr.unk3e = false;
    TheShaderMgr.unk3f = false;
    return ((((((((((((((((((((((((u64)(v2a & 1) << 10
        | (u64)(TheHiResScreen.IsActive() & 1)) << 1 | (u64)(v25 & 1)) << 4 | (u64)(v2e & 1))
        << 2 | (u64)(v3f & 1)) << 2 | (u64)(v3d & 1)) << 1 | (u64)(v39 & 1)) << 5
        | (u64)(v28 & 1)) << 1 | (u64)(v3e & 1)) << 0xb
        | (u64)(v3a & 1)) << 1 | (u64)(v38 & 1)) << 2
        | (u64)(v34 & 3)) << 1 | (u64)(v29 & 1)) << 6 | (u64)(v3c & 1)) << 1
        | (u64)(v31 & 1)) << 6 | (u64)(v3b & 1)) << 1 | (u64)(v2c & 1)) << 1 | (u64)(v30 & 1))
        << 1 | (u64)(v2f & 1)) << 1 | (u64)(v27 & 1)) << 1 | (u64)(v26 & 1)) << 1
        | (u64)(v2d & 1)) << 1 | (u64)(v2b & 1)) << 1);
}

u64 RndShaderFur::CalcShaderOpts(NgMat *mat, ShaderType s, bool b) {
    RndEnviron *env = RndEnviron::Current();
    u64 skinned = (u64)(TheShaderMgr.BoneCount() != 0) << 0xc;
    if (TheRnd.GetDrawMode() == Rnd::kDrawOcclusion) return skinned;
    int hasDiffuse = mat->GetDiffuseTex() != nullptr;
    bool prelit = mat->Prelit();
    u64 hasRealLights;
    if (!mat->UseEnviron()) {
        hasRealLights = 0;
    } else {
        hasRealLights = (((NgEnviron *)env)->NumLights_Real() >= 1) ? 1 : 0;
    }
    u64 hasApproxLights;
    if (!mat->UseEnviron()) {
        hasApproxLights = 0;
    } else {
        hasApproxLights = (((NgEnviron *)env)->NumLights_Approx() >= 1) ? 1 : 0;
    }
    u64 opts = hasApproxLights << 0x11
        | hasRealLights << 0x10
        | (((u64)(prelit & 1) << 4 | (u64)(hasDiffuse != 0)) << 4) | skinned;
    if (hasRealLights || hasApproxLights) {
        if (TheShaderMgr.AllowPerPixel() && mat->GetPerPixelLit()) {
            u64 shadowMap = TheRnd.GetShadowMap() != nullptr;
            opts |= (s64)(int)(uint)(shadowMap != 0) << 0x13;
        }
        bool hasRecvProjLights;
        if (!mat->GetRecvProjLights()) {
            hasRecvProjLights = false;
        } else {
            hasRecvProjLights = (((NgEnviron *)env)->NumLights_Proj() >= 1);
        }
        u64 hasPointCubeTex;
        if (!mat->GetRecvPointCubeTex() || ((NgEnviron *)env)->NumLights_Point() < 1) {
            hasPointCubeTex = 0;
        } else {
            hasPointCubeTex = ((NgEnviron *)env)->HasPointCubeTex() ? 1 : 0;
        }
        float aniso = mat->GetAnisotropy();
        int numPointLights = ((NgEnviron *)env)->NumLights_Point();
        int numProjLights;
        if (hasRecvProjLights) {
            numProjLights = ((NgEnviron *)env)->NumLights_Proj();
        } else {
            numProjLights = 0;
        }
        u64 projBlend;
        if (hasRecvProjLights) {
            projBlend = (((NgEnviron *)env)->GetProjectedBlend() == 1) ? 1 : 0;
        } else {
            projBlend = 0;
        }
        opts = (hasPointCubeTex << 4 | projBlend) << 0x2c
            | ((s64)numProjLights & 3U) << 0x1c
            | (((s64)numPointLights & 3U) << 0x14 | (u64)(0.0f < aniso)) << 0x14 | opts;
    }
    bool screenAligned = mat->GetScreenAligned();
    bool fog;
    if (!b) {
        fog = env->FogEnable();
    } else {
        fog = mat->GetFog();
    }
    if (fog) {
        fog = env->FogEnable();
    }
    bool colorAdjust;
    if (!b) {
        colorAdjust = env->UseColorAdjust();
    } else {
        colorAdjust = mat->ColorAdjust();
    }
    u64 hasFurTex;
    if (mat->GetFur() == nullptr || mat->GetFur()->GetFurDetail() == nullptr) {
        hasFurTex = 0;
    } else {
        hasFurTex = 1;
    }
    opts |= hasFurTex << 0x22
        | ((u64)(colorAdjust & 1)) << 0x15 | (u64)fog << 0x12 | ((u64)(screenAligned & 1)) << 0xd;
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
    if (fadeOut && !fog) {
        Vector4 fadeParams(mat->unk2d8, mat->unk2dc, mat->unk2e0, mat->unk2e4);
        TheShaderMgr.SetPConstant((PShaderConstant)0x68, fadeParams);
        opts |= ((s64)mat->unk2d4 & 3U) << 0x1a;
    }
    return (((u64)(TheHiResScreen.IsActive() & 1) << 2
        | (u64)(TheRnd.ResourceCached() & 1)) << 0x32) | opts;
}

u64 RndShaderSyncTrack::CalcShaderOpts(NgMat *mat, ShaderType s, bool b) {
    NgEnviron *env = (NgEnviron *)RndEnviron::Current();
    if (TheRnd.GetDrawMode() == Rnd::kDrawOcclusion) return 0;
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
        Vector4 fadeParams(mat->unk2d8, mat->unk2dc, mat->unk2e0, mat->unk2e4);
        TheShaderMgr.SetPConstant((PShaderConstant)0x68, fadeParams);
        shaderOpts |= ((s64)mat->unk2d4 & 3U) << 0x1a;
    }
    u64 result = (((u64)(TheHiResScreen.IsActive() & 1) << 2
        | (u64)(TheRnd.ResourceCached() & 1)) << 0x32) | shaderOpts;
    result |= 0x80000000000000;
    if (RndSpline::sGlobalDefaultSpline != nullptr) {
        result |= ((u64)(RndSpline::sGlobalDefaultSpline->mPulseDrawing & 1)) << 0x38;
    }
    return (u64)(s == kSyncTrackChargeEffectShader) << 0x3b | result;
}

void RndShaderParticles::Select(RndMat *mat, ShaderType s, bool b) {
    if (!mat) mat = TheRnd.DefaultMat();
    TheRenderState.SetFillMode((RndRenderState::FillMode)0);
    if (!RedundantState(mat, s, false, false, b)) {
        TheNgStats->mMats++;
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
        TheNgStats->mMats++;
        ((NgMat *)mat)->SetupShader(TheShaderMgr.AllowPerPixel(), true);
        u64 optsVal = CalcShaderOpts((NgMat *)mat, s, b);
        SetColorWriteMask(ShaderOptions(optsVal), mat);
        CheckForceCull(kMultimeshShader);
        CheckDistortion(mat);
        Cache(kMultimeshShader, ShaderOptions(optsVal), mat);
    }
}

void RndShaderStandard::Select(RndMat *mat, ShaderType shader_type, bool b) {
    if (!mat) mat = TheRnd.DefaultMat();
    TheRenderState.SetFillMode((RndRenderState::FillMode)0);
    bool skinned = TheShaderMgr.BoneCount() != 0;
    if (!RedundantState(mat, shader_type, skinned, TheShaderMgr.UseAO(), b)) {
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
    }
}

void RndShaderPostProc::Select(RndMat *mat, ShaderType s, bool b) {
    if (!mat) mat = TheRnd.DefaultMat();
    TheRenderState.SetFillMode((RndRenderState::FillMode)0);
    if (!RedundantState(mat, s, false, false, b)) {
        ((NgMat *)mat)->SetupShader(TheShaderMgr.AllowPerPixel(), false);
        u64 optsVal = CalcShaderOpts((NgMat *)mat, s, b);
        TheNgStats->mMats++;
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
        TheNgStats->mMats++;
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
        TheNgStats->mMats++;
        TheRenderState.SetColorWriteMask(7);
        const Hmx::Color &color = mat->GetColor();
        auto _tmp0 = Vector4(color.red, color.green, color.blue, color.alpha);
        TheShaderMgr.SetVConstant(kVS_AmbientColor, _tmp0);
        TheShaderMgr.SetPConstant(kPS_AmbientColor, Vector4(color.red, color.green, color.blue, color.alpha));
        CheckForceCull(s);
        Cache(s, ShaderOptions(optsVal), mat);
    }
}

void RndShaderVelocity::Select(RndMat *mat, ShaderType s, bool b) {
    if (!mat) mat = TheRnd.DefaultMat();
    TheRenderState.SetFillMode((RndRenderState::FillMode)0);
    bool skinned = TheShaderMgr.BoneCount() != 0;
    if (!RedundantState(mat, s, skinned, false, b)) {
        TheNgStats->mMats++;
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
        TheNgStats->mMats++;
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
        TheNgStats->mMats++;
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
        TheNgStats->mMats++;
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
        TheNgStats->mMats++;
        ((NgMat *)mat)->SetupShader(TheShaderMgr.AllowPerPixel(), true);
        CheckShadow();
        u64 optsVal = CalcShaderOpts((NgMat *)mat, shader_type, b);
        MILO_ASSERT((shader_type == kSyncTrackShader || shader_type == kSyncTrackChargeEffectShader), 0x749);
        if (shader_type == kSyncTrackChargeEffectShader) {
            shader_type = kSyncTrackShader;
        }
        SetColorWriteMask(ShaderOptions(optsVal), mat);
        CheckExtrude();
        CheckForceCull(shader_type);
        Cache(shader_type, ShaderOptions(optsVal), mat);
    }
}
