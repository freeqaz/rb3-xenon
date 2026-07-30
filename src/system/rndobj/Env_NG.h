#pragma once
#include "obj/Object.h"
#include "rndobj/Env.h"
#include "rndobj/Lit.h"

class NgEnviron : public RndEnviron {
public:
    // Hmx::Object
    // Base-name registration (see SpotlightDrawer_NG.h): retail band.exe has no
    // "NgEnviron" C string; the RndEnviron base registers the literal Environ,
    // and DC3's Env_NG.h declares Environ here too.
    OBJ_CLASSNAME(Environ);
    OBJ_SET_TYPE(NgEnviron);
    // RndEnviron
    virtual void Select(const Vector3 *);
    virtual void UpdateApproxLighting(const Vector3 *);
    virtual int NumLights_Real() const { return mNumLightsReal; }
    virtual int NumLights_Approx() const { return mNumLightsApprox; }
    virtual int NumLights_Point() const { return mNumLightsPoint; }
    virtual int NumLights_Proj() const { return mNumLightsProj; }
    virtual bool HasPointCubeTex() const { return mHasPointCubeTex; }
    virtual RndLight::ProjectedBlend GetProjectedBlend() const { return mProjectedBlend; }

    NEW_OBJ(NgEnviron)

protected:
    NgEnviron();

    RndLight::ProjectedBlend mProjectedBlend; // 0x298
    int mNumLightsReal; // 0x29c
    int mNumLightsApprox; // 0x2a0
    int mNumLightsPoint; // 0x2a4
    int mNumLightsProj; // 0x2a8
    bool mHasPointCubeTex; // 0x2ac
};
