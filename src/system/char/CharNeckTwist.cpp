// Retail inlines ObjPtr<RndTransformable>'s two-arg ctor at both member-init
// sites of this ctor (mTwist, mHead); without the force MSVC emits
// `bl ??0?$ObjPtr@VRndTransformable@@@@QAA@PAVObject@Hmx@@...` twice and
// ??0CharNeckTwist@@ misaligns (69.57%).  Found by tools/inline_budget_sweep.py
// (lane W7-B); same per-TU lever as CharServoBone/CharIKHead/CharMeshHide.
// char/ is PCH-excluded, so this #define precedes the header include.
#define RB3_TU_OBJPTR_FORCEINLINE_CTOR
#include "char/CharNeckTwist.h"
#include "math/Mtx.h"
#include "math/Rot.h"
#include "math/Trig.h"
#include "math/Utl.h"
#include "obj/Object.h"
#include "rndobj/Trans.h"

CharNeckTwist::CharNeckTwist() : mTwist(this), mHead(this) {}

BEGIN_HANDLERS(CharNeckTwist)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(CharNeckTwist)
    SYNC_PROP(head, mHead)
    SYNC_PROP(twist, mTwist)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(CharNeckTwist)
    SAVE_REVS(1, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mHead;
    bs << mTwist;
END_SAVES

BEGIN_COPYS(CharNeckTwist)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(CharNeckTwist)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mHead)
        COPY_MEMBER(mTwist)
    END_COPYING_MEMBERS
END_COPYS

// Retail Load (0x823CEAE8) keeps no BinStreamRev: it splits the packed rev into
// one file-scope aggregate (altRev +0, rev +4) with two sth stores, then reads
// Object and both ObjPtrs from the raw stream -- the same shape as
// CharTransCopy::Load.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_CharNeckTwist;

BEGIN_LOADS(CharNeckTwist)
    int rev;
    bs >> rev;
    gRevs_CharNeckTwist.rev = getHmxRev(rev);
    gRevs_CharNeckTwist.altRev = getAltRev(rev);
    Hmx::Object::Load(bs);
    bs >> mHead;
    bs >> mTwist;
END_LOADS

void CharNeckTwist::Poll() {
    if (!mHead || !mTwist || !mTwist->TransParent())
        return;
    RndTransformable *parent = mTwist->TransParent();
    Transform tf(mHead->LocalXfm());
    RndTransformable *trans;
    for (trans = mHead->TransParent(); trans && trans != parent;
         trans = trans->TransParent()) {
        Multiply(tf, trans->LocalXfm(), tf);
    }
    if (trans) {
        Hmx::Quat q;
        Vector3 v;
        MakeRotQuatUnitX(tf.m.x, q);
        Multiply(tf.m.y, q, v);
        float angle = LimitAng(std::atan2(v.z, v.y)) * 0.5f;
#ifdef HX_NATIVE
        // Native-only guard; retail applies the angle unconditionally.
        if (angle != angle)
            return;
#endif
        MakeRotMatrixX(angle, mTwist->DirtyLocalXfm().m);
    }
}

void CharNeckTwist::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    changedBy.push_back(mHead);
    change.push_back(mTwist);
}
