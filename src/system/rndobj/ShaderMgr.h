#pragma once
#include "math/Mtx.h"
#include "obj/Object.h"
#include "rndobj/Mat.h"
#include "rndobj/ShaderOptions.h"
#include <list>

class RndShaderProgram;

#define PS3_SHADERS_TYPE 'PS3S'
#define PS3_SHADERS_VERSION 1
#define XBOX_SHADERS_TYPE 'XBOX'
#define XBOX_SHADERS_VERSION 1

// vertex shader constant register indices
enum VShaderConstant {
    kVS_Color = 0,
    kVS_AmbientColor = 1,
    kVS_Specular = 2,
    kVS_ViewProjMatrix = 4,
    kVS_Specular2 = 0x13,
    kVS_TexTransform = 0x14,
    kVS_SplineData1 = 0x19,
    kVS_SplineData2 = 0x1A,
    kVS_ShockwavePos = 0x1E,
    kVS_ShockwaveNormal = 0x1F,
    kVS_ShockwaveParams = 0x20,
    kVS_RimColor = 0x3d,
    kVS_BoxMapLight0 = 0x50,
    kVS_WorldTransform = 0x5c,
};

// pixel shader constant register indices
enum PShaderConstant {
    kPS_Color = 0,
    kPS_AmbientColor = 1,
    kPS_Specular = 2,
    kPS_EmissiveTex = 3,
    kPS_EnvironMap = 4,
    kPS_ShaderCost = 4,
    kPS_Texture = 5,
    kPS_BloomParams = 7,
    kPS_Posterize = 8,
    kPS_SpotlightTex = 0xB,
    kPS_FurAlpha = 0xB,
    kPS_FurDetail = 0xC,
    kPS_FurColor = 0xC,
    kPS_Anisotropy = 0xd,
    kPS_DeNormal = 0xe,
    kPS_NgMatCustom = 0xf,
    kPS_Specular2 = 0x13,
    kPS_FurGeometry = 0x32,
    kPS_FurShell = 0x33,
    kPS_RimColor = 0x3d,
    kPS_TexProcFrequency = 0x40,
    kPS_TexProcAmplitude = 0x41,
    kPS_TexProcPhase = 0x42,
    kPS_BoxMapLight0 = 0x50,
    kPS_DetailNormal = 0x6a,
    kPS_ShadowColor = 0x6B,
    kPS_ShadowCamDir = 0x6C,
    kPS_MotionBlur = 0x69,
    kPS_NoiseSeeds = 0x70,
    kPS_NoiseParams = 0x71,
    kPS_HallOfTimeParams = 0x73,
    kPS_HallOfTimeColor = 0x74,
    kPS_Kaleidoscope = 0x75,
    kPS_GradientMap = 0x76,
    kPS_RefractStrength = 0x77,
    kPS_RefractPanning = 0x78,
    kPS_ChromaticAberration = 0x79,
    kPS_Vignette = 0x7b,
    kPS_BlendPrevious = 0x7d,
    kPS_ColorMod0 = 0x83,
    kPS_HueConverge = 0xde,
    kPS_WorldProjection = 0xdc,
};

class RndShaderMgr {
    friend class NgSpotlightDrawer;

public:
    struct ShaderTree {
        ShaderType shaderType;
        RndShaderProgram *obj; // fix type
    };
    RndShaderMgr();
    virtual ~RndShaderMgr() {}
    virtual void PreInit();
    virtual void Init();
    virtual void Terminate();
    virtual RndMat *GetWork() { return mWorkMat; }
    virtual RndMat *GetPostProcMat() { return mPostProcMat; }
    // NOTE: MSVC PPC reverses overloaded virtual function order in vtable.
    // Declare in reverse of desired vtable order.
    virtual void SetVConstant(VShaderConstant, bool) = 0;
    virtual void SetVConstant(VShaderConstant, int) = 0;
    virtual void SetVConstant(VShaderConstant, const Vector4 &) = 0; // 0x24
    virtual void SetVConstant(VShaderConstant, const float *, unsigned int) = 0;
    virtual void SetVConstant(VShaderConstant, RndTex *) = 0;
    virtual void SetVConstant4x3(VShaderConstant, const Hmx::Matrix4 &) = 0;
    virtual void SetVConstant(VShaderConstant, const Hmx::Matrix4 &) = 0; // 0x18
    virtual void SetPConstant(PShaderConstant, bool) = 0;
    virtual void SetPConstant(PShaderConstant, int) = 0;
    virtual void SetPConstant(PShaderConstant, const Vector4 &) = 0; // 0x40
    virtual void SetPConstant(PShaderConstant, RndTex *) = 0; // 0x3c
    virtual void SetPConstant(PShaderConstant, RndCubeTex *) = 0;
    virtual void SetPConstant4x3(PShaderConstant, const Hmx::Matrix4 &) = 0;
    virtual void SetPConstant(PShaderConstant, const Hmx::Matrix4 &) = 0;
    virtual RndMat *DrawHighlightMat() { return mDrawHighlightMat; }
    virtual RndMat *DrawRectMat() { return mDrawRectMat; }

