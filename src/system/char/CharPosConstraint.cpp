#include "char/CharPosConstraint.h"
#include "obj/Object.h"

CharPosConstraint::CharPosConstraint()
    : mSrc(this), mTargets(this), mBox(Vector3(1, 1, 1), Vector3(-1, -1, -1)) {}
CharPosConstraint::~CharPosConstraint() {}

BEGIN_HANDLERS(CharPosConstraint)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(CharPosConstraint)
    SYNC_PROP(source, mSrc)
    SYNC_PROP(targets, mTargets)
    SYNC_PROP(box, mBox)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(CharPosConstraint)
    SAVE_REVS(2, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mTargets;
    bs << mSrc;
    bs << mBox;
END_SAVES

BEGIN_COPYS(CharPosConstraint)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(CharPosConstraint)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mTargets)
        COPY_MEMBER(mSrc)
        COPY_MEMBER(mBox)
    END_COPYING_MEMBERS
END_COPYS

void CharPosConstraint::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    changedBy.push_back(mSrc);
    for (ObjPtrList<RndTransformable>::iterator it = mTargets.begin();
         it != mTargets.end();
         ++it) {
        change.push_back(*it);
        changedBy.push_back(*it);
    }
}

// RB3-360 retail rev dialect (rb3-Wii/ObjMacros shape), not DC3's Object.h
// BinStreamRev stack decorator.  DC3's form emits a ??0BinStream, a
// ??_7BinStreamRev@@6B@ vtable store and a ??1BinStream destructor that retail
// has none of, and dispatches each read on `&d` instead of the raw `bs`.
//
// Adjudicated for THIS unit on retail bytes: the target obj carries NO symbol
// mangled with AAVBinStreamRev@@, i.e. retail instantiated no rev-decorated
// operator>> here, so forwarding the raw stream deletes nothing.
//
// Written longhand rather than by including obj/ObjMacros.h: that header also
// swaps the SYNC_PROP and HANDLE families, which are already byte-exact here.
// No `#define gRev` alias -- several of these TUs are scatter-INCLUDED into
// another unit whose own gRev macro such an alias would silently shadow.
// The pair MUST share ONE internal-linkage aggregate (two file statics get two
// `lis` pairs), altRev FIRST (MSVC lays .bss out in REVERSE), and the padding
// MUST be an explicit member -- __declspec(align(4)) is unreliable here.
static struct {
    unsigned short altRev;
    unsigned short pad;
    unsigned short rev;
} gRevs_CharPosConstraint;
void CharPosConstraint::Load(BinStream &bs) {
    int rev;
    bs >> rev;
    gRevs_CharPosConstraint.rev = getHmxRev(rev);
    gRevs_CharPosConstraint.altRev = getAltRev(rev);
    Hmx::Object::Load(bs);
    bs >> mTargets;
    bs >> mSrc;
    if (gRevs_CharPosConstraint.rev > 1) {
        bs >> mBox;
    } else {
        mBox.Set(Vector3(1, 1, 0), Vector3(-1, -1, 1000));
    }
}

void CharPosConstraint::Poll() {
    if (mSrc) {
        const Transform &srcTrans = mSrc->WorldXfm();
        for (ObjPtrList<RndTransformable>::iterator it = mTargets.begin();
             it != mTargets.end();
             ++it) {
            RndTransformable *curTrans = *it;
            Transform tf48(curTrans->WorldXfm());
            if (mBox.mMin.x <= mBox.mMax.x) {
                float tmp = Clamp(mBox.mMin.x, mBox.mMax.x, tf48.v.x - srcTrans.v.x);
                tf48.v.x = tmp + srcTrans.v.x;
            }
            if (mBox.mMin.y <= mBox.mMax.y) {
                float tmp = Clamp(mBox.mMin.y, mBox.mMax.y, tf48.v.y - srcTrans.v.y);
                tf48.v.y = tmp + srcTrans.v.y;
            }
            if (mBox.mMin.z <= mBox.mMax.z) {
                float tmp = Clamp(mBox.mMin.z, mBox.mMax.z, tf48.v.z - srcTrans.v.z);
                tf48.v.z = tmp + srcTrans.v.z;
            }
            curTrans->SetWorldXfm(tf48);
        }
    }
}
