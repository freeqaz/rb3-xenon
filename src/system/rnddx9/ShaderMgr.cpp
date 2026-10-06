
#include "ShaderMgr.h"
#include "../../Memory.h"
#include "math/Utl.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/System.h"
#include "rnddx9/Rnd.h"
#include "rnddx9/Shader.h"
#include "rnddx9/ShaderInclude.h"
#include "rndobj/BaseMaterial.h"
#include "rndobj/Mat.h"
#include "rndobj/Rnd.h"
#include "rndobj/ShaderMgr.h"
#include "rndobj/ShaderOptions.h"
#include "rndobj/ShaderProgram.h"
#include "rndobj/Tex.h"
#include "rndobj/Utl.h"
#include "utl/FileStream.h"
#include "utl/MemTrack.h"
#include "xdk/d3d9i/d3d9.h"
#include "xdk/XGRAPHICS.h"
#include "xdk/d3dx9/d3dx9mesh.h"
#include "xdk/d3dx9/d3dx9shader.h"
#include "xdk/xgraphics/xgraphics.h"

DxShaderMgr TheDxShaderMgr;
RndShaderMgr &TheShaderMgr = TheDxShaderMgr;
DxShaderInclude TheDxShaderInclude;

#pragma region DxShader

DxShader::~DxShader() {
    if (mPreCreated) {
        MILO_ASSERT(mVShader != NULL, 0x60);
        MILO_ASSERT(mPShader != NULL, 0x61);
        mVShader = nullptr;
        mPShader = nullptr;
    } else {
        DX_RELEASE(mVShader);
        DX_RELEASE(mPShader);
    }
}

void DxShader::operator delete(void *ptr) {
    // Empty - shaders are allocated from a pool in TheShaderMgr
}


void DxShader::Select(bool b1) {
    D3DDevice_SetVertexShader(TheDxRnd.Device(), mVShader);
    D3DDevice_SetPixelShader(TheDxRnd.Device(), b1 ? nullptr : mPShader);
    if (TheRnd.ResourceCached()) {
        float min, max;
        EstimatedCost(min, max);
        static float div = SystemConfig("rnd", "estimated_cost_divisor")->Float(1);
        Vector4 v;
        v.z = 0;
        v.w = 1;
        float div1 = ((min + max) / 2.0f) / div;
        float f3 = Max(0.0f, div1);
        float f2 = Max(0.0f, 1.0f - div1);
        v.x = Min(f3, 1.0f);
        v.y = Min(f2, 1.0f);
        TheShaderMgr.SetPConstant(kPS_ShaderCost, v);
    }
}

void DxShader::Copy(const RndShaderProgram &src) {
    MILO_ASSERT(mPreCreated == false, 0xA5);
    MILO_ASSERT(src.Cached(), 0xA6);
    DX_RELEASE(mVShader);
    DX_RELEASE(mPShader);
    const DxShader &dxSrc = static_cast<const DxShader &>(src);
    mVShader = dxSrc.mVShader;
    D3DResource_AddRef(mVShader);
    mPShader = dxSrc.mPShader;
    D3DResource_AddRef(mPShader);
    mMinOverall = dxSrc.mMinOverall;
    mMaxOverall = dxSrc.mMaxOverall;
}

void DxShader::EstimatedCost(float &min, float &max) {
    if (mMinOverall < 0 || mMaxOverall < 0) {
        mMinOverall = 0;
        mMaxOverall = 0;
        if (mPShader) {
            UINT sizeOfData;
            D3DPixelShader_GetFunction(mPShader, nullptr, &sizeOfData);
            if (sizeOfData != 0) {
                std::vector<char> chars(sizeOfData);
                auto it = chars.begin();
                D3DPixelShader_GetFunction(mPShader, it, &sizeOfData);
                XGIDEALSHADERCOST shaderCost;
                if (XGEstimateIdealShaderCost(it, 0, &shaderCost) == 0) {
                    mMinOverall = shaderCost.MinOverall;
                    mMaxOverall = shaderCost.MaxOverall;
                }
            }
        }
    }
    min = mMinOverall;
    max = mMaxOverall;
}

RndShaderBuffer *DxShader::NewBuffer(unsigned int ui) { return new DxShaderBuffer(ui); }