    bool CacheShaders() const { return mCacheShaders; }
    int NumTaps() const { return unk14; }
    void SetNumTaps(int n) { unk14 = n; }
    void UpdateCache(const Transform &, int);
    void SetMeshInfo(int, bool);
    void SetShaderErrorDisplay(bool);
    bool GetShaderErrorDisplay();
    unsigned long InitShaders();
    void SetTransform(const Transform &);
    void SetAllowPerPixel(bool allow) { mAllowPerPixel = allow; }
    void Invalidate(ShaderType);
    void ToggleShowMetaMatErrors() { mShowMetaMatErrors = !mShowMetaMatErrors; }
    void ToggleShowShaderErrors() { mShowShaderErrors = !mShowShaderErrors; }
    RndShaderProgram &FindShader(ShaderType, const ShaderOptions &);
    void *AllocShader();
    int CullModeOverride() const { return mCullModeOverride; }
    bool ShowShaderErrors() const { return mShowShaderErrors; }
    bool InDepthVolume() const { return mInDepthVolume; }
    bool ShowMetaMatErrors() const { return mShowMetaMatErrors; }
    int BoneCount() const { return mBoneCount; }
    bool UseAO() const { return mUseAO; }
    bool AllowPerPixel() const { return mAllowPerPixel; }
    bool GetUnk41() const { return unk41; }

    friend class NgDOFProc;
    friend class NgLight;
    friend class NgPostProc;
    friend class RndShaderDepthVolume;
    friend class RndShaderPostProc;
    friend class RndShaderSimple;
    friend class RndSoftParticleBuffer;

protected:
    virtual void LoadShaders(const char *);
    virtual void LoadShaderFile(FileStream &);
    virtual RndShaderProgram *NewShaderProgram() = 0;

    void ShaderPoolAlloc(int);

    std::list<ShaderTree> mShaderTrees; // 0x4
    bool mUseAO;
    int mBoneCount;
    int unk14;
    bool mInDepthVolume;
    int unk1c;
    int mCullModeOverride; // 0x20 - cull mode override enum (1=none, 2=back, 3=shadow)
    bool unk24;
    bool unk25;
    bool unk26;
    bool unk27;
    bool unk28;
    bool unk29;
    bool unk2a;
    bool unk2b;
    bool unk2c;
    bool unk2d;
    bool unk2e;
    bool unk2f;
    bool unk30;
    bool unk31;
    int unk34;
    bool unk38; // mMotionBlurChecked?
    bool unk39;
    bool unk3a; // mGradientMapChecked?
    bool unk3b;
    bool unk3c;
    bool unk3d;
    bool unk3e; // mVignetteChecked?
    bool unk3f;
    bool mAllowPerPixel; // 0x40
    bool unk41;
    bool mDisplayShaderError; // 0x42
    RndMat *mWorkMat; // 0x44
    RndMat *mPostProcMat; // 0x48
    RndMat *mDrawHighlightMat; // 0x4c
    RndMat *mDrawRectMat; // 0x50
    void *mShaderPool; // 0x54
    int mShaderPoolCount; // 0x58
    int mShaderPoolAlloc; // 0x5c - shader pool alloc
    int mShaderSize;
    float *mConstantCache; // 0x64
    int mConstantCacheSize; // 0x68
    bool mCacheShaders; // 0x6c
    bool mPreInitialized; // 0x6d
    bool mShowShaderErrors; // 0x6e
    bool mShowMetaMatErrors; // 0x6f
};

extern RndShaderMgr &TheShaderMgr;
