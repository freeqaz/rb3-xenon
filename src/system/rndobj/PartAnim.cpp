// Retail inlines this TU's owner-only ObjPtr ctor(s) with the vtable
// materialization pinned AFTER the member stores -- the
// RB3_OBJPTR_FORCEINLINE_CTOR signature (see obj/ObjPtr_p.h). The
// extent census shows delta ~= -16 * (surplus bl) for this TU's ctor,
// i.e. one un-inlined ObjPtr ctor per surplus call.
#define RB3_OBJPTR_FORCEINLINE_CTOR

#include "rndobj/PartAnim.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Anim.h"
#include "rndobj/Part.h"

template BinStream &operator>><Hmx::Color>(BinStream &, Key<Hmx::Color> &);
template BinStream &
operator>>(BinStream &, std::vector<Key<Hmx::Color> > &);

// See the specialization declaration + rationale comment in obj/ObjPtr_p.h.
// Defined here (rather than generically in ObjPtr_p.h) because it needs
// RndParticleSys's complete type, which the generic header only forward-
// declares.
//
// X360 only, for the same reason as the RndEnvAnim twin in EnvAnim.cpp: the
// body reads ObjRefConcrete::mOwner, which exists only in the retail arm of
// ObjPtr_p.h -- and that header only DECLARES this specialization inside the
// same retail arm (:123). Compiling the definition natively would therefore
// also be an ODR hazard, not just a compile error: this TU would emit a strong
// specialization while every other native TU emitted the primary template.
#ifndef HX_NATIVE
template <>
ObjRefConcrete<RndParticleSys, ObjectDir>::~ObjRefConcrete() {
    if (mObject)
        mObject->Release(reinterpret_cast<ObjRefOwner *>(mOwner));
}
#endif

#pragma region Hmx::Object

// RB3-360 retail: the Load reads the archive rev from a file-scope static
// halfword (lbl_82CCxxxx, `lhz`) populated once at Load entry, not from the
// BinStreamRev member. Mirror that so the rev comparisons match.
static unsigned short sPartAnimRev;

RndParticleSysAnim::RndParticleSysAnim() : mParticleSys(this), mKeysOwner(this, this) {}

void RndParticleSysAnim::Replace(ObjRef *from, Hmx::Object *to) {
    if (RefIs(from, mKeysOwner)) {
        // When our keys owner reference is being replaced:
        if (mKeysOwner == this) {
            // We own our keys - keep owning them
            mKeysOwner.SetObjConcrete(this);
        } else {
            // Try to delegate to replacement's keys owner
            RndParticleSysAnim *sysTo = dynamic_cast<RndParticleSysAnim *>(to);
            if (sysTo) {
                mKeysOwner.SetObjConcrete(sysTo->mKeysOwner);
            } else {
                // Replacement isn't a ParticleSysAnim, take ownership
                mKeysOwner.SetObjConcrete(this);
            }
        }
        return;
    }
    Hmx::Object::Replace(from, to);
}

BEGIN_HANDLERS(RndParticleSysAnim)
    HANDLE_ACTION(set_particle_sys, SetParticleSys(_msg->Obj<RndParticleSys>(2)))
    HANDLE_SUPERCLASS(RndAnimatable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(RndParticleSysAnim)
    SYNC_SUPERCLASS(RndAnimatable)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(RndParticleSysAnim)
    // RB3-360 retail: rev written from a constant-initialized static (.data
    // lwz), not an immediate — SAVE_REVS(3,0)'s folded li mismatches.
    static int REV = 3;
    bs << REV;
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndAnimatable)
    bs << mParticleSys << mStartColorKeys << mEndColorKeys << mEmitRateKeys;
    bs << mKeysOwner << mSpeedKeys << mLifeKeys << mStartSizeKeys;
END_SAVES

BEGIN_COPYS(RndParticleSysAnim)
    CREATE_COPY_AS(RndParticleSysAnim, l)
    MILO_ASSERT(l, 0x7E);
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndAnimatable)
    COPY_MEMBER_FROM(l, mParticleSys)
    if (ty == kCopyShallow || ty == kCopyFromMax && l->mKeysOwner != l) {
        COPY_MEMBER_FROM(l, mKeysOwner)
    } else {
        mKeysOwner = this;
        mStartColorKeys = l->mKeysOwner->mStartColorKeys;
        mEndColorKeys = l->mKeysOwner->mEndColorKeys;
        mEmitRateKeys = l->mKeysOwner->mEmitRateKeys;
        mSpeedKeys = l->mKeysOwner->mSpeedKeys;
        mLifeKeys = l->mKeysOwner->mLifeKeys;
        mStartSizeKeys = l->mKeysOwner->mStartSizeKeys;
    }
END_COPYS

INIT_REVS(3, 0)

BEGIN_LOADS(RndParticleSysAnim)
    LOAD_REVS(bs)
    ASSERT_REVS(3, 0)
    sPartAnimRev = d.rev;
    if (sPartAnimRev > 2) {
        LOAD_SUPERCLASS(Hmx::Object)
    }
    LOAD_SUPERCLASS(RndAnimatable)
    d >> mParticleSys >> mStartColorKeys >> mEndColorKeys;
    if (sPartAnimRev < 2) {
        float scale = 1.0f;
        Keys<float, float> floatKeys;
        d >> floatKeys >> mKeysOwner;
        if (sPartAnimRev == 1) {
            d >> scale;
        }
        mEmitRateKeys.clear();
        mEmitRateKeys.reserve(floatKeys.size());
        for (Keys<float, float>::iterator it = floatKeys.begin(); it != floatKeys.end();
             ++it) {
            Key<Vector2> vecKey;
            vecKey.value = Vector2(it->value, it->value * scale);
            vecKey.frame = it->frame;
            mEmitRateKeys.push_back(vecKey);
        }
    } else {
        d >> mEmitRateKeys >> mKeysOwner;
    }
    if (!mKeysOwner)
        mKeysOwner = this;
    if (sPartAnimRev > 1) {
        d >> mSpeedKeys >> mLifeKeys >> mStartSizeKeys;
    }
