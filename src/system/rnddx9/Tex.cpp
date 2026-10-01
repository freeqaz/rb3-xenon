#include "rnddx9/Tex.h"
#include "Rnd.h"
#include "Tex.h"
#include "os/Debug.h"
#include "rnddx9/Rnd.h"
#include "rnddx9/TexMgr.h"
#include "rndobj/Mat_NG.h"
#include "rndobj/ShaderMgr.h"
#include "utl/MemMgr.h"
#include "xdk/xgraphics/xgraphics.h"
#include "rndobj/Rnd.h"
#include "rndobj/Tex.h"
#include "xdk/d3d9i/d3d9.h"
#include "xdk/d3d9i/d3d9types.h"

#ifdef HX_NATIVE
std::vector<DxTex *> gAllTextures;
#endif

// Scratch/destination bookkeeping for the DXT compression path.  File-local in
// retail (no map row of its own); layout taken from dc3
// (../dc3-decomp/src/system/rnddx9/Tex.cpp:29).
struct CompressLevel {
    D3DSurface *scratchSurface; // 0x0
    D3DLOCKED_RECT scratchLock; // 0x4
    D3DSURFACE_DESC scratchDesc; // 0xc
    D3DSurface *textureSurface; // 0x2c
    D3DLOCKED_RECT textureLock; // 0x30
    D3DSURFACE_DESC textureDesc; // 0x38
};

struct CompressDesc {
    D3DTexture *texture; // 0x0
    // ⚠ dc3 declares this `RndTex::AlphaCompress alpha` -- an enum, so 4 bytes,
    // which compiles to `lwz` + signed `cmpwi`.  RETAIL LOADS A BYTE:
    // `lbz r11,0x4(r30); cmplwi r11,0x0` at 0x82734148+29.  The field is
    // byte-sized and compared UNSIGNED, so dc3 is wrong about its width.
    // It still holds three values (0/1/2 -- StartCompress tests `== 2`), so it
    // is a u8, not a bool.  3 pad bytes follow; `unk8` stays at 0x8 either way.
    u8 alpha; // 0x4
    int unk8; // 0x8
    D3DFORMAT format; // 0xc
    void *tiledBuffer; // 0x10
    CompressLevel levels[16]; // 0x14
};

// 0x82734728. RB3 retail keeps no texture registry: the ctor calls only
// ??0RndTex and the dtor only ResetSurfaces + ??1RndTex (DC3 later added a
// gAllTextures list for its dump_tex debug command; the native build keeps it).
DxTex::DxTex()
    : mFormat((D3DFORMAT)-1), mTexture(0), unk84(0), mRenderTarget(0), mDepthRT(0),
      mMovieBufIdx(0), mLockedRect(), unka4(0), unka8(0), unkac(0) {
#ifdef HX_NATIVE
    gAllTextures.push_back(this);
#endif
    for (int i = 0; i < 3; i++) {
        mMovieTextures[i] = 0;
    }
}

DxTex::~DxTex() {
    ResetSurfaces();
#ifdef HX_NATIVE
    auto it = std::find(gAllTextures.begin(), gAllTextures.end(), this);
    MILO_ASSERT(it != gAllTextures.end(), 0x2D7);
    gAllTextures.erase(it);
#endif
}

void DxTex::Compress(AlphaCompress a) {
    void *v = StartCompress(a);
    DoCompress(v);
    FinishCompress(v);
}

