#include "rndobj/Mat.h"
#include "Rnd.h"
#include "Utl.h"
#include "math/Color.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/DirLoader.h"
#include "obj/Object.h"
#include "obj/Utl.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/System.h"
#include "rndobj/BaseMaterial.h"
#include "rndobj/Fur.h"
#include "rndobj/Tex.h"
#include "utl/BinStream.h"
#include "utl/FilePath.h"
#include "utl/Loader.h"
#include "utl/Symbol.h"

MatShaderOptions::MatShaderOptions() : pack(0x12), mTempMat(0) {}

namespace {
    void AddOverridePropName(String &str, Symbol &sym) {
        if (!str.empty()) {
            str += ", ";
        }
        str += sym.Str();
    }
}

// RndMat's ctor now lives in rndobj/BaseMaterial.cpp with the rest of the merged
// class (retail has ONE material class; see rndobj/BaseMaterial.h).
RndMat::~RndMat() {}

// RB3-360 retail carries the allowed_next_pass / allowed_normal_map handlers on the
// material class itself and chains straight to Hmx::Object. DC3 (newer engine)
// hoisted them into a BaseMaterial base and added an is_default handler; our
// src/system is a verbatim DC3 copy, so we inherited that refactor. Adjudicated on
// retail bytes, not on either oracle: fn_824B27C8 (Handle, 412 B) builds exactly two
// Symbols, at 0x82065658 = "allowed_next_pass" and 0x82063C20 = "allowed_normal_map",
// then tail-calls ?Handle@Object@Hmx@@. "is_default", "get_metamats" and
// "prop_is_hidden" appear ZERO times anywhere in orig/45410914/band.exe, so DC3's
// other four handlers postdate RB3. rb3-Wii agrees exactly.
// This is the ONE Handle of the merged class, and it is deliberately here rather
// than in BaseMaterial.cpp: retail's Handle is pinned at 0x82438138, inside Mat.cpp's
// .text span. The OnAllowedNextPass / OnAllowedNormalMap bodies stay in
// BaseMaterial.cpp, which this file is scatter-included into.
BEGIN_HANDLERS(RndMat)
    HANDLE(allowed_next_pass, OnAllowedNextPass)
    HANDLE(allowed_normal_map, OnAllowedNormalMap)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

// THE ONE material SyncProperty. RB3-360 retail has exactly one; our tree used to
// emit two -- this one, and a MetaMaterial override that (post-BASEMAT-2) chained
// SYNC_SUPERCLASS(RndMat) and so re-ran this entire list a SECOND time per instance.
// The MetaMaterial copy is deleted with its class; see
// docs/decomp/metamaterial-does-not-exist-in-rb3-retail-2026-08-13.md.
//
// It stays HERE, not in MetaMaterial.cpp: lane SPLITS-3 (7782ed48) re-homed the
// 0x82436488-0x82438138 span onto Mat.cpp on a whole-tree defining-set census
// (?SyncProperty@RndMat@@ is defined by {BaseMaterial.obj, Mat.obj}; MetaMaterial.obj
// is ABSENT) plus vtable-slot arithmetic controlled on two already-matched slots.
//
// Every `<prop>_edit_action` Symbol and the `IsEditable` gate around it are GONE:
// "_edit_action" occurs 0 times in orig/45410914/band.exe, with 15/15 positive
// controls from this same list firing at 1-occurrence resolution (allowed_next_pass,
// shader_variation, rim_light_under, environ_map_specmask...). rb3-Wii -- the RB3-era
// oracle -- agrees structurally: its BEGIN_PROPSYNCS(RndMat) has no IsEditable at all.
#define SYNC_MAT_PROP(s, member, dirty_flag)                                             \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s) {                                                                 \
            if (PropSync(member, _val, _prop, _i + 1, _op)) {                            \
                if (!(_op & (kPropSize | kPropGet))) {                                   \
                    mDirty |= dirty_flag;                                                \
                }                                                                        \
                return true;                                                             \
            } else                                                                       \
                return false;                                                            \
        }                                                                                \
    }

#define SYNC_PERF_PROP(s, member)                                                        \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s) {                                                                 \
            if (_op == kPropSet) {                                                       \
                member = _val.Int() > 0;                                            \
            } else {                                                                     \
                if (_op == (PropOp)0x40)                                                 \
                    return false;                                                        \
                _val = member;                                                           \
            }                                                                            \
            return true;                                                                 \
        }                                                                                \
    }

