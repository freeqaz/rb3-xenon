#include "Cam.h"
#include "Env.h"
#include "Lit.h"
#include "Mat.h"
#include "../../Memory.h"
#include "Mesh.h"
#include "Movie.h"
#include "MultiMesh.h"
#include "Part.h"
#include "RenderState.h"
#include "Tex.h"
#include "TexRenderer.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/System.h"
#include "os/Timer.h"
#include "rnddx9/CubeTex.h"
#include "rnddx9/OcclusionQueryMgr.h"
#include "rnddx9/Rnd.h"
#include "rndobj/Cam.h"
#include "rndobj/DOFProc_NG.h"
#include "rndobj/Flare.h"
#include "rndobj/HiResScreen.h"
#include "rndobj/Mat_NG.h"
#include "rndobj/Overlay.h"
#include "rndobj/PostProc.h"
#include "rndobj/PostProc_NG.h"
#include "rndobj/Rnd.h"
#include "rndobj/Rnd_NG.h"
#include "rndobj/Shader.h"
#include "rndobj/ShaderMgr.h"
#include "rndobj/ShadowMap.h"
#include "rndobj/Stats_NG.h"
#include "rndobj/Tex.h"
#include "utl/MemTrack.h"
#include "utl/Option.h"
#include "xdk/D3D9.h"
#include "xdk/d3d9i/d3d9.h"
#include "xdk/d3d9i/d3d9caps.h"
#include "xdk/d3d9i/d3d9types.h"
#include "xdk/win_types.h"
#include "xdk/xapilibi/processthreadsapi.h"
#include "xdk/xapilibi/xbase.h"
#include "xdk/xapilibi/xbox.h"

void CreateBackBuffers(int, int, D3DMULTISAMPLE_TYPE, unsigned int &, unsigned int &, D3DSurface *&, D3DSurface *&);

DxRnd::DxRnd()
    : mInited(0),
      mD3DDevice(nullptr),
      mFocusWindow(0),
      mDeviceType(D3DDEVTYPE_HAL),
      mReverseZ(1),
      mAsyncSwapNext(false),
      mAsyncSwapCurrent(false),
      mPerfCounterStart(nullptr),
      mPerfCounterEnd(nullptr),
      mGPUTimer(nullptr),
      mGPUBusyMs(0.0f),
      mGPUCountMs(0.0f),
      mCreatedPerfCounters(false),
      mPostProcDone(false),
      mSuspended(false),
      mPIXCaptureState(false),
      mPreInited(false) {
    mInited = 1;
    for (int i = 0; i < 2; i++)
        mFrontBuffers[i] = nullptr;
    mBackBuffer = nullptr;
    mWorldDepth = nullptr;
    mOffscreenRT = nullptr;
    mOffscreenDepth = nullptr;
    mFlags = 0;
    mColorRampTex = nullptr;
    mFrontBufIdx = 0;
    mNumTiles = 0;
    unk34d = true;
}

DxRnd::~DxRnd() {
    if ((unsigned int)mInited) {
        mInited = 0;
    }
}

void CDError() {
    TheDxRnd.Suspend();
    ShowDirtyDiscError();
}

void DxModal(Debug::ModalType &t, FixedString &s, bool b) { TheDxRnd.Modal(t, s, b); }

void DxRnd::PreInit(HWND__ *) {
    if (!mPreInited) {
        mPreInited = true;
        DataArray *cfg = SystemConfig("rnd");
        mDefaultVSRegAlloc = 32;
        mDefaultPSRegAlloc = 96;
        DataArray *gprArr = cfg->FindArray("shader_gpr_alloc", false);
        if (gprArr) {
            mDefaultVSRegAlloc = gprArr->Int(1);
            mDefaultPSRegAlloc = gprArr->Int(2);
        }
        MILO_ASSERT(mDefaultVSRegAlloc + mDefaultPSRegAlloc == GPU_GPRS, 0x1F0);
        MILO_ASSERT(mDefaultVSRegAlloc >= 16, 0x1F1);
        MILO_ASSERT(mDefaultPSRegAlloc >= 16, 0x1F2);
        SetDiskErrorCallback(CDError);
        mPrintGlitches = OptionBool("print_glitches", false);
        mCaptureNextFrame = false;
        mD3DDevice = nullptr;
        mFocusWindow = 0;
        NgRnd::PreInit();
        InitBuffers();
        TheShaderMgr.PreInit();
        TheRenderState.Init();
        Suspend();
        REGISTER_OBJ_FACTORY(DxTexRenderer)
        REGISTER_OBJ_FACTORY(DxCam)
        REGISTER_OBJ_FACTORY(DxEnviron);
        REGISTER_OBJ_FACTORY(DxMesh)
        DxMat::Init();
        REGISTER_OBJ_FACTORY(DxTex)
        REGISTER_OBJ_FACTORY(DxCubeTex);
        DxMultiMesh::Init();
        REGISTER_OBJ_FACTORY(DxMovie)
        DxParticleSys::Init();
        DxLight::Init();
        CreatePostTextures();
        DxTex::SetEDRamChecksEnabled(false);
        NgPostProc::Init();
        NgDOFProc::Init();
        DxTex::SetEDRamChecksEnabled(true);
        RndShadowMap::Init();
        Rnd::CreateDefaults();
        TheDebug.SetModalCallback(DxModal);
    }
}

void DxRnd::Init(HWND__ *h) {
    PreInit(h);
    mOcclusionQueryMgr = new DxRndOcclusionQueryMgr();
    NgRnd::Init();
    mAsyncSwapNext = false;
}

void DxRnd::Terminate() {
    Resume();
    DxLight::Terminate();
    DxMultiMesh::Shutdown();
    NgPostProc::Terminate();
    NgRnd::Terminate();
    RELEASE(mPreProcessTex);
    RELEASE(mPostProcessTex);
    RELEASE(mPreDepthTex);
    TerminateBuffers();
}

void DxRnd::SetSync(int sync) {
    Rnd::SetSync(sync);
    Resume();
    if (mSync == 0) {
        D3DDevice_SetRenderState_PresentInterval(TheDxRnd.Device(), 0x80000000);
    } else if (mSync == 1) {
        D3DDevice_SetRenderState_PresentInterval(TheDxRnd.Device(), 1);
    } else if (mSync == 2) {
        D3DDevice_SetRenderState_PresentInterval(TheDxRnd.Device(), 2);
    } else {
        MILO_FAIL("Not allowed to sync %d\n", mSync);
    }
}

void DxRnd::SetAspect(Aspect a) {
    if (mAspect != a) {
        Rnd::SetAspect(a);
        UpdateScalerParams();
        ResetDevice();
    }
}

void DxRnd::SetShrinkToSafeArea(bool shrink) {
    if (shrink != mShrinkToSafe) {
        Rnd::SetShrinkToSafeArea(shrink);
        UpdateScalerParams();
        ResetDevice();
    }
}