// 0x82733F88 (called by Compress). RB3 allocates the descriptor with operator
// new inside a temp-allocation scope and picks DXT5 for ANY non-zero alpha mode
// (`clrlwi. r28,r28,24; beq` -> DXT1), not DC3's `alpha == 2`.
void *DxTex::StartCompress(AlphaCompress alpha) {
    MILO_ASSERT(mTexture, 0x15A);
    for (int i = 0; i < 16; i++) {
        TheShaderMgr.SetPConstant((PShaderConstant)i, (RndTex *)nullptr);
    }
    CompressDesc *desc;
    {
        MemDoTempAllocations tmp;
        desc = new CompressDesc;
    }
    desc->alpha = alpha;
    desc->format = desc->alpha ? D3DFMT_LIN_DXT5 : D3DFMT_LIN_DXT1;
    desc->unk8 = 0;
    int numLevels = D3DBaseTexture_GetLevelCount(mTexture);
    desc->texture = (D3DTexture *)D3DDevice_CreateTexture(
        mWidth, mHeight, 1, numLevels, 0, desc->format, 0, (D3DRESOURCETYPE)3
    );
    DX_ASSERT(desc->texture, 0x16C);
    MILO_ASSERT(numLevels < 16, 0x16F);
    for (int i = 0; i < numLevels; i++) {
        desc->levels[i].scratchSurface = D3DTexture_GetSurfaceLevel(mTexture, i);
        DX_ASSERT(desc->levels[i].scratchSurface, 0x174);
        D3DSurface_LockRect(
            desc->levels[i].scratchSurface, &desc->levels[i].scratchLock, nullptr, 0x10
        );
        D3DSurface_GetDesc(desc->levels[i].scratchSurface, &desc->levels[i].scratchDesc);
        desc->levels[i].textureSurface = D3DTexture_GetSurfaceLevel(desc->texture, i);
        DX_ASSERT(desc->levels[i].textureSurface, 0x179);
        D3DSurface_LockRect(
            desc->levels[i].textureSurface, &desc->levels[i].textureLock, nullptr, 0
        );
        D3DSurface_GetDesc(desc->levels[i].textureSurface, &desc->levels[i].textureDesc);
    }
    {
        MemDoTempAllocations tmp;
        int rowPitch = desc->levels[0].scratchDesc.Width * 4;
        desc->tiledBuffer = MemAlloc(
            desc->levels[0].scratchDesc.Height * rowPitch, __FILE__, 0x183, "compress"
        );
    }
    return desc;
}

// Retail 0x82734148 (284 B), named in the map, previously fuzzy 0 for want of
// any definition to pair with: DECLARED at rnddx9/Tex.h and defined in NO
// translation unit.  StartCompress / FinishCompress are undefined the same way
// but carry no named retail row (objdiff pairs by NAME, so a `fn_` row cannot
// pair with our mangled symbol) -- they are deliberately left undefined so this
// change measures exactly one thing.
// Ported from ../dc3-decomp/src/system/rnddx9/Tex.cpp:150.
void DxTex::DoCompress(void *p) {
    CompressDesc *desc = (CompressDesc *)p;
    int numLevels = D3DBaseTexture_GetLevelCount(mTexture);
    for (int i = 0; i < numLevels; i++) {
        CompressLevel &level = desc->levels[i];
        int rowPitch = level.scratchDesc.Width * 4;
        XGUntileTextureLevel(
            level.scratchDesc.Width,
            level.scratchDesc.Height,
            desc->unk8,
            mFormat & 0x3f,
            1,
            desc->tiledBuffer,
            rowPitch,
            nullptr,
            level.scratchLock.pBits,
            nullptr
        );
        if ((int)desc->alpha == 0) {
            unsigned int *texel = (unsigned int *)desc->tiledBuffer;
            unsigned int *end =
                texel + level.scratchDesc.Width * level.scratchDesc.Height;
            for (; texel < end; texel++) {
                *texel |= 0xff000000;
            }
        }
        XGCompressSurface(
            level.textureLock.pBits,
            level.textureLock.Pitch,
            level.textureDesc.Width,
            level.textureDesc.Height,
            desc->format,
            0,
            desc->tiledBuffer,
            rowPitch,
            (D3DFORMAT)0x18280086,
            0,
            0,
            0.5f
        );
    }
}

