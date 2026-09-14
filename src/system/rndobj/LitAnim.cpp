// Retail inlines this TU's owner-only ObjPtr ctor(s) with the vtable
// materialization pinned AFTER the member stores -- the
// RB3_OBJPTR_FORCEINLINE_CTOR signature (see obj/ObjPtr_p.h). The
// extent census shows delta ~= -16 * (surplus bl) for this TU's ctor,
// i.e. one un-inlined ObjPtr ctor per surplus call.
#define RB3_OBJPTR_FORCEINLINE_CTOR

#include "rndobj/LitAnim.h"
#include "math/Color.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Anim.h"

#pragma region Hmx::Object

RndLightAnim::RndLightAnim() : mLight(this), mKeysOwner(this, this) {}

// Retail (0x82471518) has NO Hmx::Object::Replace fallback -- the body's only
// calls are __RTDynamicCast and SetOwnerObj, then blr.  Hmx::Object::Replace is
// EMPTY in the match build (its whole body is #ifdef HX_NATIVE, Object.cpp:228),
// but with no LTCG an empty out-of-line callee still costs a real `bl`, and
// there is no such `bl` in the retail body.  Kept under HX_NATIVE, where the
// base really does forward to mSinks.
// Retail also tests mKeysOwner, NOT mLight: the compared member is the held
// pointer at Object-0x8, i.e. mKeysOwner.mObject (mKeysOwner at 0x28, vbase at
// 0x38 per /d1reportSingleClassLayout -- so Object-0x8 == this+0x30 == the
// ObjOwnerPtr's mObject).  `addi r11,r3,40` on the cast result is mKeysOwner
// again, and `subi r4,r31,56` is the vbase.  Same body as the RndEnvAnim /
// RndMatAnim / RndTransAnim triple.
void RndLightAnim::Replace(ObjRef *from, Hmx::Object *to) {
    if (RefIs(from, mKeysOwner)) {
        if (!to)
            mKeysOwner.SetOwnerObj(this);
        else
            mKeysOwner.SetOwnerObj(dynamic_cast<RndLightAnim *>(to)->mKeysOwner.Ptr());
        return;
    }
#ifdef HX_NATIVE
    Hmx::Object::Replace(from, to);
#endif
}

BEGIN_HANDLERS(RndLightAnim)
    HANDLE(copy_keys, OnCopyKeys)
    HANDLE_SUPERCLASS(RndAnimatable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(RndLightAnim)
    SYNC_PROP(light, mLight)
    SYNC_PROP(color_keys, mColorKeys)
    SYNC_PROP(keys_owner, mKeysOwner)
    SYNC_SUPERCLASS(RndAnimatable)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(RndLightAnim)
    SAVE_REVS(2, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndAnimatable)
    bs << mLight << mColorKeys << mKeysOwner;
END_SAVES

BEGIN_COPYS(RndLightAnim)
    CREATE_COPY_AS(RndLightAnim, l);
    MILO_ASSERT(l, 0x72);
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndAnimatable)
    COPY_MEMBER_FROM(l, mLight)
    if (ty == kCopyShallow || ty == kCopyFromMax && l->mKeysOwner != l) {
        mKeysOwner = l->mKeysOwner;
    } else {
        mKeysOwner = this;
        mColorKeys = l->mKeysOwner->mColorKeys;
    }
END_COPYS

INIT_REVS(2, 0)

BEGIN_LOADS(RndLightAnim)
    LOAD_REVS(bs)
    ASSERT_REVS(2, 0)
    if (d.rev > 1) {
        Hmx::Object::Load(bs);
    }
    RndAnimatable::Load(bs);
    bs >> mLight;
    if (d.rev < 1) {
        Keys<Hmx::Color, Hmx::Color> keys;
        d >> keys;
    }
    d >> mColorKeys;
    if (d.rev < 1) {
        Keys<Hmx::Color, Hmx::Color> keys;
        d >> keys;
    }
    d >> mKeysOwner;
    if (!mKeysOwner) {
        mKeysOwner = this;
    }
END_LOADS

#pragma endregion
#pragma region RndAnimatable

void RndLightAnim::SetFrame(float frame, float blend) {
    RndAnimatable::SetFrame(frame, blend);
    if (mLight) {
        if (!ColorKeys().empty()) {
            Hmx::Color ref;
            ColorKeys().AtFrame(frame, ref);
            if (blend != 1.0f) {
                Interp(mLight->GetColor(), ref, blend, ref);
            }
            mLight->SetColor(ref);
        }
    }
}

float RndLightAnim::EndFrame() { return ColorKeys().LastFrame(); }

void RndLightAnim::SetKey(float frame) {
    if (mLight) {
        ColorKeys().Add(mLight->GetColor(), frame, true);
    }
}

#pragma endregion
#pragma region RndLightAnim

void RndLightAnim::SetKeysOwner(RndLightAnim *o) {
    MILO_ASSERT(o, 0x27);
    mKeysOwner = o;
}

#pragma endregion
#pragma region Handlers

DataNode RndLightAnim::OnCopyKeys(DataArray *a) {
    SetKeysOwner(this);
    mColorKeys = a->Obj<RndLightAnim>(2)->ColorKeys();
    float f = a->Float(3);
    FOREACH (it, mColorKeys) {
        Multiply(it->value, f, it->value);
    }
    return 0;
}

#pragma endregion
