// Retail inlines the owner-only ObjPtr ctor at BOTH sites in this TU:
// ??0CharBonesBlender@@ mDest(this) and the boneObjPtr local in Load.
//
// Which INLINE SHAPE matters, not just inline-vs-not. Measured here (lane DI-2/D):
//   RB3_OBJPTR_INLINE_OWNER_CTOR (empty body, no AddRef arm)
//                       ctor  87.98 / Load 100.0 / funclet fn_823D9368 99.8  -> 37/39
//   RB3_TU_OBJPTR_FORCEINLINE_CTOR  (== _EH == _DEFER_OBJECT, all identical)
//                       ctor 100.0  / Load  98.06 / funclet fn_823D9368 100  -> 38/39
// The discriminator is the two-arg body's `if (mObject) AddRef(this)` arm: the
// EH region it opens is what pins the ctor's schedule (and its EH funclet). The
// member site wants that arm; the Load local does not. No single spelling
// satisfies both -- see the residual note on Load below.
// ★★ The residual noted on Load below (retail materializes the ObjPtr vtable
// one slot BEFORE the mOwner store, we emit it one slot after) is the SAME
// base-mem-init-vs-derived-body scheduling question obj/Object.h documents for
// mObject under ..._DEFER_OBJECT, just applied to mOwner. Deferring both pins
// the mOwner store after the vptr store. See obj/Object.h (lane DS-4/C).
#define RB3_TU_OBJPTR_DEFER_OWNER
#define RB3_TU_OBJPTR_FORCEINLINE_CTOR
#include "char/CharBonesBlender.h"
#include "char/CharBoneDir.h"
#include "obj/Object.h"
#include "utl/BinStream.h"

// RB3-360 retail rev storage. Retail's LOAD_REVS keeps NO BinStreamRev: it splits
// the packed rev into two mutable file-scope shorts, and ASSERT_REVS emits nothing.
// The two words must live in ONE aligned(4) aggregate (altRev +0, rev +4) -- MSVC
// does not lay .bss out in declaration order, so two separate statics get other
// globals interleaved between them and will not fold onto one base register.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_CharBonesBlender;
#define gAltRev gRevs_CharBonesBlender.altRev
#define gRev gRevs_CharBonesBlender.rev

void CharBonesBlender::Enter() { CharBones::Enter(); }

void CharBonesBlender::ReallocateInternal() {
    CharBonesAlloc::ReallocateInternal();
    if (mDest)
        mDest->AddBones(mBones);
    CharBones::Enter();
}

BEGIN_SAVES(CharBonesBlender)
    SAVE_REVS(2, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mDest;
    bs << mClipType;
END_SAVES

BEGIN_HANDLERS(CharBonesBlender)
    HANDLE_SUPERCLASS(CharPollable)
    HANDLE_SUPERCLASS(CharBonesAlloc)
END_HANDLERS

void CharBonesBlender::SetDest(CharBonesObject *obj) {
    if (obj != mDest) {
        mDest = obj;
        if (mDest)
            mDest->AddBones(mBones);
    }
}

BEGIN_COPYS(CharBonesBlender)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(CharBonesBlender)
    BEGIN_COPYING_MEMBERS
        SetClipType(c->mClipType);
        SetDest(c->mDest);
    END_COPYING_MEMBERS
END_COPYS

BEGIN_PROPSYNCS(CharBonesBlender)
    SYNC_PROP_SET(dest, mDest.Ptr(), SetDest(_val.Obj<CharBonesObject>()))
    SYNC_PROP_SET(clip_type, mClipType, SetClipType(_val.Sym()))
    SYNC_SUPERCLASS(CharBonesObject)
END_PROPSYNCS

void CharBonesBlender::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    change.push_back(mDest);
}

BEGIN_LOADS(CharBonesBlender)
    int rev;
    bs >> rev;
    gRev = getHmxRev(rev);
    gAltRev = getAltRev(rev);
    Hmx::Object::Load(bs);
    // RESIDUAL (98.06 mpn): the inlined ObjPtr ctor's three stores are all
    // present and correct; retail materializes the ObjPtr vtable (`lis`) one
    // slot BEFORE the mOwner store, we emit it one slot after, which renames
    // r10/r11 across the next four instructions. A 2-instruction scheduler
    // transposition with no source lever (the empty-body ctor spelling fixes
    // it but costs the ctor + its EH funclet -- see the header comment).
    ObjPtr<CharBonesObject> boneObjPtr(this);
    bs >> boneObjPtr;
    Symbol s;
    if (gRev > 1)
        bs >> s;
    SetClipType(s);
    SetDest(boneObjPtr);
END_LOADS

CharBonesBlender::CharBonesBlender() : mDest(this), mClipType("") {}

void CharBonesBlender::SetClipType(Symbol s) {
    if (s != mClipType) {
        mClipType = s;
        ClearBones();
        CharBoneDir::StuffBones(*this, mClipType);
    }
}

void CharBonesBlender::Poll() {
    if (mBones.empty() || !mDest)
        return;
    Blend(*mDest);
    CharBones::Enter();
}