// 0x82734268 (called by Compress).
void DxTex::FinishCompress(void *p) {
    CompressDesc *desc = (CompressDesc *)p;
    MemFree(desc->tiledBuffer);
    int numLevels = D3DBaseTexture_GetLevelCount(mTexture);
    for (int i = 0; i < numLevels; i++) {
        CompressLevel &level = desc->levels[i];
        D3DSurface_UnlockRect(level.scratchSurface);
        D3DSurface_UnlockRect(level.textureSurface);
        if (level.textureSurface) {
            D3DResource_Release(level.textureSurface);
            level.textureSurface = nullptr;
        }
        if (level.scratchSurface) {
            D3DResource_Release(level.scratchSurface);
            level.scratchSurface = nullptr;
        }
    }
    if (mTexture) {
        D3DResource_Release(mTexture);
        mTexture = nullptr;
    }
    mFormat = desc->format;
    mTexture = desc->texture;
    if (mRenderTarget) {
        D3DResource_Release(mRenderTarget);
        mRenderTarget = nullptr;
    }
    if (mDepthRT) {
        D3DResource_Release(mDepthRT);
        mDepthRT = nullptr;
    }
    mType = kRegular;
    mBpp = desc->alpha ? 8 : 4;
    delete desc;
}

void DxTex::SetDeviceTex(D3DTexture *tex) {
    mTexture = tex;
    mType = kDeviceTexture;
    if (tex) {
        D3DSURFACE_DESC desc;
        D3DTexture_GetLevelDesc(tex, 0, &desc);
        mNumMips = 0;
        mFormat = desc.Format;
        mWidth = desc.Width;
        mHeight = desc.Height;
        mBpp = D3DFORMAT_BitsPerPixel(desc.Format);
    }
}

D3DSurface *DxTex::GetRT() {
    if (!IsRenderTarget()) {
        return nullptr;
    } else {
        D3DResource_AddRef(mRenderTarget);
        return mRenderTarget;
    }
}

D3DSurface *DxTex::GetDepthRT() { return mDepthRT; }

void DxTex::PreDeviceReset() {
    if (IsBackBuffer() || IsRenderTarget()) {
        ResetSurfaces();
    }
}

void DxTex::PostDeviceReset() {
    if (IsBackBuffer()) {
        SetBitmap(TheRnd.Width(), TheRnd.Height(), TheRnd.Bpp(), mType, false, nullptr);
    }
    if (IsRenderTarget()) {
        SyncBitmap();
    }
}

D3DSurface *DxTex::GetSurfaceLevel(int x) {
    D3DSurface *ret = D3DTexture_GetSurfaceLevel(mTexture, x);
    DX_ASSERT(ret, 0xE6);
    return ret;
}

unsigned int DxTex::TexelsPitch() const {
    D3DLOCKED_RECT rect;
    D3DTexture_LockRect(mTexture, 0, &rect, nullptr, 0);
    D3DTexture_UnlockRect(mTexture, 0);
    return rect.Pitch;
}

// 0x827349A0: GetSurfaceLevel inlines to a tail call (its DX_ASSERT is empty).
D3DSurface *DxTex::GetMovieSurface() {
    if (!(mType & kMovie)) {
        return nullptr;
    } else {
        mTexture = mMovieTextures[mMovieBufIdx];
        return GetSurfaceLevel(0);
    }
}

// 0x82733E30: three buffers, so `% 3`.
void DxTex::SwapMovieSurface() {
    MILO_ASSERT((mType & kMovie) > 0, 0x2F5);
    mMovieBufIdx = (mMovieBufIdx + 1) % 3;
    mTexture = mMovieTextures[mMovieBufIdx];
}

// 0x827355D0. Each movie buffer is released unconditionally; unlike DC3 there
// is no `mTexture == mMovieTextures[i]` check and no TexMgr resource release.
void DxTex::ResetSurfaces() {
    for (int i = 0; i < 3; i++) {
        TheDxRnd.AutoRelease(mMovieTextures[i]);
        mMovieTextures[i] = nullptr;
    }

    bool _bit0 = (mType & kRendered) != 0;
    if (((_bit0) && mNumMips) || ((mType & kMovie) && (mType & 0x20))) {
        TheDxRnd.AutoDelete(mTexture);
        mTexture = nullptr;
    }

    // 0x1204 = kMovie | kScratch | kDeviceTexture
    if (mType & 0x1204) {
        mTexture = nullptr;
    }

    TheDxRnd.AutoRelease(mTexture);
    mTexture = nullptr;
    TheDxRnd.AutoRelease(mRenderTarget);
    mRenderTarget = nullptr;
    TheDxRnd.AutoRelease(mDepthRT);
    mDepthRT = nullptr;
}

