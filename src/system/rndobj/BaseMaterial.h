#pragma once
#include <vector>
#include "math/Color.h"
#include "math/Mtx.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "obj/Object.h"
#include "rndobj/CubeTex.h"
#include "rndobj/Fur.h"
#include "rndobj/Tex.h"
#include "utl/BinStream.h"

struct bf {
    uint val;
};

struct MatShaderOptions {
    MatShaderOptions();
    union {
        struct {
            int itop : 24;
            int mHasAOCalc : 1;
            int mHasBones : 1;
            int i5 : 1;
            int i4 : 1;
            int i3 : 1;
            int i2 : 1;
            int i1 : 1;
            int i0 : 1;
        } shader_struct;
        u32 pack;

        // from bank 5
        uint value;
        bf shaderType;
        bf billboard;
        bf skinned;
        bf useAO;
    }; // 0x0
    bool mTempMat;

    void SetLast5(int mask) { pack = (pack & ~0x1f) | (mask & 0x1f); }

    void SetHasBones(bool bones) {
        shader_struct.mHasBones = 0;
        shader_struct.mHasBones = bones;
    }

    void SetHasAOCalc(bool calc) { shader_struct.mHasAOCalc = calc; }
};

enum Cull {
    /** "No culling.  User sees both front and back of polygon." */
    kCullNone = 0,
    /** "Only the front face is drawn.  The back face of the polygon is not drawn." */
    kCullRegular = 1,
    /** "The back face of polygone is drawn, but not the front." */
    kCullBackwards = 2
};

enum ShaderVariation {
    kShaderVariationNone = 0,
    kShaderVariationSkin = 1,
    kShaderVariationHair = 2,
    kShaderVariationWorldProjection = 3
};

enum StencilMode {
    kStencilIgnore = 0,
    kStencilWrite = 1,
    kStencilTest = 2,
};

enum TexGen {
    /** "use vertex UV unchanged" */
    kTexGenNone = 0,
    /** "transform vertex UV about center with stage xfm" */
    kTexGenXfm = 1,
    /** "sphere map that rotates around object with camera, xfm is direction of map, fast
     * on gs slow on cpu, flips at poles" */
    kTexGenSphere = 2,
    /** "project from direction of stage xfm in world coords" */
    kTexGenProjected = 3,
    /** "like Xfm but about origin rather than center" */
    kTexGenXfmOrigin = 4,
    /** "reflection map, like sphere map but perspective correct and does not flip, fast
     * on cpu but slow on gs" */
    kTexGenEnviron = 5,
};

enum TexWrap {
    /** "UVs outside the range [0,1] are clamped" */
    kTexWrapClamp = 0,
    /** "The image repeats itself across the surface" */
    kTexWrapRepeat = 1,
    /** "texels outside the UV range [0,1] are black" */
    kTexBorderBlack = 2,
    /** "texels outside the UV range [0,1] are white" */
    kTexBorderWhite = 3,
    /** "The image repeats itself, but is flipped every other repetition" */
    kTexWrapMirror = 4
};

enum ZMode {
    /** "always draw but don't update z-buffer" */
    kZModeDisable = 0,
    /** "draw and update z-buffer if closer than z-buffer" */
    kZModeNormal = 1,
    /** "draw if closer than or equal z-buffer but don't update z-buffer. Often used
     * with SrcAlpha or Add blending so those objects don't occlude other similar
     * objects" */
    kZModeTransparent = 2,
    /** "always draw and update z-buffer" */
    kZModeForce = 3,
    /** "draw and update z-buffer if closer than or equal to z-buffer" */
    kZModeDecal = 4,
};

struct MatPerfSettings {
    MatPerfSettings()
        : mRecvProjLights(false), mRecvPointCubeTex(false), mPS3ForceTrilinear(false) {}
    void Save(BinStream &) const;
    void LoadOld(BinStreamRev &);
    void Load(BinStream &);