void DxRnd::DoWorldEnd() {
    if (mProcCmds & kProcessWorld) {
        Rnd::DoWorldEnd();
        {
            START_AUTO_TIMER("draw");
            DoPointTests();
        }
        SavePreBuffer();
    }
}

void DxRnd::FinishPostProcess() {
    SetFrameBuffersAsSource();
    D3DDevice_SetSamplerState_MinFilter(TheDxRnd.Device(), 3, 1);
    D3DDevice_SetSamplerState_MagFilter(TheDxRnd.Device(), 3, 1);
    D3DDevice_SetSamplerState_MinFilter(TheDxRnd.Device(), 0xA, 1);
    D3DDevice_SetSamplerState_MagFilter(TheDxRnd.Device(), 0xA, 1);
    D3DDevice_SetSamplerState_MinFilter(TheDxRnd.Device(), 0xF, 1);
    D3DDevice_SetSamplerState_MagFilter(TheDxRnd.Device(), 0xF, 1);
    D3DDevice_SetSamplerState_MinFilter(TheDxRnd.Device(), 0xD, 1);
    D3DDevice_SetSamplerState_MagFilter(TheDxRnd.Device(), 0xD, 1);
    D3DDevice_SetRenderTarget_External(mD3DDevice, 0, mBackBuffer);
    D3DDevice_SetDepthStencilSurface(mD3DDevice, mWorldDepth);
    D3DDevice_Clear(
        mD3DDevice, 0, nullptr, 0x31, MakeColor(Hmx::Color(0, 0, 0.3f)), 0, 0, 0
    );
    Hmx::Rect rect(0, 0, (float)mWidth, (float)mHeight);
    RndMat *mat = TheShaderMgr.GetPostProcMat();
    mat->SetBlend((RndMat::Blend)1);
    mat->SetZMode((ZMode)0);
    TheShaderMgr.unk30 = 0;
    DrawRect(rect, mat, (ShaderType)0x10, Hmx::Color(), nullptr, nullptr);
    SavePostBuffer();
}

void DxRnd::CopyPostProcess() {
    if (mRegAlloc != 2) {
        mRegAlloc = (RegisterAlloc)2;
        D3DDevice_SetShaderGPRAllocation(mD3DDevice, 0, 0x10, 0x70);
    }
    Hmx::Rect rect(0, 0, (float)mWidth, (float)mHeight);
    RndMat *mat = TheShaderMgr.GetPostProcMat();
    mat->SetBlend((RndMat::Blend)1);
    mat->SetZMode((ZMode)0);
    TheShaderMgr.unk30 = 1;
    static bool sCopyPostInited;
    if (sCopyPostInited) {
        sCopyPostInited = true;
        D3DDevice_SetSamplerState_MinFilter(TheDxRnd.Device(), 0xE, 1);
        D3DDevice_SetSamplerState_MagFilter(TheDxRnd.Device(), 0xE, 1);
    }
    DrawRect(rect, mat, (ShaderType)0x10, Hmx::Color(), nullptr, nullptr);
    if (mRegAlloc != 1) {
        mRegAlloc = (RegisterAlloc)1;
        D3DDevice_SetShaderGPRAllocation(mD3DDevice, 0, 0x20, 0x60);
    }
}

void DxRnd::DoPostProcess() {
    SetFrameBuffersAsSource();
    if (mProcCmds & kProcessPost) {
        if (mRegAlloc != 2) {
            mRegAlloc = (RegisterAlloc)2;
            D3DDevice_SetShaderGPRAllocation(mD3DDevice, 0, 0x10, 0x70);
        }
        NgRnd::DoPostProcess();
        FinishPostProcess();
    }
    D3DDevice_SetRenderTarget_External(mD3DDevice, 0, mOffscreenRT);
    D3DDevice_SetDepthStencilSurface(mD3DDevice, mOffscreenDepth);
    BeginTiling(Hmx::Color(0, 0, 0.3), 0, 0);
    CopyPostProcess();
    if (mRegAlloc != 1) {
        mRegAlloc = (RegisterAlloc)1;
        // retail hardcodes the default GPR split here; DC3 reads mDefaultVSRegAlloc/mDefaultPSRegAlloc
        D3DDevice_SetShaderGPRAllocation(mD3DDevice, 0, 0x20, 0x60);
    }
    mPostProcDone = true;
}

void DxRnd::Suspend() {
    if (!mD3DDevice || mAsyncSwapCurrent) {
        return;
    }
    MILO_ASSERT(!mDrawing, 0x695);
    if (!mSuspended) {
        static Timer *cpuTimer = AutoTimer::GetTimer("cpu");
        if (mPrintGlitches && cpuTimer->SplitMs() > 30.0f) {
            MILO_LOG("GLITCH (pre-suspend): %i ms\n", (int)cpuTimer->SplitMs());
        }
        mAsyncSwapNext = false;
        D3DDevice_Suspend(mD3DDevice);
    }
    mSuspended = true;
}

void DxRnd::Resume() {
    if ((int)mD3DDevice) {
        if (mSuspended) {
            MILO_ASSERT(mAsyncSwapCurrent == false, 0x6AE);
            D3DDevice_Resume(mD3DDevice);
            mAsyncSwapNext = false;
        }
        mSuspended = false;
    }
}

D3DSurface *DxRnd::BackBuffer() const {
    D3DResource_AddRef(mBackBuffer);
    return mBackBuffer;
}

D3DTexture *DxRnd::FrontBuffer() { return mFrontBuffers[mFrontBufIdx - 1 & 1]; }
D3DTexture *DxRnd::NotFrontBuffer() { return mFrontBuffers[mFrontBufIdx]; }

const char *DxRnd::Error(long code) { return MakeString("code %d", code); }

void DxRnd::Present() {
    mFrontBufIdx = (mFrontBufIdx - 1) & 1;
    if (mAsyncSwapCurrent) {
        static D3DSWAP_STATUS swapStatus;
        while (D3DDevice_QuerySwapStatus(mD3DDevice, &swapStatus),
               swapStatus.EnqueuedCount != 0) {
            Sleep(0);
        }
    } else {
        D3DDevice_SynchronizeToPresentationInterval(mD3DDevice);
    }
    D3DDevice_Swap(mD3DDevice, NotFrontBuffer(), nullptr);
    if (mAsyncSwapCurrent != mAsyncSwapNext) {
        mAsyncSwapCurrent = mAsyncSwapNext;
        D3DDevice_BlockUntilIdle(mD3DDevice);
        D3DDevice_SetSwapMode(mD3DDevice, mAsyncSwapCurrent);
    }
    // retail normalizes with subic/subfe; we emit extrwi (boolean-negation residual, see
    // docs/decomp/patterns/INDEX.md "Boolean Negation") — semantically identical.
    // Tried (PIX()&2)!=0 and ?true:false: both still extrwi. The target masks the bit
    // in place (rlwinm r3,0,30,30) then normalizes; no source form reproduces that
    // without changing surrounding codegen. Documented unfixable boundary.
    mPIXCaptureState = PIXGetCaptureState() & 2;
}

