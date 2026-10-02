#pragma once
#include "obj/Object.h"
#include "os/OnlineID.h"
#include "obj/Msg.h"

class RndTex; // forward dec
struct D3DTexture;
struct _XOVERLAPPED;

class ProfilePicture {
public:
    enum State {
        kIdle,
        kFetchingUserData,
        kFetchingUserPicture,
        kComplete
    };
    ProfilePicture(int, Hmx::Object *);
    // Retail's inlined dtor is a call to Clear() (BandProfile dtor, 0x8258E2EC),
    // which also releases mUserPicture.
    ~ProfilePicture() { Clear(); }

    void FetchUserData();
    bool ReceiveUserData();
    void FetchUserPicture();
    bool ReceiveUserPicture();

    void Update();
    void Poll();
    void Succeed();
    void Fail();
    void Clear();

    State mState; // 0x0
    OnlineID mUserID; // 0x8
    _XOVERLAPPED *mOverlapped; // 0x18, gamer-picture read in flight
    D3DTexture *mTexture; // 0x1c, 64x64 target of the read
    RndTex *mUserPicture; // 0x20
    int mPadNum; // 0x24
    Hmx::Object *mCallback; // 0x28
};

DECLARE_MESSAGE(ProfilePictureFetchedMsg, "profile_picture_fetched_msg")
ProfilePictureFetchedMsg(bool success) : Message(Type(), success) {}
END_MESSAGE
