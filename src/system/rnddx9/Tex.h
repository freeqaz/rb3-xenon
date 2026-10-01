#pragma once
#include "obj/Object.h"
#include "rnddx9/Object.h"
#include "rndobj/Tex.h"
#include "xdk/D3D9.h"
#include "xdk/d3d9i/d3d9.h"

class DxTex : public RndTex, public DxObject {
public:
    virtual ~DxTex();
    OBJ_CLASSNAME(Tex)
    OBJ_SET_TYPE(Tex)
    virtual void LockBitmap(RndBitmap &, int);
    virtual void UnlockBitmap();
    virtual void MakeDrawTarget();
    virtual void FinishDrawTarget();
    virtual void Compress(AlphaCompress);
    virtual bool TexelsLock(void *&);
    virtual unsigned int TexelsPitch() const;
    virtual void Select(int);
    virtual void SyncBitmap();
    virtual void PreDeviceReset();
    virtual void PostDeviceReset();

    static void Init();
    NEW_OBJ(DxTex)
    static void SetEDRamChecksEnabled(bool enabled) { sEDRamChecksEnabled = enabled; }

    void ResolveMipChain();
    void *StartCompress(AlphaCompress);
    void DoCompress(void *);
    void FinishCompress(void *);
    void SetDeviceTex(D3DTexture *);
    D3DSurface *GetRT();
    D3DSurface *GetDepthRT();
    D3DSurface *GetMovieSurface();
    void SwapMovieSurface();
    D3DTexture *Tex() const { return mTexture; }

private:
    static bool sEDRamChecksEnabled;

    void ResetSurfaces();
    D3DSurface *GetSurfaceLevel(int);

protected:
    DxTex();

    virtual void PresyncBitmap() { ResetSurfaces(); }

    D3DFORMAT mFormat; // 0x74
    D3DTexture *mTexture; // 0x78
    int unk84;
    D3DSurface *mRenderTarget; // 0x80
    D3DSurface *mDepthRT; // 0x84
    int mMovieBufIdx; // 0x88
    // THREE movie buffers in RB3 retail (DC3 has two): ??0DxTex (0x82734728)
    // zeroes 0x8c/0x90/0x94, ResetSurfaces (0x827355D0) and SyncBitmap's movie
    // path loop 3 times, and SwapMovieSurface (0x82733E30) advances `% 3`.
    D3DTexture *mMovieTextures[3]; // 0x8c
    D3DLOCKED_RECT mLockedRect; // 0x98
    D3DSurface *unka4; // 0xa0
    int unka8; // 0xa4
    bool unkac; // 0xa8 -- set by SyncBitmap when a target overruns the EDRAM base
};
