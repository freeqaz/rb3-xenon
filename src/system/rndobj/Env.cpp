// Retail inlines this TU's owner-only ObjPtr ctor(s) with the vtable
// materialization pinned AFTER the member stores -- the
// RB3_OBJPTR_FORCEINLINE_CTOR signature (see obj/ObjPtr_p.h). The
// extent census shows delta ~= -16 * (surplus bl) for this TU's ctor,
// i.e. one un-inlined ObjPtr ctor per surplus call.
#define RB3_OBJPTR_FORCEINLINE_CTOR

#include "rndobj/Env.h"
#include "BoxMap.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Draw.h"
#include "rndobj/Trans.h"
#include "utl/BinStream.h"
#include "utl/Std.h"

// RB3-360 retail rev storage. Retail's LOAD_REVS keeps NO BinStreamRev: it splits
// the packed rev into two mutable file-scope shorts, and ASSERT_REVS emits nothing.
// The two words must live in ONE aligned(4) aggregate (altRev +0, rev +4) -- MSVC
// does not lay .bss out in declaration order, so two separate statics get other
// globals interleaved between them and will not fold onto one base register.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_Env;
#define gAltRev gRevs_Env.altRev
#define gRev gRevs_Env.rev

BoxMapLighting RndEnviron::sGlobalLighting;
RndEnviron *RndEnviron::sCurrent;
Vector3 RndEnviron::sCurrentPos;
bool RndEnviron::sCurrentPosSet;

bool RndEnviron::FogEnable() const { return mAmbientFogOwner->mFogEnable; }

Transform RndEnviron::LRFadeRef() const {
    Transform ret;
    if (mFadeRef) {
        ret = mFadeRef->WorldXfm();
    } else {
        ret.Reset();
    }
    return ret;
}

const Transform &RndEnviron::ColorXfm() const {
    static Vector3 x(1, 0, 0);
    static Vector3 y(0, 1, 0);
    static Vector3 z(0, 0, 1);
    static Transform ident(Hmx::Matrix3(x, y, z), Vector3(0, 0, 0));
    if (mUseColorAdjust)
        return mColorXfm.mColorXfm;
    else
        return ident;
}

void RndEnviron::Save(BinStream &bs) {
    bs << 0x10;
    SAVE_SUPERCLASS(Hmx::Object);
    bs << mLightsReal << mLightsApprox;
    bs << (const Vector4 &)mAmbientColor << mFogStart << mFogEnd
       << (const Vector4 &)mFogColor;
    bs << mFogEnable;
    bs << mAnimateFromPreset;
    bs << mFadeOut;
    bs << mFadeStart;
    bs << mFadeEnd;
    bs << mFadeMax;
    bs << mFadeRef << mLRFade;
    bs << mAmbientFogOwner;
    bs << mUseColorAdjust;
    mColorXfm.Save(bs);
    bs << mAOStrength;
    bs << mIntensityRate;
    bs << mExposure;
    bs << mWhitePoint;
    bs << mUseToneMapping;
}

bool RndEnviron::IsLightInList(const RndLight *light, const ObjPtrList<RndLight> &pList)
    const {
    if (light == nullptr)
        return false;
    return pList.find((const Hmx::Object *)light) != pList.end();
}

bool RndEnviron::IsFake(RndLight *l) const { return IsLightInList(l, mLightsApprox); }
bool RndEnviron::IsReal(RndLight *l) const { return IsLightInList(l, mLightsReal); }

void RndEnviron::RemoveLight(RndLight *l) {
    mLightsReal.remove(l);
    mLightsApprox.remove(l);
}

void RndEnviron::OnRemoveAllLights() {
    mLightsReal.clear();
    mLightsApprox.clear();
    mLightsOld.clear();
}

