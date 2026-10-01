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

// Retail 0x827AB850 calls MsgSource's own Handle override directly: the
// qualified `MsgSource::Handle(msg, false)`. For that call MSVC passes a
// constant `addi r4,r29,0x20` with no vbptr lookup, byte-identical to retail.
// Earlier notes here read that constant as "a MemcardMgr member cannot reach
// its virtual Object base at a fixed offset" and took the map row for a
// mispair; the qualified call is what produces it. (It is NOT a cast of
// this+0x20, which is mSaveDataBuffer.)
void MemcardMgr::SaveLoadAllComplete() {
    static SaveLoadAllCompleteMsg msg;
    MsgSource::Handle(msg, false);
}

int MemcardMgr::GetSizeNeeded() { return 0; }
