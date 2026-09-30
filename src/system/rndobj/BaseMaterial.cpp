#include "rndobj/BaseMaterial.h"
#include "Utl.h"
#include "os/File.h"
#include "rndobj/Fur.h"
#include "utl/Loader.h"
#include "obj/Data.h"
#include "obj/Dir.h"

#include "obj/Object.h"
#include "os/Debug.h"
#include "utl/BinStream.h"

RndMat *gDefaultMat;

namespace {
    bool IsMat(RndMat *mat) { return mat && mat->ClassName() == "Mat"; }
}

// The material's load revision. Retail keeps it in a file-static pair, NOT on the
// stack: RndMat::Load (0x82438F40) stores `sth rev, 0x4(r24)` / `sth alt, 0x0(r24)`
// through one base register (lbl_82CC29D8) and re-reads `lhz r11, 0x4(r24)` before
// every rev test, and MatPerfSettings::Load (0x82435608) reads the same rev word
// directly (lbl_82CC29DC). altRev at +0 / rev at +4 on one base is the
// internal-linkage align(4) co-addressing shape (cf. bandobj/BandButton.cpp).
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} sMatRevs;

#pragma region MatPerfSettings

void MatPerfSettings::Save(BinStream &bs) const {
    bs << mRecvProjLights;
    bs << mPS3ForceTrilinear;
    bs << mRecvPointCubeTex;
}

// Retail 0x82435608: one argument, and the rev test reads the material's static rev.
// (There is no separate old-version overload; this is the only perf-settings Load.)
void MatPerfSettings::Load(BinStream &bs) {
    bs >> mRecvProjLights;
    bs >> mPS3ForceTrilinear;
    if (sMatRevs.rev > 0x41)
        bs >> mRecvPointCubeTex;
}

#pragma endregion
#pragma region RndMat

RndMat::RndMat()
    : mBlend(kBlendSrc), mColor(1, 1, 1), mZMode(kZModeNormal),
      mStencilMode(kStencilIgnore), mTexGen(kTexGenNone), mTexWrap(kTexWrapRepeat),
      mDiffuseTex(this), mIntensify(false), mUseEnviron(true), mPrelit(false),
      mAlphaCut(false), mAlphaWrite(false), mAlphaThreshold(0), mNextPass(this),
      mEmissiveMultiplier(1), mSpecularRGB(0, 0, 0, 10), mSpecular2RGB(0, 0, 0, 10),
      mNormalMap(this), mEmissiveMap(this), mSpecularMap(this), mEnvironMap(this),
      mFur(this), mDeNormal(0), mAnisotropy(0), mShaderVariation(kShaderVariationNone),
      mCull(kCullRegular), mPerPixelLit(false), mScreenAligned(false),
      mEnvironMapFalloff(false), mEnvironMapSpecMask(false), mRefractEnabled(false),
      mRefractStrength(0), mRefractNormalMap(this), mRimLightUnder(false),
      mRimRGB(0, 0, 0, 10), mRimMap(this), mColorModFlags(0), mNormDetailMap(this),
      mNormDetailTiling(1), mNormDetailStrength(0), mPointLights(false), mFog(false),
      mFadeout(false), mColorAdjust(false), mDirty(3)
#ifdef RB3_DC3_MAT
      ,
      mDiffuseTex2(this), mForceAlphaWrite(false), mBloomMultiplier(1),
      mNeverFitToSpline(false), mAllowDistortionEffects(true), mShockwaveMult(1),
      mWorldProjectionTiling(0.125), mWorldProjectionStartBlend(0.8),
      mWorldProjectionEndBlend(0.9)
#endif
{
    mTexXfm.Reset();
    mColorMod.resize(3);
}

// BEGIN_HANDLERS / BEGIN_PROPSYNCS for the merged class live in rndobj/Mat.cpp
// (scatter-included below), because retail's single material Handle is pinned at
// 0x82438138 inside Mat.cpp's .text span. Lane MAT-1 adjudicated that body on
// retail bytes: exactly two Symbols, allowed_next_pass and allowed_normal_map,
// chaining straight to Hmx::Object. DC3's extra `is_default` handler (and its
// OnIsDefaultPropVal body) are dropped -- the string "is_default" occurs ZERO
// times in orig/45410914/band.exe.