// 0x82736BE8. Each DxShaderBuffer is created just before its compile and the
// compiler writes straight into its mBuffer; both compiles share one zeroed
// parameter block with a temp-register limit of 0x24. A failure only touches
// the error text (the report is stripped), the include data is freed
// directly, and the result is !failed.
bool DxShader::Compile(
    ShaderType s, const ShaderOptions &opts, RndShaderBuffer *&buf1, RndShaderBuffer *&buf2
) {
    std::vector<ShaderMacro> defines;
    opts.GenerateMacros(s, defines);
    const char *shaderName = ShaderTypeName(s);
    LPCSTR data = nullptr;
    UINT bytes = 0;
    ID3DXInclude *include = &TheDxShaderInclude;
    if (include->Open(D3DXINC_LOCAL, shaderName, nullptr, (LPCVOID *)&data, &bytes, nullptr, 0)
        < 0) {
        return false;
    }
    const D3DXMACRO *macros = reinterpret_cast<const D3DXMACRO *>(defines.begin());
    ID3DXBuffer *vError = nullptr;
    ID3DXBuffer *pError = nullptr;
    D3DXSHADER_COMPILE_PARAMETERS params;
    memset(&params, 0, sizeof(params));
    params.TempRegisterLimit = 0x24;

    DxShaderBuffer *vBuf = new DxShaderBuffer();
    buf1 = vBuf;
    defines[0].Value = "0";
    HRESULT vRes = D3DXCompileShaderExA(
        data,
        bytes,
        macros,
        include,
        "vshader",
        "vs_3_0",
        0,
        &vBuf->mBuffer,
        &vError,
        nullptr,
        &params
    );

    DxShaderBuffer *pBuf = new DxShaderBuffer();
    buf2 = pBuf;
    defines[0].Value = "1";
    HRESULT pRes = D3DXCompileShaderExA(
        data,
        bytes,
        macros,
        include,
        "pshader",
        "ps_3_0",
        0,
        &pBuf->mBuffer,
        &pError,
        nullptr,
        &params
    );

    bool failed = vRes < 0 || pRes < 0;
    if (failed) {
        if (vRes < 0 && vError) {
            MILO_NOTIFY((char *)vError->GetBufferPointer());
        }
        if (pRes < 0 && pError) {
            MILO_NOTIFY((char *)pError->GetBufferPointer());
        }
    }
    if (vError) {
        vError->Release();
        vError = nullptr;
    }
    if (pError) {
        pError->Release();
        pError = nullptr;
    }
    MemFree((void *)data);
    return !failed;
}

void DxShader::CreateVertexShader(RndShaderBuffer &buffer) {
    MILO_ASSERT(mVShader == NULL, 0x80);
    mVShader = D3DDevice_CreateVertexShader((const DWORD *)buffer.Storage());
    DX_ASSERT(mVShader, 0x82);
}

void DxShader::CreatePixelShader(RndShaderBuffer &buffer, ShaderType) {
    MILO_ASSERT(mPShader == NULL, 0x86);
    mPShader = D3DDevice_CreatePixelShader((const DWORD *)buffer.Storage());
    DX_ASSERT(mPShader, 0x88);
}

void DxShader::SetShaders(D3DVertexShader *v, D3DPixelShader *p) {
    if (mCached) {
        MILO_ASSERT(mPreCreated, 0x92);
        MILO_ASSERT(mVShader, 0x93);
        MILO_ASSERT(mPShader, 0x94);
    } else {
        MILO_ASSERT(mVShader == NULL, 0x99);
        MILO_ASSERT(mPShader == NULL, 0x9A);
        MILO_ASSERT(mPreCreated == false, 0x9B);
        mVShader = v;
        mPShader = p;
        mCached = true;
        mPreCreated = true;
    }
}

#pragma endregion
#pragma region DxShaderMgr

