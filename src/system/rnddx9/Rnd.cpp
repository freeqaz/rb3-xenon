#include "rnddx9/Rnd.h"
#include "Tex.h"
#include "math/Mtx.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/System.h"
#include "rndobj/Bitmap.h"
#include "rndobj/Cam.h"
#include "rndobj/Mat.h"
#include "rndobj/Mat_NG.h"
#include "rndobj/Rnd_NG.h"
#include "rndobj/Shader.h"
#include "rndobj/ShaderMgr.h"
#include "rndobj/Tex.h"
#include "rndobj/Utl.h"
#include "xdk/D3D9.h"
#include "xdk/d3d9i/d3d9.h"
#include "xdk/d3d9i/d3d9caps.h"
#include "xdk/d3d9i/d3d9types.h"

DxRnd TheDxRnd;

BEGIN_HANDLERS(DxRnd)
    HANDLE_ACTION(suspend, Suspend())
    HANDLE_SUPERCLASS(Rnd)
END_HANDLERS

void DxRnd::Clear(unsigned int ui, const Hmx::Color &c) {
    float f1;
    if (mReverseZ) {
        f1 = 0;
    } else {
        f1 = 1;
    }
    int mask = 0;
    if (ui & 1) {
        mask = 0xF;
    }
    if (ui & 2) {
        mask |= 0x30;
    }
    D3DDevice_Clear(mD3DDevice, 0, nullptr, mask, MakeColor(c), f1, 0, 0);
}

void DxRnd::DrawRect(
    const Hmx::Rect &rect,
    const Hmx::Color &colorRef,
    RndMat *mat,
    const Hmx::Color *colorPtr1,
    const Hmx::Color *colorPtr2
) {
    DrawRect(rect, mat, kDrawRectShader, colorRef, colorPtr1, colorPtr2);
}

namespace {
    // FVF 0x1c2 = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1,
    // i.e. 28 bytes per vertex.
    struct RectVert {
        float x, y, z; // 0x00
        unsigned long diffuse; // 0x0c
        unsigned long specular; // 0x10
        float u, v; // 0x14
    };
}

