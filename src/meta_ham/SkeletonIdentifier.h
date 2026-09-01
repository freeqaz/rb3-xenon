#pragma once
#include "gesture/GestureMgr.h"
#include "obj/Data.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "os/PlatformMgr.h"
#include "utl/Str.h"

DECLARE_MESSAGE(SkeletonIdentifiedMsg, "skeleton_identified")
    SkeletonIdentifiedMsg(int, int);
    int GetVal1() const { return mData->Int(1); }
    int GetVal2() const { return mData->Int(2); }
    int GetIndex() const { return mData->Int(3); }
END_MESSAGE

enum IdentityStatus {
    kIdentityStatus_None = 0,
    kIdentityStatus_Identifying = 1,
    kIdentityStatus_Enrolling = 2,
    kIdentityStatus_Correcting = 3,
    kIdentityStatus_WaitingForSignIn = 4,
};

class SkeletonIdentifier : public Hmx::Object {
public:
    enum {
        user_max_count = 4
    };
    class EnrolledPlayer {
    public:
        EnrolledPlayer() : mPadNum(-1) {}
        bool UpdatePlayerBinding();

        int mPadNum; // 0x0 - padnum/user index
        String mName; // 0x4
        int mEnrollmentIndex; // 0x10
    };

    SkeletonIdentifier();
    // Hmx::Object
    virtual ~SkeletonIdentifier();
    virtual DataNode Handle(DataArray *, bool);

    void Init();
    void Poll();
    String GetPlayerName(int idx) const { return mEnrolledPlayers[idx].mName; }
    int GetPlayerPadNum(int idx) const { return mEnrolledPlayers[idx].mPadNum; }
    IdentityStatus GetIdentityStatus(int);
    void CorrectIdentity(int);
    bool IsAssociatedWithProfile(int) const;
    void SetUpInitialProfiles();
    void UpdateIdentityStatus();
    void UpdateEnrolledPlayers();
    void DrawDebug();
    IdentityStatus GetIDStatus() { return mIdentityStatus; }

private:
    void SetEnrolling();
    void RequestIdentity();
    void SearchForIdentity();
    void NotifyOfRecognition(int) const;

    DataNode OnMsg(const SigninChangedMsg &);
    DataNode OnMsg(const SkeletonEnrollmentChangedMsg &);
    DataNode OnMsg(const SkeletonIdentifiedMsg &);

    IdentityStatus mIdentityStatus; // 0x28
    int mTrackingSkelIndex; // 0x2c - skeleton idx
    int mIdentifyingPlayerIndex; // 0x30 - player from skeleton
    int mWaitingPlayerIndex; // 0x34
    int mCorrectingPlayerIndex; // 0x38
    int mCorrectionTrackingID; // 0x3c - skeleton tracking id
    int mIdentificationTimeout; // 0x40
    EnrolledPlayer mEnrolledPlayers[8];
    bool mDrawDebug; // 0xe4
};

String EnrollmentIndexString(int idx);

extern SkeletonIdentifier *TheSkeletonIdentifier;
