#include "rnddx9/CubeTex.h"
#include "../../Memory.h"
#include "Rnd.h"
#include "rnddx9/Rnd.h"
#include "rndobj/Bitmap.h"
#include "rndobj/Mat_NG.h"
#include "xdk/D3D9.h"
#include "xdk/XGRAPHICS.h"

DxCubeTex::DxCubeTex() : mTex(0) {}
DxCubeTex::~DxCubeTex() { Reset(); }

void DxCubeTex::Select(int x) {
    D3DDevice_SetTexture(TheDxRnd.Device(), x, mTex, 0x8000000000000000 >> (x + 0x20U));
}

void DxCubeTex::Reset() {
    TheDxRnd.AutoRelease(mTex);
    mTex = nullptr;
    NgMat::SetCurrent(nullptr);
}

void DxCubeTex::Sync() {
    PhysMemTypeTracker tracker("D3D(phys):CubeTex");

        DX_ASSERT(mTex = D3DDevice_CreateTexture(
        props.mWidth, props.mWidth, 6, props.mNumMips + 1, 0,
        TheDxRnd.D3DFormatForBitmap(mBitmap[kCubeFaceRight]), 0,
        D3DRTYPE_CUBETEXTURE
    ), 0x38);

    XGTEXTURE_DESC desc;
    XGGetTextureDesc(mTex, 0, &desc);

    NgMat::SetCurrent(nullptr);
    int numMips = props.mNumMips + 1;

    for (int face = 0; face < 6; face++) {
        RndBitmap bitmap;
        bitmap.Reset();

        RndBitmap *pWork = &mBitmap[face];

        if (pWork->Width() != 0 && pWork->Height() != 0) {
            if (pWork->Buffer() != nullptr || pWork->Bpp() == 0x18) {
                bitmap.Create(*pWork, 0x20, pWork->NumMips(), nullptr);
                pWork = &bitmap;
            }

            if (numMips > 0) {
                int mipIdx = 0;
                do {
                    if (pWork != nullptr) {
                        u32 rowPitch = pWork->DxtRowBytes();
                        XGTileTextureLevel(desc.Width, desc.Height, mipIdx, 0, 0, nullptr, nullptr,
                                         pWork->Buffer(), rowPitch, nullptr);
                    }
                    pWork = pWork->nextMip();
                    mipIdx++;
                } while (mipIdx < numMips);
            }

            mBitmap[face].Reset();
        }

        bitmap.Reset();
    }

}

// sw2 scatter-include (default/system/rnddx9/CubeTex <- band3/meta_band/AppLabel.cpp)
#define gRev gRev_AppLabel
#define gAltRev gAltRev_AppLabel
#include "band3/meta_band/AppLabel.cpp"
#undef gRev
#undef gAltRev

// sw2 scatter-include (default/system/rnddx9/CubeTex <- rnddx9/MultiMesh.cpp)
#include "rnddx9/MultiMesh.cpp"

// sw2 scatter-include (default/system/rnddx9/CubeTex <- rnddx9/Cam.cpp)
#include "rnddx9/Cam.cpp"

// sw2 scatter-include (default/system/rnddx9/CubeTex <- rnddx9/Lit.cpp)
#include "rnddx9/Lit.cpp"

// sw2 scatter-include (default/system/rnddx9/CubeTex <- rnddx9/Part.cpp)
#include "rnddx9/Part.cpp"