// W16-A: this 6-param overload was DECLARED in rnddx9/Rnd.h and DEFINED NOWHERE.
// The 5-param overload above forwards to it, so the match build emitted no body
// at all and retail's 1,512 B row read fuzzy 0 for want of anything to pair with.
// Ported from the dc3-decomp oracle (src/system/rnddx9/Rnd.cpp), which is matched
// there; the three codegen comments below are dc3's own measured findings and are
// load-bearing -- do not "simplify" any of them.
void DxRnd::DrawRect(
    const Hmx::Rect &rect,
    RndMat *mat,
    ShaderType shader,
    const Hmx::Color &color,
    const Hmx::Color *color1,
    const Hmx::Color *color2
) {
    RectVert verts[4];
    float z = mReverseZ ? 1.0f : 0.0f;
    verts[0].z = z;
    verts[0].x = rect.x;
    verts[0].y = rect.y;
    verts[1].x = rect.x;
    verts[1].z = z;
    verts[2].y = rect.y;
    verts[2].z = z;
    verts[3].z = z;
    verts[1].y = rect.y + rect.h;
    verts[2].x = rect.x + rect.w;
    verts[3].x = rect.x + rect.w;
    verts[3].y = rect.y + rect.h;

    RndMat *next = mat ? mat->NextPass() : nullptr;

    // The four corner colours. Written as chained assignments because that is
    // what merges the per-branch tails the way the target does; separate
    // statements per corner add 7 instructions.
    if (mat && !mat->Prelit()) {
        verts[0].diffuse = verts[1].diffuse = verts[2].diffuse = verts[3].diffuse =
            MakeColor(mat->GetColor());
    } else if (color1) {
        // Horizontal gradient: the right-hand corners take color1.
        verts[0].diffuse = verts[1].diffuse = MakeColor(color);
        verts[2].diffuse = verts[3].diffuse = MakeColor(*color1);
    } else if (color2) {
        // Vertical gradient: the bottom corners take color2.
        verts[0].diffuse = verts[2].diffuse = MakeColor(color);
        verts[1].diffuse = verts[3].diffuse = MakeColor(*color2);
    } else {
        verts[0].diffuse = verts[1].diffuse = verts[2].diffuse = verts[3].diffuse =
            MakeColor(color);
    }

    for (int i = 0; i < 4; i++) {
        verts[i].specular = 0;
    }

    const Transform *texXfm = nullptr;
    if (mat && mat->GetTexGen() == kTexGenXfmOrigin) {
        texXfm = &mat->TexXfm();
    }
    // Each corner's UV is the unit-square coordinate pushed through the
    // material's texture transform. Written out per corner rather than through a
    // helper: the target inlines the corner constants, and MSVC folds a multiply
    // by 1.0f but NOT one by 0.0f (x*0.0f is not x for NaN/Inf), so the 0.0f
    // factors have to survive into the source.
    if (texXfm) {
        verts[0].u = texXfm->m.x.x * 0.0f - texXfm->m.y.x * 0.0f + texXfm->v.x;
        verts[0].v = texXfm->m.y.y * 0.0f - texXfm->m.x.y * 0.0f + texXfm->v.y;
    } else {
        verts[0].u = 0.0f;
        verts[0].v = 0.0f;
    }
    if (texXfm) {
        verts[1].u = texXfm->m.x.x * 0.0f - texXfm->m.y.x * 1.0f + texXfm->v.x;
        verts[1].v = texXfm->m.y.y * 1.0f - texXfm->m.x.y * 0.0f + texXfm->v.y;
    } else {
        verts[1].u = 0.0f;
        verts[1].v = 1.0f;
    }
    if (texXfm) {
        verts[2].u = texXfm->m.x.x * 1.0f - texXfm->m.y.x * 0.0f + texXfm->v.x;
        verts[2].v = texXfm->m.y.y * 0.0f - texXfm->m.x.y * 1.0f + texXfm->v.y;
    } else {
        verts[2].u = 1.0f;
        verts[2].v = 0.0f;
    }
    if (texXfm) {
        verts[3].u = texXfm->m.x.x * 1.0f - texXfm->m.y.x * 1.0f + texXfm->v.x;
        verts[3].v = texXfm->m.y.y * 1.0f - texXfm->m.x.y * 1.0f + texXfm->v.y;
    } else {
        verts[3].u = 1.0f;
        verts[3].v = 1.0f;
    }

    static Transform sScreenXfm(
        Hmx::Matrix3(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f),
        Vector3(0.0f, 0.0f, 0.0f)
    );
    // Binding the manager to a local reference is what keeps its pointer in a
    // callee-saved register across the Matrix4 temporary's constructor, which is
    // what the target does; without it MSVC reloads the global afterwards and the
    // whole callee-saved allocation shifts. It has to be declared HERE, not at the
    // top of the function: hoisting it costs an extra register.
    RndShaderMgr &shaderMgr = TheShaderMgr;
    shaderMgr.SetVConstant(kVS_ViewProjMatrix, Hmx::Matrix4(sScreenXfm));
    TheShaderMgr.SetTransform(sScreenXfm);
    D3DDevice_SetRenderState_ViewportEnable(TheDxRnd.Device(), 0);
    D3DDevice_SetRenderState_HalfPixelOffset(TheDxRnd.Device(), 1);
    D3DDevice_SetFVF(mD3DDevice, 0x1c2);
    RndMat *pass = mat;
    do {
        RndShader::SelectConfig(pass, shader, false);
        D3DDevice_DrawVerticesUP(
            mD3DDevice, D3DPT_TRIANGLESTRIP, 4, verts, sizeof(RectVert)
        );
        pass = next;
        next = next ? next->NextPass() : nullptr;
    } while (pass);
    D3DDevice_SetRenderState_ViewportEnable(TheDxRnd.Device(), 1);
    D3DDevice_SetRenderState_HalfPixelOffset(TheDxRnd.Device(), 0);
    if (RndCam::Current()) {
        TheShaderMgr.SetVConstant(
            kVS_ViewProjMatrix, RndCam::Current()->GetViewProjMatrix()
        );
    }
}