END_LOADS

void RndParticleSysAnim::Print() {
    TheDebug << "   particleSys: " << mParticleSys << "\n";
    TheDebug << "   framesOwner: " << mKeysOwner << "\n";
    TheDebug << "   startColorKeys: " << mStartColorKeys << "\n";
    TheDebug << "   endColorKeys: " << mEndColorKeys << "\n";
    TheDebug << "   emitRateKeys: " << mEmitRateKeys << "\n";
    TheDebug << "   speedKeys: " << mSpeedKeys << "\n";
    TheDebug << "   startSizeKeys: " << mStartSizeKeys << "\n";
    TheDebug << "   lifeKeys: " << mLifeKeys << "\n";
}

#pragma endregion
#pragma region RndAnimatable

void RndParticleSysAnim::SetFrame(float frame, float blend) {
    RndAnimatable::SetFrame(frame, blend);
    if (mParticleSys) {
        if (!StartColorKeys().empty()) {
            Hmx::Color colorlow(mParticleSys->StartColorLow());
            Hmx::Color colorhigh(mParticleSys->StartColorHigh());
            StartColorKeys().AtFrame(frame, colorlow);
            Add(colorlow, mParticleSys->StartColorHigh(), colorhigh);
            Subtract(colorhigh, mParticleSys->StartColorLow(), colorhigh);
            if (blend != 1.0f) {
                Interp(mParticleSys->StartColorLow(), colorlow, blend, colorlow);
                Interp(mParticleSys->StartColorHigh(), colorhigh, blend, colorhigh);
            }
            mParticleSys->SetStartColor(colorlow, colorhigh);
        }
        if (!EndColorKeys().empty()) {
            Hmx::Color colorlow(mParticleSys->EndColorLow());
            Hmx::Color colorhigh(mParticleSys->EndColorHigh());
            EndColorKeys().AtFrame(frame, colorlow);
            Add(colorlow, mParticleSys->EndColorHigh(), colorhigh);
            Subtract(colorhigh, mParticleSys->EndColorLow(), colorhigh);
            if (blend != 1.0f) {
                Interp(mParticleSys->StartColorLow(), colorlow, blend, colorlow);
                Interp(mParticleSys->StartColorHigh(), colorhigh, blend, colorhigh);
            }
            mParticleSys->SetEndColor(colorlow, colorhigh);
        }
        if (!EmitRateKeys().empty()) {
            Vector2 rate(mParticleSys->EmitRate());
            EmitRateKeys().AtFrame(frame, rate);
            if (blend != 1.0f) {
                Interp(mParticleSys->EmitRate(), rate, blend, rate);
            }
            mParticleSys->SetEmitRate(rate.x, rate.y);
        }
        if (!SpeedKeys().empty()) {
            Vector2 speed(mParticleSys->Speed());
            SpeedKeys().AtFrame(frame, speed);
            if (blend != 1.0f) {
                Interp(mParticleSys->Speed(), speed, blend, speed);
            }
            mParticleSys->SetSpeed(speed.x, speed.y);
        }
        if (!LifeKeys().empty()) {
            Vector2 life(mParticleSys->Life());
            LifeKeys().AtFrame(frame, life);
            if (blend != 1.0f) {
                Interp(mParticleSys->Life(), life, blend, life);
            }
            mParticleSys->SetLife(life.x, life.y);
        }
        if (!StartSizeKeys().empty()) {
            Vector2 startsize(mParticleSys->StartSize());
            StartSizeKeys().AtFrame(frame, startsize);
            if (blend != 1.0f) {
                Interp(mParticleSys->StartSize(), startsize, blend, startsize);
            }
            mParticleSys->SetStartSize(startsize.x, startsize.y);
        }
    }
}

float RndParticleSysAnim::EndFrame() {
    float last =
        Max(StartColorKeys().LastFrame(),
            EndColorKeys().LastFrame(),
            EmitRateKeys().LastFrame());
    last = Max(last, SpeedKeys().LastFrame(), LifeKeys().LastFrame());
    last = Max(last, StartSizeKeys().LastFrame());
    return last;
}

void RndParticleSysAnim::SetKey(float frame) {
    if (mParticleSys) {
        StartColorKeys().Add(mParticleSys->StartColorLow(), frame, true);
        EndColorKeys().Add(mParticleSys->EndColorLow(), frame, true);
        EmitRateKeys().Add(mParticleSys->EmitRate(), frame, true);
        SpeedKeys().Add(mParticleSys->Speed(), frame, true);
        LifeKeys().Add(mParticleSys->Life(), frame, true);
        StartSizeKeys().Add(mParticleSys->StartSize(), frame, true);
    }
}

#pragma endregion
#pragma region RndParticleSysAnim

void RndParticleSysAnim::SetParticleSys(RndParticleSys *sys) { mParticleSys = sys; }

// sw2 scatter-include (default/PartAnim <- hamobj/HamSupereasyData.cpp)
#define gRev gRev_HamSupereasyData
#define gAltRev gAltRev_HamSupereasyData
#include "hamobj/HamSupereasyData.cpp"
#undef gRev
#undef gAltRev