BEGIN_PROPSYNCS(RndMat)
    // No `metamaterial` property in RB3-360 retail -- the string "metamaterial"
    // occurs 0 times in orig/45410914/band.exe. See rndobj/Mat.h.
    SYNC_MAT_PROP(intensify, mIntensify, 2)
    SYNC_MAT_PROP(blend, (int &)mBlend, 2)
    SYNC_MAT_PROP(color, mColor, 1)
    SYNC_MAT_PROP(alpha, mColor.alpha, 1)
    SYNC_MAT_PROP(use_environ, mUseEnviron, 2)
    SYNC_MAT_PROP(z_mode, (int &)mZMode, 2)
    SYNC_MAT_PROP(stencil_mode, (int &)mStencilMode, 2)
    SYNC_MAT_PROP(tex_gen, (int &)mTexGen, 2)
    SYNC_MAT_PROP(tex_wrap, (int &)mTexWrap, 2)
    SYNC_MAT_PROP(tex_xfm, mTexXfm, 2)
    SYNC_MAT_PROP(diffuse_tex, mDiffuseTex, 2)
#ifdef RB3_DC3_MAT
    SYNC_MAT_PROP(diffuse_tex2, mDiffuseTex2, 2)
#endif
    SYNC_MAT_PROP(prelit, mPrelit, 2)
    SYNC_MAT_PROP(alpha_cut, mAlphaCut, 2)
    SYNC_PROP_MODIFY(alpha_threshold, mAlphaThreshold, mDirty |= 2)
    SYNC_MAT_PROP(alpha_write, mAlphaWrite, 2)
#ifdef RB3_DC3_MAT
    SYNC_PROP(force_alpha_write, mForceAlphaWrite)
#endif
    SYNC_MAT_PROP(next_pass, mNextPass, 2)
    SYNC_MAT_PROP(cull, (int &)mCull, 2)
    SYNC_MAT_PROP(per_pixel_lit, mPerPixelLit, 2)
    SYNC_MAT_PROP(emissive_multiplier, mEmissiveMultiplier, 2)
    SYNC_MAT_PROP(specular_rgb, mSpecularRGB, 1)
    SYNC_MAT_PROP(specular_power, mSpecularRGB.alpha, 1)
    SYNC_MAT_PROP(specular2_rgb, mSpecular2RGB, 1)
    SYNC_MAT_PROP(specular2_power, mSpecular2RGB.alpha, 1)
    SYNC_MAT_PROP(normal_map, mNormalMap, 2)
    SYNC_MAT_PROP(emissive_map, mEmissiveMap, 2) {
        static Symbol _s("specular_map");
        if (sym == _s) {
            if (_op == kPropSet) {
                SetSpecularMap(_val.Obj<RndTex>());
            } else {
                if (_op == (PropOp)0x40)
                    return false;
                _val = mSpecularMap.Ptr();
            }
            return true;
        }
    }
    SYNC_MAT_PROP(environ_map, mEnvironMap, 2)
    SYNC_MAT_PROP(environ_map_falloff, mEnvironMapFalloff, 2)
    SYNC_MAT_PROP(environ_map_specmask, mEnvironMapSpecMask, 2)
    SYNC_MAT_PROP(de_normal, mDeNormal, 2)
    SYNC_MAT_PROP(anisotropy, mAnisotropy, 2)
    SYNC_MAT_PROP(norm_detail_tiling, mNormDetailTiling, 2)
    SYNC_MAT_PROP(norm_detail_strength, mNormDetailStrength, 2)
    SYNC_MAT_PROP(norm_detail_map, mNormDetailMap, 2)
    SYNC_MAT_PROP(rim_rgb, mRimRGB, 2)
    SYNC_MAT_PROP(rim_power, mRimRGB.alpha, 2)
    SYNC_MAT_PROP(rim_map, mRimMap, 2)
    SYNC_MAT_PROP(rim_light_under, mRimLightUnder, 2)
    SYNC_MAT_PROP(refract_enabled, mRefractEnabled, 2)
    SYNC_MAT_PROP(refract_strength, mRefractStrength, 2)
    SYNC_MAT_PROP(refract_normal_map, mRefractNormalMap, 2)
    SYNC_MAT_PROP(screen_aligned, mScreenAligned, 2)
    SYNC_MAT_PROP(shader_variation, (int &)mShaderVariation, 2) {
        static Symbol _s("point_lights");
        if (sym == _s) {
            return PropSync(mPointLights, _val, _prop, _i + 1, _op);
        }
    }
    {
        static Symbol _s("fog");
        if (sym == _s) {
            return PropSync(mFog, _val, _prop, _i + 1, _op);
        }
    }
    {
        static Symbol _s("fade_out");
        if (sym == _s) {
            return PropSync(mFadeout, _val, _prop, _i + 1, _op);
        }
    }
    {
        static Symbol _s("color_adjust");
        if (sym == _s) {
            return PropSync(mColorAdjust, _val, _prop, _i + 1, _op);
        }
    }
    {
        static Symbol _s("fur");
        if (sym == _s) {
            return PropSync(mFur, _val, _prop, _i + 1, _op);
        }
    }
    SYNC_PERF_PROP(recv_proj_lights, mPerfSettings.mRecvProjLights)
    SYNC_PERF_PROP(recv_point_cube_tex, mPerfSettings.mRecvPointCubeTex)
    SYNC_PERF_PROP(ps3_force_trilinear, mPerfSettings.mPS3ForceTrilinear)
