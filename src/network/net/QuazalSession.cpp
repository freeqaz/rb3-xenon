#include "net/QuazalSession.h"
#include "Core/CallContext.h"
#include "net/NetSession.h"
#include "os/Debug.h"

Quazal::CallContext *QuazalSession::mTerminatingContext;

// Retail constructs QuazalSession OUT OF LINE: MakeQuazalSessionJob::IsFinished
// emits `new` -> store to the EH object-under-construction slot -> null check ->
// `lbz r4,0xc(r30)` (mHosting) -> `bl fn_823F2F08`.  0x823F2F08 sits in the
// unpinned gap between this TU's pinned .text ranges (0x823F2CF8..0x823F3038), so
// the real ctor is a genuine out-of-line function in this same TU -- the rb3-Wii
// oracle simply never decompiled its body and left `{}` behind.  An empty ctor is
// always inlined at /O1 /Ob2, which deletes the null check and the call together
// and costs IsFinished 10 instructions.  auto_inline(off) restores retail's call
// shape (per MetaPerformer.cpp / BandCharacter.cpp; __declspec(noinline) is NOT a
// substitute in this codebase -- see VocalPlayer.cpp:1576).  The body itself stays
// empty rather than invented: retail's callee is a placeholder (fn_823F2F08) and
// is therefore uncharged, so only the CALL SHAPE is observable here.
#pragma auto_inline(off)
QuazalSession::QuazalSession(bool b) {
    // Retail's first block, read verbatim off fn_823F2F08:
    //     b .L2 / .L1: bl fn_823F2B80 / .L2: lwz r11,mTerminatingContext@l(r30)
    //     cmplwi cr6,r11,0 / bne cr6,.L1
    // i.e. `while (mTerminatingContext) <pump>();`.  The pump (fn_823F2B80) is
    // unidentified, but NetSession.cpp:126 already spells the same wait idiom as
    // `while (QuazalSession::StillDeleting()) QuazalSession::Poll();`, so Poll()
    // is the corroborated reading rather than an invention.
    while (mTerminatingContext)
        Poll();
}
#pragma auto_inline(on)

MakeQuazalSessionJob::MakeQuazalSessionJob(QuazalSession **addr, bool host)
    : mSessionAddress(addr), mHosting(host) {
    MILO_ASSERT(!(*addr), 0xF3);
}

bool MakeQuazalSessionJob::IsFinished() {
    if (QuazalSession::mTerminatingContext)
        return false;
    else {
        *mSessionAddress = new QuazalSession(mHosting);
        return true;
    }
}

void MakeQuazalSessionJob::Cancel(Hmx::Object *) {
    TheNetSession->OnCreateSessionJobComplete(false);
}

void MakeQuazalSessionJob::OnCompletion(Hmx::Object *) {
    TheNetSession->OnCreateSessionJobComplete(true);
}
