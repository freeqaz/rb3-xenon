#include "rndobj/EnvAnim.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "math/Color.h"
#include "math/Utl.h"
#include "math/Vec.h"
#include "utl/BinStream.h"

RndEnvAnim::RndEnvAnim() : mEnviron(this), mKeysOwner(this, this) {}

// X360 only. The whole point of this specialization is to release via the
// ObjRefConcrete::mOwner member, and mOwner exists ONLY in the retail arm of
// obj/ObjPtr_p.h (the `#else` at :74-140); the HX_NATIVE arm (:19-73) has a
// two-member ObjRefConcrete whose dtor releases via `this`. So natively there
// is nothing to specialize -- and EnvAnim.h's matching declaration is likewise
// #ifndef'd, so native TUs use the primary template uniformly and no ODR
// hazard is created between this TU and the ones that instantiate implicitly.
#ifndef HX_NATIVE
template <>
ObjRefConcrete<RndEnvAnim, ObjectDir>::~ObjRefConcrete() {
    if (mObject)
        mObject->Release(this->RefOwner());
}
#endif

void RndEnvAnim::Replace(ObjRef *ref, Hmx::Object *obj) {
    if (RefIs(ref, mKeysOwner)) {
        if (!obj)
            mKeysOwner.SetObjConcrete(this);
        else
            mKeysOwner.SetObjConcrete(dynamic_cast<RndEnvAnim *>(obj)->mKeysOwner.Ptr());
        return;
    }
    Hmx::Object::Replace(ref, obj);
}

BEGIN_HANDLERS(RndEnvAnim)
    HANDLE_SUPERCLASS(RndAnimatable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(RndEnvAnim)
    SYNC_SUPERCLASS(RndAnimatable)
END_PROPSYNCS

// Retail loads the save revision from a MUTABLE .data global (0x82C70A34,
// value 4) rather than folding an immediate, so this must not be a literal or
// a const: at /O1 either would become `li r11, 4`.
static int gSaveRev = 4;

void RndEnvAnim::Save(BinStream &bs) {
    bs << gSaveRev;
    Hmx::Object::Save(bs);
    RndAnimatable::Save(bs);
    bs << mEnviron << mAmbientColorKeys << mKeysOwner << mFogColorKeys;
    bs << mFogRangeKeys;
}

void RndEnvAnim::Load(BinStream &bs) {
    int rev;
    bs >> rev;
    BinStreamRev d(bs, rev);
    if (rev > 3)
        Hmx::Object::Load(bs);
    RndAnimatable::Load(bs);
    d >> mEnviron >> mAmbientColorKeys >> mKeysOwner;
    if (!mKeysOwner)
        mKeysOwner = this;
    if (rev > 1)
        d >> mFogColorKeys;
    if (rev > 2)
        d >> mFogRangeKeys;
}

BEGIN_COPYS(RndEnvAnim)
    CREATE_COPY_AS(RndEnvAnim, l)
    MILO_ASSERT(l, 0x6B);
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndAnimatable)
    COPY_MEMBER_FROM(l, mEnviron)
    if (ty == kCopyShallow) {
        COPY_MEMBER_FROM(l, mKeysOwner)
    } else {
        mKeysOwner = this;
        mAmbientColorKeys = l->mKeysOwner->mAmbientColorKeys;
        mFogColorKeys = l->mKeysOwner->mFogColorKeys;
        mFogRangeKeys = l->mKeysOwner->mFogRangeKeys;
    }
END_COPYS

void RndEnvAnim::Print() {
    TextStream &ts = TheDebug;
    ts << "   environ: " << mEnviron.Ptr() << "\n";
    ts << "   keysOwner: " << mKeysOwner.Ptr() << "\n";
    ts << "   ambientColorKeys: " << mAmbientColorKeys << "\n";
    ts << "   fogColorKeys: " << mFogColorKeys << "\n";
    ts << "   fogRangeKeys: " << mFogRangeKeys << "\n";
}

float RndEnvAnim::EndFrame() {
    return Max(FogColorKeys().LastFrame(), AmbientColorKeys().LastFrame());
}

void RndEnvAnim::SetFrame(float frame, float blend) {
    RndAnimatable::SetFrame(frame, blend);
    if (mEnviron) {
        if (!AmbientColorKeys().empty()) {
            Hmx::Color col(mEnviron->AmbientColor());
            AmbientColorKeys().AtFrame(frame, col);
            if (blend != 1.0f) {
                Interp(mEnviron->AmbientColor(), col, blend, col);
            }
            mEnviron->SetAmbientColor(col);
        }
        if (!FogColorKeys().empty()) {
            Hmx::Color col(mEnviron->FogColor());
            FogColorKeys().AtFrame(frame, col);
            if (blend != 1.0f) {
                Interp(mEnviron->FogColor(), col, blend, col);
            }
            mEnviron->SetFogColor(col);
        }
        if (!FogRangeKeys().empty()) {
            Vector2 vec(mEnviron->FogStart(), mEnviron->FogEnd());
            FogRangeKeys().AtFrame(frame, vec);
            if (blend != 1.0f) {
                Interp(mEnviron->FogStart(), vec.x, blend, vec.x);
                Interp(mEnviron->FogEnd(), vec.y, blend, vec.y);
            }
            mEnviron->SetFogRange(vec.x, vec.y);
        }
    }
}

void RndEnvAnim::SetKey(float frame) {
    const ObjPtr<RndEnviron> &_ref0 = mEnviron;
    if (_ref0) {
        FogColorKeys().Add(_ref0->FogColor(), frame, true);
        FogRangeKeys().Add(
            Vector2(_ref0->FogStart(), _ref0->FogEnd()), frame, true
        );
        AmbientColorKeys().Add(_ref0->AmbientColor(), frame, true);
    }
}
