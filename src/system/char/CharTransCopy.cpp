#include "char/CharTransCopy.h"
#include "obj/Object.h"
#include "utl/BinStream.h"

// RB3-360 retail rev storage. Retail's LOAD_REVS keeps NO BinStreamRev: it splits
// the packed rev into two mutable file-scope shorts, and ASSERT_REVS emits nothing.
// The two words must live in ONE aligned(4) aggregate (altRev +0, rev +4) -- MSVC
// does not lay .bss out in declaration order, so two separate statics get other
// globals interleaved between them and will not fold onto one base register.
// Verified on retail fn_823C7C08: `lwz r11,0x50(r1)` / `srwi r11,r11,16` /
// `sth r11, lbl_82CBF7B4@l(r9)` / `sth r10, 0x4(r8)` -- i.e. getAltRev() to +0 and
// getHmxRev() to +4, with NO BinStream ctor/dtor pair anywhere in the body.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_CharTransCopy;
#define gAltRev gRevs_CharTransCopy.altRev
#define gRev gRevs_CharTransCopy.rev

CharTransCopy::CharTransCopy() : mSrc(this), mDest(this) {}
CharTransCopy::~CharTransCopy() {}

void CharTransCopy::Poll() {
    if (!mSrc || !mDest)
        return;
    mDest->SetLocalXfm(mSrc->LocalXfm());
}

void CharTransCopy::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    change.push_back(mDest);
    changedBy.push_back(mSrc);
}

BEGIN_HANDLERS(CharTransCopy)
    HANDLE_SUPERCLASS(RndPollable)
    HANDLE_SUPERCLASS(Hmx::Object)
    // NB: rb3-Wii spells a HANDLE_CHECK(0x4C) here.  In THIS tree END_HANDLERS
    // already emits the `if (_warn) (void)(PathName(this), sym);` tail that the
    // retail body's call to PathName (0x82757BA8) evidences, and ObjMacros.h's
    // HANDLE_CHECK belongs to the parallel rb3-Wii macro set whose INIT_REVS /
    // SYNC_PROP signatures are incompatible with Object.h's.  Do not re-add it.
END_HANDLERS

BEGIN_PROPSYNCS(CharTransCopy)
    SYNC_PROP(src, mSrc)
    SYNC_PROP(dest, mDest)
END_PROPSYNCS

// ⚠ THE ORACLE IS THE DEFECT HERE.  rb3-Wii spells this `SAVE_OBJ(CharTransCopy,
// 0x2D)`, i.e. an assert-only stub with NO body.  RB3-360 retail has a REAL save:
// fn_823C7B88 writes `li r11,1` through a 4-byte BinStream::Write (SAVE_REVS(1,0)),
// calls Hmx::Object::Save (0x8275AB90) once, then streams the two members.  Written
// from the retail bytes, per the standing rule that retail outranks both oracles.
BEGIN_SAVES(CharTransCopy)
    SAVE_REVS(1, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mSrc;
    bs << mDest;
END_SAVES

BEGIN_COPYS(CharTransCopy)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(CharTransCopy)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mSrc)
        COPY_MEMBER(mDest)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_LOADS(CharTransCopy)
    int rev;
    bs >> rev;
    gRev = getHmxRev(rev);
    gAltRev = getAltRev(rev);
    Hmx::Object::Load(bs);
    bs >> mSrc;
    bs >> mDest;
END_LOADS
