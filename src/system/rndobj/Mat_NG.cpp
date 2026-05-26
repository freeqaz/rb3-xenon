#include "rndobj/Mat_NG.h"
#include "BaseMaterial.h"
#include "ShaderMgr.h"
#include "math/Color.h"
#include "math/Rot.h"
#include "math/Trig.h"
#include "os/Debug.h"
#include "rnddx9/RenderState.h"
#include "rndobj/BaseMaterial.h"
#include "rndobj/Cam.h"
#include "rndobj/Env.h"
#include "rndobj/HiResScreen.h"
#include "rndobj/Rnd.h"
#include "rndobj/ShaderMgr.h"
#include "rndobj/Tex.h"
#include "rndobj/Utl.h"

NgMat *NgMat::sCurrent;

void MakeTex3(const Transform &xfm, bool center, Hmx::Matrix4 &out) {
    out.x.x = xfm.m.x.x;
    out.x.y = -xfm.m.x.y;
    out.x.z = 0.0f;
    out.x.w = 0.0f;
    out.y.x = -xfm.m.y.x;
    out.y.y = xfm.m.y.y;
    out.y.z = 0.0f;
    out.y.w = 0.0f;
    out.z.x = 0.0f;
    out.z.y = 0.0f;
    out.z.z = 1.0f;
    out.z.w = 0.0f;
    float tx = xfm.v.x;
    if (center) {
        float ty = xfm.v.y;
        out.w.z = 0.0f;
        out.w.w = 1.0f;
        float ntx = -tx - 0.5f;
        out.w.x = out.x.x * ntx + (ty - 0.5f) * out.y.x + 0.5f;
        out.w.y = out.x.y * ntx + (ty - 0.5f) * out.y.y + 0.5f;
    } else {
        out.w.x = tx;
        out.w.y = xfm.v.y;
        out.w.z = 0.0f;
        out.w.w = 1.0f;
    }
}

Hmx::Object *NgMat::NewObject() { return new NgMat(); }

RndRenderState::ClampMode sTexWrapClampModes[6] = {
    (RndRenderState::ClampMode)2, (RndRenderState::ClampMode)0,
    (RndRenderState::ClampMode)6, (RndRenderState::ClampMode)6,
    (RndRenderState::ClampMode)1, (RndRenderState::ClampMode)0
};

NgMat::NgMat() {}

NgMat::~NgMat() {
    if (sCurrent == this)
        sCurrent = nullptr;
}

bool NgMat::AllowFog() const {
    return mBlend != kBlendDest && mBlend != kBlendAdd && mBlend != kBlendSubtract
        && mBlend != kBlendSrcAlphaAdd;
}

bool NgMat::AllowHDR() const {
    return (mBlend != kBlendSrcAlpha && mBlend != kBlendSrcAlphaAdd
            && mBlend != kPreMultAlpha)
        && !mAlphaCut && !mAlphaWrite;
}

void NgMat::SetupShader(bool b1, bool b2) {
    if (b2)
        SetupAmbient();
    if (this != sCurrent || mDirty) {
        if (mDirty & 2) {
            RefreshState();
        }
        SetBasicState();
        if (b2) {
            SetRegularShaderConst(b1);
        }
        mDirty = 0;
        sCurrent = this;
    }
}