#ifdef RB3_DC3_MAT
    SYNC_MAT_PROP(bloom_multiplier, mBloomMultiplier, 2)
    SYNC_MAT_PROP(never_fit_to_spline, mNeverFitToSpline, 2)
    SYNC_MAT_PROP(allow_distortion_effects, mAllowDistortionEffects, 2)
    SYNC_MAT_PROP(shockwave_mult, mShockwaveMult, 2)
    SYNC_MAT_PROP(world_projection_tiling, mWorldProjectionTiling, 2)
    SYNC_MAT_PROP(world_projection_start_blend, mWorldProjectionStartBlend, 2)
    SYNC_MAT_PROP(world_projection_end_blend, mWorldProjectionEndBlend, 2)
#endif
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

// ⛔ The DC3 rev-0x46 Save/Copy/Load layer that used to sit here is DELETED with the
// BaseMaterial merge. It was the derived half of a two-class split retail does not
// have, and each of its three members had a retail counterpart proving it surplus:
//
//   Save  -- retail's ONE material Save is fn_82435DC0 (vtable slot 8), 988 B,
//            byte-exact at mpn/fuzzy 100 against the rev-0x44 body now in
//            BaseMaterial.cpp. A rev-0x46 wrapper writing a second rev word has no
//            retail counterpart.
//   Load  -- retail's ONE material Load is fn_82438F40. It reads the rev EXACTLY
//            ONCE and then loads members inline; there is no second rev read, no
//            minVer assert and no out-of-line LoadOld call. Deleting this layer with
//            Save keeps the pair symmetric on rev 0x44.
//   Copy  -- its three COPY_MEMBERs (mShaderOptions / mColorModFlags / mColorMod)
//            moved into the merged Copy in BaseMaterial.cpp, along with mDirty = 3.
//
// ⚠ NOT reconstructed: retail's Load INLINES an old-version path (its tail is the
// mRefractEnabled ? *= 0.15f : = 0 code that our RndMat::LoadOld still ends with).
// RndMat::LoadOld is therefore kept but unreferenced. fn_82438F40 is unnamed in
// scripts/target_symbol_map.json, so NO instrument scores a reconstruction of it --
// that needs its own lane, adjudicating all 578 instructions on retail bytes.

void RndMat::Init() {
    REGISTER_OBJ_FACTORY(RndMat);
    RndMat *mat = Hmx::Object::New<RndMat>();
    SetDefaultMat(mat);
}

// Retail's material Terminate has no sMetaMaterials to release -- there is no
// metamaterial ObjectDir in RB3-360 (lane METAMAT-1).
void RndMat::Terminate() {}

float RndMat::GetRefractStrength() { return mRefractStrength; }
RndTex *RndMat::GetRefractNormalMap() {
    return mRefractNormalMap ? mRefractNormalMap : mNormalMap;
}

