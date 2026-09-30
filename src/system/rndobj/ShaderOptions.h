#pragma once
#include "types.h"
#include "utl/Str.h"
#include <vector>

// Retail RB3 (and rb3-Wii oracle) ShaderType: exactly 26 members (0..25).
// The trailing DC3-only shaders (sync_track, playerdepth_shell2, yuv_to_*,
// player_greenscreen, crew_photo, twirl, killalpha, allwhite, ...) are Dance
// Central-specific and do NOT exist in Rock Band 3 — retail encodes
// kUnwrapUVShader=0x14, kBloomGlareShader=0x19 and kMaxShaderTypes=0x1a.
// They are kept behind HX_NATIVE for the native/DC3 engine only.
enum ShaderType {
    kBloomShader = 0,
    kBlurShader = 1,
    kDepthVolumeShader = 2,
    kDownsampleShader = 3,
    kDownsample4xShader = 4,
    kDownsampleDepthShader = 5,
    kDrawRectShader = 6,
    kErrorShader = 7,
    kFurShader = 8,
    kLineNozShader = 9,
    kLineShader = 10,
    kMovieShader = 11,
    kMultimeshShader = 12,
    kMultimeshBBShader = 13,
    kParticlesShader = 14,
    kPostprocessErrorShader = 15,
    kPostprocessShader = 16,
    kShadowmapShader = 17,
    kStandardShader = 18,
    kStandardBBShader = 19,
    kUnwrapUVShader = 20,
    kVelocityCameraShader = 21,
    kVelocityObjectShader = 22,
    kPlayerDepthVisShader = 23,
    kPlayerDepthShellShader = 24,
    kBloomGlareShader = 25,
#ifdef HX_NATIVE
    kSyncTrackShader,
    kSyncTrackChargeEffectShader,
    kPlayerDepthShell2Shader,
    kDepthBuffer3DShader,
    kYUVtoRGBShader,
    kYUVtoBlackAndWhiteShader,
    kPlayerGreenScreenShader,
    kPlayerDepthGreenScreenShader,
    kCrewPhotoShader,
    kTwirlShader,
    kKillAlphaShader,
    kAllWhiteShader,
#endif
    kMaxShaderTypes
};

struct ShaderMacro {
    ShaderMacro(const char *n = nullptr, const char *v = nullptr) : Name(n), Value(v) {}

    ShaderMacro &operator=(const ShaderMacro &other) {
        this->Name = other.Name;
        this->Value = other.Value;
        return *this;
    }

    const char *Name; // 0x0
    const char *Value; // 0x4
};

/** The 64-bit shader option word.
 *
 *  The named bits below are the material-shader view of the word (standard,
 *  multimesh, particles, fur, sync_track); the post-process shaders reuse most
 *  of the low bits for unrelated effects and build `flags` by hand.
 *
 *  ⚠ MSVC on Xenon allocates bitfields from the MOST significant bit down, so
 *  the FIRST field declared here is bit 63 and the LAST is bit 0.  The numbers
 *  in the comments are LSB-relative and match the shifts `GenerateMacros` uses.
 *  Assigning one of these fields is what produces the target's
 *  `and rX, rX, ~mask` + `rldimi rX, rY, bit, 63-bit` pair; writing the same
 *  thing as a shift-and-or expression does not. */
struct ShaderOptions {
    ShaderOptions(u64 u) : flags(u) {}

    void GenerateMacros(ShaderType, std::vector<ShaderMacro> &) const;

    union {
        u64 flags; // 0x0
        struct {
            u64 mUnused63 : 1; // 63
            u64 mHueConverge : 1; // 62
            u64 mFastCheapLighting : 1; // 61
            u64 mShockwave : 1; // 60
            u64 mSyncTrackChargeEffect : 1; // 59
            u64 mUnused58 : 1; // 58
            u64 mUnused57 : 1; // 57
            u64 mSplinePulse : 1; // 56
            u64 mFitToSpline : 1; // 55
            u64 mFlipNormal : 1; // 54
            u64 mIntensify : 1; // 53
            u64 mHiResScreen : 1; // 52
            u64 mSpotlight : 1; // 51
            u64 mShowShaderCost : 1; // 50
            u64 mEnvironMapSpecMask : 1; // 49
            u64 mPointCubeTex : 1; // 48
            u64 mNoiseMidtone : 1; // 47
            u64 mRefractWorld : 1; // 46
            u64 mSoftDepthBlend : 1; // 45
            u64 mProjLightMultiply : 1; // 44
            u64 mEnvironMapFalloff : 1; // 43
            u64 mVelocity : 1; // 42
            u64 mNumPoint : 2; // 40-41
            u64 mToneMapping : 1; // 39
            u64 mEnableAO : 1; // 38
            u64 mRimLight : 1; // 37
            u64 mVignette : 1; // 36
            u64 mDisplayError : 1; // 35
            u64 mFurDetail : 1; // 34
            u64 mColorMod : 2; // 32-33
            u64 mCustomVariation : 2; // 30-31
            u64 mNumProj : 2; // 28-29
            u64 mFadeOut : 2; // 26-27
            u64 mBillboard : 1; // 25
            u64 mNormDetail : 1; // 24
            u64 mExtrude : 1; // 23
            u64 mPseudoHDR : 1; // 22
            u64 mColorXfm : 1; // 21
            u64 mAnisotropic : 1; // 20
            u64 mShadowBuffer : 1; // 19
            u64 mFog : 1; // 18
            u64 mApproxLights : 1; // 17
            u64 mRealLights : 1; // 16
            u64 mRimLightMap : 1; // 15
            u64 mRimLightUnder : 1; // 14
            u64 mScreenAligned : 1; // 13
            u64 mSkinned : 1; // 12
            u64 mTexGen : 2; // 10-11
            u64 mUnused9 : 1; // 9
            u64 mPrelit : 1; // 8
            u64 mGlowMap : 1; // 7
            u64 mCopyPrevious : 1; // 6
            u64 mNormalMap : 1; // 5
            u64 mDiffuseMap : 1; // 4
            u64 mEnvironMap : 1; // 3
            u64 mSpecular : 1; // 2
            u64 mSpecularMap : 1; // 1
            u64 mPerPixelLighting : 1; // 0
        };
    };
};

void InitShaderOptions();
const char *ShaderTypeName(ShaderType);
ShaderType ShaderTypeFromName(const char *);
const char *ShaderSourcePath(const char *);
const char *ShaderCachedPath(const char *, u64, bool);
bool IsPostProcShaderType(ShaderType);
void ShaderMakeOptionsString(ShaderType, const ShaderOptions &, String &);