void DxShaderMgr::PreInit() {
    mShaderSize = 0x38;
    RndShaderMgr::PreInit();
    RELEASE(mWorkMat);
    mWorkMat = Hmx::Object::New<RndMat>();
    RELEASE(mPostProcMat);
    mPostProcMat = Hmx::Object::New<RndMat>();
    RELEASE(mDrawHighlightMat);
    mDrawHighlightMat = Hmx::Object::New<RndMat>();
    mDrawHighlightMat->SetUseEnv(false);
    mDrawHighlightMat->SetZMode(kZModeForce);
    mDrawHighlightMat->SetBlend(RndMat::kBlendSrc);
    mDrawHighlightMat->SetAlphaCut(false);
    RELEASE(mDrawRectMat);
    mDrawRectMat = Hmx::Object::New<RndMat>();
    mDrawRectMat->SetZMode(kZModeDisable);
    mDrawRectMat->SetUseEnv(false);
    mDrawRectMat->SetPreLit(true);
    mDrawRectMat->SetBlend(RndMat::kBlendSrcAlpha);
    mDrawRectMat->SetAlphaCut(false);
}

void DxShaderMgr::Terminate() {
    RELEASE(mDrawHighlightMat);
    RELEASE(mDrawRectMat);
    RELEASE(mWorkMat);
    RELEASE(mPostProcMat);
    RndShaderMgr::Terminate();
}

/** The dirty bit(s) the GPU command buffer needs when `count` float4 constants
 * starting at register `reg` are written straight into D3DDevice::m_Constants.
 * Each mask bit covers a block of four registers, so a write spanning more than
 * one block sets more than one bit -- hence the arithmetic shift, which smears
 * the top bit down across `span` extra blocks before the logical shift moves it
 * to `start`. */
static inline UINT64 ShaderConstantDirtyMask(unsigned int reg, unsigned int count) {
    unsigned int start = reg >> 2;
    unsigned int span = ((reg + count - 1) >> 2) - start;
    return (UINT64)((INT64)0x8000000000000000 >> span) >> start;
}

// 0x82735e50 (DxShaderMgr vtable slot 8).
void DxShaderMgr::SetVConstant(
    VShaderConstant vsc, const float *__restrict data, unsigned int count
) {
    unsigned int start = (unsigned int)vsc >> 2;
    unsigned int span = ((vsc + count - 1) >> 2) - start;
    UINT64 mask = (UINT64)((INT64)0x8000000000000000 >> span) >> start;
    D3DDevice_SetVertexShaderConstantFN(TheDxRnd.Device(), vsc, data, count, mask);
}

// 0x82735c30 (vtable slot 10) / 0x82735ec8 (slot 17).
void DxShaderMgr::SetVConstant(VShaderConstant vsc, int i) {
    D3DDevice_SetVertexShaderConstantI(TheDxRnd.Device(), vsc, &i, 1);
}

void DxShaderMgr::SetPConstant(PShaderConstant psc, int i) {
    D3DDevice_SetPixelShaderConstantI(TheDxRnd.Device(), psc, &i, 1);
}

// 0x82735c68 (vtable slot 9) / 0x82735f00 (slot 16): the four floats are
// written straight into m_Constants and the block's dirty bit is or'd into
// m_Pending.m_Mask[0] (vertex) / [1] (pixel).
void DxShaderMgr::SetVConstant(VShaderConstant vsc, const Vector4 &v) {
    D3DDevice *dev = TheDxRnd.Device();
    float x = v.x, y = v.y, z = v.z, w = v.w;
    float *dst = (float *)&dev->m_Constants.VertexShaderF[vsc];
    int start = (unsigned int)vsc >> 2;
    dev->m_Pending.m_Mask[0] |= (UINT64)0x8000000000000000 >> start;
    dst[0] = x;
    dst[1] = y;
    dst[2] = z;
    dst[3] = w;
}

void DxShaderMgr::SetPConstant(PShaderConstant psc, const Vector4 &v) {
    D3DDevice *dev = TheDxRnd.Device();
    float x = v.x, y = v.y, z = v.z, w = v.w;
    float *dst = (float *)&dev->m_Constants.PixelShaderF[psc];
    int start = (unsigned int)psc >> 2;
    dev->m_Pending.m_Mask[1] |= (UINT64)0x8000000000000000 >> start;
    dst[0] = x;
    dst[1] = y;
    dst[2] = z;
    dst[3] = w;
}

/** Shader constants are column-major, Hmx::Matrix4 is row-major, so every
 * upload here is a transpose. 4x3 drops the fourth column.
 * 0x82735da0 (slot 12) / 0x82736038 (slot 19). */