// 0x82735160 (DxTex vtable slot 22).
void DxTex::LockBitmap(RndBitmap &bm, int flags) {
    if (!mTexture) {
        RndTex::LockBitmap(bm, flags);
        return;
    }
    bool wantRead = (flags & 1) > 0;
    bool wantWrite = (flags & 4) > 0;
    if (!wantRead && !wantWrite) {
        return;
    }
    bool renderTarget = (mType & kRendered) > 0;
    bool frontBuffer = (mType & kFrontBuffer) > 0;
    if ((renderTarget || frontBuffer) && wantRead) {
        if (frontBuffer) {
            unka4 = D3DTexture_GetSurfaceLevel(TheDxRnd.NotFrontBuffer(), 0);
        } else {
            unka4 = GetSurfaceLevel(0);
        }
    } else if ((mType & kBackBuffer) > 0 && wantRead) {
        D3DDevice_Resolve(
            TheDxRnd.Device(), 0, nullptr, mTexture, nullptr, 0, 0, nullptr, 1.0f, 0,
            nullptr
        );
        unka4 = GetSurfaceLevel(0);
    } else if ((mType & kMovie) > 0) {
        unka4 = nullptr;
    } else if ((mType & (kRegular | kScratch)) > 0) {
        unka4 = GetSurfaceLevel(0);
    }
    if (!unka4) {
        return;
    }
    unka8 = flags;
    if (wantRead && !wantWrite) {
        bm.Create(
            mWidth,
            mHeight,
            0,
            D3DFORMAT_BitsPerPixel(mFormat),
            TheDxRnd.BitmapOrderForD3DFormat(mFormat),
            nullptr,
            nullptr,
            nullptr
        );
        D3DSurface_LockRect(unka4, &mLockedRect, nullptr, 0x10);
        XGTEXTURE_DESC desc;
        XGGetTextureDesc((D3DBaseTexture *)unka4, 0, &desc);
        if (desc.Format & 0x100) {
            XGUntileSurface(
                bm.Pixels(),
                bm.DxtRowBytes(),
                nullptr,
                mLockedRect.pBits,
                desc.WidthInBlocks,
                desc.HeightInBlocks,
                nullptr,
                desc.BytesPerBlock
            );
        } else {
            memcpy(bm.Pixels(), mLockedRect.pBits, bm.PixelBytes());
        }
        D3DSurface_UnlockRect(unka4);
        if (unka4) {
            D3DResource_Release(unka4);
            unka4 = nullptr;
        }
    } else if (wantWrite) {
        D3DSurface_LockRect(unka4, &mLockedRect, nullptr, 0);
        bm.Create(
            mWidth,
            mHeight,
            0,
            D3DFORMAT_BitsPerPixel(mFormat),
            TheDxRnd.BitmapOrderForD3DFormat(mFormat),
            nullptr,
            mLockedRect.pBits,
            mLockedRect.pBits
        );
    }
}

// 0x827353F8 (slot 23).
void DxTex::UnlockBitmap() {
    if (mTexture) {
        if (unka4) {
            D3DSurface_UnlockRect(unka4);
            if (unka4) {
                D3DResource_Release(unka4);
                unka4 = nullptr;
            }
            if ((unka8 & 0x4) > 0) {
                HRESULT hr = D3DXFilterTexture(mTexture, nullptr, -1, -1);
                DX_ASSERT_CODE(hr, 0x618);
            }
        }
        mLockedRect.Pitch = 0;
        mLockedRect.pBits = nullptr;
        unka4 = nullptr;
        unka8 = 0;
    }
}

// 0x82733E60 (slot 30). No fallback to Rnd's null texture: a DxTex with no
// D3D texture binds null.
void DxTex::Select(int x) {
    D3DTexture *tex = mTexture;
    if (mType & 0x8) {
        if (mType == kFrontBuffer) {
            tex = TheDxRnd.FrontBuffer();
        } else {
            D3DDevice_Resolve(
                TheDxRnd.Device(), 0, nullptr, mTexture, nullptr, 0, 0, nullptr, 1.0f,
                0, nullptr
            );
        }
    }
    D3DDevice_SetTexture(TheDxRnd.Device(), x, tex, (1ULL << 63) >> (unsigned int)(x + 32));
}

