#include "ui/UIColor.h"
#include "obj/Object.h"
#include "utl/BinStream.h"

const Hmx::Color &UIColor::GetColor() const { return mColor; }
void UIColor::SetColor(const Hmx::Color &color) { mColor = color; }
UIColor::UIColor() : mColor(1, 1, 1, 1) {}

void UIColor::Save(BinStream &bs) {
    bs << 0;
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mColor;
}

BEGIN_COPYS(UIColor)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(UIColor)
    MILO_ASSERT(c, 0x34);
    COPY_MEMBER(mColor)
END_COPYS

BEGIN_HANDLERS(UIColor)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(UIColor)
    SYNC_PROP(color, mColor)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

// ---------------------------------------------------------------------------
// LOCAL REVS DIALECT (lane W16-BO). Two competing LOAD_REVS definitions reach
// this TU and the WRONG one wins: obj/Object.h:1805 expands LOAD_REVS(bs) to
// `int revs; bs >> revs; BinStreamRev d(bs, revs);`, which built a dead
// BinStreamRev and left this body 128 B with 1 of 30 words equal to retail.
// Retail fn_82802240 (120 B) instead splits the packed int into two file-scope
// shorts, exactly obj/ObjMacros.h:647's dialect:
//   li r5,4 / addi r4,r1,0x50 / bl ?ReadEndian@BinStream@@QAAXPAXH@Z  -> bs >> rev
//   lwz r11,0x50(r1) / mr r10,r11 / srwi r11,r11,16
//   sth r11, lbl_82E077F4+0    -> the SHIFTED half at base+0
//   sth r10, lbl_82E077F4+4    -> the TRUNCATED half at base+4
//   bl ?Load@Object@Hmx@@UAAXAAVBinStream@@@Z            -> Hmx::Object::Load(bs)
//   addi r4,r31,0x28 / bl ??5@YAAAVBinStream@@AAV0@AAVColor@Hmx@@@Z -> bs >> mColor
// utl/BinStream.h:199-200 settles WHICH short is which: getHmxRev(packed) is the
// truncation and getAltRev(packed) is `(unsigned)packed >> 0x10` (note retail's
// LOGICAL srwi, not srawi). So base+0 is gAltRev and base+4 is gRev, and since
// declaration order is what fixes .bss placement, gAltRev must be declared FIRST
// -- identical to the arrangement proven on retail bytes in ui/UILabel.cpp:52-89,
// which is worth 1,132 B there. ASSERT_REVS expands to nothing because retail has
// no version guard: the asm goes straight from the rev split into
// Hmx::Object::Load with no MILO_FAIL arm.
// Bracketed with push_macro/pop_macro so the dialect cannot leak if this file is
// ever whole-file #included by a COMDAT-scatter owner (it is not today -- checked).
#pragma push_macro("INIT_REVS")
#pragma push_macro("LOAD_REVS")
#pragma push_macro("ASSERT_REVS")
#undef INIT_REVS
#undef LOAD_REVS
#undef ASSERT_REVS
// Two-arg form, so the INIT_REVS(0, 0) call site below is unchanged.
#define INIT_REVS(rev, alt)                                                              \
    static unsigned short gAltRev = alt;                                                 \
    static unsigned short gRev = rev;
#define LOAD_REVS(bs)                                                                    \
    int rev;                                                                             \
    bs >> rev;                                                                           \
    gRev = getHmxRev(rev);                                                                \
    gAltRev = getAltRev(rev);
#define ASSERT_REVS(rev1, rev2)

INIT_REVS(0, 0)

BEGIN_LOADS(UIColor)
    LOAD_REVS(bs);
    ASSERT_REVS(0, 0);
    Hmx::Object::Load(bs);
    bs >> mColor;
END_LOADS

#pragma pop_macro("ASSERT_REVS")
#pragma pop_macro("LOAD_REVS")
#pragma pop_macro("INIT_REVS")
