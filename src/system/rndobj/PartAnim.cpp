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

// W17-OWN: the ObjRefConcrete<RndParticleSys, ObjectDir> dtor specialisation that lived
// here is gone -- see the note after ~ObjRefConcrete in obj/ObjPtr_p.h.

#pragma region Hmx::Object


RndParticleSysAnim::RndParticleSysAnim() : mParticleSys(this), mKeysOwner(this, this) {}

// Retail 0x8247F168 (140 B). See RndCamAnim::Replace for the full derivation:
// the ring compares the VIRTUAL BASE of the held pointer (Hmx::Object is a
// virtual base of RndParticleSysAnim), there is no Hmx::Object::Replace
// fallback, and the cast arm must bind a reference so &q->mKeysOwner stays
// materialized (addi r11,r3,0x64; lwz r4,0x8(r11)).
void RndParticleSysAnim::Replace(ObjRef *from, Hmx::Object *to) {
    if (static_cast<Hmx::Object *>(mKeysOwner.Ptr())
        == reinterpret_cast<Hmx::Object *>(from)) {
        if (!to) {
            mKeysOwner.SetOwnerObj(this);
        } else {
            const ObjOwnerPtr<RndParticleSysAnim> &ko =
                dynamic_cast<RndParticleSysAnim *>(to)->mKeysOwner;
            mKeysOwner.SetOwnerObj(ko.Ptr());
        }
    }
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

// Retail RndParticleSysAnim::Load (0x82480D88) reads the revision word whole
// into a stack local (`bs >> rev`, then lwz/cmpwi against 2/1) -- rb3-Wii's
// shape -- and passes the raw stream to every read. No rev wrapper exists
// (band.exe has no `.?AVBinStreamRev@@` descriptor).
BEGIN_LOADS(RndParticleSysAnim)
    int rev;
    bs >> rev;
    if (rev > 2) {
        Hmx::Object::Load(bs);
    }
    RndAnimatable::Load(bs);
    bs >> mParticleSys >> mStartColorKeys >> mEndColorKeys;
    if (rev < 2) {
        float scale = 1.0f;
        Keys<float, float> floatKeys;
        bs >> floatKeys >> mKeysOwner;
        if (rev == 1) {
            bs >> scale;
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
        bs >> mEmitRateKeys >> mKeysOwner;
    }
    if (!mKeysOwner)
        mKeysOwner = this;
    if (rev > 1) {
        bs >> mSpeedKeys >> mLifeKeys >> mStartSizeKeys;
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