void DxRnd::UpdateScalerParams() {
    float width = (float)mVideoMode.dwDisplayWidth;
    float height = (float)mVideoMode.dwDisplayHeight;
    bool letterbox = mAspect == kLetterbox && !mLowRes;
    // Letterbox: constrain to 16:9 aspect ratio (9/16 = 0.5625)
    if (letterbox && width * 0.5625f < height) {
        height = width * 0.5625f;
    }
    // Shrink to safe area: 95% of display dimensions
    if (mShrinkToSafe) {
        width *= 0.95f;
        if (!letterbox) {
            height *= 0.95f;
        }
    }
    D3DVIDEO_SCALER_PARAMETERS& scaler = mPresentParams.VideoScalerParameters;
    scaler.ScaledOutputWidth = width;
    scaler.ScaledOutputHeight = height;
}

void DxRnd::TerminateBuffers() {
    PreDeviceReset();
    if (mD3DDevice) {
        D3DDevice_Release(mD3DDevice);
        mD3DDevice = nullptr;
    }
}

void DxRnd::SetupGamma() {
    DataArray *cfg = SystemConfig("rnd");
    float gamma;
    if (cfg->FindData("gamma", gamma, false)) {
        D3DGAMMARAMP ramp;
        for (unsigned short i = 0; i < 0x100; i++) {
            float fval = (float)(int)i * 0.00390625f;
            float fpow = std::pow(fval, gamma);
            unsigned short ival = (unsigned short)(fpow * 1024.0f);
            unsigned short usVal = ival * 64;
            ramp.red[i] = usVal;
            ramp.green[i] = usVal;
            ramp.blue[i] = usVal;
        }
        D3DDevice_SetGammaRamp(mD3DDevice, 0, &ramp);
    }
}

void DxRnd::SetDefaultRenderStates() {
    D3DCAPS9 caps;
    memset(&caps, 0, sizeof(D3DCAPS9));
    GetDeviceCaps(&caps);
    D3DDevice_SetRenderState_AlphaRef(TheDxRnd.Device(), 0);
    D3DDevice_SetRenderState_AlphaFunc(TheDxRnd.Device(), D3DCMP_GREATER);
    unsigned int maxPointSize = (int)caps.MaxPointSize;
    D3DDevice_SetRenderState_PointSizeMax(TheDxRnd.Device(), maxPointSize);
    D3DDevice_SetRenderState_SeparateAlphaBlendEnable(TheDxRnd.Device(), 1);
    D3DDevice_SetRenderState_SrcBlendAlpha(TheDxRnd.Device(), 1);
    D3DDevice_SetRenderState_DestBlendAlpha(TheDxRnd.Device(), 1);
    D3DDevice_SetRenderState_BlendOpAlpha(TheDxRnd.Device(), 3);

    if (caps.MaxTextureBlendStages != 0) {
        unsigned int stage_offset = 0;
        unsigned int i = 0;
        do {
            D3DDevice_SetSamplerState_MinFilter(TheDxRnd.Device(), i, 1);
            D3DDevice_SetSamplerState_MagFilter(TheDxRnd.Device(), i, 1);

            unsigned char* device = reinterpret_cast<unsigned char*>(TheDxRnd.Device());
            unsigned int* stage_ptr = reinterpret_cast<unsigned int*>(device + stage_offset + 0x48C);
            *stage_ptr = (*stage_ptr & 0xFE7FFFFF) | 0x800000;

            unsigned long long* state64 = reinterpret_cast<unsigned long long*>(device + 0x18);
            unsigned long long shift64 = (unsigned long long)(i + 0x20) & 0x7F;
            unsigned long long mask = 0x8000000000000000ULL;
            *state64 |= mask >> shift64;

            i++;
            stage_offset += 0x18;
        } while (i < caps.MaxTextureBlendStages);
    }

    D3DDevice_SetRenderState_PresentImmediateThreshold(TheDxRnd.Device(), 100);
}

void DxRnd::InitRenderState() {
    PhysMemTypeTracker tracker("D3D(phys):Global");
    if (!mD3DDevice) {
        return;
    }
    SetDefaultRenderStates();
    D3DXSetDXT3DXT5(1);
    SetupGamma();
}

void DxRnd::BeginTiling(const Hmx::Color &c, float f, unsigned int ui) {
    if (mNumTiles == 0) {
        D3DDevice_Clear(mD3DDevice, 0, nullptr, 0x31, MakeColor(c), f, ui, 0);
    } else {
        XMVECTOR v = {c.red, c.green, c.blue, c.alpha};
        D3DDevice_BeginTiling(mD3DDevice, 0, mNumTiles, &mTileRect, &v, f, ui);
        mTilingActive = true;
    }
}

void DxRnd::SetFrameBuffersAsSource() {
    D3DDevice_SetTexture(mD3DDevice, 6, mPreProcessBuffer, 0x02000000);
    D3DDevice_SetSamplerState_MinFilter(TheDxRnd.Device(), 6, 1);
    D3DDevice_SetSamplerState_MagFilter(TheDxRnd.Device(), 6, 1);
    D3DDevice_SetSamplerState_MipFilter(TheDxRnd.Device(), 6, 2, 0x02000000);
    D3DDevice_SetSamplerState_AddressU(TheDxRnd.Device(), 6, 2, 0x02000000);

    D3DDevice_SetTexture(mD3DDevice, 9, mFrontBufferDepth, 0x400000);
    D3DDevice_SetSamplerState_MinFilter(TheDxRnd.Device(), 9, 0);
    D3DDevice_SetSamplerState_MagFilter(TheDxRnd.Device(), 9, 0);
    D3DDevice_SetSamplerState_MipFilter(TheDxRnd.Device(), 9, 2, 0x400000);
    D3DDevice_SetSamplerState_AddressU(TheDxRnd.Device(), 9, 2, 0x400000);

    D3DDevice_SetTexture(mD3DDevice, 14, mPostProcessBuffer, 0x20000);
    D3DDevice_SetSamplerState_MinFilter(TheDxRnd.Device(), 14, 1);
    D3DDevice_SetSamplerState_MagFilter(TheDxRnd.Device(), 14, 1);
    D3DDevice_SetSamplerState_MipFilter(TheDxRnd.Device(), 14, 2, 0x20000);
    D3DDevice_SetSamplerState_AddressU(TheDxRnd.Device(), 14, 2, 0x20000);
}

