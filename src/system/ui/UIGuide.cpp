#include "ui/UIGuide.h"
#include "obj/Object.h"
#include "utl/BinStream.h"

UIGuide::UIGuide() : mType(kGuideVertical), mPos(0.5f) {}

UIGuide::~UIGuide() {}

BEGIN_COPYS(UIGuide)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(UIGuide)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mType)
        COPY_MEMBER(mPos)
    END_COPYING_MEMBERS
END_COPYS

void UIGuide::Save(BinStream &bs) {
    bs << 1;
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mType;
    bs << mPos;
}

// RB3 retail's UIGuide::Load uses the rb3-Wii/ObjMacros.h rev dialect (two
// mutable file-scope shorts written by hand), NOT DC3's Object.h BinStreamRev
// stack decorator.  Adjudicated on retail bytes at 0x82826670 (140 B): the body
// stores rev>>16 and rev as HALFWORDS into a global pair 4 bytes apart
// (lbl_82E07E04 / +0x4) and never constructs a stream object -- our BinStreamRev
// form emitted ??0BinStream, a ??_7BinStreamRev@@6B@ vtable store and a
// ??1BinStream destructor call that retail has none of.
//
// Spelled out inline rather than `#include "obj/ObjMacros.h"`, because that
// header also swaps the SYNC_PROP and HANDLE families, and UIGuide's
// SyncProperty/Handle are already byte-exact under the Object.h dialect.
//
// The anonymous align(4) struct is the SAME established lever as
// bandobj/OvershellDir.cpp (and per its comment BandSwatch/BandWardrobe/
// BandDirector): retail folds both rev words onto ONE base register with
// offsets 0x0/0x4, which needs a single internal-linkage symbol -- two separate
// file statics give the right offsets but MSVC orders them by first USE, and
// two DECLARE_REVS class statics make MSVC emit TWO `lis` pairs because it
// cannot relate two external symbols. Measured on the settled worktree:
// two file statics assigned (rev, alt) 94.1% / assigned (alt, rev) 97.0% /
// class statics 88.7%.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gUIGuideRevs;

BEGIN_LOADS(UIGuide)
    int rev;
    bs >> rev;
    // ObjMacros.h LOAD_REVS order (rev, then alt); the stores then schedule in
    // address order, giving retail's mr/srwi/sth+0x0/sth+0x4 sequence.
    gUIGuideRevs.rev = getHmxRev(rev);
    gUIGuideRevs.altRev = getAltRev(rev);
    // NOT LOAD_SUPERCLASS(): the Object.h dialect spells that `parent::Load(d.stream)`
    // and there is no `d` here.  Retail passes `bs` itself (r4 = bs at 0x828266C0).
    Hmx::Object::Load(bs);
    bs >> (int &)mType >> mPos;
END_LOADS

BEGIN_PROPSYNCS(UIGuide)
    SYNC_PROP(pos, mPos)
    SYNC_PROP(type, (int &)mType)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_HANDLERS(UIGuide)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS
