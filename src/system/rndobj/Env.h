#pragma once
#include "BoxMap.h"
#include "Lit.h"
#include "math/Vec.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/Timer.h"
#include "rndobj/ColorXfm.h"
#include "rndobj/Trans.h"
#include "rndobj/Lit.h"
#include "utl/MemMgr.h"

// Retail RB3-360 RndEnviron derives DIRECTLY from Hmx::Object — NOT from
// RndTransformable + RndDrawable (DC3's newer lineage). Verified from the retail
// machine code:
//   * RndEnviron::Save (fn_823F51C0) streams members starting at 0x28 — exactly
//     where the Hmx::Object base ends (vtable@0, mTypeProps@4..0x10, mTypeDef@0x10,
//     mNote@0x14, mName@0x18, mDir@0x1c, mRefs ring@0x20 = size 0x28). A
//     RndTransformable+RndDrawable base would push the first member to ~0xd8.
//   * LightPreset::FillEnvPresetData / AnimateEnvFromPreset load the ambient-fog
//     owner pointer at [env+0x7c] (ObjOwnerPtr mAmbientFogOwner@0x74, .Ptr()@0x7c)
//     and the owned env's fog floats at +0x84/+0x88 — DC3's layout had these
//     ~0xB0 higher (mAmbientFogOwner@0x14c).
//   * RndEnviron::OnRemoveAllLights (fn_823F5430) compares [this+0x7c] to confirm
//     mAmbientFogOwner.Ptr()@0x7c.
//   * The ctor (fn_823F5BB8) + Save + SyncProperty pin the tail: three byte-packed
//     bools @0x15c/0x15d/0x15e, mAOStrength@0x160, Timer@0x168, tail floats
//     @0x198..0x1a4, mUseToneMapping@0x1a8.
// rb3-Wii's Env.h is the same Hmx::Object lineage; retail differs only by the
// larger 0x28 Object base (Wii's is 0x1c) and by carrying mNumLights*/mHasPointCubeTex
// in the NgEnviron subclass rather than in RndEnviron itself.
class RndEnviron : public Hmx::Object {
    friend class LightPreset;
    friend class SpotlightDrawer;
public:
    virtual ~RndEnviron();
    virtual void Replace(ObjRef *, Hmx::Object *);
    OBJ_CLASSNAME(Environ);
    OBJ_SET_TYPE(Environ);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void Select(const Vector3 *);
    virtual void UpdateApproxLighting(const Vector3 *);
    virtual int NumLights_Real() const { return mLightsReal.size(); }
    virtual int NumLights_Approx() const { return mLightsApprox.size(); }
    virtual bool IsFake(RndLight *) const;
    virtual bool IsReal(RndLight *) const;

    OBJ_MEM_OVERLOAD(0x1B);
    NEW_OBJ(RndEnviron)
    static void Init() { REGISTER_OBJ_FACTORY(RndEnviron) }
    static RndEnviron *Current() { return sCurrent; }
    static Vector3 *CurrentPos() {
        if (sCurrentPosSet)
            return &sCurrentPos;
        else
            return nullptr;
    }

    void SetUseApproxLocal(bool b) { mUseApprox_Local = b; }
    void SetUseApproxGlobal(bool b) { mUseApprox_Global = b; }
    void SetUseApproxes(bool b) {
        SetUseApproxLocal(b);
        SetUseApproxGlobal(b);
    }
    bool GetUseApprox() const { return mUseApprox_Local || mUseApprox_Global; }
    bool UsesApproxLocal() const { return mUseApprox_Local; }
    bool UsesApproxGlobal() const { return mUseApprox_Global; }

    const Transform &ColorXfm() const;
    bool FogEnable() const;
    Transform LRFadeRef() const;
    void RemoveLight(RndLight *);
    void AddLight(RndLight *);
    bool IsValidRealLight(const RndLight *l) const;
    RndEnviron *AmbientFogOwner() const { return mAmbientFogOwner.Ptr(); }
    const Hmx::Color &AmbientColor() const { return mAmbientFogOwner->mAmbientColor; }
    void SetAmbientColor(const Hmx::Color &col) {
        mAmbientFogOwner->mAmbientColor.Set(col.red, col.green, col.blue);
    }
    bool FadeOut() const { return mFadeOut; }
    bool UseColorAdjust() const { return mUseColorAdjust; }
    float FadeStart() const { return mFadeStart; }
    float FadeEnd() const { return mFadeEnd; }
    const Hmx::Color& FogColor() const { return mAmbientFogOwner->mFogColor; }
    void SetFogColor(const Hmx::Color &col) {
        mAmbientFogOwner->mFogColor.Set(col.red, col.green, col.blue);
    }
    void SetFogEnable(bool b) { mAmbientFogOwner->mFogEnable = b; }
    ObjPtrList<RndLight>& LightsReal() { return mLightsReal; }
    ObjPtrList<RndLight>& LightsApprox() { return mLightsApprox; }
    bool AOEnabled() const { return mAOEnabled; }
    float AOStrength() const { return mAOStrength; }
    bool UseToneMapping() const { return mUseToneMapping; }

