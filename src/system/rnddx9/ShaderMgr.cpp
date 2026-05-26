
#include "ShaderMgr.h"
#include "Memory.h"
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
DxShaderInclude &TheDxShaderInclude = DxShaderInclude();

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

bool DxShader::Compile(
    ShaderType s, const ShaderOptions &opts, RndShaderBuffer *&buf1, RndShaderBuffer *&buf2
) {
    std::vector<ShaderMacro> defines;
    opts.GenerateMacros(s, defines);
    const char *shaderName = ShaderTypeName(s);
    MILO_ASSERT(streq("PIXEL_SHADER", defines[0].Name), 0xBB);
    MILO_ASSERT(!mVShader, 0xBD);
    MILO_ASSERT(!mPShader, 0xBE);

    LPCSTR data = nullptr;
    UINT bytes = 0;
    if (TheDxShaderInclude.Open(
            D3DXINC_LOCAL, shaderName, nullptr, (LPCVOID *)&data, &bytes, nullptr, 0
        )
        < 0) {
        return false;
    }

    buf1 = new DxShaderBuffer();
    buf2 = new DxShaderBuffer();

    defines[0].Value = "0";
    ID3DXBuffer *vShader = nullptr;
    ID3DXBuffer *vError = nullptr;
    HRESULT vRes = D3DXCompileShaderExA(
        data,
        bytes,
        reinterpret_cast<const D3DXMACRO *>(defines.begin()),
        &TheDxShaderInclude,
        "vshader",
        "vs_3_0",
        0,
        vShader,
        vError,
        nullptr,
        nullptr
    );

    defines[0].Value = "1";
    ID3DXBuffer *pShader = nullptr;
    ID3DXBuffer *pError = nullptr;
    HRESULT pRes = D3DXCompileShaderExA(
        data,
        bytes,
        reinterpret_cast<const D3DXMACRO *>(defines.begin()),
        &TheDxShaderInclude,
        "pshader",
        "ps_3_0",
        0,
        pShader,
        pError,
        nullptr,
        nullptr
    );

    if (vRes < 0 || pRes < 0) {
        if (vRes < 0) {
            if (vError == nullptr) {
                TheDebug.Notify(MakeString("VShader '%s' compile failure: %d", shaderName, vRes));
            } else {
                TheDebug.Notify((char *)vError->GetBufferPointer());
            }
        }
        if (pRes < 0) {
            if (pError == nullptr) {
                TheDebug.Notify(MakeString("PShader '%s' compile failure: %d", shaderName, pRes));
            } else {
                TheDebug.Notify((char *)pError->GetBufferPointer());
            }
        }
    }

    if (vError != nullptr) {
        vError->Release();
        vError = nullptr;
    }
    if (pError != nullptr) {
        pError->Release();
        pError = nullptr;
    }

    TheDxShaderInclude.Close(data);

    return (vRes >= 0) && (pRes >= 0);
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
    CreateAndSetMetaMat(mWorkMat);
    RELEASE(mPostProcMat);
    mPostProcMat = Hmx::Object::New<RndMat>();
    CreateAndSetMetaMat(mPostProcMat);
    RELEASE(mDrawHighlightMat);
    mDrawHighlightMat = Hmx::Object::New<RndMat>();
    mDrawHighlightMat->SetUseEnv(false);
    mDrawHighlightMat->SetZMode(kZModeForce);
    mDrawHighlightMat->SetBlend(BaseMaterial::kBlendSrc);
    mDrawHighlightMat->SetAlphaCut(false);
    CreateAndSetMetaMat(mDrawHighlightMat);
    RELEASE(mDrawRectMat);
    mDrawRectMat = Hmx::Object::New<RndMat>();
    mDrawRectMat->SetZMode(kZModeDisable);
    mDrawRectMat->SetUseEnv(false);
    mDrawRectMat->SetPreLit(true);
    mDrawRectMat->SetBlend(BaseMaterial::kBlendSrcAlpha);
    mDrawRectMat->SetAlphaCut(false);
    CreateAndSetMetaMat(mDrawRectMat);
}

void DxShaderMgr::Terminate() {
    RELEASE(mDrawHighlightMat);
    RELEASE(mDrawRectMat);
    RELEASE(mWorkMat);
    RELEASE(mPostProcMat);
    RndShaderMgr::Terminate();
}

void DxShaderMgr::SetVConstant(VShaderConstant, const float *, unsigned int) {}

void DxShaderMgr::SetVConstant(VShaderConstant vsc, RndTex *tex) {
    if (tex) {
        tex->Select(vsc);
    } else {
        D3DDevice_SetTexture(
            TheDxRnd.Device(), vsc, nullptr, 0x8000000000000000 >> (vsc + 0x20U)
        );
    }
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
            void *bases[4];
            bases[0] = nullptr;
            bases[1] = nullptr;
            bases[2] = nullptr;
            bases[3] = nullptr;
            for (unsigned int j = 0; j < 2; j++) {
                SIZE_T size1, size2;
                fs >> size1;
                fs >> size2;
                BeginMemTrackFileName(fs.Name());
                bases[j] = XMemAlloc(size1, 0x20800000);
                bases[j + 2] = XMemAlloc(size2, 0xB5800000);
                EndMemTrackFileName();
                fs.Read(bases[j], size1);
                fs.Read(bases[j + 2], size2);
            }
            ShaderPoolAlloc(alloc);
            RndSplasherSuspend();
            for (unsigned int j = 0; j < alloc; j++) {
                u64 shaderOptsMask;
                fs >> shaderOptsMask;
                D3DPixelShader *pPS = nullptr;
                D3DVertexShader *pVS = nullptr;
                for (int k = 0; k < 2; k++) {
                    unsigned int ic0;
                    unsigned int ibc;
                    fs >> ic0;
                    fs >> ibc;
                    void *addr = (void *)((unsigned int)bases[k] + ic0);
                    void *physAddr = (void *)((unsigned int)bases[k + 2] + ibc);
                    if (k - 1) {
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
