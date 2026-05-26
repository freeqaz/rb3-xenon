#include "rnddx9/TexMgr.h"
#include "Rnd.h"
#include "rnddx9/Rnd.h"
#include "rndobj/TexMgr.h"
#include "utl/MemTrack.h"
#include "xdk/D3D9.h"
#include "xdk/d3d9i/d3d9.h"

DxRndTexMgr TheDxTexMgr;
TexMgr &TheTexMgr = TheDxTexMgr;

void DxRndTexMgr::OnReleaseResource(void *v) {
    D3DResource *resource = static_cast<D3DResource *>(v);
    TheDxRnd.AutoRelease(resource);
}

bool DxRndTexMgr::
    CreateSurface(const char *filename, Hmx::CRC key, UINT w, UINT h, UINT levels, DWORD, D3DFORMAT fmt, DWORD pool, D3DTexture **pTex, void **) {
    void *data = Get(key);
    if (data) {
        *pTex = (D3DTexture *)data;
        return true;
    } else {
        BeginMemTrackFileName(filename);
        *pTex = (D3DTexture *)D3DDevice_CreateTexture(
            w, h, 1, levels, 0, fmt, pool, D3DRTYPE_TEXTURE
        );
        DX_ASSERT(*pTex, 0x23);
        EndMemTrackFileName();
        ReserveRes(key, *pTex);
        return false;
    }
}