// 0x82734400 (called by FinishDrawTarget).
void DxTex::ResolveMipChain() {
    if (mType != kShadowMap) {
        D3DDevice_Resolve(
            TheDxRnd.Device(), 0, nullptr, mTexture, nullptr, 0, 0, nullptr, 1.0f, 0,
            nullptr
        );
        D3DDevice_SetRenderTarget_External(TheDxRnd.Device(), 0, mRenderTarget);
        D3DDevice_SetDepthStencilSurface(TheDxRnd.Device(), nullptr);
    } else {
        D3DDevice_Resolve(
            TheDxRnd.Device(), 4, nullptr, mTexture, nullptr, 0, 0, nullptr, 1.0f, 0,
            nullptr
        );
        MILO_ASSERT(!mRenderTarget, 0x237);
        D3DDevice_SetRenderTarget_External(TheDxRnd.Device(), 0, nullptr);
        D3DDevice_SetDepthStencilSurface(TheDxRnd.Device(), mDepthRT);
        D3DDevice_SetSamplerState_MinFilter(TheDxRnd.Device(), 0, 1);
        D3DDevice_SetSamplerState_MagFilter(TheDxRnd.Device(), 0, 1);
        D3DDevice_SetSamplerState_MipFilter3(TheDxRnd.Device(), 0, 1, 0x80000000);
    }
    if (mNumMips != 0) {
        unsigned int numLevels = D3DBaseTexture_GetLevelCount(mTexture);
        for (unsigned int level = 1; level < numLevels; level++) {
            D3DDevice_SetSamplerState_MinMipLevel(TheDxRnd.Device(), 0, level - 1);
            D3DDevice_SetSamplerState_MaxMipLevel(TheDxRnd.Device(), 0, level - 1);
            D3DSURFACE_DESC desc;
            D3DLineTexture_GetLevelDesc((D3DLineTexture *)mTexture, level, &desc);
            D3DRECT rect;
            rect.x1 = rect.y1 = 0;
            rect.x2 = desc.Width;
            rect.y2 = desc.Height;
            RndMat *mat = TheShaderMgr.GetWork();
            mat->SetDiffuseTex(this);
            mat->SetTexWrap(kTexWrapClamp);
            mat->SetBlend(RndMat::kBlendSrc);
            if (mType == kShadowMap) {
                mat->SetZMode(kZModeForce);
            } else {
                mat->SetZMode(kZModeDisable);
            }
            Hmx::Rect quad(0.0f, 0.0f, desc.Width, desc.Height);
            if (mType != kShadowMap) {
                TheDxRnd.DrawRect(
                    quad, mat, kDownsampleShader, Hmx::Color(), nullptr, nullptr
                );
                D3DDevice_Resolve(
                    TheDxRnd.Device(), 0, &rect, mTexture, nullptr, level, 0, nullptr,
                    1.0f, 0, nullptr
                );
            } else {
                TheDxRnd.DrawRect(
                    quad, mat, kDownsampleDepthShader, Hmx::Color(), nullptr, nullptr
                );
                D3DDevice_Resolve(
                    TheDxRnd.Device(), 4, &rect, mTexture, nullptr, level, 0, nullptr,
                    1.0f, 0, nullptr
                );
            }
        }
        D3DDevice_SetSamplerState_MinMipLevel(TheDxRnd.Device(), 0, 13);
        D3DDevice_SetSamplerState_MaxMipLevel(TheDxRnd.Device(), 0, 0);
        D3DDevice_SetSamplerState_MinFilter(TheDxRnd.Device(), 0, 1);
        D3DDevice_SetSamplerState_MagFilter(TheDxRnd.Device(), 0, 1);
        D3DDevice_SetSamplerState_MipFilter3(TheDxRnd.Device(), 0, 1, 0x80000000);
    }
}

// 0x82735490 (slot 25).
void DxTex::FinishDrawTarget() {
    MILO_ASSERT(mType & kRendered, 0x122);
    ResolveMipChain();
    D3DDevice_SetPredication(TheDxRnd.Device(), 0);
    TheDxRnd.SetReverseZ(true);
}

