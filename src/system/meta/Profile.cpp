#include "meta/Profile.h"
#include "os/PlatformMgr.h"
#include "os/UserMgr.h"

Profile::Profile(int pnum) : mDirty(0), mPadNum(pnum), mState(kMetaProfileUnloaded) {}
Profile::~Profile() { mDirty = true; }

int Profile::GetPadNum() const { return mPadNum; }
void Profile::MakeDirty() { mDirty = true; }

BEGIN_HANDLERS(Profile)
    HANDLE_EXPR(get_pad_num, mPadNum)
    HANDLE_EXPR(get_name, GetName())
    HANDLE_EXPR(has_cheated, HasCheated())
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

bool Profile::IsUnsaved() const {
    bool b = HasCheated();
    if (b != false) {
        b = false;
    } else
        b = mDirty != false;
    return b;
}

void Profile::SaveLoadComplete(ProfileSaveState state) { SetSaveState(state); }

bool Profile::IsAutosaveEnabled() const { return mState == kMetaProfileLoaded; }

bool Profile::HasValidSaveData() const {
    return mState == kMetaProfileLoaded || mState == kMetaProfileError;
}

ProfileSaveState Profile::GetSaveState() const { return mState; }

// Retail's actual body (per Ghidra: a bare tail call, no vcall/UserName()
// chain, no ThePlatformMgr reference at all) is a straight forward of
// TheUserMgr->GetLocalUserFromPadNum(mPadNum) reinterpreted as const char*.
#pragma auto_inline(off)
const char *Profile::GetName() const {
    return (const char *)TheUserMgr->GetLocalUserFromPadNum(mPadNum);
}
#pragma auto_inline(on)

void Profile::SetSaveState(ProfileSaveState state) {
    MILO_ASSERT(mState != kMetaProfileUnchanged, 0x78);
    if (state != kMetaProfileUnchanged)
        mState = state;
}