void RndEnviron::Replace(ObjRef *from, Hmx::Object *to) {
    if (RefIs(from, mAmbientFogOwner)) {
        if (mAmbientFogOwner == this) {
            mAmbientFogOwner = this;
        } else {
            RndEnviron *env = dynamic_cast<RndEnviron *>(to);
            if (env) {
                mAmbientFogOwner.SetObjConcrete(env->mAmbientFogOwner.Ptr());
            } else {
                mAmbientFogOwner = this;
            }
        }
        return;
    }
    Hmx::Object::Replace(from, to);
}

BEGIN_LOADS(RndEnviron)
    int rev;
    bs >> rev;
    gRev = getHmxRev(rev);
    gAltRev = getAltRev(rev);
    if (gRev > 1)
        Hmx::Object::Load(bs);
    if (gRev < 3) {
        RndDrawable::DumpLoad(bs);
    }
    if (gRev < 0xF) {
        bs >> mLightsOld;
    } else {
        bs >> mLightsReal;
        bs >> mLightsApprox;
    }
    bs >> mAmbientColor;
    bs >> mFogStart;
    bs >> mFogEnd;
    if (gRev < 1) {
        int dummy;
        bs >> dummy;
    }
    bs >> mFogColor;
    if (gRev < 1) {
        int enabled;
        bs >> enabled;
        mFogEnable = enabled;
    } else {
        bs >> mFogEnable;
    }
    if (gRev > 3)
        bs >> mAnimateFromPreset;
    if (gRev > 4) {
        bs >> mFadeOut;
        bs >> mFadeStart;
        bs >> mFadeEnd;
        if (gRev > 5)
            bs >> mFadeMax;
    }
    if (gRev > 8) {
        bs >> mFadeRef;
        bs >> (Hmx::Color &)mLRFade;
    }
    if (gRev > 6) {
        bs >> mAmbientFogOwner;
        if (!mAmbientFogOwner) {
            mAmbientFogOwner = this;
        }
    }
    if (gRev > 7) {
        bs >> mUseColorAdjust;
        mColorXfm.Load(bs);
    }
    if (gRev > 9) {
        if (gRev < 0xD) {
            int dummy;
            bs >> dummy;
        }
        bs >> mAOStrength;
    }
    if (gRev > 0xA) {
        bs >> mIntensityRate;
        bs >> mExposure;
        bs >> mWhitePoint;
        bs >> mUseToneMapping;
    }
    if (gRev == 0xB) {
        int dummy;
        bs >> dummy;
    } else if (gRev > 0xB && gRev < 0xE) {
        int dummy;
        bs >> dummy;
    }
END_LOADS

