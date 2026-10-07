// Retail inlines the ObjPtr two-arg ctor at this TU's member-init sites; the
// in-class (plain inline) definition lets MSVC choose per site, as retail did.
#define RB3_OBJPTR_INLINE_TWOARG_CTOR
#include "char/CharBlendBone.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "math/Rot.h"
#include "math/Utl.h"
#include <cstring>

#pragma region CharBlendBone

CharBlendBone::CharBlendBone()
    : mTargets(this), mSrc1(this), mSrc2(this), mTransX(false), mTransY(false),
      mTransZ(false), mRotation(false) {}

BEGIN_PROPSYNCS(CharBlendBone)
    // RETAIL NAME IS PLURAL.  Arbitrated on RETAIL BYTES (lane CQ-3): the first
    // property-name literal in the 696 B retail body is "targets", not "target"
    // (our member is already mTargets).  Renamed unconditionally -- this is a
    // correctness fix, not a match-only one: RB3 .milo data keys off "targets".
    SYNC_PROP(targets, mTargets)
    SYNC_PROP(src_one, mSrc1)
    SYNC_PROP(src_two, mSrc2)
    SYNC_PROP(trans_x, mTransX)
    SYNC_PROP(trans_y, mTransY)
    SYNC_PROP(trans_z, mTransZ)
    SYNC_PROP(rotation, mRotation)
    // DC3-era addition; retail's chain ends at `rotation` (7 literals).  The
    // MEMBER is gone too (see CharBlendBone.h), so this cannot be HX_NATIVE-
    // parked any more -- it is removed outright.
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(CharBlendBone)
    SAVE_REVS(3, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mTargets;
    bs << mSrc1;
    bs << mSrc2;
    bs << mTransX;
    bs << mTransY;
    bs << mTransZ;
    bs << mRotation;
END_SAVES

BEGIN_COPYS(CharBlendBone)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(CharBlendBone)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mTargets)
        COPY_MEMBER(mSrc1)
        COPY_MEMBER(mSrc2)
        COPY_MEMBER(mTransX)
        COPY_MEMBER(mTransY)
        COPY_MEMBER(mTransZ)
        COPY_MEMBER(mRotation)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(3, 0)

// Retail Load keeps no BinStreamRev: it splits the packed rev into one aligned
// file-scope aggregate (altRev +0, rev +4) and reads everything from the raw
// stream.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_CharBlendBone;

BEGIN_LOADS(CharBlendBone)
    int rev;
    bs >> rev;
    gRevs_CharBlendBone.rev = getHmxRev(rev);
    gRevs_CharBlendBone.altRev = getAltRev(rev);
    MILO_ASSERT(gRevs_CharBlendBone.rev > 2, 0x66);
    Hmx::Object::Load(bs);
    bs >> mTargets;
    bs >> mSrc1;
    bs >> mSrc2;
    bs >> mTransX;
    bs >> mTransY;
    bs >> mTransZ;
    bs >> mRotation;
END_LOADS

void CharBlendBone::Poll() {
    for (ObjList<ConstraintSystem>::iterator it = mTargets.begin(); it != mTargets.end();
         ++it) {
        RndTransformable *target = it->mTarget;
        if (target && mSrc1 && mSrc2) {
            const Transform &xfm1 = mSrc1->WorldXfm();
            const Transform &xfm2 = mSrc2->WorldXfm();
            Transform tf48(target->WorldXfm());
            if (mTransX || mTransY || mTransZ) {
                if (mTransX) {
                    Interp(xfm1.v.x, xfm2.v.x, it->mWeight, tf48.v.x);
                }
                if (mTransY) {
                    Interp(xfm1.v.y, xfm2.v.y, it->mWeight, tf48.v.y);
                }
                if (mTransZ) {
                    Interp(xfm1.v.z, xfm2.v.z, it->mWeight, tf48.v.z);
                }
            }
            if (mRotation) {
                Interp(xfm1.m, xfm2.m, it->mWeight, tf48.m);
            }
            target->SetWorldXfm(tf48);
        }
    }
}

void CharBlendBone::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    changedBy.push_back(mSrc1);
    changedBy.push_back(mSrc2);
    for (ObjList<ConstraintSystem>::iterator it = mTargets.begin();
         it != mTargets.end();
         ++it) {
        change.push_back((*it).mTarget);
    }
}

BEGIN_HANDLERS(CharBlendBone)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

#pragma endregion CharBlendBone
#pragma region CharBlendBone::ConstraintSystem

CharBlendBone::ConstraintSystem::ConstraintSystem(Hmx::Object *o)
    : mTarget(o), mWeight(0.5f) {}

// Retail places this writer at 0x823C3300, at the head of this TU's region, and
// DC3's map attributes the same spelling to CharBlendBone.obj. (dc3-decomp
// defines it in CharIKHand.cpp, which is where this tree inherited it from.)
BinStream &operator<<(BinStream &bs, const CharBlendBone::ConstraintSystem &cs) {
    bs << cs.mTarget;
    bs << cs.mWeight;
    return bs;
}

BinStream &operator>>(BinStream &bs, CharBlendBone::ConstraintSystem &cs) {
    bs >> cs.mTarget;
    bs >> cs.mWeight;
    return bs;
}

BEGIN_CUSTOM_PROPSYNC(CharBlendBone::ConstraintSystem)
    SYNC_PROP(target, o.mTarget)
    SYNC_PROP(weight, o.mWeight)
END_CUSTOM_PROPSYNC

#pragma endregion CharBlendBone::ConstraintSystem

// sw2 scatter-include (default/CharBlendBone <- obj/PropSync.cpp)
// ⚠ NATIVE: guarded because THIS INCLUDEE HAS MULTIPLE UNCONDITIONAL
// INCLUDERS in the native fork surface, which cmake/ScatterIncludes.cmake
// cannot resolve by pruning: its rule drops an includee that is emitted by an
// includer in the same target, and with N>1 includers that still leaves N
// copies. Guarding EVERY includer makes the edges inert natively, so
// obj/PropSync.cpp is compiled standalone exactly once -- which is the shape
// every native target had before X2 widened the glob. X360 is untouched: the
// scatter-include is a COMDAT-placement device for the match build, and it
// stays fully active there.
#ifndef HX_NATIVE
#define gRev gRev_PropSync
#define gAltRev gAltRev_PropSync
#include "obj/PropSync.cpp"
#undef gRev
#undef gAltRev
#endif
