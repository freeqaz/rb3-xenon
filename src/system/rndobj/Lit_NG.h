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
    virtual void BlurShadowRT(float, float);

    bool WantShadows() const;
    bool SphereConeTest(const Vector3 &, float);
    void SetShadowTransforms();
    bool HaveShadows(std::vector<RndDrawable *> &);

    // One shadow render target, blurred in place. The third word is a
    // TheRnd.DrawCount() stamp: the ctor sets it to -1, CheckShadowMap skips
    // its work when it equals the current draw count and stores the count
    // when done. The Hmx::Object vbase sits at NgLight+0x16c.
    RndTex *mShadowRT; // 0x15c
    RndTex *mShadowMapTex; // 0x160
    int mShadowDrawCount; // 0x164
};