BEGIN_PROPSYNCS(RndEnviron)
    SYNC_PROP(lights_real, mLightsReal)
    SYNC_PROP(lights_approx, mLightsApprox)
    SYNC_PROP(ambient_color, mAmbientFogOwner->mAmbientColor)
    SYNC_PROP(ambient_alpha, mAmbientFogOwner->mAmbientColor.alpha)
    SYNC_PROP(fog_enable, mAmbientFogOwner->mFogEnable)
    SYNC_PROP(fog_start, mAmbientFogOwner->mFogStart)
    SYNC_PROP(fog_end, mAmbientFogOwner->mFogEnd)
    SYNC_PROP(fog_color, mAmbientFogOwner->mFogColor)
    SYNC_PROP(ambient_fog_owner, mAmbientFogOwner)
    SYNC_PROP(fade_out, mFadeOut)
    SYNC_PROP(fade_start, mFadeStart)
    SYNC_PROP(fade_end, mFadeEnd)
    SYNC_PROP(fade_max, mFadeMax)
    SYNC_PROP(fade_ref, mFadeRef)
    SYNC_PROP(left_out, mLRFade.x)
    SYNC_PROP(left_opaque, mLRFade.y)
    SYNC_PROP(right_opaque, mLRFade.z)
    SYNC_PROP(right_out, mLRFade.w)
    SYNC_PROP(ao_strength, mAOStrength)
    SYNC_PROP(intensity_rate, mIntensityRate)
    SYNC_PROP(exposure, mExposure)
    SYNC_PROP(white_point, mWhitePoint)
    SYNC_PROP(tone_map, mUseToneMapping)
    SYNC_PROP(use_color_adjust, mUseColorAdjust)
    SYNC_PROP_MODIFY(hue, mColorXfm.mHue, mColorXfm.AdjustColorXfm())
    SYNC_PROP_MODIFY(saturation, mColorXfm.mSaturation, mColorXfm.AdjustColorXfm())
    SYNC_PROP_MODIFY(lightness, mColorXfm.mLightness, mColorXfm.AdjustColorXfm())
    SYNC_PROP_MODIFY(brightness, mColorXfm.mBrightness, mColorXfm.AdjustColorXfm())
    SYNC_PROP_MODIFY(contrast, mColorXfm.mContrast, mColorXfm.AdjustColorXfm())
    SYNC_PROP_MODIFY(in_lo, mColorXfm.mLevelInLo, mColorXfm.AdjustColorXfm())
    SYNC_PROP_MODIFY(in_hi, mColorXfm.mLevelInHi, mColorXfm.AdjustColorXfm())
    SYNC_PROP_MODIFY(out_lo, mColorXfm.mLevelOutLo, mColorXfm.AdjustColorXfm())
    SYNC_PROP_MODIFY(out_hi, mColorXfm.mLevelOutHi, mColorXfm.AdjustColorXfm())
    SYNC_PROP(animate_from_preset, mAnimateFromPreset)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_COPYS(RndEnviron)
    COPY_SUPERCLASS(Hmx::Object)
    if (ty != kCopyFromMax) {
        CREATE_COPY(RndEnviron)
        BEGIN_COPYING_MEMBERS
            COPY_MEMBER(mLightsReal)
            COPY_MEMBER(mLightsApprox)
            COPY_MEMBER(mLightsOld)
            COPY_MEMBER(mFadeOut)
            COPY_MEMBER(mFadeStart)
            COPY_MEMBER(mFadeEnd)
            COPY_MEMBER(mFadeMax)
            COPY_MEMBER(mFadeRef)
            COPY_MEMBER(mLRFade)
            COPY_MEMBER(mUseColorAdjust)
            COPY_MEMBER(mColorXfm)
            COPY_MEMBER(mAnimateFromPreset)
            COPY_MEMBER(mAOEnabled)
            COPY_MEMBER(mAOStrength)
            COPY_MEMBER(mIntensityRate)
            COPY_MEMBER(mExposure)
            COPY_MEMBER(mWhitePoint)
            COPY_MEMBER(mUseToneMapping)
            if (ty != kCopyShallow) {
                mAmbientFogOwner = this;
                mAmbientColor = c->mAmbientFogOwner->mAmbientColor;
                mFogColor = c->mAmbientFogOwner->mFogColor;
                mFogStart = c->mAmbientFogOwner->mFogStart;
                mFogEnd = c->mAmbientFogOwner->mFogEnd;
                mFogEnable = c->mAmbientFogOwner->mFogEnable;
            } else {
                COPY_MEMBER(mAmbientFogOwner)
            }
        END_COPYING_MEMBERS
    }
END_COPYS

bool RndEnviron::IsValidRealLight(const RndLight *l) const {
    bool ret = false;
    RndLight::Type ty = l->GetType();
    if (ty == RndLight::kPoint || ty == RndLight::kFakeSpot)
        ret = true;
    return ret;
}

void RndEnviron::AddLight(RndLight *l) {
    if (IsLightInList(l, mLightsReal) || IsLightInList(l, mLightsApprox)) {
        MILO_NOTIFY("%s already in %s", l->Name(), Name());
    } else {
        if (IsValidRealLight(l))
            mLightsReal.push_back(l);
        else
            mLightsApprox.push_back(l);
    }
}