// 0x82733DC8 (slot 27). Unconditional: retail does not test mTexture.
bool DxTex::TexelsLock(void *&p) {
    UINT baseData;
    XGGetTextureLayout(
        mTexture, &baseData, nullptr, nullptr, nullptr, 0, nullptr, nullptr, nullptr,
        nullptr, 0
    );
    p = (void *)baseData;
    return true;
}

// 0x82733D20 (slot 24).
void DxTex::MakeDrawTarget() {
    MILO_ASSERT(mType & kRendered, 0xF0);
    if (mTexture) {
        TheDxRnd.Resume();
        D3DDevice_SetPredication(TheDxRnd.Device(), 3);
        D3DDevice_SetRenderTarget_External(TheDxRnd.Device(), 0, mRenderTarget);
        D3DDevice *dev = TheDxRnd.Device();
        D3DSurface *depth;
        if (mType != kDepthVolumeMap) {
            depth = mDepthRT;
        } else {
            depth = nullptr;
        }
        D3DDevice_SetDepthStencilSurface(dev, depth);
        NgMat::SetCurrent(nullptr);
        TheDxRnd.SetReverseZ(mType != kShadowMap);
    }
}

// Hi-Z tile count for a depth surface: an inline whose by-value parameters are
// overwritten with their aligned values (retail homes the 16-aligned height to
// the frame, `stw r11,0x84(r31)` at 0x82734D34).
static inline UINT HierarchicalZTiles(UINT Width, UINT Height) {
    Width = (Width + 31) & ~31;
    Height = (Height + 15) & ~15;
    return Width * Height / 0x200;
}

// Thin by-value wrapper: its inlined parameters are what retail homes to the
// frame just before each D3DDevice_CreateSurface call (0x82734BCC/0x82734D90).
static inline D3DSurface *CreateEdramSurface(
    UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample,
    const D3DSURFACE_PARAMETERS *pParams
) {
    return D3DDevice_CreateSurface(Width, Height, Format, MultiSample, pParams);
}