void DxRnd::DrawLine(const Vector3 &v1, const Vector3 &v2, const Hmx::Color &c, bool b4) {
    // Vertex buffer layout: 2 vertices (xyz + color each) + Transform matrix
    // Total: 8 floats + 48 bytes = 96 bytes (24 floats)
    float vertices[24];
    unsigned long colorVal = MakeColor(c);

    // First vertex
    vertices[0] = v1.x;
    vertices[1] = v1.y;
    vertices[2] = v1.z;
    *(unsigned long *)&vertices[3] = colorVal;

    // Second vertex
    vertices[4] = v2.x;
    vertices[5] = v2.y;
    vertices[6] = v2.z;
    *(unsigned long *)&vertices[7] = colorVal;

    // Initialize identity transform in-place (vertices[8..19])
    Transform &xfm = reinterpret_cast<Transform &>(vertices[8]);
    xfm.Reset();

    TheShaderMgr.SetTransform(xfm);
    RndShader::SelectConfig(nullptr, b4 ? kLineShader : kLineNozShader, false);
    D3DDevice_SetFVF(mD3DDevice, 0x42);
    D3DDevice_DrawVerticesUP(mD3DDevice, D3DPT_LINELIST, 2, vertices, 0x10);
}

void DxRnd::MakeDrawTarget() {
    if (mWorldEnded) {
        D3DDevice_SetRenderTarget_External(mD3DDevice, 0, mOffscreenRT);
        D3DDevice_SetDepthStencilSurface(mD3DDevice, mOffscreenDepth);
    } else {
        D3DDevice_SetRenderTarget_External(mD3DDevice, 0, mBackBuffer);
        D3DDevice_SetDepthStencilSurface(mD3DDevice, mWorldDepth);
    }
    NgMat::SetCurrent(nullptr);
}

void DxRnd::SetViewport(const Viewport &v) {
    if (GetGfxMode() == kNewGfx) {
        NgRnd::SetViewport(v);
    }
    D3DVIEWPORT9 dxViewport;
    dxViewport.X = v.X;
    dxViewport.Y = v.Y;
    dxViewport.Width = v.Width;
    dxViewport.Height = v.Height;
    if (mReverseZ) {
        dxViewport.MinZ = 1.0f - v.MinZ;
        dxViewport.MaxZ = 1.0f - v.MaxZ;
    } else {
        dxViewport.MinZ = v.MinZ;
        dxViewport.MaxZ = v.MaxZ;
    }
    D3DDevice_SetViewport(mD3DDevice, &dxViewport);
}

bool DxRnd::Offscreen() const {
    D3DSurface *back = BackBuffer();
    D3DSurface *target = D3DDevice_GetRenderTarget(mD3DDevice, 0);
    bool ret = target != back;
    if (target) {
        D3DResource_Release(target);
    }
    if (back) {
        D3DResource_Release(back);
    }
    return ret;
}

void DxRnd::DrawLargeQuad(
    const LargeQuadRenderData &data, const Transform &tf, RndMat *mat, ShaderType s
) {
    RndMat *next = mat ? dynamic_cast<RndMat *>(mat->NextPass()) : nullptr;
    RndMat *it = mat;
    do {
        RndShader::SelectConfig(it, s, false);
        D3DDevice_SetIndices(mD3DDevice, data.mIndexBuffer);
        D3DDevice_SetStreamSource(mD3DDevice, 0, data.mVertexBuffer, 0, 20, 1);
        D3DDevice_SetFVF(mD3DDevice, 0x102);
        TheShaderMgr.SetVConstant(kVS_WorldTransform, Hmx::Matrix4(tf));
        DxTex *tex = static_cast<DxTex *>(mat->GetDiffuseTex());
        D3DDevice_SetTexture(mD3DDevice, 0x10, tex->Tex(), 0x8000);
        D3DDevice_SetTexture(mD3DDevice, 0, tex->Tex(), 0x80000000);
        D3DDevice_DrawIndexedVertices(
            mD3DDevice, D3DPT_QUADLIST, 0, 0, (data.mHeight - 1) * (data.mWidth - 1) * 4
        );
        it = next;
        next = next ? dynamic_cast<RndMat *>(next->NextPass()) : nullptr;
    } while (it != nullptr);
    D3DDevice_SetIndices(mD3DDevice, nullptr);
    D3DDevice_SetStreamSource(mD3DDevice, 0, nullptr, 0, 0, 1);
    D3DDevice_SetTexture(mD3DDevice, 0x10, nullptr, 0x8000);
}

