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
    TheShaderMgr.SetPConstant(kPS_FurGeometry, furGeom);

    // Constant 0xc: color interpolation between roots and ends tints
    float diffRed = (mEndsTint.red - mRootsTint.red);
    float diffGreen = (mEndsTint.green - mRootsTint.green);
    float diffBlue = (mEndsTint.blue - mRootsTint.blue);
    float diffAlpha = (mEndsTint.alpha - mRootsTint.alpha);
    diffRed = diffRed * fShell;
    diffGreen = diffGreen * fShell;
    diffBlue = diffBlue * fShell;
    diffAlpha = diffAlpha * fShell;
    Vector4 furColor(
        mRootsTint.red + diffRed,
        mRootsTint.green + diffGreen,
        mRootsTint.blue + diffBlue,
        mRootsTint.alpha + diffAlpha
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
    TheShaderMgr.SetPConstant(kPS_FurShell, furShell);

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