void DxRnd::PerfCountersInit() {
    if (!mCreatedPerfCounters) {
        mCreatedPerfCounters = true;
        mPerfCounterStart = D3DDevice_CreatePerfCounters(mD3DDevice, 1);
        DX_ASSERT(mPerfCounterStart, 0x230);
        mPerfCounterEnd = D3DDevice_CreatePerfCounters(mD3DDevice, 1);
        DX_ASSERT(mPerfCounterEnd, 0x231);
        D3DPERFCOUNTER_EVENTS perfEvents;
        memset(&perfEvents, 0, sizeof(D3DPERFCOUNTER_EVENTS));
        perfEvents.RBBM[0] = GPUPE_RBBM_NRT_BUSY;
        perfEvents.CP[0] = GPUPE_CP_COUNT;
        perfEvents.RBBM[1] = GPUPE_RBBM_COUNT;
        D3DDevice_EnablePerfCounters(mD3DDevice, true);
        D3DDevice_SetPerfCounterEvents(mD3DDevice, &perfEvents, 0);
        mGPUTimer = AutoTimer::GetTimer("gs");
    }
}

void DxRnd::PerfCountersStart() {
    MILO_ASSERT(mGsTiming == true, 0x249);
    MILO_ASSERT(mCreatedPerfCounters == true, 0x24A);
    MILO_ASSERT(mGPUTimer != NULL, 0x24B);
    MILO_ASSERT(mPerfCounterStart != NULL, 0x24C);
    MILO_ASSERT(mPerfCounterEnd != NULL, 0x24D);
    mGPUTimer->SetLastMs(mGPUBusyMs * 1.075f);
    D3DDevice_QueryPerfCounters(mD3DDevice, mPerfCounterStart, 1);
}

void DxRnd::PerfCountersStop() {
    MILO_ASSERT(mGsTiming == true, 0x25D);
    MILO_ASSERT(mCreatedPerfCounters == true, 0x25E);
    MILO_ASSERT(mGPUTimer != NULL, 0x25F);
    MILO_ASSERT(mPerfCounterStart != NULL, 0x260);
    MILO_ASSERT(mPerfCounterEnd != NULL, 0x261);
    D3DDevice_QueryPerfCounters(mD3DDevice, mPerfCounterEnd, 1);
    D3DPERFCOUNTER_VALUES startValues;
    HRESULT code = D3DPerfCounters_GetValues(mPerfCounterStart, &startValues, 0, nullptr);
    DX_ASSERT_CODE(code, 0x269);
    D3DPERFCOUNTER_VALUES endValues;
    code = D3DPerfCounters_GetValues(mPerfCounterEnd, &endValues, 0, nullptr);
    DX_ASSERT_CODE(code, 0x26A);
    auto *startLargeIntegers = (ULARGE_INTEGER *)&startValues;
    auto *endLargeIntegers = (ULARGE_INTEGER *)&endValues;
    for (size_t i = 0; i < sizeof(D3DPERFCOUNTER_VALUES) / sizeof(ULARGE_INTEGER); i++) {
        endLargeIntegers[i].QuadPart -= startLargeIntegers[i].QuadPart;
    }
    mGPUBusyMs = endLargeIntegers[1].QuadPart * 2e-06f;
    mGPUCountMs = endLargeIntegers[2].QuadPart * 2e-06f;
}

void DxRnd::EndTiling(D3DBaseTexture *tex, int flags) {
    int tileMode = 0;
    if (tex && flags) {
        tileMode = (flags & 0x3F) << 0x1A;
    }
    if (mTilingActive) {
        MILO_ASSERT(mNumTiles > 0, 0x480);
        HRESULT hr = D3DDevice_EndTiling(mD3DDevice, tileMode, nullptr, tex, nullptr, 0, 0, nullptr);
        DX_ASSERT_CODE(hr, 0x481);
        mTilingActive = false;
    } else {
        MILO_ASSERT(mNumTiles == 0, 0x486);
        D3DDevice_Resolve(mD3DDevice, tileMode, nullptr, tex, nullptr, 0, 0, nullptr, 0, 0, nullptr);
    }
}

void CreateBackBuffers(
    int width,
    int height,
    D3DMULTISAMPLE_TYPE multisample,
    unsigned int &edramBase,
    unsigned int &edramHzBase,
    D3DSurface *&colorSurface,
    D3DSurface *&depthSurface
) {
    UINT depthSize = XGSurfaceSize(width, height, D3DFMT_D24FS8, multisample);
    UINT colorSize = XGSurfaceSize(width, height, D3DFMT_A8R8G8B8, multisample);

    unsigned int adjustedWidth = width;
    unsigned int adjustedHeight = height;
    if ((int)multisample >= 1) {
        adjustedHeight *= 2;
    }
    if ((int)multisample == 2) {
        adjustedWidth *= 2;
    }

    edramBase = 0x800;
    edramHzBase = 0xE10;

    edramBase -= colorSize;

    edramHzBase -= (((adjustedWidth + 0x1F) >> 5) * ((adjustedHeight + 0xF) >> 4)) & 0x7FFFFF;

    D3DSURFACE_PARAMETERS params;
    params.Base = edramBase;
    params.HierarchicalZBase = edramHzBase;
    depthSurface = D3DDevice_CreateSurface(width, height, D3DFMT_D24FS8, multisample, &params);
    DX_ASSERT(depthSurface, 0x2CE);

    edramBase -= depthSize;

    params.Base = edramBase;
    params.HierarchicalZBase = -1;
    colorSurface = D3DDevice_CreateSurface(width, height, D3DFMT_A8R8G8B8, multisample, &params);
    DX_ASSERT(colorSurface, 0x2D4);
}

void DxRnd::SavePreBuffer() {
    XMVECTOR vector;
    Hmx::Color c = mClearColor;
    vector.x = c.red;
    vector.y = c.green;
    vector.z = c.blue;
    D3DDevice_Resolve(
        mD3DDevice, 0x14, nullptr, mFrontBufferDepth, nullptr, 0, 0, nullptr, 1, 0, nullptr
    );

    D3DDevice_Resolve(
        mD3DDevice, 0x300, nullptr, mPreProcessBuffer, nullptr, 0, 0, &vector, 0, 0, nullptr
    );

    vector.w = 0.f;
}

void DxRnd::SavePostBuffer() {
    D3DDevice_Resolve(
        mD3DDevice, 0, nullptr, mPostProcessBuffer, nullptr, 0, 0, nullptr, 0, 0, nullptr
    );
}

