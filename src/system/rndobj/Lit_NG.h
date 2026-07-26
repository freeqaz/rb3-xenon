#pragma once
#include "math/Vec.h"
#include "obj/Object.h"
#include "rndobj/Draw.h"
#include "rndobj/Lit.h"
#include "rndobj/Tex.h"

class NgLight : public RndLight {
public:
    OBJ_CLASSNAME(Light)
    OBJ_SET_TYPE(Light)
    virtual void Copy(const Hmx::Object *, CopyType);
    virtual void Load(BinStream &);
    virtual ~NgLight();

    NEW_OBJ(NgLight);

    void CheckShadowMap();
    RndTex *GetShadowMapTex() const { return mShadowMapTex; }

    static void Init();

protected:
    NgLight();
    virtual void RenderShadows(std::vector<RndDrawable *> &);
    virtual void SetAndClearShadowViewport();
    virtual void BlurShadowRT();

    bool WantShadows() const;
    bool SphereConeTest(const Vector3 &, float);
    RndTex *CreateShadowTex();
    void SetShadowTransforms();
    bool HaveShadows(std::vector<RndDrawable *> &);

    // NOTE(laneAJ-c): dc3 is newer and added a trailing `int unk18c` here (a
    // TheRnd.DrawCount() re-entrancy cache guarding CheckShadowMap). RB3
    // retail predates it: the vbase Hmx::Object subobject sits at NgLight+0x16c
    // in the target but at +0x170 with the extra word (verified with
    // /d1reportSingleClassLayoutNgLight), which shifted NgLight::Load and
    // NgLight::Copy's `this` adjustments by 4.
    RndTex *mShadowRT; // 0x15c
    RndTex *mShadowMapTex; // 0x160
    RndTex *unk188; // 0x164
};
