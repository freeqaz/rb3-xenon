#include "rndobj/EnvAnim.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "math/Color.h"
#include "math/Utl.h"
#include "math/Vec.h"
#include "utl/BinStream.h"

RndEnvAnim::RndEnvAnim() : mEnviron(this), mKeysOwner(this, this) {}

template <>
ObjRefConcrete<RndEnvAnim, ObjectDir>::~ObjRefConcrete() {
    if (mObject)
        mObject->Release(this->RefOwner());
}

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

void RndEnvAnim::Save(BinStream &) { MILO_ASSERT(0, 0x46); }

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