void DxRnd::SetShaderRegisterAlloc(RegisterAlloc s) {
    MILO_ASSERT(s >=0 && s < kNumRegAlloc, 0x6BA);
    if (mRegAlloc != s) {
        mRegAlloc = s;
        switch (s) {
        case 0:
            D3DDevice_SetShaderGPRAllocation(mD3DDevice, 0, 0, 0);
            break;
        case 1:
            // Literal 0x20/0x60, NOT mDefaultVSRegAlloc/mDefaultPSRegAlloc.
            // DC3 promoted these two to configurable members; RB3 retail did
            // not have them yet.  Witness: SetShaderRegisterAlloc has no
            // standalone COMDAT in retail (it is absent from report.json), so
            // it is only ever seen inlined -- and the inlined copy inside
            // BeginDrawing at 0x8273CEF0 emits `li r5,0x20; li r6,0x60`, while
            // NOTHING in the whole of Rnd_Xbox.s ever loads 0x3a4/0x3a8/0x3ac.
            D3DDevice_SetShaderGPRAllocation(mD3DDevice, 0, 0x20, 0x60);
            break;
        case 2:
            D3DDevice_SetShaderGPRAllocation(mD3DDevice, 0, 0x10, 0x70);
            break;
        case 3:
            D3DDevice_SetShaderGPRAllocation(mD3DDevice, 0, 0x10, 0x70);
            break;
        default:
            MILO_NOTIFY("Invalid Shader Register Allocation");
            break;
        }
    }
}

RndTex *DxRnd::GetCurrentFrameTex(bool resolvePreProcess) {
    if (!mPostProcDone) {
        if (resolvePreProcess) {
            D3DDevice_Resolve(
                mD3DDevice, 0, nullptr, mPreProcessBuffer, nullptr, 0, 0, nullptr, 0, 0, nullptr
            );
        }
        return PreProcessTexture();
    }
    return PostProcessTexture();
}

bool DxRnd::CanModal(Debug::ModalType t) {
    if (mTilingActive) {
        // Retail tests t as a byte (clrlwi. r11,r4,24), allowing EndTiling for any
        // non-kModalWarn modal (notify/fail), not just kModalFail. The byte cast is
        // what reproduces the low-8-bit truthiness test.
        if ((unsigned char)t) {
            EndTiling(FrontBuffer(), 0);
        } else {
            return false;
        }
    }
    return true;
}

void DxRnd::ModalDraw(Debug::ModalType t, const char *cc) {
    bool wasSuspended = mSuspended;
    Resume();
    D3DSurface *savedStencilSurface = D3DDevice_GetDepthStencilSurface(mD3DDevice);
    D3DSurface *savedRenderTarget = D3DDevice_GetRenderTarget(mD3DDevice, 0);
    D3DDevice_SetRenderTarget_External(mD3DDevice, 0, mBackBuffer);
    D3DDevice_SetDepthStencilSurface(mD3DDevice, 0);
    Hmx::Color color(0, 0.1f, 0.5f, 0);
    // Retail tests t as a byte (clrlwi. r10,r25,24), same truthiness idiom as
    // DxRnd::CanModal above -- true for kModalNotify(1) and kModalFail(2), not
    // an equality check against kModalFail specifically.
    if ((unsigned char)t) {
        color.alpha = 0.25f;
        color.green = 0;
        color.blue = 0;
    }
    D3DDevice_Clear(mD3DDevice, 0, nullptr, 0x31, MakeColor(color), 0, 0, 0);
    Rnd::DrawStringScreen(cc, Vector2(0.025f, 0.025f), Hmx::Color(1, 1, 1, 1), true);
    RndOverlay::DrawAll(true);
    D3DDevice_Resolve(
        mD3DDevice, 0, nullptr, FrontBuffer(), nullptr, 0, 0, nullptr, 0, 0, nullptr
    );
    if (mRegAlloc != 0) {
        mRegAlloc = (RegisterAlloc)0;
        D3DDevice_SetShaderGPRAllocation(mD3DDevice, 0, 0, 0);
    }
    Present();
    D3DDevice_SetRenderTarget_External(mD3DDevice, 0, savedRenderTarget);
    D3DDevice_SetDepthStencilSurface(mD3DDevice, savedStencilSurface);
    if (wasSuspended) {
        Suspend();
    }
}

void DxRnd::InitBuffers() {
    PhysMemTypeTracker tracker("D3D(phys):Global");
    memset(&mPresentParams, 0, sizeof(D3DPRESENT_PARAMETERS));
    memset(&mVideoMode, 0, sizeof(XVIDEO_MODE));
    XGetVideoMode(&mVideoMode);
    static Symbol rnd("rnd");
    static Symbol low_res("low_res");
    static Symbol force_hd("force_hd");
    auto& _ref0 = mVideoMode.fIsHiDef;
    if (SystemConfig(rnd)->FindInt(force_hd) != 0) {
        _ref0 = true;
        mVideoMode.fIsWideScreen = true;
    } else if (SystemConfig(rnd)->FindInt(low_res) != 0) {
        mFlags |= 1;
    }
    mLowRes = mFlags & 1;
    mAspect = mLowRes ? kWidescreen : kRegular;
    mHeight = mLowRes ? 540 : 720;
    int i11, i10;
    if (_ref0 != 0 || mLowRes != 0) {
        i11 = (mHeight << 4) / 9;
        i10 = (mHeight << 4) / 9;
    } else {
        i11 = (mHeight << 2) / 3;
        i10 = (mHeight << 2) / 3;
    }
    mWidth = i11;
    if (!(mFlags & 1)) {
        mNumTiles = 2;
        if (mFlags & 2) {
            i11 = i11 / 2;
            i10 = i10 / 2;
        } else {
            i10 = i10 / 2;
        }
    }
    mPresentParams.Windowed = 0;
    mPresentParams.DisableAutoBackBuffer = 1;
    mPresentParams.DisableAutoFrontBuffer = 1;
    mPresentParams.BackBufferWidth = mWidth;
    mPresentParams.BackBufferHeight = mHeight;
    mPresentParams.PresentationInterval = 0;
    mPresentParams.SwapEffect = D3DSWAPEFFECT_DISCARD;
    UpdateScalerParams();
    mRenderThreadId = GetCurrentThreadId();
    {
        BeginMemTrackObjectName("D3D->CreateDevice");
        HRESULT hr = Direct3D_CreateDevice(
            0, mDeviceType, &mFocusWindow, 1, &mPresentParams, &mD3DDevice
        );
        DX_ASSERT_CODE(hr, 0x367);
        EndMemTrackObjectName();
    }
    if (!(mFlags & 1)) {
        MILO_ASSERT(mNumTiles > 0, 0x36D);
        BeginMemTrackObjectName("CreateBackBuffers:World");
        CreateBackBuffers(
            mWidth, mHeight, D3DMULTISAMPLE_NONE, mEdramBase, mEdramHzBase, mBackBuffer, mWorldDepth
        );
        EndMemTrackObjectName();
        BeginMemTrackObjectName("CreateBackBuffers:UI");
        CreateBackBuffers(
            i10, i11, D3DMULTISAMPLE_2_SAMPLES, mEdramBase, mEdramHzBase, mOffscreenRT, mOffscreenDepth
        );
    } else {
        MILO_ASSERT(mNumTiles == 0, 0x37E);
        BeginMemTrackObjectName("CreateBackBuffers:World");
        CreateBackBuffers(
            mWidth, mHeight, D3DMULTISAMPLE_2_SAMPLES, mEdramBase, mEdramHzBase, mBackBuffer, mWorldDepth
        );
        EndMemTrackObjectName();
        BeginMemTrackObjectName("CreateBackBuffers:UI");
        CreateBackBuffers(
            mWidth, mHeight, D3DMULTISAMPLE_2_SAMPLES, mEdramBase, mEdramHzBase, mOffscreenRT, mOffscreenDepth
        );
    }
    EndMemTrackObjectName();
    {
        BeginMemTrackObjectName("CreateTexture:PreProcessBuffer");
        mPreProcessBuffer = static_cast<D3DTexture *>(D3DDevice_CreateTexture(
            mWidth, mHeight, 1, 1, 0, D3DFMT_A8R8G8B8, 0, D3DRTYPE_TEXTURE
        ));
        DX_ASSERT(mPreProcessBuffer, 0x390);
        EndMemTrackObjectName();
    }
    {
        BeginMemTrackObjectName("CreateTexture:PostProcessBuffer");
        mPostProcessBuffer = static_cast<D3DTexture *>(D3DDevice_CreateTexture(
            mWidth, mHeight, 1, 1, 0, D3DFMT_A8R8G8B8, 0, D3DRTYPE_TEXTURE
        ));
        DX_ASSERT(mPostProcessBuffer, 0x394);
        EndMemTrackObjectName();
    }
    for (int i = 0; i < 2; i++) {
        BeginMemTrackObjectName("CreateTexture:FrontBuffer");
        mFrontBuffers[i] = static_cast<D3DTexture *>(D3DDevice_CreateTexture(
            mWidth, mHeight, 1, 1, 0, D3DFMT_A8R8G8B8, 0, D3DRTYPE_TEXTURE
        ));
        DX_ASSERT(mFrontBuffers[i], 0x39C);
        EndMemTrackObjectName();
    }

    BeginMemTrackObjectName("CreateTexture:FrontBufferDepth");
    mFrontBufferDepth = static_cast<D3DTexture *>(D3DDevice_CreateTexture(
        mWidth, mHeight, 1, 1, 0, D3DFMT_D24FS8, 0, D3DRTYPE_TEXTURE
    ));
    DX_ASSERT(mFrontBufferDepth, 0x3A2);
    EndMemTrackObjectName();
    PostDeviceReset();
    int temp27 = ((((mHeight + 0x1F) >> 5) * ((mWidth + 0x1F) >> 5)) << 0xC);
    for (int i = 0; i < 2; i++) {
        memset(mFrontBuffers[i], 0, temp27);
    }
    mRegAlloc = (RegisterAlloc)0;
    D3DDevice_SetShaderGPRAllocation(mD3DDevice, 0, 0, 0);
    Present();
    SetSync(mSync);
}