void DxRnd::SetVertShaderTex(RndTex *tex, int sampler) {
    D3DBaseTexture *texPtr;
    if (tex) {
        texPtr = static_cast<DxTex *>(tex)->Tex();
    } else {
        texPtr = nullptr;
    }
    int slot = sampler + 0x10;
    u32 shift = slot + 0x20;
    D3DDevice_SetTexture(
        mD3DDevice,
        slot,
        texPtr,
        0x8000000000000000 >> shift
    );
}

void DxRnd::PreDeviceReset() {
    if (mOcclusionQueryMgr) {
        mOcclusionQueryMgr->ReleaseQueries();
    }
    FOREACH (it, mDxObjects) {
        (*it)->PreDeviceReset();
    }
    ReleaseAutoRelease();
}

void DxRnd::PostDeviceReset() {
    FOREACH (it, mDxObjects) {
        (*it)->PostDeviceReset();
    }
    MakeDrawTarget();
    InitRenderState();
}

D3DFORMAT DxRnd::D3DFormatForBitmap(const RndBitmap &bitmap) {
    int fmt = bitmap.Order() & 0x38;
    int bpp = bitmap.Bpp();
    D3DFORMAT result = (D3DFORMAT)-1;
    if (fmt != 0) {
        switch (fmt) {
        case 8:
            result = D3DFMT_DXT1;
            break;
        case 0x10:
            result = D3DFMT_DXT3;
            break;
        case 0x18:
            result = D3DFMT_DXT5;
            break;
        case 0x20:
            result = D3DFMT_DXN;
            break;
        default:
            MILO_WARN("Invalid dxt format: %d", fmt);
            MILO_ASSERT(fmt != D3DFMT_UNKNOWN, 999);
            break;
        }
    } else {
        switch (bpp) {
        case 4:
        case 8:
            result = D3DFMT_A8R8G8B8;
            break;
        case 0x10:
            result = D3DFMT_A1R5G5B5;
            break;
        case 0x18:
            result = D3DFMT_X8R8G8B8;
            break;
        case 0x20:
            result = D3DFMT_A8R8G8B8;
            break;
        default:
            MILO_FAIL("Invalid bpp: %d", bpp);
            MILO_ASSERT(fmt != D3DFMT_UNKNOWN, 999);
            break;
        }
    }
    return result;
}

int DxRnd::BitmapOrderForD3DFormat(D3DFORMAT fmt) {
    switch (fmt) {
    case D3DFMT_DXT1:
    case D3DFMT_LIN_DXT1:
        return 8;
    case D3DFMT_DXT3:
    case D3DFMT_LIN_DXT3:
        return 0x10;
    case D3DFMT_DXT5:
    case D3DFMT_LIN_DXT5:
        return 0x18;
    case D3DFMT_DXN:
    case D3DFMT_LIN_DXN:
        return 0x20;
    default:
        return 0;
    }
}

void DxRnd::ResetDevice() {
    PreDeviceReset();
    HRESULT res = D3DDevice_Reset(mD3DDevice, &mPresentParams);
    DX_ASSERT_CODE(res, 0xD6);
    PostDeviceReset();
}

long DxRnd::GetDeviceCaps(D3DCAPS9 *cap) {
    D3DDEVTYPE deviceType = mDeviceType;
    return Direct3D_GetDeviceCaps(0, deviceType, cap);
}

void DxRnd::DrawSafeArea(float percent, bool widescreen, const Hmx::Color &color) {
    if (mShrinkToSafe)
        percent = percent * 1.0526316f;

    float realAspect = (float)mHeight / mWidth;
    float targetAspect;
    if (widescreen) {
        targetAspect = 16.f / 9.f;
    } else {
        targetAspect = 4.f / 3.f;
    }

    float v1y = (1.0f - percent) * 0.5f;
    float v1x = v1y + (1.0f - targetAspect * realAspect) * 0.5f;
    float v2y = 1.0f - v1y;
    float v2x = 1.0f - v1x;

    Vector2 vec1;
    vec1.y = v1y;
    vec1.x = v1x;

    Vector2 vec2;
    vec2.x = v2x;
    vec2.y = v2y;

    UtilDrawRect2D(vec1, vec2, color);
}
