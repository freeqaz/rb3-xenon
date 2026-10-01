#include "rndobj/TransProxy.h"
#include "obj/Object.h"
#include "rndobj/Trans.h"

RndTransProxy::RndTransProxy() : mProxy(this) {}

BEGIN_HANDLERS(RndTransProxy)
    HANDLE_SUPERCLASS(RndTransformable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(RndTransProxy)
    SYNC_PROP_MODIFY(proxy, mProxy, Sync())
    SYNC_PROP_MODIFY(part, mPart, Sync())
    SYNC_SUPERCLASS(RndTransformable)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(RndTransProxy)
    SAVE_REVS(1, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndTransformable)
    bs << mProxy;
    bs << mPart;
END_SAVES

BEGIN_COPYS(RndTransProxy)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndTransformable)
    CREATE_COPY(RndTransProxy)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mProxy)
        COPY_MEMBER(mPart)
        Sync();
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(1, 0)

#ifndef HX_NATIVE
// Retail (0x8245C948) splits the revision into a file-static {altRev, rev}
// pair and reads straight off `bs` (no BinStreamRev).
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_TransProxy;
BEGIN_LOADS(RndTransProxy)
    int rev;
    bs >> rev;
    gRevs_TransProxy.rev = getHmxRev(rev);
    gRevs_TransProxy.altRev = getAltRev(rev);
    Hmx::Object::Load(bs);
    if (gRevs_TransProxy.rev > 0) {
        RndTransformable::Load(bs);
    }
    bs >> mProxy;
    bs >> mPart;
    Sync();
END_LOADS
#else
BEGIN_LOADS(RndTransProxy)
    LOAD_REVS(bs)
    ASSERT_REVS(1, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    if (d.rev > 0) {
        LOAD_SUPERCLASS(RndTransformable)
    }
    bs >> mProxy;
    bs >> mPart;
    Sync();
END_LOADS
#endif

void RndTransProxy::PreSave(BinStream &bs) { SetTransParent(nullptr, false); }
void RndTransProxy::PostSave(BinStream &bs) { Sync(); }

void RndTransProxy::SetProxy(class ObjectDir *dir) {
    if (mProxy != dir) {
        mProxy = dir;
        Sync();
    }
}

void RndTransProxy::SetPart(Symbol sym) {
    if (mPart != sym) {
        mPart = sym;
        Sync();
    }
}

void RndTransProxy::Sync() {
    SetTransParent(0, false);
    if (mProxy && mPart.Null()) {
        RndTransformable *trans = dynamic_cast<RndTransformable *>(mProxy.Ptr());
        if (trans) {
            SetTransParent(trans, false);
            return;
        }
    }
    if (mProxy) {
        RndTransformable *trans = mProxy->Find<RndTransformable>(mPart.Str(), false);
        if (trans) {
            SetTransParent(dynamic_cast<RndTransformable *>(trans), false);
            return;
        }
    }
    SetTransParent(nullptr, false);
}