void DxShaderMgr::SetVConstant4x3(VShaderConstant vsc, const Hmx::Matrix4 &mtx) {
    D3DDevice *dev = TheDxRnd.Device();
    float c00 = mtx.x.x, c01 = mtx.y.x, c02 = mtx.z.x, c03 = mtx.w.x;
    float c10 = mtx.x.y, c11 = mtx.y.y, c12 = mtx.z.y, c13 = mtx.w.y;
    float c20 = mtx.x.z, c21 = mtx.y.z, c22 = mtx.z.z, c23 = mtx.w.z;
    float *__restrict dst = (float *)&dev->m_Constants.VertexShaderF[vsc];
    dev->m_Pending.m_Mask[0] |= ShaderConstantDirtyMask(vsc, 3);
    dst[0] = c00;
    dst[1] = c01;
    dst[2] = c02;
    dst[3] = c03;
    dst[4] = c10;
    dst[5] = c11;
    dst[6] = c12;
    dst[7] = c13;
    dst[8] = c20;
    dst[9] = c21;
    dst[10] = c22;
    dst[11] = c23;
}

void DxShaderMgr::SetPConstant4x3(PShaderConstant psc, const Hmx::Matrix4 &mtx) {
    D3DDevice *dev = TheDxRnd.Device();
    float c00 = mtx.x.x, c01 = mtx.y.x, c02 = mtx.z.x, c03 = mtx.w.x;
    float c10 = mtx.x.y, c11 = mtx.y.y, c12 = mtx.z.y, c13 = mtx.w.y;
    float c20 = mtx.x.z, c21 = mtx.y.z, c22 = mtx.z.z, c23 = mtx.w.z;
    float *__restrict dst = (float *)&dev->m_Constants.PixelShaderF[psc];
    dev->m_Pending.m_Mask[1] |= ShaderConstantDirtyMask(psc, 3);
    dst[0] = c00;
    dst[1] = c01;
    dst[2] = c02;
    dst[3] = c03;
    dst[4] = c10;
    dst[5] = c11;
    dst[6] = c12;
    dst[7] = c13;
    dst[8] = c20;
    dst[9] = c21;
    dst[10] = c22;
    dst[11] = c23;
}

// 0x82735cc0 (slot 6) / 0x82735f58 (slot 13).
void DxShaderMgr::SetVConstant(VShaderConstant vsc, const Hmx::Matrix4 &mtx) {
    D3DDevice *dev = TheDxRnd.Device();
    float c00 = mtx.x.x, c01 = mtx.y.x, c02 = mtx.z.x, c03 = mtx.w.x;
    float c10 = mtx.x.y, c11 = mtx.y.y, c12 = mtx.z.y, c13 = mtx.w.y;
    float c20 = mtx.x.z, c21 = mtx.y.z, c22 = mtx.z.z, c23 = mtx.w.z;
    float c30 = mtx.x.w, c31 = mtx.y.w, c32 = mtx.z.w, c33 = mtx.w.w;
    float *__restrict dst = (float *)&dev->m_Constants.VertexShaderF[vsc];
    dev->m_Pending.m_Mask[0] |= ShaderConstantDirtyMask(vsc, 4);
    dst[0] = c00;
    dst[1] = c01;
    dst[2] = c02;
    dst[3] = c03;
    dst[4] = c10;
    dst[5] = c11;
    dst[6] = c12;
    dst[7] = c13;
    dst[8] = c20;
    dst[9] = c21;
    dst[10] = c22;
    dst[11] = c23;
    dst[12] = c30;
    dst[13] = c31;
    dst[14] = c32;
    dst[15] = c33;
}

