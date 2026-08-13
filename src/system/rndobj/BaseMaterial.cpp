#include "rndobj/BaseMaterial.h"
#include "Utl.h"
#include "obj/Data.h"
#include "obj/Dir.h"

#include "obj/Object.h"
#include "os/Debug.h"
#include "utl/BinStream.h"

RndMat *gDefaultMat;

namespace {
    bool IsMat(RndMat *mat) { return mat && mat->ClassName() == "Mat"; }
}

#pragma region MatPerfSettings

void MatPerfSettings::Save(BinStream &bs) const {
    bs << mRecvProjLights;
    bs << mPS3ForceTrilinear;
    bs << mRecvPointCubeTex;
}

void MatPerfSettings::LoadOld(BinStreamRev &bs) {
    bs >> mRecvProjLights;
    bs >> mPS3ForceTrilinear;
    if (bs.rev > 0x41)
        bs >> mRecvPointCubeTex;
}

void MatPerfSettings::Load(BinStream &bs) {
    bs >> mRecvProjLights;
    bs >> mPS3ForceTrilinear;
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

INIT_REVS(0x44, 0)

// Retail's material Load is fn_82438F40 (pinned, unnamed, inside rndobj/Utl.cpp's
// span). Read directly, it settles the serialization shape that the two-class split
// obscured: it reads the rev EXACTLY ONCE, then loads members inline at the
// BaseMaterial offsets in this order (r30+0x28 mBlend, +0x2c mColor, +0x99
// mUseEnviron, +0x9a mPrelit), stores mDirty at +0x188 and takes &mColorMod at
// +0x158. There is NO second rev read, no minVer assert and no out-of-line LoadOld
// call -- retail inlined its old-version path (its tail is the mRefractEnabled
// *= 0.15f code). So the DC3 rev-0x46 outer layer in Mat.cpp is scaffolding, exactly
// like RndMat::Save was, and dropping BOTH keeps save/load symmetric on rev 0x44 --
// the rev retail's byte-exact 988 B Save at 0x82435dc0 actually writes.
BEGIN_LOADS(RndMat)
    LOAD_REVS(bs)
    ASSERT_REVS(0x44, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    mDirty = 3;
    ResetColors(mColorMod, 3);
    d >> (int &)mBlend;
    mBlend = CheckBlendMode(mBlend, this);
    d.stream >> mColor >> mUseEnviron >> mPrelit;
    d >> (int &)mZMode;
    d >> mAlphaCut >> mAlphaThreshold >> mAlphaWrite;
    d >> (int &)mTexGen >> (int &)mTexWrap >> mTexXfm >> mDiffuseTex >> mNextPass;
    d >> mIntensify;
    // `cull` loads as a BOOL, unconditionally. Was `if (d.rev < 3) {...} else
    // { d >> (int &)mCull; }` -- the else branch was a REAL MEMORY BUG, and the
    // branch itself does not exist in retail.
    //
    // Adjudicated on RETAIL BYTES (not on the header comment, and not on an oracle):
    // retail's material Load is fn_82438F40, and at 0x8243909C it does
    //     addi r4, r30, 0x11c   <- &mCull
    //     bl   fn_8227DB80      <- NO `li r5, N` size argument
    // fn_8227DB80 is `BinStream::operator>>(bool &)`: `li r5,1` (read ONE byte),
    // `lbz`, `subic`/`subfe` (the b = uc != 0 normalization), `stb` (store ONE byte).
    // The sized helper fn_827C5058 (= ReadEndian(void*, int)) is what retail uses for
    // the 4-byte members, always preceded by `li r5, 0x4` (+0x110 mDeNormal,
    // +0x114 mAnisotropy, +0x118 mShaderVariation). mCull is read with the SAME
    // sizeless 1-byte helper as its four 1-byte neighbours at +0x11d/+0x11e/+0x11f/
    // +0x120 -- so the field is one byte and the overload is `bool &`.
    //
    // Why the old form was a memory bug: mCull is ONE byte at 0x11c (compiler-
    // verified via /d1reportSingleClassLayout -- mPerPixelLit sits at 0x11d), so an
    // `int &` read wrote 0x11c-0x11f, clobbering mPerPixelLit (0x11d), mScreenAligned
    // (0x11e) and mEnvironMapFalloff (0x11f) on every rev>=3 load, and consumed 3
    // bytes too many from the stream. It also broke save/load symmetry: the matching
    // half of the pair, `?Save@BaseMaterial@@UAAXAAVBinStream@@@Z` (988 B, fuzzy/mpn
    // 100.0 -- its bytes ARE retail), writes `bs << mCull` = one byte.
    //
    // The rev<3 branch is dropped because retail has NO rev guard here (contrast the
    // genuinely guarded later reads, which all carry `lhz r11,0x4(r24); cmplwi; ble`).
    // It is also semantically redundant: the old path read a bool and normalized it,
    // which is exactly what the bool& overload does. It was almost certainly
    // inherited from DC3, which is NEWER than RB3 and renumbers these revs.
    //
    // ⚠ `Cull` has three values (kCullNone/kCullRegular/kCullBackwards), so this read
    // clamps kCullBackwards to 1 -- but retail's own subic/subfe does exactly that.
    // Member type left `unsigned char` deliberately: retyping ripples into
    // Save/Copy/propsync with no retail evidence for those bodies (SYNCPROP-1's call).
    d >> (bool &)mCull;
    d >> mEmissiveMultiplier;
    d.stream >> mSpecularRGB >> mNormalMap;
    d.stream >> mEmissiveMap >> mSpecularMap;
    d.stream >> mEnvironMap >> mEnvironMapFalloff >> mEnvironMapSpecMask;
    d >> mPerPixelLit >> (int &)mStencilMode;
    d.stream >> mFur >> mDeNormal >> mAnisotropy;
    d >> mNormDetailTiling >> mNormDetailStrength >> mNormDetailMap;
    d >> mPointLights >> mFog >> mFadeout >> mColorAdjust;
    d.stream >> mRimRGB >> mRimMap >> mRimLightUnder;
    d >> mScreenAligned;
    d >> (int &)mShaderVariation;
    d >> mSpecular2RGB;
    mPerfSettings.Load(d.stream);
    d >> mRefractEnabled;
    d >> mRefractStrength;
    d >> mRefractNormalMap;
#ifdef RB3_DC3_MAT
    if (d.rev > 1) {
        d >> mBloomMultiplier;
    }
    if (d.rev > 3) {
        d >> mNeverFitToSpline;
        if (d.rev < 5) {
            bool b1;
            d >> b1;
            d >> b1;
        }
        if (d.rev >= 6) {
            d >> mAllowDistortionEffects;
            d >> mShockwaveMult;
        }
    }
    if (d.rev > 6) {
        d >> mWorldProjectionTiling;
        d >> mWorldProjectionStartBlend;
        d >> mWorldProjectionEndBlend;
        d >> mDiffuseTex2;
    }
    if (d.rev > 7) {
        d >> mForceAlphaWrite;
    }
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
