#pragma once
#include "math/Color.h"
#include "math/Mtx.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "rndobj/BaseMaterial.h"
#include "rndobj/MetaMaterial.h"
#include "rndobj/Tex.h"
#include "utl/BinStream.h"

// MatShaderOptions + bf now defined in rndobj/BaseMaterial.h (mShaderOptions is a
// BaseMaterial member in retail RB3-360).

class RndMat : public BaseMaterial {
    friend class NgSpotlightDrawer;

public:
    enum PropDisplay {
        kPropDisplayHidden = 0,
        kPropDisplayReadOnly = 1
    };
    virtual ~RndMat();
    OBJ_CLASSNAME(Mat);
    OBJ_SET_TYPE(Mat);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);

    NEW_OBJ(RndMat);
    OBJ_MEM_OVERLOAD(69); // nice

    bool GetRefractEnabled(bool bypass_frame_check);
    float GetRefractStrength();
    RndTex *GetRefractNormalMap();
    void SetZMode(ZMode mode) {
        mZMode = mode;
        mDirty |= 2;
    }
    void SetTexWrap(TexWrap wrap) {
        mTexWrap = wrap;
        mDirty |= 2;
    }
    void SetBlend(Blend blend) {
        mBlend = blend;
        mDirty |= 2;
    }
    void SetTexGen(TexGen gen) {
        mTexGen = gen;
        mDirty |= 2;
    }
    void SetAlphaWrite(bool write) {
        mAlphaWrite = write;
        mDirty |= 2;
    }
    void SetAlphaCut(bool cut) {
        mAlphaCut = cut;
        mDirty |= 2;
    }
    void SetUseEnv(bool use_env) {
        mUseEnviron = use_env;
        mDirty |= 2;
    }
    void SetPreLit(bool lit) {
        mPrelit = lit;
        mDirty |= 2;
    }
    void SetAlphaThreshold(int thresh) { mAlphaThreshold = thresh; }
    void SetPerPixelLit(bool lit) {
        mPerPixelLit = lit;
        mDirty |= 2;
    }
    void SetPointLights(bool lit) { mPointLights = lit; }
    void SetColor(const Hmx::Color &col) {
        mColor.Set(col.red, col.green, col.blue);
        mDirty |= 1;
    }
    void SetColor(float r, float g, float b) {
        mColor.Set(r, g, b);
        mDirty |= 1;
    }
    void SetAlpha(float a) {
        mColor.alpha = a;
        mDirty |= 1;
    }
    void SetShaderOpts(const MatShaderOptions &opts) { mShaderOptions = opts; }
    void SetTexXfm(const Transform &xfm) {
        mTexXfm = xfm;
        mDirty |= 2;
    }
    void SetDiffuseTex(RndTex *tex) {
        mDiffuseTex = tex;
        mDirty |= 2;
    }
    void SetNormalMap(RndTex *tex) {
        mNormalMap = tex;
        mDirty |= 2;
    }
    void SetCull(Cull cull) {
        mCull = cull;
        mDirty |= 2;
    }
    bool Dirty() const { return mDirty; }
    void MarkDirty(int flags) { mDirty |= flags; }

    void SetColorMod(const Hmx::Color &, int);
    void SetSpecularMap(RndTex *);
    void SetMetaMat(MetaMaterial *, bool);
    MetaMaterial *CreateMetaMaterial(bool);
    MetaMaterial *GetMetaMaterial() const { return mMetaMaterial; }
    int GetColorModFlags() const { return mColorModFlags; }
    void SetColorModFlags(ColorModFlags flags) {
        mColorModFlags = flags;
        mDirty |= 2;
    }

    static void Init();
    static void Terminate();
    static void ReloadMetaMaterials();
    static void UpdateAllMatPropertiesFromMetaMat(ObjectDir *);
    static void ReloadAndUpdateMat(ObjectDir *dir) {
        ReloadMetaMaterials();
        UpdateAllMatPropertiesFromMetaMat(dir);
    }

protected:
    RndMat();

    bool IsEditable(Symbol);
    MatPropEditAction GetMetaMatPropAction(Symbol);
    bool OnGetPropertyDisplay(PropDisplay, Symbol);
    void UpdatePropertiesFromMetaMat();
    void LoadOld(BinStreamRev &);

    DataNode OnGetMetaMaterials(const DataArray *);
    DataNode OnGetMetaMaterialsDir(const DataArray *);

    static ObjectDir *sMetaMaterials;
    static ObjectDir *LoadMetaMaterials();

    // mColorModFlags / mColorMod / mShaderOptions / mDirty are BaseMaterial members
    // in retail RB3-360 (they fall inside the 0x28..0x18c BaseMaterial range).
    ObjPtr<MetaMaterial> mMetaMaterial; // 0x18c
    bool mToggleDisplayAllProps; // 0x198
    bool mOwnsMetaMat; // 0x199 - whether this mat retains ownership of its MetaMaterial
    bool mUpdatingFromMetaMat; // 0x19a - guard against re-entrant UpdatePropertiesFromMetaMat
};

RndMat *LookupOrCreateMat(const char *, ObjectDir *);