// 0x82734A28 (DxTex vtable slot 32). The RB3 shape, read off retail bytes:
//  - the device caps are still queried, but both caps asserts are compiled out;
//  - no allocation goes through Begin/EndMemTrackFileName, and the physical
//    allocations take PhysicalAllocTracked's three-argument form;
//  - the regular path has no TexMgr/CRC lookup, no lowest-mip substitution and
//    no cap on the mip count (numLevels = NumMips() + 1);
//  - three movie buffers;
//  - the final path is two-way: scratch -> 0x28280044 / 16 bpp, otherwise
//    D3DFMT_LIN_L8 / 8 bpp.
// The two EDRAM-overflow reports are compiled out but still evaluate
// PathName(this) (retail: bl PathName at 0x82734BE8 / 0x82734DAC).
void DxTex::SyncBitmap() {
    PhysMemTypeTracker tracker("D3D(phys):Tex");
    PreDeviceReset();

    D3DCAPS9 d3dcaps;
    D3DDevice_GetDeviceCaps(TheDxRnd.Device(), &d3dcaps);
    MILO_ASSERT(
        (mWidth <= d3dcaps.MaxTextureWidth) && (mHeight <= d3dcaps.MaxTextureHeight), 941
    );
    MILO_ASSERT(
        !(d3dcaps.TextureCaps & D3DPTEXTURECAPS_SQUAREONLY) || mWidth == mHeight, 942
    );

    bool isRendered = (mType & kRendered) != 0;
    bool isMovie = (mType & kMovie) != 0;
    bool isScratch = (mType & kScratch) != 0;

    if (isRendered) {
        if (mWidth != 0 && mHeight != 0) {
            if (mType == kShadowMap) {
                mFormat = D3DFMT_D24S8;
            } else {
                mFormat = D3DFMT_A8R8G8B8;
            }
            unkac = false;
            UINT colorTiles = 0;
            UINT tiles;
            D3DSURFACE_PARAMETERS params;
            if (mType == kShadowMap) {
                mRenderTarget = nullptr;
            } else {
                D3DFORMAT edramFormat = mFormat;
                switch (edramFormat) {
                case D3DFMT_A16B16G16R16:
                    edramFormat = D3DFMT_A16B16G16R16_EDRAM;
                    break;
                case D3DFMT_A2B10G10R10:
                    edramFormat = D3DFMT_A2B10G10R10F_EDRAM;
                    break;
                case D3DFMT_G16R16:
                    edramFormat = D3DFMT_G16R16_EDRAM;
                    break;
                }
                memset(&params, 0, sizeof(params));
                params.ColorExpBias = 0;
                params.Base = 0;
                params.HierarchicalZBase = -1;
                params.HiZFunc = D3DHIZFUNC_DEFAULT;
                {
                    int gpuFormat = edramFormat & 0x3f;
                    UINT bytesPerPixel = 4;
                    UINT alignedWidth = (((UINT)mWidth + 79) / 80) * 80;
                    UINT alignedHeight = ((UINT)mHeight + 15) & ~15;
                    if (gpuFormat == 0x15 || gpuFormat == 0x20 || gpuFormat == 0x25) {
                        bytesPerPixel = 8;
                    }
                    tiles = alignedHeight * alignedWidth * bytesPerPixel / 0x1400;
                }
                colorTiles = tiles;
                if (tiles < 0x800) {
                    if (sEDRamChecksEnabled && tiles > TheDxRnd.EdramBase()) {
                        unkac = true;
                    }
                    mRenderTarget = CreateEdramSurface(
                        mWidth, mHeight, edramFormat, D3DMULTISAMPLE_NONE, &params
                    );
                    DX_ASSERT(mRenderTarget, 1026);
                } else {
                    MILO_FAIL(
                        "Render target '%s' exceeds available\nEDRAM area (requested %d of %d color tiles)\n",
                        PathName(this), tiles, 0x800
                    );
                    mRenderTarget = nullptr;
                }
            }

            if (mNumMips != 0) {
                mTexture = new D3DTexture;
                UINT dwTextureSize = XGSetTextureHeaderEx(
                    mWidth, mHeight, mNumMips, 0, mFormat, 0, 1, 0, -1, 0, mTexture, nullptr,
                    nullptr
                );
                MILO_ASSERT(dwTextureSize != 0, 1059);
                void *textureBuffer =
                    PhysicalAllocTracked(dwTextureSize, 0x404, "Tex(phys)");
                MILO_ASSERT(textureBuffer != NULL, 1065);
                XGOffsetBaseTextureAddress(mTexture, textureBuffer, textureBuffer);
            } else {
                mTexture = (D3DTexture *)D3DDevice_CreateTexture(
                    mWidth, mHeight, 1, 1, 0, mFormat, 0, (D3DRESOURCETYPE)3
                );
                DX_ASSERT(mTexture, 1073);
            }

            bool wantsDepth = (mType & kRendered) && !(mType & 0x20);
            if (wantsDepth || mType == kDepthVolumeMap) {
                D3DFORMAT depthFormat = D3DFMT_D24FS8;
                if (mType == kShadowMap) {
                    depthFormat = D3DFMT_D24S8;
                }
                D3DSURFACE_PARAMETERS depthParams;
                memset(&depthParams, 0, sizeof(depthParams));
                depthParams.Base = colorTiles;
                depthParams.ColorExpBias = 0;
                depthParams.HierarchicalZBase = 0;
                depthParams.HiZFunc = D3DHIZFUNC_DEFAULT;
                UINT hzTiles;
                {
                    int gpuFormat = depthFormat & 0x3f;
                    UINT bytesPerPixel = 4;
                    UINT alignedWidth = (((UINT)mWidth + 79) / 80) * 80;
                    UINT alignedHeight = ((UINT)mHeight + 15) & ~15;
                    if (gpuFormat == 0x15 || gpuFormat == 0x20 || gpuFormat == 0x25) {
                        bytesPerPixel = 8;
                    }
                    tiles = colorTiles + alignedHeight * alignedWidth * bytesPerPixel / 0x1400;
                    hzTiles = HierarchicalZTiles(mWidth, mHeight);
                }
                if (tiles < 0x800 && hzTiles < 0xe10) {
                    if (sEDRamChecksEnabled
                        && (tiles > TheDxRnd.EdramBase()
                            || hzTiles > TheDxRnd.EdramHzBase())) {
                        unkac = true;
                    }
                    mDepthRT = CreateEdramSurface(
                        mWidth, mHeight, depthFormat, D3DMULTISAMPLE_NONE, &depthParams
                    );
                    DX_ASSERT(mDepthRT, 1114);
                } else {
                    MILO_FAIL(
                        "Depth surface '%s' exceeds available EDRAM or hi-z area\n(requested %d of %d color tiles and %d of %d hi-z tiles)\nDepth surface creation failed.",
                        PathName(this), tiles, 0x800, hzTiles, 0xe10
                    );
                    mDepthRT = nullptr;
                }
            } else {
                mDepthRT = nullptr;
            }
        }
    } else if (IsBackBuffer()) {
        mFormat = D3DFMT_A8R8G8B8;
        mTexture = (D3DTexture *)D3DDevice_CreateTexture(
            mWidth, mHeight, 1, 1, 0, mFormat, 0, (D3DRESOURCETYPE)3
        );
        DX_ASSERT(mTexture, 1161);
    } else if (!isMovie && !isScratch) {
        if (!mBitmap.Pixels()) {
            return;
        }
        RndBitmap &bitmap = mBitmap;
        mFormat = TheDxRnd.D3DFormatForBitmap(bitmap);
        int numLevels = bitmap.NumMips() + 1;
        mTexture = (D3DTexture *)D3DDevice_CreateTexture(
            mWidth, mHeight, 1, numLevels, 0, mFormat, 0, (D3DRESOURCETYPE)3
        );
        XGTEXTURE_DESC desc;
        XGGetTextureDesc(mTexture, 0, &desc);
        RndBitmap converted;
        D3DLOCKED_RECT rect;
        RndBitmap *bmp = &bitmap;
        if (bitmap.Palette() || bitmap.Bpp() == 0x18) {
            converted.Create(*bmp, 0x20, bmp->Order(), nullptr);
            bmp = &converted;
        }
        for (int level = 0; level < numLevels; level++) {
            MILO_ASSERT(bmp, 1332);
            mTexture->LockRect(level, &rect, nullptr, 0);
            // Retail forms the GPU format and the pixel pointer BEFORE calling
            // DxtRowBytes (r22/r23 at 0x82734EF0/0x82734EF4), so both are locals.
            DWORD gpuFormat = desc.Format & 0x3f;
            void *pixels = bmp->Pixels();
            XGTileTextureLevel(
                desc.Width, desc.Height, level, gpuFormat, numLevels == 1, rect.pBits,
                nullptr, pixels, bmp->DxtRowBytes(), nullptr
            );
            mTexture->UnlockRect(level);
            bmp = bmp->nextMip();
        }
        bitmap.Reset();
    } else if (!isScratch && !(mType & 0x20)) {
        // Movie triple-buffer: DXT1 with GPUENDIAN_NONE.
        mFormat = (D3DFORMAT)0x1a200012;
        if (mWidth != 0 && mHeight != 0) {
            for (int i = 0; i < 3; i++) {
                mMovieTextures[i] = (D3DTexture *)D3DDevice_CreateTexture(
                    mWidth, mHeight, 1, 1, 0, mFormat, 1, (D3DRESOURCETYPE)3
                );
                DX_ASSERT(mMovieTextures[i], 1231);
            }
            mTexture = mMovieTextures[(mMovieBufIdx + 1) % 3];
        }
    } else {
        if (isScratch) {
            mFormat = (D3DFORMAT)0x28280044;
            mBpp = 0x10;
        } else {
            mFormat = D3DFMT_LIN_L8;
            mBpp = 8;
        }
        mTexture = new D3DTexture;
        UINT size = XGSetTextureHeader(
            mWidth, mHeight, 1, 4, mFormat, 0, 0, -1, 0, mTexture, nullptr, nullptr
        );
        size = (size + 0xfff) & ~0xfff;
        void *ptr = PhysicalAllocTracked(size, 4, "Tex(phys)");
        MILO_ASSERT(ptr, 1198);
        XGOffsetResourceAddress(mTexture, ptr);
    }
}
