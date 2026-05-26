#pragma once
#include "rndobj/TexMgr.h"
#include "utl/CRC.h"
#include "xdk/D3D9.h"

class DxRndTexMgr : public TexMgr {
public:
    bool
    CreateSurface(const char *, Hmx::CRC, unsigned int, unsigned int, unsigned int, unsigned long, D3DFORMAT, unsigned long, D3DTexture **, void **);

protected:
    virtual void OnReleaseResource(void *);
};

extern DxRndTexMgr TheDxTexMgr;