void DxShaderMgr::SetPConstant(PShaderConstant psc, const Hmx::Matrix4 &mtx) {
    D3DDevice *dev = TheDxRnd.Device();
    float c00 = mtx.x.x, c01 = mtx.y.x, c02 = mtx.z.x, c03 = mtx.w.x;
    float c10 = mtx.x.y, c11 = mtx.y.y, c12 = mtx.z.y, c13 = mtx.w.y;
    float c20 = mtx.x.z, c21 = mtx.y.z, c22 = mtx.z.z, c23 = mtx.w.z;
    float c30 = mtx.x.w, c31 = mtx.y.w, c32 = mtx.z.w, c33 = mtx.w.w;
    float *__restrict dst = (float *)&dev->m_Constants.PixelShaderF[psc];
    dev->m_Pending.m_Mask[1] |= ShaderConstantDirtyMask(psc, 4);
    dst[0] = c00;
    dst[1] = c01;
    dst[2] = c02;
    dst[3] = c03;
    dst[4] = c10;
    dst[5] = c11;
    dst[6] = c12;
    dst[7] = c13;
    dst[8] = c20;
    dst[9] = c21;
    dst[10] = c22;
    dst[11] = c23;
    dst[12] = c30;
    dst[13] = c31;
    dst[14] = c32;
    dst[15] = c33;
}

// 0x82736158 (slot 14). With no cube texture retail unbinds the sampler
// (tail-call into D3DDevice_SetTexture with a null texture); it does NOT
// fall back to Rnd's null texture the way SetPConstant(RndTex *) does.
void DxShaderMgr::SetPConstant(PShaderConstant psc, RndCubeTex *tex) {
    if (tex) {
        tex->Select(psc);
    } else {
        D3DDevice_SetTexture(
            TheDxRnd.Device(), psc, nullptr, 0x8000000000000000 >> (psc + 0x20U)
        );
    }
}

void DxShaderMgr::SetVConstant(VShaderConstant vsc, RndTex *tex) {
    if (tex) {
        tex->Select(vsc);
    } else {
        D3DDevice_SetTexture(
            TheDxRnd.Device(), vsc, nullptr, 0x8000000000000000 >> (vsc + 0x20U)
        );
    }
}

// ★ This body was MISSING entirely (declared in both ShaderMgr.h's, defined
// nowhere), and the gap was CONCEALED by a wrong map name: retail 0x82735bf0
// was pinned as SetPConstant(bool), so our SetPConstant(bool) scored 100%
// against the VERTEX function.  It read 100% only because the callee is an
// unnamed placeholder, which name_check forgives -- objdiff is structurally
// unable to tell these two apart.  Settled on retail bytes: 0x82735bf0 and
// 0x82735e88 are byte-identical except their `bl`, and the two callees differ
// in exactly one instruction, `addi r11, r11, 0x9e0` vs `0x9e4`.  The final
// index is ((StartRegister>>5) + K)*4, so K=0x9e0 -> 0x2780 == m_Constants
// (0x480) + VertexShaderB (0x2300), K=0x9e4 -> 0x2790 == PixelShaderB (0x2310).
// => 0x8285d248 is SetVertexShaderConstantB, so slot 11 is the V overload.
void DxShaderMgr::SetVConstant(VShaderConstant vsc, bool b) {
    BOOL val = b;
    D3DDevice_SetVertexShaderConstantB(TheDxRnd.Device(), vsc, &val, 1);
}
void DxShaderMgr::SetPConstant(PShaderConstant psc, bool b) {
    BOOL val = b;
    D3DDevice_SetPixelShaderConstantB(TheDxRnd.Device(), psc, &val, 1);
}

void DxShaderMgr::SetPConstant(PShaderConstant psc, RndTex *tex) {
    if (!tex) {
        tex = TheRnd.GetNullTexture();
    }
    if (tex) {
        tex->Select(psc);
    } else {
        D3DDevice_SetTexture(
            TheDxRnd.Device(), psc, nullptr, 0x8000000000000000 >> (psc + 0x20U)
        );
    }
}

