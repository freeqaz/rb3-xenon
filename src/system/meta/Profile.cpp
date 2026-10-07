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

// Retail 0x827A5018: the associated LocalUser's UserName(), a vcall through
// the User virtual base, with the user lookup inlined.  Callers that want the
// LocalUser itself call GetLocalUser() (0x827A4F38).
const char *Profile::GetName() const {
    return TheUserMgr->GetLocalUserFromPadNum(mPadNum)->UserName();
}

// Retail 0x827A4F38, a 16-byte forwarder.  MemcardMgr's SigninChangedMsg
// handler calls it and hands the result to PlatformMgr::HasUserSigninChanged.
#pragma auto_inline(off)
LocalUser *Profile::GetLocalUser() const {
    return TheUserMgr->GetLocalUserFromPadNum(mPadNum);
}
#pragma auto_inline(on)

void Profile::SetSaveState(ProfileSaveState state) {
    MILO_ASSERT(mState != kMetaProfileUnchanged, 0x78);
    if (state != kMetaProfileUnchanged)
        mState = state;
}
