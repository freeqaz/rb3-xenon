#include "os/ProfilePicture.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"

// The fetch itself is platform code (ProfilePicture_Xbox.cpp). Poll is retail
// 0x82B902F0; ReceiveUserData is the ICF'd `return true` there.

void ProfilePicture::Update() {
    switch (mState) {
    case kComplete: {
        OnlineID id;
        ThePlatformMgr.GetOnlineID(mPadNum, &id);
        if (id == mUserID)
            return;
        mUserID = id;
    }
    case kIdle:
        mState = kFetchingUserData;
        FetchUserData();
        break;
    default:
        break;
    }
}

void ProfilePicture::Succeed() {
    MILO_ASSERT(mState == kComplete, 0x4A);
    ProfilePictureFetchedMsg msg(true);
    mCallback->Handle(msg, false);
}

void ProfilePicture::Fail() {
    mState = kIdle;
    ProfilePictureFetchedMsg msg(false);
    mCallback->Handle(msg, false);
}

void ProfilePicture::Poll() {
    switch (mState) {
    case kFetchingUserData:
        if (ReceiveUserData()) {
            mState = kFetchingUserPicture;
            FetchUserPicture();
        }
        break;
    case kFetchingUserPicture:
        if (ReceiveUserPicture()) {
            mState = kComplete;
            Succeed();
        }
        break;
    default:
        break;
    }
}