void DxShaderMgr::LoadShaderFile(FileStream &fs) {
    RndSplasherResume();
    PhysMemTypeTracker tracker("D3D(phys):ShaderCache");
    unsigned int fileType, fileVersion;
    fs >> fileType;
    fs >> fileVersion;
    if (fileType == XBOX_SHADERS_TYPE && fileVersion == XBOX_SHADERS_VERSION) {
        unsigned int num;
        fs >> num;
        for (unsigned int i = 0; i < num; i++) {
            Symbol name;
            fs >> name;
            ShaderType shaderType = ShaderTypeFromName(name.Str());
            unsigned int alloc;
            fs >> alloc;
            // TWO arrays of 2, not one of 4: retail (0x827366F0) holds their
            // two addresses in separate registers and indexes both with the
            // SAME scaled counter -- `addi r10, r31, 0x80` / `addi r8, r31, 0x88`
            // then `lwzx r10, r29, r10` / `lwzx r8, r29, r8`. A single
            // bases[4] indexed by k and k+2 gives one base register.
            void *bases[2];
            void *physBases[2];
            bases[0] = nullptr;
            bases[1] = nullptr;
            physBases[0] = nullptr;
            physBases[1] = nullptr;
            for (unsigned int j = 0; j < 2; j++) {
                SIZE_T size1, size2;
                fs >> size1;
                fs >> size2;
                bases[j] = XMemAlloc(size1, 0x20800000);
                physBases[j] = XMemAlloc(size2, 0xB5800000);
                fs.Read(bases[j], size1);
                fs.Read(physBases[j], size2);
            }
            ShaderPoolAlloc(alloc);
            RndSplasherSuspend();
            for (unsigned int j = 0; j < alloc; j++) {
                u64 shaderOptsMask;
                fs >> shaderOptsMask;
                D3DPixelShader *pPS = nullptr;
                D3DVertexShader *pVS = nullptr;
                // k is unsigned: the loop bound is `cmplwi cr6, r30, 0x8`.
                for (unsigned int k = 0; k < 2; k++) {
                    unsigned int ic0;
                    unsigned int ibc;
                    fs >> ic0;
                    fs >> ibc;
                    void *addr = (void *)((unsigned int)bases[k] + ic0);
                    void *physAddr = (void *)((unsigned int)physBases[k] + ibc);
                    // `!(k - 1)`, i.e. the SECOND record is the vertex shader.
                    // Retail builds the condition as a boolean VALUE --
                    // `subi r7, r28, 0x1` / `cntlzw r7, r7` /
                    // `extrwi. r7, r7, 1, 26` -- which is 1 exactly when
                    // k - 1 == 0, and `beq` branches to the pixel-shader arm
                    // otherwise. A bare `if (k - 1)` selects the OPPOSITE arm
                    // (record 0 registered as the vertex shader).
                    bool isVertexShader = !(k - 1);
                    if (isVertexShader) {
                        pVS = (D3DVertexShader *)addr;
                        XGRegisterVertexShader(pVS, physAddr);
                    } else {
                        pPS = (D3DPixelShader *)addr;
                        XGRegisterPixelShader(pPS, physAddr);
                    }
                }
                MILO_ASSERT(pPS != NULL, 0x1FA);
                MILO_ASSERT(pVS != NULL, 0x1FB);
                DxShader &shader =
                    static_cast<DxShader &>(FindShader(shaderType, shaderOptsMask));
                shader.SetShaders(pVS, pPS);
                RndSplasherPoll();
            }
            RndSplasherResume();
        }
    }
    RndSplasherSuspend();
}

RndShaderProgram *DxShaderMgr::NewShaderProgram() { return new DxShader(); }

// 0x82736130: binds a raw D3D texture to a sampler (DxRnd::InitRenderState
// binds the colour-ramp texture to sampler 15 through it).
void DxShaderMgr::SetTexture(int sampler, D3DBaseTexture *tex) {
    D3DDevice_SetTexture(
        TheDxRnd.Device(), sampler, tex, 0x8000000000000000 >> (sampler + 0x20U)
    );
}

// W16-A scatter-include (default/system/rnddx9/ShaderMgr <- rnddx9/Tex.cpp).
// Tex.cpp was in-tree but wired NOWHERE: absent from objects.json and included
// by no compiled TU, so the match build emitted no DxTex bodies at all and six
// named rows in this unit read fuzzy 0 for want of a definition to pair with.
// Retail puts them in THIS unit's span -- every DxTex address the map names
// (0x82734148 DoCompress, 0x82734360 Compress, 0x827343B0 TexelsPitch,
// 0x827347D0 StaticClassName, 0x82734848 ClassName, 0x827349D8 GetRT,
// 0x827350E8 SetDeviceTex) falls inside the pinned .text 0x82733CB0-0x82736EE8.
#include "rnddx9/Tex.cpp"