bool RndMat::GetRefractEnabled(bool b) {
    return mRefractEnabled == 1 && mRefractStrength > 0.0f
        && (mRefractNormalMap ? mRefractNormalMap : mNormalMap)
        && (b || TheRnd.GetCurrentFrameTex(false));
}

bool RndMat::OnGetPropertyDisplay(PropDisplay display, Symbol s) {
    MILO_ASSERT(display == kPropDisplayHidden || display == kPropDisplayReadOnly, 0x357);
    return false;
}

void RndMat::SetColorMod(const Hmx::Color &color, int index) {
    MILO_ASSERT(index >= 0 && index < kColorModNum, 0x230);
    mColorMod[index] = color;
    mDirty |= 2;
}

RndMat *LookupOrCreateMat(const char *shader, ObjectDir *dir) {
    const char *fileStr = MakeString("%s.mat", FileGetBase(shader));
    RndMat *mat = dir->Find<RndMat>(fileStr, false);
    if (!mat) {
        mat = dir->Find<RndMat>(FileGetBase(shader), false);
        if (!mat) {
            // Retail-360 does not save/restore the old edit mode here; it
            // unconditionally clears it (no lbz of TheLoadMgr.mEditMode, and
            // the restore call passes a literal 0).
            TheLoadMgr.SetEditMode(true);
            mat = dir->New<RndMat>(fileStr);
            TheLoadMgr.SetEditMode(false);
        }
    }
    return mat;
}

void RndMat::SetSpecularMap(RndTex *tex) {
    if (tex && !mSpecularMap) {
        if (mSpecularRGB.Pack() == 0) {
            mSpecularRGB.Set(1, 1, 1, mSpecularRGB.alpha);
        }
    }
    mSpecularMap = tex;
    mDirty |= 2;
}