void NgMat::SetBasicState() {
    RndRenderState::CullMode cm;
    switch (mCull) {
    case kCullNone:
        cm = (RndRenderState::CullMode)0;
        break;
    case kCullRegular:
        cm = (RndRenderState::CullMode)2;
        break;
    case kCullBackwards:
        cm = (RndRenderState::CullMode)6;
        break;
    default:
        cm = (RndRenderState::CullMode)2;
        break;
    }
    TheRenderState.SetCullMode(cm);
    TheRenderState.SetBlendEnable(mBlendEnable);
    TheRenderState.SetBlendOp(mBlendOp);
    TheRenderState.SetBlend(mBlendSrc, mBlendDest, mBlendSrc, mBlendDest);
    TheRenderState.SetAlphaTestEnable(mAlphaCut);
    if (mAlphaCut) {
        TheRenderState.SetAlphaFunc((RndRenderState::TestFunc)5, mAlphaThreshold);
    }
    TheRenderState.SetDepthTestEnable(mDepthTestEnable);
    TheRenderState.SetDepthWriteEnable(mDepthWriteEnable);
    TheRenderState.SetDepthFunc(mDepthFunc);
    if (mStencilMode == kStencilIgnore) {
        TheRenderState.SetStencilTestEnable(false);
    } else {
        TheRenderState.SetStencilTestEnable(true);
        TheRenderState.SetStencilFunc(mStencilFunc, 0);
        TheRenderState.SetStencilOp(
            (RndRenderState::StencilOp)0, (RndRenderState::StencilOp)0, mStencilZFail
        );
    }
    RndRenderState::ClampMode cur = sTexWrapClampModes[mTexWrap];
    if (mDiffuseTex) {
        TheRenderState.SetTextureClamp(0, cur);
    }
    if (mDiffuseTex2) {
        TheRenderState.SetTextureClamp(6, cur);
    }
    if (mNormalMap) {
        TheRenderState.SetTextureClamp(1, cur);
    }
    if (mSpecularMap) {
        TheRenderState.SetTextureClamp(2, cur);
    }
    if (mEmissiveMap) {
        TheRenderState.SetTextureClamp(3, cur);
    }
    if (cur == 6) {
        bool white = mTexWrap == kTexBorderWhite;
        TheRenderState.SetBorderColor(0, white);
        TheRenderState.SetBorderColor(6, white);
        TheRenderState.SetBorderColor(1, white);
        TheRenderState.SetBorderColor(2, white);
        TheRenderState.SetBorderColor(3, white);
    }
}

