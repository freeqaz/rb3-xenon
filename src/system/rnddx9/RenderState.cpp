#include "RenderState.h"
#include "rnddx9/Rnd.h"

RndRenderState TheRenderState;

// Maps TestFunc to D3DCMPFUNC. Two banks of 8: [0-7] normal Z, [8-15] reversed Z.
D3DCMPFUNC RndRenderState::tf2cf[] = {
    // Normal Z
    D3DCMP_ALWAYS,      // 0
    D3DCMP_LESS,        // 1
    D3DCMP_EQUAL,       // 2
    D3DCMP_LESSEQUAL,   // 3
    D3DCMP_GREATER,     // 4
    D3DCMP_NOTEQUAL,    // 5
    D3DCMP_GREATEREQUAL,// 6
    D3DCMP_NEVER,       // 7
    // Reversed Z (flip less/greater)
    D3DCMP_ALWAYS,      // 8
    D3DCMP_GREATER,     // 9
    D3DCMP_EQUAL,       // 10
    D3DCMP_GREATEREQUAL,// 11
    D3DCMP_LESS,        // 12
    D3DCMP_NOTEQUAL,    // 13
    D3DCMP_LESSEQUAL,   // 14
    D3DCMP_NEVER,       // 15
};

void RndRenderState::SetBlendEnable(bool b) {
    D3DDevice_SetRenderState_AlphaBlendEnable(TheDxRnd.Device(), (u8)b);
}

void RndRenderState::SetBlendOp(BlendOp op) {
    D3DDevice_SetRenderState_BlendOp(TheDxRnd.Device(), (int)op);
}

void RndRenderState::SetBlend(
    Blend srcblend, Blend dstblend, Blend srcblenda, Blend dstblenda
) {
    D3DDevice_SetRenderState_SrcBlend(TheDxRnd.Device(), (int)srcblend);
    D3DDevice_SetRenderState_DestBlend(TheDxRnd.Device(), (int)dstblend);
    D3DDevice_SetRenderState_SrcBlendAlpha(TheDxRnd.Device(), (int)srcblenda);
    D3DDevice_SetRenderState_DestBlendAlpha(TheDxRnd.Device(), (int)dstblenda);
}

void RndRenderState::SetColorWriteMask(uint mask) {
    D3DDevice_SetRenderState_ColorWriteEnable(TheDxRnd.Device(), mask);
}

void RndRenderState::SetFillMode(FillMode mode) {
    D3DDevice_SetRenderState_FillMode(TheDxRnd.Device(), (int)mode);
}

void RndRenderState::SetCullMode(CullMode mode) {
    D3DDevice_SetRenderState_CullMode(TheDxRnd.Device(), (int)mode);
}

void RndRenderState::SetAlphaTestEnable(bool b) {
    D3DDevice_SetRenderState_AlphaTestEnable(TheDxRnd.Device(), b);
}

void RndRenderState::SetAlphaFunc(TestFunc tf, unsigned int ref) {
    D3DDevice_SetRenderState_AlphaRef(TheDxRnd.Device(), ref);
    D3DDevice_SetRenderState_AlphaFunc(TheDxRnd.Device(), tf2cf[tf]);
}

void RndRenderState::SetDepthTestEnable(bool b) {
    D3DDevice_SetRenderState_ZEnable(TheDxRnd.Device(), b);
}
void RndRenderState::SetDepthWriteEnable(bool b) {
    D3DDevice_SetRenderState_ZWriteEnable(TheDxRnd.Device(), b);
}

void RndRenderState::SetDepthFunc(TestFunc tf) {
    D3DDevice_SetRenderState_ZFunc(TheDxRnd.Device(), tf2cf[TheDxRnd.ReverseZ() * 8 + tf]);
}

void RndRenderState::SetStencilTestEnable(bool b) {
    D3DDevice_SetRenderState_StencilEnable(TheDxRnd.Device(), (u8)b);
}

void RndRenderState::SetStencilFunc(TestFunc tf, u8 ref) {
    D3DDevice_SetRenderState_StencilRef(TheDxRnd.Device(), ref);
    D3DDevice_SetRenderState_StencilFunc(
        TheDxRnd.Device(), tf2cf[TheDxRnd.ReverseZ() * 8 + tf]
    );
}

void RndRenderState::SetStencilOp(StencilOp fail, StencilOp zfail, StencilOp pass) {
    D3DDevice_SetRenderState_StencilFail(TheDxRnd.Device(), (int)fail);
    D3DDevice_SetRenderState_StencilZFail(TheDxRnd.Device(), (int)zfail);
    D3DDevice_SetRenderState_StencilPass(TheDxRnd.Device(), (int)pass);
}

void RndRenderState::Init(void) {
    SetTextureClamp(4, (ClampMode)2);
    SetTextureClamp(5, (ClampMode)2);
    SetTextureFilter(5, (FilterMode)1, false);
}