    /** "Check this option to allow the material to receive projected lighting" */
    bool mRecvProjLights;
    /** "Check this option to allow the material to receive projected cube maps from a
     * point light" */
    bool mRecvPointCubeTex;
    /** "Force trilinear filtering of diffuse map (PS3 only)" */
    bool mPS3ForceTrilinear;
};

// RB3-360 retail has ONE material class, RndMat, deriving directly from
// Hmx::Object -- the BaseMaterial/RndMat split is a DC3-era refactor we inherited
// with src/system. Settled on retail bytes by lane BASEMAT-1 and merged by
// BASEMAT-2; see docs/decomp/basematerial-is-a-dc3-refactor-2026-08-13.md.
//
// The class lives in BaseMaterial.h (rather than Mat.h) purely because
// BaseMaterial.cpp is the root of the decomp unit that scatter-includes Mat.cpp
// and carries retail's material Save; the FILE names are unit-boundary artifacts
// and no longer name a class. Moving them would churn splits.txt, which another
// lane owns.
class RndMat : public Hmx::Object {
    friend class NgLight;
    friend class NgSpotlightDrawer;

public:
    enum PropDisplay {
        kPropDisplayHidden = 0,
        kPropDisplayReadOnly = 1
    };
    enum ColorModFlags {
        kColorModNone = 0,
        kColorModAlphaPack = 1,
        kColorModAlphaUnpackModulate = 2,
        kColorModModulate = 3,
        kColorModNum = 3
    };
    enum Blend {
        /** "Don't show this material at all; just show the frame buffer" */
        kBlendDest = 0,
        /** "Don't blend this material at all" */
        kBlendSrc = 1,
        /** "Output is material + frame buffer" */
        kBlendAdd = 2,
        /** "Output is (material x mat alpha) + (frame buffer x (1 - mat alpha))" */
        kBlendSrcAlpha = 3,
        /** "Output is (material x mat alpha) + frame buffer" */
        kBlendSrcAlphaAdd = 4,
        /** "Output is frame buffer - material" */
        kBlendSubtract = 5,
        /** "Output is frame buffer x material" */
        kBlendMultiply = 6,
        /** "Output is material + (frame buffer x (1 - mat alpha)" */
        kPreMultAlpha = 7,
        /** "Lightens the frame buffer based on the lightness of the material" */
        kScreen = 8,
        /** "Compares the material and frame buffer and picks the lightest value, per
         * channel"
         */
        kLighten = 9,
        /** "Compares the material and frame buffer and picks the darkest value, per
         * channel"
         */
        kDarken = 10
    };

    friend void SetColorWriteMask(const struct ShaderOptions &, RndMat *);
    friend void CheckDistortion(RndMat *);
    friend void CheckDistortionOpts(RndMat *, struct ShaderOptions &);