void DxRnd::CreatePostTextures() {
    RELEASE(mPreProcessTex);
    mPreProcessTex = Hmx::Object::New<DxTex>();
    mPreProcessTex->SetDeviceTex(mPreProcessBuffer);
    RELEASE(mPreDepthTex);
    mPreDepthTex = Hmx::Object::New<DxTex>();
    mPreDepthTex->SetDeviceTex(mFrontBufferDepth);
    RELEASE(mPostProcessTex);
    mPostProcessTex = Hmx::Object::New<DxTex>();
    mPostProcessTex->SetDeviceTex(mPostProcessBuffer);
}

static DWORD sPointTestFence = -1;

void DxRnd::DoPointTests() {
    // Block on previous fence if set
    if (sPointTestFence != (DWORD)-1) {
        D3DDevice_BlockOnFence(sPointTestFence);
        sPointTestFence = -1;
    }

    // Early out if no occlusion query manager or hi-res screen is active
    if (!mOcclusionQueryMgr)
        return;
    if (TheHiResScreen.IsActive())
        return;

    // Process query results from previous frame.
    // Retail (fn_8273D0A0) loads `it->mFlare` ONCE per branch and uses it for
    // both stores; writing `it->mFlare->` twice makes MSVC reload it because
    // the first store may alias the pointer.  The visibility bool is computed
    // BEFORE the flare pointer is loaded, so it is a local of its own.
    for (std::vector<RndPointTest>::iterator it = mPointTestQueries.begin(); it !=mPointTestQueries.end(); ++it) {
        unsigned int result;
        if (mOcclusionQueryMgr->GetQueryResults(it->mPointQueryIdx, result)) {
            bool visible = result != 0;
            RndFlare *flare = it->mFlare;
            flare->SetOcclusionReady(true);
            flare->SetVisible(visible);
        }
        if (mOcclusionQueryMgr->GetQueryResults(it->mAreaQueryIdx, result)) {
            RndFlare *flare = it->mFlare;
            flare->SetOcclusionResult((float)(int)result);
            flare->SetOcclusionReady(true);
        }
    }

    // Update frame index.  Retail (and DC3 retail, 0x8261B04C/0x8261B06C)
    // dispatches vtable slot 0x20 BEFORE slot 0x1c: with the compiler-verified
    // slot layout (7 = OnBeginFrame @0x1c, 8 = OnEndFrame @0x20) the
    // byte-faithful call order is End, increment, Begin.
    mOcclusionQueryMgr->ToggleFrameIndex();
    mOcclusionQueryMgr->OnEndFrame();
    mOcclusionQueryMgr->IncrementFrameCounter();
    mOcclusionQueryMgr->OnBeginFrame();

    // Count point tests needed
    int numTests = 0;
    for (std::list<PointTest>::iterator it = mPointTests.begin(); it !=mPointTests.end(); ++it) {
        numTests++;
    }

    // Resize mPointTestQueries to match mPointTests count.  Retail inlines
    // resize() (erase arm / _M_fill_insert arm) and builds the fill value
    // {0, -1, -1} on the stack at 0x58(r1) -- RndPointTest's default ctor.
    mPointTestQueries.resize(numTests);

    // Early out if no point tests
    if (mPointTests.empty())
        return;

    // Setup identity transform
    Transform xfm;
    xfm.Reset();
    TheShaderMgr.SetTransform(xfm);

    // Setup view matrix.  Retail passes the Matrix4 constructor's return
    // value (`mr r5, r3`) straight to SetVConstant -- an unnamed temporary.
    TheShaderMgr.SetVConstant(kVS_ViewProjMatrix, Hmx::Matrix4(xfm));

    // Setup shader state
    RndShader::SelectConfig(nullptr, kStandardShader, false);
    D3DDevice_SetPixelShader(mD3DDevice, nullptr);
    D3DDevice_SetFVF(mD3DDevice, 0x4042);

    // Disable color writes and blending for occlusion testing
    D3DDevice_SetRenderState_ColorWriteEnable(TheDxRnd.Device(), 0);
    D3DDevice_SetRenderState_AlphaBlendEnable(TheDxRnd.Device(), 0);
    D3DDevice_SetRenderState_AlphaTestEnable(TheDxRnd.Device(), 0);
    D3DDevice_SetRenderState_ZWriteEnable(TheDxRnd.Device(), 0);
    D3DDevice_SetRenderState_ZEnable(TheDxRnd.Device(), 1);

    // Set z-compare function based on mReverseZ.  Retail bool-ifies
    // mReverseZ, masks with 3 (`clrlwi r11, r11, 30`) and adds 1, so the two
    // values are 4 and 1 = D3DCMP_GREATER / D3DCMP_LESS on the 360 enum.
    D3DDevice_SetRenderState_ZFunc(
        TheDxRnd.Device(), (D3DCMPFUNC)(mReverseZ ? D3DCMP_GREATER : D3DCMP_LESS)
    );

    // Set point size
    float pointSize = 1.0f;
    D3DDevice_SetRenderState_PointSize(TheDxRnd.Device(), *(DWORD*)&pointSize);
    D3DDevice_SetRenderState_ViewportEnable(TheDxRnd.Device(), 0);
    D3DDevice_SetRenderState_HalfPixelOffset(TheDxRnd.Device(), 1);

    // Process each point test
    int idx = 0;
    for (std::list<PointTest>::iterator it = mPointTests.begin(); it !=mPointTests.end(); ++it, ++idx) {
#ifdef HX_NATIVE
        // Retail RB3 has no stats increment in this loop (nothing touches a
        // global between the loop head and the flare's mPointTest load).
        TheNgStats->mFlares++;
#endif

        RndFlare *flare = it->mFlare;
        RndPointTest &test = mPointTestQueries[idx];
        // Retail stores mFlare FIRST, then the two -1s.
        test.mFlare = flare;
        test.mPointQueryIdx = -1;
        test.mAreaQueryIdx = -1;

        // Point test
        if (flare->GetPointTest()) {
            struct PointVertex {
                float x, y, z;
                float w;
                DWORD color;
            };
            PointVertex vtx;
            vtx.x = (float)it->x;
            vtx.y = (float)it->y;
            vtx.z = (float)it->z * 5.9604651881e-08f;
            vtx.w = 1.0f;
            vtx.color = 0;

            // Retail hands CreateQuery the MEMBER by reference (`addi r27,
            // r28, 4` / `mr r4, r27`), keeps the cached manager (r26) live
            // across CreateQuery into BeginQuery, and gates BeginQuery and
            // DrawVerticesUP+EndQuery with TWO separate tests of the same
            // bool (r25), reloading the index from 0x0(r27) each time.
            bool ok = mOcclusionQueryMgr->CreateQuery(test.mPointQueryIdx);
            if (ok) {
                mOcclusionQueryMgr->BeginQuery(test.mPointQueryIdx);
            }
            if (ok) {
                D3DDevice_DrawVerticesUP(mD3DDevice, D3DPT_POINTLIST, 1, &vtx, sizeof(PointVertex));
                mOcclusionQueryMgr->EndQuery(test.mPointQueryIdx);
            }
        }

        // Area test.  Retail reloads the flare from `test.mFlare`
        // (`lwz r11, 0x0(r28)`), not from the `flare` local.
        if (test.mFlare->GetAreaTest()) {
            struct QuadVertex {
                float x, y, z;
                float w;
                DWORD color;
            };
            QuadVertex verts[4];

            // The rect is held BY REFERENCE (`addi r10, r11, 0x134`), so w/h
            // are read as 0x8(r10)/0xc(r10) rather than off the flare.
            verts[0].x = test.mFlare->GetArea().x;
            verts[0].y = test.mFlare->GetArea().y;
            verts[0].z = (float)it->z * 5.9604651881e-08f;
            verts[0].w = 1.0f;
            verts[0].color = 0;

            // Every copy is `verts[n] = verts[0]` (a 5-word lwzu/stwu loop)
            // and the adjusted components are read back from verts[0], not
            // from the copy -- except verts[3].y, which reloads its own slot.
            // Every `fadds` takes the RECT term first.
            verts[1] = verts[0];
            verts[1].y = test.mFlare->GetArea().h + verts[0].y;

            verts[2] = verts[0];
            verts[2].x = test.mFlare->GetArea().w + verts[0].x;

            verts[3] = verts[0];
            verts[3].x = test.mFlare->GetArea().w + verts[0].x;
            verts[3].y = test.mFlare->GetArea().h + verts[3].y;

            bool ok = mOcclusionQueryMgr->CreateQuery(test.mAreaQueryIdx);
            if (ok) {
                mOcclusionQueryMgr->BeginQuery(test.mAreaQueryIdx);
            }
            if (ok) {
                D3DDevice_DrawVerticesUP(mD3DDevice, D3DPT_TRIANGLESTRIP, 4, verts, sizeof(QuadVertex));
                mOcclusionQueryMgr->EndQuery(test.mAreaQueryIdx);
            }
        } else {
            test.mFlare->SetOcclusionReady(true);
            test.mFlare->SetVisible(true);
        }
    }

    // Insert fence for next frame
    sPointTestFence = D3DDevice_InsertFence(mD3DDevice);

    // Clear current material
    NgMat::SetCurrent(nullptr);

    // Restore render states
    D3DDevice_SetRenderState_ColorWriteEnable(TheDxRnd.Device(), 0xF);
    D3DDevice_SetRenderState_ViewportEnable(TheDxRnd.Device(), 1);
    D3DDevice_SetRenderState_HalfPixelOffset(TheDxRnd.Device(), 0);

    // Restore camera if set
    if (RndCam::Current()) {
        TheShaderMgr.SetVConstant(kVS_ViewProjMatrix, RndCam::Current()->GetViewProjMatrix());
    }
}


