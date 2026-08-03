#include "char/CharBoneOffset.h"
#include "char/CharPollable.h"
#include "hamobj/PhotoSpotlightPositioner.h"
#include "obj/Object.h"
// ObjPtr<RndEnviron> (via rndobj/Group.h) now has a vtable that force-
// instantiates ObjRefConcrete<RndEnviron>::SetObj/Load — needs RndEnviron
// complete in this TU.
#include "rndobj/Env.h"

CharBoneOffset::CharBoneOffset() : mDest(this), mOffset(0, 0, 0) {}

BEGIN_HANDLERS(PhotoSpotlightPositioner)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_HANDLERS(CharBoneOffset)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(CharBoneOffset)
    SYNC_PROP(dest, mDest)
    SYNC_PROP(offset, mOffset)
END_PROPSYNCS

BEGIN_SAVES(CharBoneOffset)
    SAVE_REVS(1, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mDest;
    bs << mOffset;
END_SAVES

BEGIN_COPYS(CharBoneOffset)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(CharBoneOffset)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mDest)
        COPY_MEMBER(mOffset)
    END_COPYING_MEMBERS
END_COPYS

// RB3-360 retail rev dialect (rb3-Wii/ObjMacros shape): the packed rev is split
// into two HALFWORDS stored four bytes apart onto ONE internal-linkage align(4)
// base, and the RAW incoming BinStream is forwarded to every read and to the
// superclass Load.  DC3's Object.h BinStreamRev stack decorator additionally
// emits ??0BinStream, a ??_7BinStreamRev@@6B@ vtable store and a ??1BinStream
// destructor that retail has none of, and dispatches each read on `&d`.
//
// Written longhand rather than by including obj/ObjMacros.h: that header also
// swaps the SYNC_PROP and HANDLE families, which are already byte-exact here.
// The pair MUST share one aggregate -- two separate file statics are laid out
// independently and will not fold onto a single base register.  No `#define
// gRev` alias: several of these TUs are scatter-INCLUDED into another unit
// (e.g. rndobj/Anim.cpp includes rndobj/MotionBlur.cpp) whose own gRev macro
// the alias would silently shadow for the rest of the amalgamated TU.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_CharBoneOffset;
BEGIN_LOADS(CharBoneOffset)
    int rev;
    bs >> rev;
    gRevs_CharBoneOffset.rev = getHmxRev(rev);
    gRevs_CharBoneOffset.altRev = getAltRev(rev);
    Hmx::Object::Load(bs);
    bs >> mDest;
    bs >> mOffset;
END_LOADS

void CharBoneOffset::Poll() {
    if (!mDest || !mDest->TransParent())
        return;
    Transform tf(mDest->LocalXfm());
    tf.v += mOffset;
    Transform tRes;
    Multiply(tf, mDest->TransParent()->WorldXfm(), tRes);
    mDest->SetWorldXfm(tRes);
}

void CharBoneOffset::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    change.push_back(mDest);
    if (mDest && mDest->TransParent())
        changedBy.push_back(mDest->TransParent());
}
