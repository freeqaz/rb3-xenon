#include "rndobj/CamAnim.h"
#include "obj/Object.h"
#include "rndobj/Anim.h"
#include "rndobj/Utl.h"
#include "utl/BinStream.h"

#pragma region Hmx::Object

RndCamAnim::RndCamAnim() : mCam(this, 0), mKeysOwner(this, this) {}

RndCamAnim::~RndCamAnim() {}

void RndCamAnim::Replace(ObjRef *from, Hmx::Object *to) {
    if (RefIs(from, mKeysOwner)) {
        if (mKeysOwner == this) {
            RndCamAnim *camTo = dynamic_cast<RndCamAnim *>(to);
            if (camTo) {
                mKeysOwner = camTo->KeysOwner();
            }
        } else {
            mKeysOwner = this;
        }
        return;
    } else
        Hmx::Object::Replace(from, to);
}

BEGIN_HANDLERS(RndCamAnim)
    HANDLE_SUPERCLASS(RndAnimatable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(RndCamAnim)
#ifdef HX_NATIVE
    // RB3-360 retail enumerates NO properties here -- these three are DC3-era
    // additions.  Proven from retail asm (lane CP-2, 2026-08-02), not from oracle
    // agreement: ?SyncProperty@RndCamAnim@@ is 120 B in retail and contains
    // exactly two calls -- DataNode::Sym, then RndAnimatable::SyncProperty with
    // the same `subi r3,...,0x24` this-adjustment we emit.  There is no room for
    // a property chain, and none of the three ??0Symbol@@ constructions or
    // PropSync calls our 392 B body emits appears anywhere in it.  rb3-Wii (the
    // RB3-era oracle) independently agrees: SYNC_SUPERCLASS(RndAnimatable) only.
    // Kept for the native port, which drives object property editing through
    // SyncProperty.
    SYNC_PROP(cam, mCam)
    SYNC_PROP(fov_keys, mFovKeys)
    SYNC_PROP(keys_owner, mKeysOwner)
#endif
    SYNC_SUPERCLASS(RndAnimatable)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(RndCamAnim)
    SAVE_REVS(2, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndAnimatable)
    bs << mCam << mFovKeys << mKeysOwner;
END_SAVES

BEGIN_COPYS(RndCamAnim)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndAnimatable)
    CREATE_COPY(RndCamAnim)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mCam)
        if (ty == kCopyShallow || ty == kCopyFromMax && c->mKeysOwner != c) {
            mKeysOwner = c->mKeysOwner;
        } else {
            mKeysOwner = this;
            mFovKeys = c->mKeysOwner->mFovKeys;
        }
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(2, 0)

BEGIN_LOADS(RndCamAnim)
    LOAD_REVS(bs)
    ASSERT_REVS(2, 0)
    if (d.rev > 0) {
        Hmx::Object::Load(bs);
    }
    RndAnimatable::Load(bs);
    bs >> mCam >> mFovKeys >> mKeysOwner;
    if (d.rev < 2) {
        FOREACH (it, mFovKeys) {
            it->value = ConvertFov(it->value, 0.75);
        }
    }
    if (!mKeysOwner) {
        mKeysOwner = this;
    }
END_LOADS

#pragma endregion
#pragma region RndAnimatable

void RndCamAnim::SetFrame(float frame, float blend) {
    RndAnimatable::SetFrame(frame, blend);
    if (mCam) {
        if (!FovKeys().empty()) {
            float ref = mCam->YFov();
            FovKeys().AtFrame(frame, ref);
            if (blend != 1) {
                Interp(mCam->YFov(), ref, blend, ref);
            }
            mCam->SetFrustum(mCam->NearPlane(), mCam->FarPlane(), ref, 1.0f);
        }
    }
}

float RndCamAnim::EndFrame() { return FovKeys().LastFrame(); }

void RndCamAnim::SetKey(float frame) {
    if (mCam) {
        FovKeys().Add(mCam->YFov(), frame, true);
    }
}

#pragma endregion

// See the specialization declaration + rationale comment in obj/ObjPtr_p.h
// (lane DR-2 census).  Retail's ObjRefConcrete<RndCam, ObjectDir> dtor passes
// mOwner, not `this`, as the ring-ref to Release -- a single-instruction
// `replace` at 116 B / fuzzy 97.931.  Defined here because this is the TU whose
// pinned .text range retail placed the COMDAT in, and because RndCam's complete
// type (needed by mObject->Release) arrives via rndobj/CamAnim.h -> rndobj/Cam.h.
//
// X360 only: the body reads ObjRefConcrete::mOwner, which exists only in the
// retail arm of ObjPtr_p.h -- and that header only DECLARES this specialization
// in the same arm.  Compiling it natively would be an ODR hazard, not merely a
// compile error.
#ifndef HX_NATIVE
template <>
ObjRefConcrete<RndCam, ObjectDir>::~ObjRefConcrete() {
    if (mObject)
        mObject->Release(reinterpret_cast<ObjRefOwner *>(mOwner));
}
#endif