void RndEnviron::ReclassifyLights() {
    if (!mLightsOld.empty()) {
        for (ObjPtrList<RndLight>::iterator it = mLightsOld.begin();
             it != mLightsOld.end();
             ++it) {
            AddLight(*it);
        }
        mLightsOld.clear();
    }
}

void RndEnviron::Select(const Vector3 *v) {
    sCurrent = this;
    sCurrentPosSet = v;
    if (v) {
        sCurrentPos = *v;
    } else {
        sCurrentPos.Zero();
    }
    ReclassifyLights();
}

DataNode RndEnviron::OnAllowableLights_Real(const DataArray *da) {
    DataArrayPtr ptr;
    for (ObjDirItr<RndLight> it(Dir(), true); it != nullptr; ++it) {
        if (!IsLightInList(it, mLightsReal) && !IsLightInList(it, mLightsApprox)
            && IsValidRealLight(it) == 1U) {
            ptr->Insert(ptr->Size(), &*it);
        }
    }
    static DataNode &milo_prop_path = DataVariable("milo_prop_path");
    if (milo_prop_path.Type() == kDataArray) {
        if (milo_prop_path.Array()->Size() == 2) {
            ptr->Insert(
                ptr->Size(), *NextItr(mLightsReal.begin(), milo_prop_path.Array()->Int(1))
            );
        }
    }
    return ptr;
}

DataNode RndEnviron::OnAllowableLights_Approx(const DataArray *da) {
    DataArrayPtr ptr;
    for (ObjDirItr<RndLight> it(Dir(), true); it != 0; ++it) {
        if (!IsLightInList(it, mLightsReal) && !IsLightInList(it, mLightsApprox)) {
            ptr->Insert(ptr->Size(), &*it);
        }
    }
    static DataNode &milo_prop_path = DataVariable("milo_prop_path");
    if (milo_prop_path.Type() == kDataArray) {
        if (milo_prop_path.Array()->Size() == 2) {
            ptr->Insert(
                ptr->Size(),
                *NextItr(mLightsApprox.begin(), milo_prop_path.Array()->Int(1))
            );
        }
    }
    return ptr;
}

BEGIN_HANDLERS(RndEnviron)
    HANDLE_ACTION(remove_all_lights, OnRemoveAllLights())
    HANDLE_ACTION(toggle_ao, mAOEnabled = !mAOEnabled)
    HANDLE_ACTION(remove_light, RemoveLight(_msg->Obj<RndLight>(2)))
    HANDLE(allowable_lights_real, OnAllowableLights_Real)
    HANDLE(allowable_lights_approx, OnAllowableLights_Approx)
    HANDLE_ACTION(select, Select(nullptr))
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

// Retail RB3-360 RndEnviron derives from Hmx::Object, not RndDrawable/
// RndTransformable, so it has no Draw()/Showing()/WorldXfm(). Selection is
// driven directly through Select() by the venue render path.

RndEnviron::~RndEnviron() {
    if (sCurrent == this) {
        sCurrent = nullptr;
        sCurrentPosSet = false;
        sCurrentPos.Zero();
    }
}

RndEnviron::RndEnviron()
    : mLightsReal(this), mLightsApprox(this), mLightsOld(this), mAmbientColor(0, 0, 0, 1),
      mAmbientFogOwner(this, this), mFogEnable(0), mFogStart(0), mFogEnd(1),
      mFogColor(1, 1, 1), mFadeOut(0), mFadeStart(0), mFadeEnd(1000), mFadeMax(1),
      mFadeRef(this), mLRFade(0, 0, 0, 0), mColorXfm(), mUseColorAdjust(0),
      mAnimateFromPreset(1), mAOEnabled(1), mAOStrength(1), mUpdateTimer(),
      mIntensityAverage(0), mIntensityRate(0.1f), mExposure(1), mWhitePoint(1),
      mUseToneMapping(0), mUseApprox_Local(1), mUseApprox_Global(1) {
    mUpdateTimer.Restart();
}

void RndEnviron::UpdateApproxLighting(const Vector3 *) {}