    virtual ~RndMat();
    OBJ_CLASSNAME(Mat);
    OBJ_SET_TYPE(Mat);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);

    // Tag 0x3C (BaseMaterial's), NOT the DC3 RndMat's 69: retail's material factory
    // ?NewObject@...@0x8240f5d0 pairs at mpn 100 with the 0x3C-tagged allocation.
    OBJ_MEM_OVERLOAD(0x3C);
    NEW_OBJ(RndMat)

    static void Init();
    static void Terminate();

    const DataNode *GetDefaultPropVal(Symbol);
    RndMat *NextPass() const { return mNextPass; }
    RndTex *GetDiffuseTex() const { return mDiffuseTex; }
    RndTex *NormalMap() const { return mNormalMap; }
    ZMode GetZMode() const { return mZMode; }
    bool IsNextPass(RndMat *m);
    const Transform &TexXfm() const { return mTexXfm; }
    Transform &TexXfm() { return mTexXfm; }
    TexGen GetTexGen() const { return mTexGen; }
    const Hmx::Color &GetColor() const { return mColor; }
    Hmx::Color &GetColor() { return mColor; }
    float Alpha() const { return mColor.alpha; }
    bool UseEnviron() const { return mUseEnviron; }
    bool PointLights() const { return mPointLights; }
    bool ColorAdjust() const { return mColorAdjust; }
    bool FadeOut() const { return mFadeout; }
    bool Prelit() const { return mPrelit; }
    Blend GetBlend() const { return mBlend; }
    Cull GetCull() const { return (Cull)mCull; }
    StencilMode GetStencil() const { return mStencilMode; }
    bool GetAlphaCut() const { return mAlphaCut; }
    bool GetAlphaWrite() const { return mAlphaWrite; }
    int GetAlphaThreshold() const { return mAlphaThreshold; }
    TexWrap GetTexWrap() const { return mTexWrap; }
    const Hmx::Color& GetSpecularRGB() const { return mSpecularRGB; }
    const Hmx::Color& GetRimRGB() const { return mRimRGB; }
    bool GetRimLightUnder() const { return mRimLightUnder; }
    float GetEmissiveMultiplier() const { return mEmissiveMultiplier; }
    bool GetIntensify() const { return mIntensify; }
    RndTex* GetEmissiveMap() const { return mEmissiveMap; }
    ShaderVariation GetShaderVariation() const { return mShaderVariation; }
    void SetShaderVariation(ShaderVariation v) { mShaderVariation = v; }
    const Hmx::Color& GetSpecular2RGB() const { return mSpecular2RGB; }
    RndTex* GetSpecularMap() const { return mSpecularMap; }
    RndTex* GetRimMap() const { return mRimMap; }
    float GetDeNormal() const { return mDeNormal; }
    float GetAnisotropy() const { return mAnisotropy; }
    RndTex* GetNormDetailMap() const { return mNormDetailMap; }
    float GetNormDetailTiling() const { return mNormDetailTiling; }
    float GetNormDetailStrength() const { return mNormDetailStrength; }
    bool GetFog() const { return mFog; }
    bool GetUseEnviron() const { return mUseEnviron; }
    RndCubeTex* GetEnvironMap() const { return mEnvironMap; }
    bool GetEnvironMapFalloff() const { return mEnvironMapFalloff; }
    bool GetEnvironMapSpecMask() const { return mEnvironMapSpecMask; }
    bool GetPerPixelLit() const { return mPerPixelLit; }
    bool GetScreenAligned() const { return mScreenAligned; }
    bool GetRecvProjLights() const { return mPerfSettings.mRecvProjLights; }
    bool GetRecvPointCubeTex() const { return mPerfSettings.mRecvPointCubeTex; }
    RndFur* GetFur() const { return mFur; }

    bool GetRefractEnabled(bool bypass_frame_check);
    float GetRefractStrength();
    RndTex *GetRefractNormalMap();
    void SetZMode(ZMode mode) {
        mZMode = mode;
        mDirty |= 2;
    }
    void SetTexWrap(TexWrap wrap) {
        mTexWrap = wrap;
        mDirty |= 2;
    }
    void SetBlend(Blend blend) {
        mBlend = blend;
        mDirty |= 2;
    }
    void SetTexGen(TexGen gen) {
        mTexGen = gen;
        mDirty |= 2;
    }
    void SetAlphaWrite(bool write) {
        mAlphaWrite = write;
        mDirty |= 2;
    }
    void SetAlphaCut(bool cut) {
        mAlphaCut = cut;
        mDirty |= 2;
    }
    void SetUseEnv(bool use_env) {
        mUseEnviron = use_env;
        mDirty |= 2;
    }
    void SetPreLit(bool lit) {
        mPrelit = lit;
        mDirty |= 2;
    }
    void SetAlphaThreshold(int thresh) { mAlphaThreshold = thresh; }
    void SetPerPixelLit(bool lit) {
        mPerPixelLit = lit;
        mDirty |= 2;
    }
    void SetPointLights(bool lit) { mPointLights = lit; }
    void SetColor(const Hmx::Color &col) {
        mColor.Set(col.red, col.green, col.blue);
        mDirty |= 1;
    }
    void SetColor(float r, float g, float b) {
        mColor.Set(r, g, b);
        mDirty |= 1;
    }
    void SetAlpha(float a) {
        mColor.alpha = a;
        mDirty |= 1;
    }
    void SetShaderOpts(const MatShaderOptions &opts) { mShaderOptions = opts; }
    void SetTexXfm(const Transform &xfm) {
        mTexXfm = xfm;
        mDirty |= 2;
    }
    void SetDiffuseTex(RndTex *tex) {
        mDiffuseTex = tex;
        mDirty |= 2;
    }
    void SetNormalMap(RndTex *tex) {
        mNormalMap = tex;
        mDirty |= 2;
    }
    void SetCull(Cull cull) {
        mCull = cull;
        mDirty |= 2;
    }
    bool Dirty() const { return mDirty; }
    void MarkDirty(int flags) { mDirty |= flags; }

    void SetColorMod(const Hmx::Color &, int);
    void SetSpecularMap(RndTex *);
    int GetColorModFlags() const { return mColorModFlags; }
    void SetColorModFlags(ColorModFlags flags) {
        mColorModFlags = flags;
        mDirty |= 2;
    }

