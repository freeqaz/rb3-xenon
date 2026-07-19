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

struct ShaderOptions {
    ShaderOptions(u64 u) : flags(u) {}

    void GenerateMacros(ShaderType, std::vector<ShaderMacro> &) const;

    u64 flags; // 0x0
};

void InitShaderOptions();
const char *ShaderTypeName(ShaderType);
ShaderType ShaderTypeFromName(const char *);
const char *ShaderSourcePath(const char *);
const char *ShaderCachedPath(const char *, u64, bool);
bool IsPostProcShaderType(ShaderType);
void ShaderMakeOptionsString(ShaderType, const ShaderOptions &, String &);