// Retires everything AutoRelease()/AutoDelete() queued while mReleaseImmediate
// was false.  A resource that is still bound to the device cannot be freed yet,
// so it is carried over into the next frame's pending list.
//
// The two shift expressions are the XDK's D3DTAG "pending mask" arithmetic with
// a *runtime* index: for a literal stream/sampler MSVC folds it to a constant,
// but inside these loops the whole expansion survives into the code -- retail
// hoists the 1<<63 out of both loops (li r11,1; rldicr r28,r11,63,63) and emits
// one srd per iteration.
//
// Ported from DC3's rnddx9/Rnd_Xbox.cpp with ONE retail-driven correction: DC3
// frees the texture's physical page with the 4-argument PhysicalFreeTracked(p,
// __FILE__, line, ""), but retail's call site at 0x8273CC08 sets ONLY r3 before
// the bl (r4-r6 are volatile across the XGGetTextureLayout call immediately
// above it), so RB3 calls the 1-argument PhysicalFree.  The map names that
// address ?PhysicalFreeTracked@@YAXPAXPBDH1@Z because the two fold under ICF --
// the tracked overload ignores p2/p3/p4, so both compile to the same body.
void DxRnd::ReleaseAutoRelease() {
    D3DDevice_SetVertexShader(mD3DDevice, nullptr);
    D3DDevice_SetPixelShader(mD3DDevice, nullptr);
    D3DDevice_SetIndices(mD3DDevice, nullptr);
    for (int sampler = 0; sampler < 16; sampler++) {
        D3DDevice_SetTexture(
            mD3DDevice, sampler, nullptr, 0x8000000000000000 >> (sampler + 0x20U)
        );
    }
    for (int stream = 0; stream < 4; stream++) {
        D3DDevice_SetStreamSource(
            mD3DDevice,
            stream,
            nullptr,
            0,
            0,
            0x8000000000000000 >> (((0x5FU - stream) * 0x5556U >> 16) + 0x20U)
        );
    }

    std::vector<D3DResource *> stillBoundResources;
    for (std::vector<D3DResource *>::iterator it = mPendingReleases.begin();
         it != mPendingReleases.end();
         ++it) {
        if (D3DResource_IsSet(*it, mD3DDevice)) {
            stillBoundResources.push_back(*it);
        } else if (*it) {
            (*it)->Release();
            *it = nullptr;
        }
    }
    mPendingReleases.clear();
    mPendingReleases.swap(stillBoundResources);

    std::vector<D3DBaseTexture *> stillBoundTextures;
    for (std::vector<D3DBaseTexture *>::iterator it = mPendingDeletes.begin();
         it != mPendingDeletes.end();
         ++it) {
        D3DBaseTexture *tex = *it;
        if (tex) {
            if (D3DResource_IsSet(tex, mD3DDevice)) {
                stillBoundTextures.push_back(tex);
            } else {
                UINT data;
                XGGetTextureLayout(
                    tex,
                    &data,
                    nullptr,
                    nullptr,
                    nullptr,
                    0,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                    0
                );
                PhysicalFree((void *)data);
                delete tex;
            }
        }
    }
    mPendingDeletes.clear();
    mPendingDeletes.swap(stillBoundTextures);
}

