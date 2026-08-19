#pragma once
#include "obj/Object.h"
#include "rndobj/BaseMaterial.h"
#include "rndobj/Draw.h"
#include "rndobj/PostProc.h"
#include "rndobj/Tex.h"

class RndSoftParticleBuffer : public Hmx::Object, public PostProcessor {
public:
    RndSoftParticleBuffer();
    virtual ~RndSoftParticleBuffer();
    // Hmx::Object
    OBJ_CLASSNAME(SoftParticleBuffer);
    OBJ_SET_TYPE_ENGINE(SoftParticleBuffer);
    // PostProcessor
    virtual void DoPost();
    virtual const char *GetProcType() { return "SoftParticleBuffer"; }

    NEW_OBJ(RndSoftParticleBuffer)
    void Queue(RndDrawable *, RndMat::Blend);

private:
    void AllocateData(unsigned int, unsigned int, unsigned int);
    void FreeData();
    void BlurSurface();

    RndTex *mSurfaces[2]; // 0x2c
    int unk38; // 0x34
    ObjPtrList<RndDrawable> mSoftParticleDrawList; // 0x38
};