void RndMat::LoadOld(BinStreamRev &d) {
    Hmx::Object::Load(d.stream);
    d >> (int &)mBlend;
    mBlend = CheckBlendMode(mBlend, this);
    d >> mColor;
    d >> mUseEnviron >> mPrelit;
    d >> (int &)mZMode;
    d >> mAlphaCut;
    if (d.rev > 0x25) {
        d >> mAlphaThreshold;
    }
    d >> mAlphaWrite;
    d >> (int &)mTexGen;
    d >> (int &)mTexWrap;
    d >> mTexXfm;
    d >> mDiffuseTex;
    d.stream >> mNextPass >> mIntensify;
    bool cullValue;
    d >> cullValue;
    mCull = (Cull)cullValue;
    d >> mEmissiveMultiplier;
    d.stream >> mSpecularRGB >> mNormalMap;
    d.stream >> mEmissiveMap >> mSpecularMap;
    if (d.rev < 0x33) {
        ObjPtr<RndTex> tex(this);
        d >> tex;
    }
    d >> mEnvironMap;
    if (d.rev > 0x3C) {
        d >> mEnvironMapFalloff;
        if (d.rev > 0x42) {
            d >> mEnvironMapSpecMask;
        }
    }
    if (d.rev < 0x25) {
        if (mSpecularMap) {
            mSpecularRGB.Set(1, 1, 1, mSpecularRGB.alpha);
        }
    }
    if (d.rev > 0x19) {
        d >> mPerPixelLit;
    }
    if (d.rev > 0x1A && d.rev < 0x32) {
        bool unusedValue;
        d >> unusedValue;
    }
    if (d.rev > 0x1B) {
        d >> (int &)mStencilMode;
    }
    if (d.rev < 0x29 && d.rev > 0x1C) {
        Symbol unusedSymbol;
        d >> unusedSymbol;
    }
    if (d.rev > 0x20) {
        d >> mFur;
    } else if (d.rev > 0x1D) {
        bool old = TheLoadMgr.EditMode();
        TheLoadMgr.SetEditMode(true);
        const char *name = MakeString("%s.fur", FileGetBase(Name()));
        ObjectDir *dir = Dir();
        RndFur *fur = Hmx::Object::New<RndFur>();
        if (name) {
            fur->SetName(name, dir);
        }
        TheLoadMgr.SetEditMode(old);
        if (fur->LoadOld(d)) {
            mFur = fur;
        } else {
            delete fur;
            mFur = nullptr;
        }
    }
    if (d.rev > 0x21 && d.rev < 0x31) {
        bool unusedBool;
        Hmx::Color unusedColor;
        d >> unusedBool >> unusedColor;
        if (d.rev > 0x22) {
            ObjPtr<RndTex> tex(this);
            d >> tex;
        }
    }
    if (d.rev > 0x23) {
        d >> mDeNormal;
        d >> mAnisotropy;
    }
    if (d.rev > 0x26) {
        if (d.rev < 0x2A) {
            bool unusedValue;
            d >> unusedValue;
        }
        d >> mNormDetailTiling;
        d >> mNormDetailStrength;
        if (d.rev < 0x2A) {
            int unusedInt;
            Hmx::Color unusedColor;
            d >> unusedInt;
            d >> unusedColor;
        }
        d >> mNormDetailMap;
        if (d.rev < 0x2A) {
            ObjPtr<RndTex> tex(this);
            d >> tex;
        }
        if (d.rev < 0x28) {
            mNormDetailStrength = 0;
        }
    }
    if (d.rev > 0x2A) {
        if (d.rev > 0x2C) {
            d >> mPointLights;
        } else {
            int pointLightsValue;
            d >> pointLightsValue;
            mPointLights = pointLightsValue > 1;
        }
        if (d.rev < 0x3F) {
            bool unusedValue;
            d >> unusedValue;
        }
        d >> mFog >> mFadeout;
        if (d.rev > 0x2B && d.rev < 0x2E) {
            bool unusedValue;
            d >> unusedValue;
        }
        if (d.rev > 0x2E) {
            d >> mColorAdjust;
        }
    }
    if (d.rev > 0x2F) {
        d >> mRimRGB;
        d >> mRimMap;
        if (d.rev > 0x39) {
            d >> mRimLightUnder;
        } else {
            bool unusedValue;
            d >> unusedValue;
            float red = mRimRGB.red * 2.857143f;
            float green = mRimRGB.green * 2.857143f;
            float blue = mRimRGB.blue * 2.857143f;
            mRimRGB.red = Min(red, 1.0f);
            mRimRGB.green = Min(green, 1.0f);
            mRimRGB.blue = Min(blue, 1.0f);
        }
        if (d.rev < 0x3B) {
            mRimRGB.red = 0;
            mRimRGB.green = 0;
            mRimRGB.blue = 0;
        }
    }
    if (d.rev > 0x30) {
        d >> mScreenAligned;
    }
    if (d.rev > 0x31 && d.rev < 0x33) {
        bool isSkinned;
        d >> isSkinned;
        if (isSkinned) {
            mShaderVariation = kShaderVariationSkin;
        }
    }
    if (d.rev > 0x32) {
        d >> (int &)mShaderVariation;
        d >> mSpecular2RGB;
    }
    if (d.rev > 0x33 && d.rev < 0x44) {
        std::vector<Hmx::Color> colors;
        if (d.rev < 0x35) {
            bool unusedBool;
            d >> unusedBool;
        } else {
            int unusedInt;
            d >> unusedInt;
        }
        if (d.rev > 0x34 && d.rev < 0x3C) {
            Hmx::Color unusedColor;
            d >> unusedColor;
        }
        if (d.rev >= 0x3C) {
            d >> colors;
        }
    }
    if (d.rev > 0x35 && d.rev < 0x3E) {
        ObjPtr<Hmx::Object> obj(this);
        d >> obj;
    }
    if (d.rev > 0x36 && d.rev < 0x3F) {
        bool forceTrilinear;
        d >> forceTrilinear;
        mPerfSettings.mPS3ForceTrilinear = forceTrilinear;
    }
    if (d.rev > 0x37 && d.rev < 0x39) {
        int unusedX, unusedY;
        d >> unusedX >> unusedY;
    }
    if (d.rev > 0x3E) {
        mPerfSettings.LoadOld(d);
    }
    if (d.rev > 0x3F) {
        d >> mRefractEnabled;
        d >> mRefractStrength;
        d >> mRefractNormalMap;
        if (d.rev < 0x41) {
            if (mRefractEnabled) {
                mRefractStrength *= 0.15f;
            } else {
                mRefractStrength = 0;
            }
        }
    }
}