// Retail 0x8273CEF0 (396 B).  W16-V refused to pin this on adjacency because it
// opens with a bit test on a global at 0x82E04FFC that DC3's BeginDrawing does
// not have.  That global is not a render-state member: it is the MSVC local-
// static initialisation GUARD BIT for the `cpuTimer` static below, with the
// Timer* itself at 0x82E04FF8.  Retail proves it by construction --
//   lwz r11,0x4ffc(r10); clrlwi. r9,r11,31; bne <skip>; ori r11,r11,1; stw ...
//   addi r3,r31,0x50; addi r4,lbl_820010B0; bl ??0Symbol@@QAA@PBD@Z
//   lwz r3,0(r3);            bl ?GetTimer@AutoTimer@@SAPAVTimer@@VSymbol@@@Z
//   stw r3,0x4ff8(r11)
// -- test bit 0, set bit 0, run the initialiser once, store the pointer.  Note
// the Symbol temporary: RB3's AutoTimer::GetTimer takes a *Symbol* (our
// os/Timer.h:293 agrees), where DC3's takes a const char*, so the ctor call is
// part of the argument and not a separate statement.
//
// DC3 IS NEWER and its BeginDrawing has four such statics plus mPrintGlitches /
// MILO_LOG glitch reporting, mCaptureNextFrame / PIXCaptureGpuFrame, and
// mGSTiming / PerfCounters.  None of that is in retail's 396 bytes -- there is
// exactly ONE guard bit and ONE Timer* store, and no Timer::Start/Stop call at
// all -- so this is DC3's body with the later additions removed, not a port.
//
// The three virtual calls are read off the compiler's own vtable report rather
// than guessed: lwz r11,0(r3) then +0xe4 / +0x118 / +0x120 are slots 57 / 70 /
// 72 = Rnd::DrawPreClear / DxRnd::Resume / NgRnd::ResetStats.  The four members
// are compiler-verified too: 0x2c mClearColor, 0x1c4 mD3DDevice, 0x398
// mSuspended, 0x39c mRegAlloc.
//
// The colour pack is MakeColor's: retail scales by 255.0f (lbl_82033A50 =
// 0x437F0000) via fmuls/fctidz and then splices with
//   rlwimi r8,r11,8,16,23 ; clrlwi r11,r8,16 ; rlwimi r7,r11,8,0,23
// which is (red&0xFF)<<16 | (green&0xFF)<<8 | (blue&0xFF).  The Z argument is
// lbl_82000D78 = 0.0f, i.e. the literal 0 of the existing 8-argument call form
// already used at lines 426 and 653.
void DxRnd::BeginDrawing() {
    static Timer *cpuTimer = AutoTimer::GetTimer("cpu");
    if (mSuspended) {
        Resume();
    }
    Present();
    if (MainThread()) {
        ReleaseAutoRelease();
    }
    Rnd::BeginDrawing();
    DrawPreClear();
    Hmx::Color clearColor = mClearColor;
    // NOT MakeColor(): that helper packs alpha as a fourth channel (Rnd.h:248),
    // and retail loads exactly THREE floats here -- lfs f13,0x60 / f12,0x64 /
    // f11,0x68 = red/green/blue, three fmuls, three fctidz.  MakeColor's alpha
    // term shows up as a surplus `lfs f10,0x64(r31)` + `fmuls`.  The splice
    // rlwimi r8,r11,8,16,23 ; clrlwi r11,r8,16 ; rlwimi r7,r11,8,0,23 is
    // exactly the RGB expression below.
    D3DDevice_Clear(
        mD3DDevice,
        0,
        nullptr,
        0x31,
        ((unsigned long)(clearColor.red * 255.0f) & 0xFF) << 16
            | ((unsigned long)(clearColor.green * 255.0f) & 0xFF) << 8
            | ((unsigned long)(clearColor.blue * 255.0f) & 0xFF),
        0,
        0,
        0
    );
    SetShaderRegisterAlloc((RegisterAlloc)1);
    ResetStats();
    NgMat::SetCurrent(nullptr);
}

// COMDAT-scatter owner-TU includes (sw scatter-scan): retail linker
// interleaved these owners' COMDATs into this TU's .text span.
#define gRev gRev_AccomplishmentManager
#define gAltRev gAltRev_AccomplishmentManager
#include "band3/meta_band/AccomplishmentManager.cpp"
#undef gRev
#undef gAltRev

// laneW homing scan: DxMesh's dtor + scalar-deleting dtor COMDATs were scattered
// by the retail linker into this TU's pinned .text span (0x827385D0, 0x82738AB8).
#include "rnddx9/Mesh.cpp"
