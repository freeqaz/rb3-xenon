#pragma once
#include "obj/Object.h"
#include "rndobj/DOFProc.h"
#include "rndobj/PostProc.h"
#include "rndobj/Tex.h"

class NgDOFProc : public DOFProc, public PostProcessor {
public:
    NgDOFProc();
    virtual ~NgDOFProc();
    // registers as DOFProc — transparent factory replacement
    OBJ_CLASSNAME(DOFProc);
    OBJ_SET_TYPE(DOFProc);
    virtual float FocalPlane() { return mFocalPlane; }
    virtual float BlurDepth() { return mBlurDepth; }
    virtual float MaxBlur() { return mMaxBlur; }
    virtual float MinBlur() { return mMinBlur; }
    // PostProcessor
    virtual void DoPost();
    virtual const char *GetProcType() { return "NgDOFProc"; }

    static void Init();
    static void Terminate();
    NEW_OBJ(NgDOFProc)

protected:
    // DOFProc
    virtual void Set(const RndCam *, float, float, float, float);
    virtual bool Enabled() const;
    virtual void UnSet() { mEnabled = false; }

    bool mEnabled; // 0x30
    float mDepthOfFieldScale; // 0x34
    float mDepthOfFieldBias; // 0x38
    float mFocalPlane; // 0x3c
    float mBlurDepth; // 0x40
    float mMinBlur; // 0x44
    float mMaxBlur; // 0x48
    RndTex *mBlurTex[2]; // 0x4c
};