void NgMat::SetRegularShaderConst(bool perPixel) {
    // Texture params: emissive multiplier, intensify, bloom
    if (mEmissiveMap || mIntensify || AllowHDR()) {
        Vector4 texParams(
            mEmissiveMultiplier,
            (float)((int)(unsigned char)mIntensify + 1),
            mBloomMultiplier,
            0.0f
        );
        TheShaderMgr.SetPConstant(kPS_Texture, texParams);
    }

    // Normal map
    if (mNormalMap) {
        TheShaderMgr.SetPConstant((PShaderConstant)1, (RndTex *)mNormalMap);
        float deNormal = 1.0f - mDeNormal;
        Vector4 deNormalVec(deNormal, deNormal, deNormal, deNormal);
        TheShaderMgr.SetPConstant(kPS_DeNormal, deNormalVec);
        TheRenderState.SetTextureFilter(1, (RndRenderState::FilterMode)1, false);
    }

    // Detail normal map
    if (mNormDetailMap) {
        TheShaderMgr.SetPConstant(kPS_DeNormal, (RndTex *)mNormDetailMap);
        TheRenderState.SetTextureFilter(0xe, (RndRenderState::FilterMode)1, false);
        TheRenderState.SetTextureClamp(0xe, (RndRenderState::ClampMode)0);
        Vector4 detailParams(mNormDetailStrength, mNormDetailTiling, 1.0f, 1.0f);
        TheShaderMgr.SetPConstant(kPS_DetailNormal, detailParams);
    }

    // Emissive map
    if (mEmissiveMap) {
        TheShaderMgr.SetPConstant(kPS_EmissiveTex, (RndTex *)mEmissiveMap);
    }

    // Material color
    Hmx::Color color = mColor;
    if (mBlend == kPreMultAlpha) {
        PreMultiplyAlpha(color);
    }
    {
        Vector4 colorV(color.red, color.green, color.blue, color.alpha);
        TheShaderMgr.SetVConstant(kVS_Color, colorV);
    }
    {
        Vector4 colorP(color.red, color.green, color.blue, color.alpha);
        TheShaderMgr.SetPConstant(kPS_Color, colorP);
    }

    // Diffuse textures
    TheShaderMgr.SetPConstant(kPS_Color, (RndTex *)mDiffuseTex);
    TheShaderMgr.SetPConstant((PShaderConstant)6, (RndTex *)mDiffuseTex2);
    TheRenderState.SetTextureFilter(0, (RndRenderState::FilterMode)1, mPerfSettings.mPS3ForceTrilinear);
    TheRenderState.SetTextureFilter(6, (RndRenderState::FilterMode)1, mPerfSettings.mPS3ForceTrilinear);

    // NgMat custom constants
    TheShaderMgr.SetPConstant(kPS_NgMatCustom, *(const Vector4 *)&mTexHalfPixelX);

    // Specular - copy to local Color structs
    Hmx::Color specColor = mSpecularRGB;
    Hmx::Color spec2Color = mSpecular2RGB;

    float specPow = specColor.alpha;
    specPow = (specPow - 0.5f >= 0.0f) ? specPow : 0.5f;

    float spec2Pow = spec2Color.alpha;
    spec2Pow = (spec2Pow - 0.5f >= 0.0f) ? spec2Pow : 0.5f;

    float specRed, specGreen, specBlue;
    float spec2Red, spec2Green, spec2Blue;

    if (!perPixel && mPerPixelLit && mSpecularMap) {
        specRed = specColor.red * 0.4f;
        specGreen = specColor.green * 0.4f;
        specBlue = specColor.blue * 0.4f;
        specPow *= 0.4f;
        spec2Red = spec2Color.red * 0.4f;
        spec2Green = spec2Color.green * 0.4f;
        spec2Blue = spec2Color.blue * 0.4f;
        spec2Pow *= 0.4f;
    } else {
        specBlue = specColor.blue;
        specGreen = specColor.green;
        specRed = specColor.red;
        spec2Blue = spec2Color.blue;
        spec2Green = spec2Color.green;
        spec2Red = spec2Color.red;
    }

    {
        Vector4 specV(specRed, specGreen, specBlue, specPow);
        TheShaderMgr.SetVConstant(kVS_Specular, specV);
    }
    {
        Vector4 specP(specRed, specGreen, specBlue, specPow);
        TheShaderMgr.SetPConstant(kPS_Specular, specP);
    }

    if (mSpecularMap) {
        TheShaderMgr.SetPConstant(kPS_Specular, (RndTex *)mSpecularMap);
        TheRenderState.SetTextureFilter(2, (RndRenderState::FilterMode)1, false);
    }

    // Rim light - copy to local
    Hmx::Color rimColor = mRimRGB;

    float rimRed = rimColor.red;
    float rimGreen = rimColor.green;
    float rimBlue = rimColor.blue;
    float rimPow = rimColor.alpha;
    rimPow = (rimPow - 0.5f >= 0.0f) ? rimPow : 0.5f;

    {
        Vector4 rimV(rimRed, rimGreen, rimBlue, rimPow);
        TheShaderMgr.SetVConstant(kVS_RimColor, rimV);
    }
    {
        Vector4 rimP(rimRed, rimGreen, rimBlue, rimPow);
        TheShaderMgr.SetPConstant(kPS_RimColor, rimP);
    }

    if (mRimMap) {
        TheShaderMgr.SetPConstant(kPS_NgMatCustom, (RndTex *)mRimMap);
        TheRenderState.SetTextureFilter(0xf, (RndRenderState::FilterMode)1, false);
    }

    // Color mod
    if (mColorModFlags) {
        for (int i = 0; i < 3; i++) {
            const Hmx::Color &c = mColorMod[i];
            Vector4 v(c.red, c.green, c.blue, c.alpha);
            TheShaderMgr.SetPConstant(
                (PShaderConstant)(kPS_ColorMod0 + i), v
            );
        }
    }

    // Environment map
    if (mEnvironMap) {
        TheShaderMgr.SetPConstant(kPS_EnvironMap, (RndCubeTex *)mEnvironMap);
        TheRenderState.SetTextureFilter(4, (RndRenderState::FilterMode)1, false);
    }

    // Specular2 / shader variation
    if (mShaderVariation != kShaderVariationNone) {
        {
            Vector4 spec2V(spec2Red, spec2Green, spec2Blue, spec2Pow);
            TheShaderMgr.SetVConstant(kVS_Specular2, spec2V);
        }
        {
            Vector4 spec2P(spec2Red, spec2Green, spec2Blue, spec2Pow);
            TheShaderMgr.SetPConstant(kPS_Specular2, spec2P);
        }
        float blendRange = 1.0f / (mWorldProjectionEndBlend - mWorldProjectionStartBlend);
        float blendOffset = -(mWorldProjectionStartBlend * blendRange);
        Vector4 wpParams(mWorldProjectionTiling, blendRange, blendOffset, 0.0f);
        TheShaderMgr.SetPConstant(kPS_WorldProjection, wpParams);
    }

    // Tex transform matrix
    TheShaderMgr.SetVConstant(kVS_TexTransform, mTexGenMatrix);

    // Anisotropy
    if (0.0f < mAnisotropy) {
        Vector4 anisoVec(mAnisotropy, 0.0f, 0.0f, 1.0f);
        TheShaderMgr.SetPConstant(kPS_Anisotropy, anisoVec);
    }

    // Refraction
    if (GetRefractEnabled(false)) {
        RndTex *refractNormal = GetRefractNormalMap();
        RndTex *screenTex = TheRnd.GetCurrentFrameTex(TheHiResScreen.IsActive());
        if (refractNormal && screenTex) {
            TheShaderMgr.SetPConstant((PShaderConstant)1, refractNormal);
            TheRenderState.SetTextureFilter(1, (RndRenderState::FilterMode)1, false);
            TheRenderState.SetTextureClamp(1, sTexWrapClampModes[mTexWrap]);
            TheShaderMgr.SetPConstant((PShaderConstant)6, screenTex);
            TheRenderState.SetTextureFilter(6, (RndRenderState::FilterMode)1, false);
            TheRenderState.SetTextureClamp(6, (RndRenderState::ClampMode)2);
            float refractStr = GetRefractStrength();
            Vector4 refractVec(refractStr, refractStr, refractStr, refractStr);
            TheShaderMgr.SetPConstant(kPS_RefractStrength, refractVec);
        }
    }
}

