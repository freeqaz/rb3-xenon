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

// 0x8273EC10 (DxCubeTex vtable slot 23). Each face is uploaded level by level
// through the cube-texture lock; a face with no bitmap only evaluates the
// (stripped) report's PathName(this); the current material is cleared last.
void DxCubeTex::Sync() {
    PhysMemTypeTracker tracker("D3D(phys):CubeTex");
    D3DFORMAT format = TheDxRnd.D3DFormatForBitmap(mBitmap[kCubeFaceRight]);
    int numLevels = props.mNumMips + 1;
    mTex = D3DDevice_CreateTexture(
        props.mWidth, props.mWidth, 6, numLevels, 0, format, 0, D3DRTYPE_CUBETEXTURE
    );
    XGTEXTURE_DESC desc;
    XGGetTextureDesc(mTex, 0, &desc);
    for (int face = 0; face < 6; face++) {
        RndBitmap bitmap;
        RndBitmap *bmp = &mBitmap[face];
        if (bmp->Width() != 0 && bmp->Height() != 0) {
            if (bmp->Palette() || bmp->Bpp() == 0x18) {
                bitmap.Create(*bmp, 0x20, bmp->Order(), nullptr);
                bmp = &bitmap;
            }
            for (int level = 0; level < numLevels; level++) {
                D3DLOCKED_RECT rect;
                D3DCubeTexture_LockRect(
                    (D3DCubeTexture *)mTex, (D3DCUBEMAP_FACES)face, level, &rect, nullptr, 0
                );
                DWORD gpuFormat = desc.Format & 0x3f;
                void *pixels = bmp->Pixels();
                XGTileTextureLevel(
                    desc.Width, desc.Height, level, gpuFormat, 0, rect.pBits, nullptr,
                    pixels, bmp->DxtRowBytes(), nullptr
                );
                D3DCubeTexture_UnlockRect(
                    (D3DCubeTexture *)mTex, (D3DCUBEMAP_FACES)face, level
                );
                bmp = bmp->nextMip();
            }
            mBitmap[face].Reset();
        } else {
            MILO_FAIL("%s: cube face missing", PathName(this));
        }
    }
    NgMat::SetCurrent(nullptr);
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
