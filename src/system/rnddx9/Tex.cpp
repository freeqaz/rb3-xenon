#include "rnddx9/Tex.h"
#include "Rnd.h"
#include "Tex.h"
#include "os/Debug.h"
#include "rnddx9/Rnd.h"
#include "rnddx9/TexMgr.h"
#include "rndobj/Rnd.h"
#include "rndobj/Tex.h"
#include "xdk/d3d9i/d3d9.h"
#include "xdk/d3d9i/d3d9types.h"

std::vector<DxTex *> gAllTextures;

DxTex::DxTex()
    : mFormat((D3DFORMAT)-1), mTexture(0), unk84(0), mRenderTarget(0), mDepthRT(0),
      mMovieBufIdx(0), mLockedRect(), unka4(0), unka8(0), unkac(0) {
    gAllTextures.push_back(this);
    for (int i = 0; i < 2; i++) {
        mMovieTextures[i] = 0;
    }
}

DxTex::~DxTex() {
    ResetSurfaces();
    auto it = std::find(gAllTextures.begin(), gAllTextures.end(), this);
    MILO_ASSERT(it != gAllTextures.end(), 0x2D7);
    gAllTextures.erase(it);
}

void DxTex::Compress(AlphaCompress a) {
    void *v = StartCompress(a);
    DoCompress(v);
    FinishCompress(v);
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

D3DSurface *DxTex::GetMovieSurface() {
    if (!(mType & kMovie)) {
        return nullptr;
    } else {
        mTexture = mMovieTextures[mMovieBufIdx];
        return GetSurfaceLevel(0);
    }
}

void DxTex::SwapMovieSurface() {
    MILO_ASSERT((mType & kMovie) > 0, 0x2F5);
    mMovieBufIdx = (mMovieBufIdx + 1) % 2;
    mTexture = mMovieTextures[mMovieBufIdx];
}

void DxTex::ResetSurfaces() {
    // Clean up movie surface double-buffer
    for (int i = 0; i < 2; i++) {
        if (mTexture == mMovieTextures[i]) {
            mTexture = nullptr;
        }
        TheDxRnd.AutoRelease(mMovieTextures[i]);
        mMovieTextures[i] = nullptr;
    }

    // Delete main texture for certain types
    bool _bit0 = (mType & kRendered) != 0;
    if (((_bit0) && mNumMips) || ((mType & kMovie) && (mType & 0x20))
        || (mType & kScratch) || (mType & kRegularLinear)) {
        TheDxRnd.AutoDelete(mTexture);
        mTexture = nullptr;
    }

    // Clear texture pointer for movie/scratch/device types (0x1204 = kMovie | kScratch | kDeviceTexture)
    if (mType & 0x1204) {
        mTexture = nullptr;
    }

    // Release managed texture resource
    if (!TheDxTexMgr.ReleaseRes(unk2c)) {
        TheDxRnd.AutoRelease(mTexture);
    }
    mTexture = nullptr;

    // Clean up render target surfaces
    TheDxRnd.AutoRelease(mRenderTarget);
    mRenderTarget = nullptr;
    TheDxRnd.AutoRelease(mDepthRT);
    mDepthRT = nullptr;
}
