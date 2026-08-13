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
    // RB3-360 retail has no per-instance MetaMaterial (see the note at the end of
    // this class); kept as a symbol so rndobj/Utl.cpp and rndobj/Shader.cpp
    // compile and the native engine still links.
    MetaMaterial *GetMetaMaterial() const { return nullptr; }
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
    bool OnGetPropertyDisplay(PropDisplay, Symbol);
    void UpdatePropertiesFromMetaMat();
    void LoadOld(BinStreamRev &);

    DataNode OnGetMetaMaterials(const DataArray *);
    DataNode OnGetMetaMaterialsDir(const DataArray *);

    static ObjectDir *sMetaMaterials;
    static ObjectDir *LoadMetaMaterials();

    // mColorModFlags / mColorMod / mShaderOptions / mDirty are BaseMaterial members
    // in retail RB3-360 (they fall inside the 0x28..0x18c BaseMaterial range).
    //
    // ⛔ RndMat ADDS NO INSTANCE MEMBERS IN RB3-360 RETAIL: sizeof(RndMat) ==
    // sizeof(BaseMaterial) == 396 (0x18c). DC3 (a NEWER engine) added a 16-byte
    // MetaMaterial ownership block here --
    //     ObjPtr<MetaMaterial> mMetaMaterial;   // 12 B
    //     bool mToggleDisplayAllProps, mOwnsMetaMat, mUpdatingFromMetaMat; // +pad
    // -- and our src/system is a verbatim DC3 copy, so we inherited it. Removed
    // (lane MAT-1), adjudicated on retail bytes, not on either oracle:
    //
    //   * retail's tagged allocation for the class named "Mat" is 396, and
    //     NgMat::NewObject allocates 0x250 = 592 = 396 + sizeof(NgMat's own).
    //   * NgMat::RefreshState showed a UNIFORM +16 `this`-relative offset delta
    //     on ~74 of 86 offset-bearing instructions (r31 == this here: the
    //     BaseMaterial-range `addi r3, r31, 0x4c` is byte-identical on both
    //     sides, and this function's stack refs are r1-relative).
    //   * binary-absence, with controls: `MetaMaterial`, `metamaterial`,
    //     `meta_material`, `owns_meta_mat`, `toggle_display_all_props`,
    //     `updating_from_meta` and `_edit_action` occur ZERO times in
    //     orig/45410914/band.exe, while 9 of 10 BaseMaterial DTA property names
    //     (next_pass, emissive_map, normal_map, ...) ARE present -- so the
    //     screen can fire.
    //   * retail carries an RTTI type descriptor for `.?AVRndMat@@` and
    //     `.?AVNgMat@@` but NONE for `.?AVMetaMaterial@@`.
    //   * target_symbol_map.json pins ZERO rows for any of the 14 MetaMaterial
    //     methods.
    //
    // The STATIC machinery (sMetaMaterials / LoadMetaMaterials /
    // CreateMetaMaterial / OnGetMetaMaterials) costs no object bytes and is left
    // in place so the native engine and the 10 CreateAndSetMetaMat call sites
    // keep compiling; only the per-instance state is gone.
};

RndMat *LookupOrCreateMat(const char *, ObjectDir *);
