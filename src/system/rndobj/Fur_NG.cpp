#include "Fur_NG.h"
#include "rnddx9/RenderState.h"
#include "rndobj/ShaderMgr.h"
#include "rndobj/Shader.h"
#include "rndobj/ShaderOptions.h"
#include "rndobj/Mat.h"
#include "rndobj/Mat_NG.h"
#include "rndobj/Mesh.h"
#include "math/Vec.h"

bool NgFur::Prep(RndMesh *, RndMat *) const {
    TheShaderMgr.SetPConstant(kPS_FurDetail, mFurDetail);
    TheRenderState.SetTextureFilter(12, (RndRenderState::FilterMode)1, false);
    return true;
}
bool NgFur::Shell(int layerIdx, RndMesh *mesh, RndMat *mat) const {
    float zeroVal = 0.0f;
    float fShell;
    float curveVal;
    if (layerIdx != 0) {
        fShell = (float)layerIdx / (float)(mLayers - 1);
    } else {
        fShell = zeroVal;
    }
    if (layerIdx != 0) {
        curveVal = (float)pow((double)fShell, (double)mCurvature);
    } else {
        curveVal = zeroVal;
    }

    // Constant 0x32: fur geometry params
    float gravStretch = mGravity * mStretch;
    float gravSlide = mGravity * mSlide;
    Vector4 furGeom(
        mStretch * fShell,
        mSlide * curveVal,
        gravStretch * fShell,
        gravSlide * curveVal
    );
    // Fur geometry (stretch/slide) drives vertex displacement, so it is a
    // VERTEX-shader constant: target dispatches vtable+0x24 =
    // SetVConstant(VShaderConstant, Vector4 const&), not +0x40 =
    // SetPConstant(PShaderConstant, Vector4 const&). The register index value is
    // shared between the VS/PS constant enums (0x32).  [DC3 oracle agrees]
    TheShaderMgr.SetVConstant((VShaderConstant)kPS_FurGeometry, furGeom);

    // Constant 0xc: color interpolation between roots and ends tints
    // ★ FMA-contraction lever: retail computes the four (ends-roots)*fShell
    // products as separate `fmuls` and only then four `fadds`, whereas MSVC
    // X360 /O1 contracts plain float locals into four `fmadds`. Routing the
    // products through the members of a named object breaks the expression
    // tree so the backend peephole cannot re-fuse them. (#pragma fp_contract
    // is a no-op at these flags -- measured, it changed nothing.)
    Hmx::Color diff;
    diff.red = (mEndsTint.red - mRootsTint.red);
    diff.green = (mEndsTint.green - mRootsTint.green);
    diff.blue = (mEndsTint.blue - mRootsTint.blue);
    diff.alpha = (mEndsTint.alpha - mRootsTint.alpha);
    diff.red = diff.red * fShell;
    diff.green = diff.green * fShell;
    diff.blue = diff.blue * fShell;
    diff.alpha = diff.alpha * fShell;
    Vector4 furColor(
        mRootsTint.red + diff.red,
        mRootsTint.green + diff.green,
        mRootsTint.blue + diff.blue,
        mRootsTint.alpha + diff.alpha
    );
    TheShaderMgr.SetPConstant(kPS_FurColor, furColor);

    // Constant 0x33: shell thickness and vertex data
    float oneVal = 1.0f;
    float shellExponent = -(mShellOut * 0.7f - oneVal);
    float shellThickness;
    if (layerIdx != 0) {
        shellThickness = mThickness * (float)pow((double)fShell, (double)shellExponent);
    } else {
        shellThickness = mThickness / (float)mLayers;
    }

    int numBones = (int)mesh->NumBones();
    float vertCount;
    if (numBones > 1) {
        vertCount = (float)numBones;
    } else {
        vertCount = oneVal;
    }

    Vector4 furShell(shellThickness, vertCount, zeroVal, zeroVal);
    // Shell thickness / vertex count is likewise a VERTEX-shader constant
    // (target dispatches vtable+0x24, not +0x40).  [DC3 oracle agrees]
    TheShaderMgr.SetVConstant((VShaderConstant)kPS_FurShell, furShell);

    // Constant 0xb: alpha processing params
    float alphaExp = mAlphaFalloff * 2.0f + oneVal;
    float alphaResult;
    if (layerIdx != 0) {
        float fShellFull = (float)layerIdx / (float)mLayers;
        alphaResult = (float)pow((double)fShellFull, (double)alphaExp);
    } else {
        alphaResult = zeroVal;
    }

    float alphaScale = oneVal / (oneVal - alphaResult);
    float alphaBias = -(alphaScale * alphaResult);
    Vector4 furAlpha(alphaScale, alphaBias, mFurTiling, zeroVal);
    TheShaderMgr.SetPConstant(kPS_FurAlpha, furAlpha);

    RndShader::SelectConfig(mat, (ShaderType)8, false);

    if (layerIdx == 0) {
        TheRenderState.SetBlend((RndRenderState::Blend)1, (RndRenderState::Blend)0, (RndRenderState::Blend)1, (RndRenderState::Blend)1);
        TheRenderState.SetDepthTestEnable(true);
        TheRenderState.SetDepthWriteEnable(true);
        TheRenderState.SetDepthFunc((RndRenderState::TestFunc)1);
        NgMat::SetCurrent(nullptr);
    }
    return true;
}

NgFur::NgFur() {}