protected:
    RndMat();
    bool PropValDifferent(Symbol, RndMat *);

    DataNode OnAllowedNextPass(const DataArray *);
    DataNode OnAllowedNormalMap(const DataArray *);

    bool OnGetPropertyDisplay(PropDisplay, Symbol);
    // Unreferenced since the DC3 rev-0x46 Load layer was removed (retail's Load
    // reads ONE rev and inlines its own old-version path). Kept, not deleted --
    // reconstructing retail's inlined old path is a separate lane's job.
    void LoadOld(BinStreamRev &);

    static void SetDefaultMat(RndMat *);

    // ==== Retail RB3-360 layout, 0x28..0x18c (BaseMaterial size 0x18c). ====
    // Derived byte-exact from ctor fn_82425998 + Save@BaseMaterial (0x824233C0).
    // Declaration order == offset order (MSVC lays members in decl order).
    /** "How to blend poly into screen" */
    Blend mBlend; // 0x28
    /** "Base material color" */
    Hmx::Color mColor; // 0x2c
    /** "How to read and write z-buffer" */
    ZMode mZMode; // 0x3c
    /** "How to read and write the stencil buffer" */
    StencilMode mStencilMode; // 0x40
    /** "How to generate texture coordinates" */
    TexGen mTexGen; // 0x44
    /** "Texture mapping mode" */
    TexWrap mTexWrap; // 0x48
    /** "Transform for coordinate generation" */
    Transform mTexXfm; // 0x4c
    /** "Base texture map, modulated with color and alpha" */
    ObjPtr<RndTex> mDiffuseTex; // 0x8c
    /** "Double the intensity of base map" */
    bool mIntensify; // 0x98
    /** "Modulate with environment ambient and lights" */
    bool mUseEnviron; // 0x99
    /** "Use vertex color and alpha for base or ambient" */
    bool mPrelit; // 0x9a
    /** "Cut zero alpha pixels from z-buffer" */
    bool mAlphaCut; // 0x9b
    /** "Write pixel alpha to screen" */
    bool mAlphaWrite; // 0x9c
    /** "Alpha level below which gets cut". Ranges from 0 to 255. */
    int mAlphaThreshold; // 0xa0
    /** "Next material for object" */
    ObjPtr<RndMat> mNextPass; // 0xa4
    /** "Multiplier to apply to emission" */
    float mEmissiveMultiplier; // 0xb0
    /** "Specular color." */
    Hmx::Color mSpecularRGB; // 0xb4
    /** "Secondary specular color.  Only valid for certain shader variations." */
    Hmx::Color mSpecular2RGB; // 0xc4
    /** "Texture map to define lighting normals." */
    ObjPtr<RndTex> mNormalMap; // 0xd4
    /** "Map for self illumination" */
    ObjPtr<RndTex> mEmissiveMap; // 0xe0
    /** "Texture map for specular color (RGB) and glossiness (Alpha)." */
    ObjPtr<RndTex> mSpecularMap; // 0xec
    /** "Cube texture for reflections" */
    ObjPtr<RndCubeTex> mEnvironMap; // 0xf8
    /** "Use fur shader" */
    ObjPtr<RndFur> mFur; // 0x104
    /** "Amount to diminish normal map bumpiness". */
    float mDeNormal; // 0x110
    /** "Specular power in downward (strand) direction, 0 to disable". */
    float mAnisotropy; // 0x114
    /** "Select a variation on the shader." */
    ShaderVariation mShaderVariation; // 0x118
    /** "Cull backface polygons" (stored as 1 byte in retail) */
    unsigned char mCull; // 0x11c
    /** "Use per-pixel lighting" */
    bool mPerPixelLit; // 0x11d
    /** "Projected material from camera's POV" */
    bool mScreenAligned; // 0x11e
    /** "Causes the reflection to increase at glancing angles." */
    bool mEnvironMapFalloff; // 0x11f
    /** "Masks the reflection by the specular map alpha channel" */
    bool mEnvironMapSpecMask; // 0x120
    /** "When enabled, this material will refract the screen under the material" */
    bool mRefractEnabled; // 0x121
    /** "The scale of the refraction of the screen under the material." */
    float mRefractStrength; // 0x124
    /** "Normal map used to distort the screen under the material." */
    ObjPtr<RndTex> mRefractNormalMap; // 0x128
    /** "Rim effect highlights the undersides of meshes" */
    bool mRimLightUnder; // 0x134
    /** "Rim lighting color." */
    Hmx::Color mRimRGB; // 0x138
    /** "Texture map that defines the rim lighting color/power." */
    ObjPtr<RndTex> mRimMap; // 0x148
    int mColorModFlags; // 0x154
    std::vector<Hmx::Color> mColorMod; // 0x158
    /** "Detail map texture" */
    ObjPtr<RndTex> mNormDetailMap; // 0x164
    /** "Texture tiling scale for the detail map" */
    float mNormDetailTiling; // 0x170
    /** "Strength of the detail map bumpiness" */
    float mNormDetailStrength; // 0x174
    /** "Is the Mat lit with point lights?" */
    bool mPointLights; // 0x178
    /** "Is the Mat affected by fog?" */
    bool mFog; // 0x179
    /** "Is the Mat affected its Environment's fade_out?" */
    bool mFadeout; // 0x17a
    /** "Is the Mat affected its Environment's color adjust?" */
    bool mColorAdjust; // 0x17b
    /** "Performance options for this material" */
    MatPerfSettings mPerfSettings; // 0x17c
    MatShaderOptions mShaderOptions; // 0x180
    int mDirty; // 0x188
    // RndMat ends at 0x18c (396) in retail RB3-360 -- and adds NO members beyond
    // this point, which is why the BaseMaterial merge is layout-neutral.

#ifdef RB3_DC3_MAT
    // DC3-only members — absent from retail RB3-360 (verified: no ctor init slot,
    // not in BaseMaterial::Save@0x824233C0). Gated out so size == retail 0x18c.
    ObjPtr<RndTex> mDiffuseTex2;
    bool mForceAlphaWrite;
    float mBloomMultiplier;
    bool mNeverFitToSpline;
    bool mAllowDistortionEffects;
    float mShockwaveMult;
    float mWorldProjectionTiling;
    float mWorldProjectionStartBlend;
    float mWorldProjectionEndBlend;
#endif
};

RndMat::Blend CheckBlendMode(RndMat::Blend b, RndMat *);
RndMat *LookupOrCreateMat(const char *, ObjectDir *);