    float FogStart() const { return mAmbientFogOwner->mFogStart; }
    float FogEnd() const { return mAmbientFogOwner->mFogEnd; }
    void SetFogRange(float start, float end) {
        mAmbientFogOwner->mFogStart = start;
        mAmbientFogOwner->mFogEnd = end;
    }
    void SetFadeRange(float start, float end) {
        mFadeStart = start;
        mFadeEnd = end;
    }

protected:
    RndEnviron();

    bool IsLightInList(const RndLight *, const ObjPtrList<RndLight> &) const;
    void OnRemoveAllLights();
    void ReclassifyLights();
    DataNode OnAllowableLights_Real(const DataArray *);
    DataNode OnAllowableLights_Approx(const DataArray *);

    static BoxMapLighting sGlobalLighting;
    static RndEnviron *sCurrent;
    static Vector3 sCurrentPos;
    static bool sCurrentPosSet;

    // Retail RB3-360 member layout (verified from RndEnviron::Save / ctor /
    // LightPreset anchors). Hmx::Object base ends at 0x28.
    ObjPtrList<RndLight> mLightsReal; // 0x28
    ObjPtrList<RndLight> mLightsApprox; // 0x3c
    ObjPtrList<RndLight> mLightsOld; // 0x50
    Hmx::Color mAmbientColor; // 0x64
    ObjOwnerPtr<RndEnviron> mAmbientFogOwner; // 0x74 (.Ptr()@0x7c)
    bool mFogEnable; // 0x80
    float mFogStart; // 0x84
    float mFogEnd; // 0x88
    Hmx::Color mFogColor; // 0x8c
    bool mFadeOut; // 0x9c
    float mFadeStart; // 0xa0
    float mFadeEnd; // 0xa4
    float mFadeMax; // 0xa8
    ObjPtr<RndTransformable> mFadeRef; // 0xac
    Vector4 mLRFade; // 0xb8, mLeftOut, mLeftOpaque, mRightOpaque, mRightOut
    RndColorXfm mColorXfm; // 0xc8 (size 0x94 -> ends 0x15c)
    bool mUseColorAdjust; // 0x15c
    bool mAnimateFromPreset; // 0x15d
    friend class LightPreset;
public:
    bool GetAnimateFromPreset() const { return mAnimateFromPreset; }
protected:
    bool mAOEnabled; // 0x15e
    float mAOStrength; // 0x160
    Timer mUpdateTimer; // 0x168 (size 0x30 -> ends 0x198)
    float mIntensityAverage; // 0x198
    float mIntensityRate; // 0x19c
    float mExposure; // 0x1a0
    float mWhitePoint; // 0x1a4
    bool mUseToneMapping; // 0x1a8
    bool mUseApprox_Local; // 0x1a9
    bool mUseApprox_Global; // 0x1aa
};

class RndEnvironTracker {
public:
    RndEnvironTracker(RndEnviron *env, const Vector3 *v3)
        : mOld(RndEnviron::Current()), mOldPosSet(RndEnviron::CurrentPos()) {
        if (mOldPosSet) {
            mOldPos = *RndEnviron::CurrentPos();
        } else {
            mOldPos.Zero();
        }
        if (env) {
            if (env != RndEnviron::Current() || !VecEqual(v3, RndEnviron::CurrentPos())) {
                env->Select(v3);
            }
        }
    }
    ~RndEnvironTracker() {
        Vector3 *vptr = mOldPosSet ? &mOldPos : nullptr;
        if (mOld) {
            if (mOld != RndEnviron::Current()
                || !VecEqual(vptr, RndEnviron::CurrentPos())) {
                mOld->Select(vptr);
            }
        }
    }

protected:
    bool VecEqual(const Vector3 *v1, const Vector3 *v2) const {
        if (v1 && v2) {
            return *v1 == *v2;
        } else
            return v1 == v2;
    }

    RndEnviron *mOld; // 0x0
    Vector3 mOldPos; // 0x4
    bool mOldPosSet; // 0x10
};