void NgMat::RefreshState() {
    // Half-pixel offset from diffuse texture dimensions
    if (!mDiffuseTex || mDiffuseTex->Width() == 0 || mDiffuseTex->Height() == 0) {
        mTexHalfPixelX = 0.0f;
        mTexHalfPixelY = 0.0f;
        mTexHalfPixelNegX = 0.0f;
        mTexHalfPixelNegY = 0.0f;
    } else {
        int w = mDiffuseTex->Width();
        int h = mDiffuseTex->Height();
        mTexHalfPixelY = 0.5f / h;
        mTexHalfPixelX = 0.5f / w;
        mTexHalfPixelNegY = -0.5f / h;
        mTexHalfPixelNegX = -0.5f / w;
    }

    // Blend mode switch
    switch (mBlend) {
    case kBlendDest:
        mBlendSrc = (RndRenderState::Blend)0;
        mBlendOp = (RndRenderState::BlendOp)0;
        mBlendDest = (RndRenderState::Blend)1;
        mBlendEnable = true;
        break;
    case kBlendSrc:
        mBlendSrc = (RndRenderState::Blend)1;
        mBlendDest = (RndRenderState::Blend)0;
        mBlendOp = (RndRenderState::BlendOp)0;
        mBlendEnable = false;
        break;
    case kBlendAdd:
        mBlendSrc = (RndRenderState::Blend)1;
        mBlendOp = (RndRenderState::BlendOp)0;
        mBlendDest = (RndRenderState::Blend)1;
        mBlendEnable = true;
        break;
    case kBlendSrcAlpha:
        mBlendOp = (RndRenderState::BlendOp)0;
        mBlendSrc = (RndRenderState::Blend)6;
        mBlendDest = (RndRenderState::Blend)7;
        mBlendEnable = true;
        break;
    case kBlendSrcAlphaAdd:
        mBlendOp = (RndRenderState::BlendOp)0;
        mBlendSrc = (RndRenderState::Blend)6;
        mBlendDest = (RndRenderState::Blend)1;
        mBlendEnable = true;
        break;
    case kBlendSubtract:
        mBlendSrc = (RndRenderState::Blend)1;
        mBlendOp = (RndRenderState::BlendOp)4;
        mBlendDest = (RndRenderState::Blend)1;
        mBlendEnable = true;
        break;
    case kBlendMultiply:
        mBlendSrc = (RndRenderState::Blend)0;
        mBlendDest = (RndRenderState::Blend)4;
        mBlendOp = (RndRenderState::BlendOp)0;
        mBlendEnable = true;
        break;
    case kPreMultAlpha:
        mBlendSrc = (RndRenderState::Blend)1;
        mBlendOp = (RndRenderState::BlendOp)0;
        mBlendDest = (RndRenderState::Blend)7;
        mBlendEnable = true;
        break;
    case kScreen:
        mBlendOp = (RndRenderState::BlendOp)0;
        mBlendSrc = (RndRenderState::Blend)9;
        mBlendDest = (RndRenderState::Blend)1;
        mBlendEnable = true;
        break;
    case kLighten:
        mBlendSrc = (RndRenderState::Blend)1;
        mBlendOp = (RndRenderState::BlendOp)3;
        mBlendDest = (RndRenderState::Blend)1;
        mBlendEnable = true;
        break;
    case kDarken:
        mBlendSrc = (RndRenderState::Blend)1;
        mBlendOp = (RndRenderState::BlendOp)2;
        mBlendDest = (RndRenderState::Blend)1;
        mBlendEnable = true;
        break;
    default:
        break;
    }

    // ZMode switch
    switch (mZMode) {
    case kZModeDisable:
        mDepthTestEnable = false;
        mDepthFunc = (RndRenderState::TestFunc)1;
        mDepthWriteEnable = false;
        break;
    case kZModeNormal:
        mDepthTestEnable = true;
        mDepthWriteEnable = true;
        mDepthFunc = (RndRenderState::TestFunc)1;
        break;
    case kZModeTransparent:
        mDepthTestEnable = true;
        mDepthFunc = (RndRenderState::TestFunc)2;
        mDepthWriteEnable = false;
        break;
    case kZModeForce:
        mDepthTestEnable = true;
        mDepthWriteEnable = true;
        mDepthFunc = (RndRenderState::TestFunc)0;
        break;
    case kZModeDecal:
        mDepthTestEnable = true;
        mDepthWriteEnable = true;
        mDepthFunc = (RndRenderState::TestFunc)2;
        break;
    }

    // Stencil mode
    if (mStencilMode == kStencilWrite) {
        mStencilFunc = (RndRenderState::TestFunc)0;
        mStencilZFail = (RndRenderState::StencilOp)3;
    } else {
        mStencilFunc = (RndRenderState::TestFunc)4;
        mStencilZFail = (RndRenderState::StencilOp)0;
    }

    // Static projected texgen transform (Y/Z swap)
    static Transform sProjectedXfm(
        Hmx::Matrix3(1, 0, 0, 0, 0, 1, 0, 1, 0),
        Vector3(0, 0, 0)
    );

    // Second texgen matrix - identity
    mTexGenMatrix2.Zero();
    mTexGenMatrix2.x.x = 1.0f;
    mTexGenMatrix2.y.y = 1.0f;
    mTexGenMatrix2.w.w = 1.0f;
    mTexGenMatrix2.z.z = 1.0f;

    // TexGen
    Transform xfmTmp;
    Hmx::Matrix3 mtxTmp;
    Vector3 vecTmp;
    switch (mTexGen) {
    case kTexGenNone:
        mTexGenMatrix.Zero();
        mTexGenMatrix.w.w = 1.0f;
        mTexGenMatrix.z.z = 1.0f;
        mTexGenMatrix.y.y = 1.0f;
        mTexGenMatrix.x.x = 1.0f;
        break;
    case kTexGenXfm:
    case kTexGenXfmOrigin:
        MakeTex3(mTexXfm, mTexGen == kTexGenXfm, mTexGenMatrix);
        xfmTmp.v.x = 0.0f;
        xfmTmp.v.y = 0.0f;
        xfmTmp.v.z = 0.0f;
        Normalize(mTexXfm.m, xfmTmp.m);
        MakeTex3(xfmTmp, mTexGen == kTexGenXfm, mTexGenMatrix2);
        break;
    case kTexGenSphere:
    case kTexGenEnviron: {
        xfmTmp.m.Set(
            mTexXfm.m.x.x, mTexXfm.m.y.x, mTexXfm.m.z.x,
            mTexXfm.m.x.y, mTexXfm.m.y.y, mTexXfm.m.z.y,
            mTexXfm.m.x.z, mTexXfm.m.y.z, mTexXfm.m.z.z
        );

        Multiply(RndCam::Current()->WorldXfm().m, xfmTmp.m, xfmTmp.m);

        if (mTexGen == kTexGenSphere) {
            MakeEuler(xfmTmp.m, vecTmp);
            vecTmp.x = LimitAng(vecTmp.x) * 0.5f;
            vecTmp.z = LimitAng(vecTmp.z) * 0.5f;
            MakeRotMatrix(vecTmp, xfmTmp.m, true);
        }

        {
            const float *cam = (const float *)&RndCam::Current()->WorldXfm();
            float c0 = cam[0], c4 = cam[4], c8 = cam[8];
            float c1 = cam[1], c5 = cam[5], c9 = cam[9];
            float c2 = cam[2], c6 = cam[6], c10 = cam[10];
            mtxTmp.x.x = c0; mtxTmp.x.y = c4; mtxTmp.x.z = c8;
            mtxTmp.y.x = c1; mtxTmp.y.y = c5; mtxTmp.y.z = c9;
            mtxTmp.z.x = c2; mtxTmp.z.y = c6; mtxTmp.z.z = c10;
        }
        Multiply(mtxTmp, xfmTmp.m, xfmTmp.m);

        mtxTmp.x.x = 0.5f;
        mtxTmp.x.y = 0.0f;
        mtxTmp.x.z = 0.0f;
        mtxTmp.y.x = 0.0f;
        mtxTmp.y.y = 0.0f;
        mtxTmp.y.z = 1.0f;
        mtxTmp.z.x = 0.0f;
        mtxTmp.z.y = -0.5f;
        mtxTmp.z.z = 0.0f;
        Multiply(xfmTmp.m, mtxTmp, xfmTmp.m);

        xfmTmp.v.x = 0.5f;
        xfmTmp.v.y = 0.5f;
        xfmTmp.v.z = 0.0f;
        mTexGenMatrix = Hmx::Matrix4(xfmTmp);
        break;
    }
    case kTexGenProjected: {
        FastInvert(mTexXfm, xfmTmp);
        Transform projXfm = sProjectedXfm;
        projXfm.v.z = -1.0f;
        Multiply(xfmTmp, projXfm, xfmTmp);
        mTexGenMatrix = Hmx::Matrix4(xfmTmp);
        break;
    }
    default:
        MILO_ASSERT(0x139, false);
        break;
    }

    // Second blend switch - set unk2d4 and unk2d8-2e4
    switch (mBlend) {
    case kBlendDest:
        break;
    case kBlendSrc:
        unk2d4 = 0;
        break;
    case kBlendAdd:
    case kBlendSrcAlphaAdd:
    case kBlendSubtract:
    case kScreen:
    case kLighten:
        unk2d8 = 0.0f;
        unk2dc = 0.0f;
        unk2e0 = 0.0f;
        unk2e4 = 0.0f;
        unk2d4 = 2;
        break;
    case kBlendSrcAlpha:
    case kPreMultAlpha:
        unk2d8 = 0.0f;
        unk2dc = 0.0f;
        unk2e0 = 0.0f;
        unk2e4 = 0.0f;
        unk2d4 = 1;
        break;
    case kBlendMultiply:
    case kDarken:
        unk2d8 = 1.0f;
        unk2dc = 1.0f;
        unk2e0 = 1.0f;
        unk2e4 = 1.0f;
        unk2d4 = 2;
        break;
    default:
        MILO_ASSERT(false, 0x139);
        break;
    }
}

void NgMat::SetupAmbient() {
    f32 x, y, z, w;
    if (mUseEnviron) {
        auto _tmp0 = RndEnviron::Current()->AmbientColor();
        const Vector4 &v4 =
            reinterpret_cast<const Vector4 &>(_tmp0);
        w = v4.w;
        z = v4.z;
        y = v4.y;
        x = v4.x;
    } else {
        x = 1.0f;
        y = 1.0f;
        z = 1.0f;
        w = 1.0f;
    }
    Vector4 v4_1(x, y, z, w);
    TheShaderMgr.SetVConstant(kVS_AmbientColor, v4_1);
    Vector4 v4_2(x, y, z, w);
    TheShaderMgr.SetPConstant(kPS_AmbientColor, v4_2);
}
