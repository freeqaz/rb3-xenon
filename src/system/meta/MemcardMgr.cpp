#include "meta/MemcardMgr.h"
#include "os/Debug.h"

MemcardMgr TheMemcardMgr;

void MemcardMgr::SetProfileSaveBuffer(void *v, int i) {
    mSaveDataBuffer = v;
    mSaveDataLength = i;
}

void MemcardMgr::SaveLoadProfileComplete(Profile *pProfile, int state) {
    MILO_ASSERT(pProfile, 0x1B);
    pProfile->SaveLoadComplete((ProfileSaveState)state);
}

// =====================================================================
// DO NOT "fix" this by moving Hmx::Object to a constant offset in MemcardMgr.
// Stuck at 82.6%.  The remaining 7 instructions are: we reach the Hmx::Object
// base through a VIRTUAL-BASE DISPLACEMENT (lwz 0x4(r29) / lwz 0x4(r11) / add /
// addi +4) where the paired target uses a constant `addi r4, r29, 0x20`.
//
// That looks like a layout defect.  IT IS NOT.  Retail's own bytes refute it:
//
//   ?Init@MemcardMgr@@QAAXXZ                     316 B, fuzzy 100.0
//   ?OnSaveGame@MemcardMgr@@QAAXPAVProfile@@...  180 B, fuzzy 100.0
//
// BOTH are MemcardMgr member functions, BOTH match retail exactly, and BOTH
// contain the SAME vbase-displacement sequence -- vbptr at 0x4, vbtable index 1,
// then +4 -- that this function emits.  (13 such sequences across
// MemcardMgr_Xbox retail asm.)  So MemcardMgr's Hmx::Object base IS virtual in
// retail and OUR LAYOUT IS ALREADY CORRECT, proven by 496 bytes of exactly
// matching retail code.  Making Object reachable at a constant this+0x20 would
// BREAK those two 100% rows to chase 120 bytes.  Net-negative and wrong.
//
// The leading layout is independently confirmed too: retail fn_827AB7E8 is
// `stw r4,0x20(r3); stw r5,0x24(r3); blr` = SetProfileSaveBuffer above, i.e.
// retail also has mSaveDataBuffer at 0x20 and mSaveDataLength at 0x24.
//
// => the real conclusion is that the MAP ROW IS A MISPAIR.  A MemcardMgr member
// function CANNOT emit a constant offset to its own virtual base, so the target
// at 0x827AB850 is not this function.  It is a same-shaped one-liner
// (`static SomeMsg msg; Hmx::Object::Handle(msg, false);`) belonging to some
// other class that inherits Hmx::Object NON-virtually at +0x20.  The row was
// added on body SHAPE (guard word / ori 1 / ctor / atexit), which is one of the
// most common shapes in the binary, not on identity.
//
// The row is left in place deliberately: it pays 0 bytes and 0 matched functions
// either way (82.6% never crosses, and matched_code is all-or-nothing per row),
// so deleting it would gain nothing while risking the loss of a real pairing.
// Anyone re-homing it should look for a class with a NON-virtual Hmx::Object
// base at +0x20.
//
// ⛔ 2026-08-10 (lane GLM-LAND-4): an automated candidate proposed exactly the
// change this block forbids --
//     ((Hmx::Object *)((char *)this + 0x20))->Hmx::Object::Handle(msg, false);
// -- and it does reach 100% on this row. REFUSED, and it must stay refused: by
// the SetProfileSaveBuffer evidence above, this+0x20 is mSaveDataBuffer, a
// void* DATA member. The cast would call a virtual-class method on a raw data
// pointer -- type confusion, not a match. It scores only because the row is
// mispaired, so the "+1 matched / +120 bytes" would be credit for another
// class's function bought with a real memory bug. If the generator re-proposes
// it, this is the answer.
// =====================================================================
void MemcardMgr::SaveLoadAllComplete() {
    static SaveLoadAllCompleteMsg msg;
    Hmx::Object::Handle(msg, false);
}

int MemcardMgr::GetSizeNeeded() { return 0; }