BEGIN_SAVES(RndMat)
    SAVE_REVS(0x44, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mBlend << (const Vector4 &)mColor << mUseEnviron << mPrelit;
    bs << mZMode << mAlphaCut << mAlphaThreshold << mAlphaWrite;
    bs << mTexGen << mTexWrap << mTexXfm << mDiffuseTex << mNextPass;
    bs << mIntensify;
    bs << mCull << mEmissiveMultiplier;
    bs << (const Vector4 &)mSpecularRGB << mNormalMap;
    bs << mEmissiveMap << mSpecularMap;
    bs << mEnvironMap << mEnvironMapFalloff << mEnvironMapSpecMask;
    bs << mPerPixelLit << mStencilMode;
    bs << mFur << mDeNormal << mAnisotropy;
    bs << mNormDetailTiling << mNormDetailStrength << mNormDetailMap;
    bs << mPointLights << mFog << mFadeout << mColorAdjust;
    bs << (const Vector4 &)mRimRGB << mRimMap << mRimLightUnder;
    bs << mScreenAligned << mShaderVariation << (const Vector4 &)mSpecular2RGB;
    mPerfSettings.Save(bs);
    bs << mRefractEnabled << mRefractStrength << mRefractNormalMap;
#ifdef RB3_DC3_MAT
    bs << mBloomMultiplier << mNeverFitToSpline;
    bs << mAllowDistortionEffects << mShockwaveMult;
    bs << mWorldProjectionTiling;
    bs << mWorldProjectionStartBlend;
    bs << mWorldProjectionEndBlend;
    bs << mDiffuseTex2;
    bs << mForceAlphaWrite;
#endif
END_SAVES

BEGIN_COPYS(RndMat)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(RndMat)
    BEGIN_COPYING_MEMBERS
        if (ty == kCopyFromMax) {
            if (!mDiffuseTex != !c->mDiffuseTex) {
                COPY_MEMBER(mDiffuseTex)
            }
#ifdef RB3_DC3_MAT
            if (!mDiffuseTex2 != !c->mDiffuseTex2) {
                COPY_MEMBER(mDiffuseTex2)
            }
#endif
        } else {
            COPY_MEMBER(mZMode)
            COPY_MEMBER(mStencilMode)
            COPY_MEMBER(mBlend)
            COPY_MEMBER(mColor)
            COPY_MEMBER(mPrelit)
            COPY_MEMBER(mUseEnviron)
            COPY_MEMBER(mAlphaCut)
            COPY_MEMBER(mAlphaThreshold)
            COPY_MEMBER(mAlphaWrite)
#ifdef RB3_DC3_MAT
            COPY_MEMBER(mForceAlphaWrite)
#endif
            COPY_MEMBER(mTexGen)
            COPY_MEMBER(mTexWrap)
            COPY_MEMBER(mTexXfm)
            COPY_MEMBER(mDiffuseTex)
#ifdef RB3_DC3_MAT
            COPY_MEMBER(mDiffuseTex2)
#endif
            COPY_MEMBER(mNextPass)
            COPY_MEMBER(mCull)
            COPY_MEMBER(mEmissiveMultiplier)
            COPY_MEMBER(mSpecularRGB)
            COPY_MEMBER(mSpecular2RGB)
            COPY_MEMBER(mNormalMap)
            COPY_MEMBER(mEmissiveMap)
            COPY_MEMBER(mSpecularMap)
            COPY_MEMBER(mEnvironMap)
            COPY_MEMBER(mEnvironMapFalloff)
            COPY_MEMBER(mEnvironMapSpecMask)
            COPY_MEMBER(mIntensify)
            COPY_MEMBER(mPerPixelLit)
            COPY_MEMBER(mFur)
            COPY_MEMBER(mDeNormal)
            COPY_MEMBER(mAnisotropy)
            COPY_MEMBER(mNormDetailTiling)
            COPY_MEMBER(mNormDetailStrength)
            COPY_MEMBER(mNormDetailMap)
            COPY_MEMBER(mPointLights)
            COPY_MEMBER(mFog)
            COPY_MEMBER(mFadeout)
            COPY_MEMBER(mColorAdjust)
            COPY_MEMBER(mRimRGB)
            COPY_MEMBER(mRimMap)
            COPY_MEMBER(mRimLightUnder)
            COPY_MEMBER(mScreenAligned)
            COPY_MEMBER(mShaderVariation)
            COPY_MEMBER(mPerfSettings)
            COPY_MEMBER(mRefractEnabled)
            COPY_MEMBER(mRefractStrength)
            COPY_MEMBER(mRefractNormalMap)
#ifdef RB3_DC3_MAT
            COPY_MEMBER(mBloomMultiplier)
            COPY_MEMBER(mNeverFitToSpline)
            COPY_MEMBER(mAllowDistortionEffects)
            COPY_MEMBER(mShockwaveMult)
            COPY_MEMBER(mWorldProjectionTiling)
            COPY_MEMBER(mWorldProjectionStartBlend)
            COPY_MEMBER(mWorldProjectionEndBlend)
#endif
            // folded in from the DC3 RndMat::Copy layer (mShaderOptions /
            // mColorModFlags / mColorMod are members of the ONE retail material
            // class, so they belong in its ONE Copy)
            COPY_MEMBER(mShaderOptions)
            COPY_MEMBER(mColorModFlags)
            COPY_MEMBER(mColorMod)
        }
        mDirty = 3;
    END_COPYING_MEMBERS
END_COPYS

// Retail's material Load is fn_82438F40 (vtable slot 10 of RndMat's vtable at
// 0x8206572C, after Save 0x82435DC0 and Copy 0x82438C28). It reads the rev EXACTLY
// ONCE into the file-static pair above, then loads every member with the
// old-version handling INLINE -- there is no out-of-line LoadOld, no minVer assert
// (ASSERT_REVS is compiled out) and no CheckBlendMode. Reconstructed instruction by
// instruction against retail (lane W17-MAT); DC3's rev-0x46 two-layer Load and our
// former RndMat::LoadOld(BinStreamRev &) were the same code split in two.
//
// Statement boundaries are load-bearing: an out-of-line operator>> (bool, Color,
// Vector3, Symbol) returns the stream in r3, so a CHAINED read passes that r3 on
// while a separate statement reloads the saved stream register. Retail chains
// exactly mUseEnviron>>mPrelit, mCull>>mEmissiveMultiplier>>mSpecularRGB>>mNormalMap,
// the unused bool>>Color pair, and mFog>>mFadeout; everything else is re-read from
// the saved `bs`.
BEGIN_LOADS(RndMat)
    int revs;
    bs >> revs;
    sMatRevs.rev = getHmxRev(revs);
    sMatRevs.altRev = getAltRev(revs);
    Hmx::Object::Load(bs);
    bs >> (int &)mBlend;
    bs >> mColor;
    bs >> mUseEnviron >> mPrelit;
    bs >> (int &)mZMode;
    bs >> mAlphaCut;
    if (sMatRevs.rev > 0x25) {
        bs >> mAlphaThreshold;
    }
    bs >> mAlphaWrite;
    bs >> (int &)mTexGen;
    bs >> (int &)mTexWrap;
    bs >> mTexXfm;
    bs >> mDiffuseTex;
    bs >> mNextPass;
    bs >> mIntensify;
    mDirty = 3;
    // `cull` loads as a BOOL, unconditionally. Adjudicated on RETAIL BYTES: at
    // 0x8243909C retail does `addi r4, r30, 0x11c` then `bl fn_8227DB80` =
    // `BinStream::operator>>(bool &)` with NO `li r5, N` size argument (read ONE byte,
    // then the subic/subfe b = uc != 0 normalization). mCull is one byte at 0x11c
    // (compiler-verified; mPerPixelLit sits at 0x11d), so the old `int &` read was a
    // memory bug that clobbered 0x11d-0x11f. Save writes `bs << mCull` = one byte.
    // ⚠ `Cull` has three values, so this clamps kCullBackwards to 1 -- as retail does.
    bs >> (bool &)mCull >> mEmissiveMultiplier >> mSpecularRGB >> mNormalMap;
    bs >> mEmissiveMap >> mSpecularMap;
    if (sMatRevs.rev < 0x33) {
        ObjPtr<RndTex> tex(this);
        bs >> tex;
    }
    bs >> mEnvironMap;
    if (sMatRevs.rev > 0x3C) {
        bs >> mEnvironMapFalloff;
        if (sMatRevs.rev > 0x42) {
            bs >> mEnvironMapSpecMask;
        }
    }
    if (sMatRevs.rev < 0x25 && mSpecularMap) {
        mSpecularRGB.Set(1, 1, 1, mSpecularRGB.alpha);
    }
    if (sMatRevs.rev > 0x19) {
        bs >> mPerPixelLit;
    }
    if (sMatRevs.rev > 0x1A && sMatRevs.rev < 0x32) {
        bool unused;
        bs >> unused;
    }
    if (sMatRevs.rev > 0x1B) {
        bs >> (int &)mStencilMode;
    }
    if (sMatRevs.rev < 0x29 && sMatRevs.rev > 0x1C) {
        Symbol unused;
        bs >> unused;
    }
    if (sMatRevs.rev > 0x20) {
        bs >> mFur;
    } else if (sMatRevs.rev > 0x1D) {
        // Retail does NOT save/restore the edit mode here: SetEditMode(1) ...
        // SetEditMode(0), both literal (same as LookupOrCreateMat).
        TheLoadMgr.SetEditMode(true);
        const char *name = MakeString("%s.fur", FileGetBase(Name()));
        ObjectDir *dir = Dir();
        RndFur *fur = Hmx::Object::New<RndFur>();
        if (name) {
            fur->SetName(name, dir);
        }
        TheLoadMgr.SetEditMode(false);
        if (fur->LoadOld(bs, sMatRevs.rev)) {
            mFur = fur;
        } else {
            delete fur;
            // = `mFur = nullptr`, open-coded as retail has it (0x82439314: lwz
            // 0x10c / beq / bl Hmx::Object::Release / stw r23(=0), 0x8) -- our
            // compiler otherwise emits bl SetObjConcrete(0) and cross-jumps it with
            // the `mFur = fur` call above, which costs r23 and 8 instructions.
            mFur.ReleaseObjConcrete();
        }
    }
    if (sMatRevs.rev > 0x21 && sMatRevs.rev < 0x31) {
        bool unusedBool;
        Hmx::Color unusedColor;
        bs >> unusedBool >> unusedColor;
        if (sMatRevs.rev > 0x22) {
            ObjPtr<RndTex> tex(this);
            bs >> tex;
        }
    }
    if (sMatRevs.rev > 0x23) {
        bs >> mDeNormal;
        bs >> mAnisotropy;
    }
    if (sMatRevs.rev > 0x26) {
        if (sMatRevs.rev < 0x2A) {
            bool unused;
            bs >> unused;
        }
        bs >> mNormDetailTiling;
        bs >> mNormDetailStrength;
        if (sMatRevs.rev < 0x2A) {
            int unusedInt;
            Hmx::Color unusedColor;
            bs >> unusedInt;
            bs >> unusedColor;
        }
        bs >> mNormDetailMap;
        if (sMatRevs.rev < 0x2A) {
            ObjPtr<RndTex> tex(this);
            bs >> tex;
        }
        if (sMatRevs.rev < 0x28) {
            mNormDetailStrength = 0;
        }
    }
    if (sMatRevs.rev > 0x2A) {
        if (sMatRevs.rev > 0x2C) {
            bs >> mPointLights;
        } else {
            int pointLights;
            bs >> pointLights;
            mPointLights = pointLights > 1;
        }
        if (sMatRevs.rev < 0x3F) {
            bool unused;
            bs >> unused;
        }
        bs >> mFog >> mFadeout;
        if (sMatRevs.rev > 0x2B && sMatRevs.rev < 0x2E) {
            bool unused;
            bs >> unused;
        }
        if (sMatRevs.rev > 0x2E) {
            bs >> mColorAdjust;
        }
    }
    if (sMatRevs.rev > 0x2F) {
        bs >> mRimRGB;
        bs >> mRimMap;
        if (sMatRevs.rev > 0x39) {
            bs >> mRimLightUnder;
        } else {
            bool unused;
            bs >> unused;
            float red = mRimRGB.red * 2.857143f;
            float green = mRimRGB.green * 2.857143f;
            float blue = mRimRGB.blue * 2.857143f;
            mRimRGB.red = Min(red, 1.0f);
            mRimRGB.green = Min(green, 1.0f);
            mRimRGB.blue = Min(blue, 1.0f);
        }
        if (sMatRevs.rev < 0x3B) {
            mRimRGB.red = 0;
            mRimRGB.green = 0;
            mRimRGB.blue = 0;
        }
    }
    if (sMatRevs.rev > 0x30) {
        bs >> mScreenAligned;
    }
    if (sMatRevs.rev > 0x31 && sMatRevs.rev < 0x33) {
        bool isSkinned;
        bs >> isSkinned;
        if (isSkinned) {
            mShaderVariation = kShaderVariationSkin;
        }
    }
    if (sMatRevs.rev > 0x32) {
        bs >> (int &)mShaderVariation;
        bs >> mSpecular2RGB;
    }
    // Unconditional, and HERE: retail calls ResetColors(&mColorMod, 3) at 0x82439668,
    // after the rev>0x32 block (the rb3-Wii position, not DC3's top-of-Load one).
    ResetColors(mColorMod, 3);
    if (sMatRevs.rev > 0x33 && sMatRevs.rev < 0x44) {
        std::vector<Hmx::Color> colors;
        if (sMatRevs.rev < 0x35) {
            bool unused;
            bs >> unused;
        } else {
            int unused;
            bs >> unused;
        }
        if (sMatRevs.rev > 0x34 && sMatRevs.rev < 0x3C) {
            Hmx::Color unused;
            bs >> unused;
        }
        if (sMatRevs.rev >= 0x3C) {
            bs >> colors;
        }
    }
    if (sMatRevs.rev > 0x35 && sMatRevs.rev < 0x3E) {
        ObjPtr<Hmx::Object> obj(this);
        bs >> obj;
    }
    if (sMatRevs.rev > 0x36 && sMatRevs.rev < 0x3F) {
        bool forceTrilinear;
        bs >> forceTrilinear;
        mPerfSettings.mPS3ForceTrilinear = forceTrilinear;
    }
    if (sMatRevs.rev > 0x37 && sMatRevs.rev < 0x39) {
        int unusedX, unusedY;
        bs >> unusedX;
        bs >> unusedY;
    }
    if (sMatRevs.rev > 0x3E) {
        mPerfSettings.Load(bs);
    }
    if (sMatRevs.rev > 0x3F) {
        bs >> mRefractEnabled;
        bs >> mRefractStrength;
        bs >> mRefractNormalMap;
        if (sMatRevs.rev < 0x41) {
            if (mRefractEnabled) {
                mRefractStrength *= 0.15f;
            } else {
                mRefractStrength = 0;
            }
        }
    }
#ifdef RB3_DC3_MAT
    // DC3-only members (not in retail RB3; the flag is defined by no build). Kept
    // symmetric with the RB3_DC3_MAT tail of Save above.
    bs >> mBloomMultiplier >> mNeverFitToSpline;
    bs >> mAllowDistortionEffects >> mShockwaveMult;
    bs >> mWorldProjectionTiling;
    bs >> mWorldProjectionStartBlend;
    bs >> mWorldProjectionEndBlend;
    bs >> mDiffuseTex2;
    bs >> mForceAlphaWrite;
#endif
END_LOADS

void RndMat::SetDefaultMat(RndMat *mat) {
    MILO_ASSERT(!gDefaultMat, 0x55);
    gDefaultMat = mat;
}

const DataNode *RndMat::GetDefaultPropVal(Symbol s) {
    const DataNode *node = gDefaultMat->Property(s, true);
    MILO_ASSERT(node, 0x129);
    return node;
}

bool RndMat::PropValDifferent(Symbol s, RndMat *base) {
    if (!base) {
        base = gDefaultMat;
    }
    MILO_ASSERT(base, 0x133);
    if (s == "tex_xfm") {
        return base->mTexXfm != mTexXfm;
    } else {
        const DataNode *node = Property(s);
        MILO_ASSERT(node, 0x13C);
        DataNode var(*node);
        DataNode othervar(*base->Property(s));
        if (s == "shader_combos") {
            return var > othervar;
        } else {
            return var != othervar;
        }
    }
}

// DC3's BaseMaterial::OnIsDefaultPropVal and its `is_default` handler are deleted
// with the merge: retail's material Handle builds exactly two Symbols (lane MAT-1),
// and the string "is_default" occurs ZERO times in orig/45410914/band.exe.

__declspec(noinline) RndMat::Blend CheckBlendMode(RndMat::Blend b, RndMat *) {
    return b;
}

bool RndMat::IsNextPass(RndMat *m) {
    for (RndMat *it = this; it != nullptr; it = it->NextPass()) {
        if (it == m) {
            return true;
        }
    }
    return false;
}

DataNode RndMat::OnAllowedNextPass(const DataArray *a) {
    int matCount = 0;
    for (ObjDirItr<RndMat> it(Dir(), true); it != nullptr; ++it) {
        if (IsMat(it)) {
            matCount++;
        }
    }
    matCount += 2;
    DataArrayPtr ptr(new DataArray(matCount));
    int idx = 0;
    ptr->Node(idx++) = NULL_OBJ;

    if (mNextPass) {
        ptr->Node(idx++) = mNextPass.Ptr();
    }

    for (ObjDirItr<RndMat> it(Dir(), true); it != nullptr; ++it) {
        if (IsMat(it) && !IsNextPass(it)) {
            ptr->Node(idx++) = &*it;
        }
    }
    ptr->Resize(idx);
    return ptr;
}

DataNode RndMat::OnAllowedNormalMap(const DataArray *a) {
    return GetNormalMapTextures(Dir());
}

#pragma endregion

// sw2 scatter-include (default/BaseMaterial <- rndobj/Mat.cpp)
#define gRev gRev_Mat
#define gAltRev gAltRev_Mat
#include "rndobj/Mat.cpp"
#undef gRev
#undef gAltRev
