#pragma once
#include "IdentityInfo.h"
#include "SkeletonRecoverer.h"
#include "gesture/LiveCameraInput.h"
#include "gesture/IdentityInfo.h"
#include "gesture/Skeleton.h"
#include "gesture/SkeletonQualityFilter.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "rndobj/Dir.h"
#include "rndobj/Tex.h"
#include "xdk/XAPILIB.h"

DECLARE_MESSAGE(KinectHardwareStatusMsg, "kinect_status_changed")
KinectHardwareStatusMsg(int i) : Message(Type(), i) {}
END_MESSAGE

DECLARE_MESSAGE(KinectUserBindingChangedMsg, "kinect_user_binding_changed")
KinectUserBindingChangedMsg(int i) : Message(Type(), i) {}
END_MESSAGE

DECLARE_MESSAGE(SkeletonEnrollmentChangedMsg, "skeleton_enrollment_changed")
SkeletonEnrollmentChangedMsg() : Message(Type()) {}
END_MESSAGE

#define NUM_SKELETONS 6

// size 0x4294
class GestureMgr : public Hmx::Object, public SkeletonCallback {
public:
    // Hmx::Object
    virtual ~GestureMgr();
    virtual DataNode Handle(DataArray *, bool);
    // SkeletonCallback
    virtual void Clear() {}
    virtual void Update(const struct SkeletonUpdateData &) {}
    virtual void PostUpdate(const struct SkeletonUpdateData *);
    virtual void Draw(const BaseSkeleton &, class SkeletonViz &) {}

    bool IsSkeletonValid(int) const;
    bool IsSkeletonSitting(int) const;
    bool IsSkeletonSideways(int) const;
    Skeleton *GetActiveSkeleton();
    IdentityInfo *GetIdentityInfo(int);
    void SetIdentificationEnabled(bool);
    int GetSkeletonIndexByTrackingID(int) const;
    Skeleton *GetSkeletonByTrackingID(int);
    Skeleton *GetSkeletonByEnrollmentIndex(int);
    Skeleton &GetSkeleton(int);
    const Skeleton &GetSkeleton(int) const;
    SkeletonQualityFilter &GetSkeletonQualityFilter(int);
    LiveCameraInput *GetLiveCameraInput() const;
    void SetTrackedSkeletons(int, int);
    void UpdateTrackedSkeletons();
    int GetActiveSkeletonIndex() const;
    int GetSecondarySkeletonIndex(bool requireValid) const;
    void SetInControllerMode(bool);
    void SetInVoiceMode(bool);
    bool InVoiceMode() const { return mInVoiceMode; }
    void SetGesturingWithVoice(bool);
    bool GesturingWithVoice() const { return mGesturingWithVoice; }
    void SetInDoubleUserMode(bool);
    bool InDoubleUserMode() const { return mInDoubleUserMode; }
    void StartTrackAllSkeletons();
    void CancelTrackAllSkeletons();
    void Poll();
    void DrawSkeletonKinectData();
    bool IsTrackingAllSkeletons() const;
    int GetPlayerSkeletonID(int);
    void SetPlayerSkeletonID(int, int);
    int GetPlayerFilteredSkeletonID(int, bool);
    bool IDEnabled() { return mIDEnabled; }
    bool GetInShellMode() { return mInShellMode; }
    void SetInShellMode(bool b) { mInShellMode = b; }
    int GetPauseOnSkeletonLossMode() { return mPauseOnSkeletonLossMode; }

    void ShowGestureGuide() {
        int id = 0;
        if (mActiveSkelTrackingID > 0) {
            id = mActiveSkelTrackingID;
        }
        XShowNuiGuideUI(id);
    }

    void SetUnk30AtPos(int pos, int val) { unk30[pos] = val; }
    int PauseOnSkeletonLossMode() const { return mPauseOnSkeletonLossMode; }

    int TogglePauseOnSkeletonLoss() {
        mPauseOnSkeletonLossMode = (mPauseOnSkeletonLossMode + 1) % 3;
        return mPauseOnSkeletonLossMode;
    }
    void AutoTilt() {
        if (mOverlapped.InternalLow != ERROR_IO_PENDING) {
            memset(&mOverlapped, 0, sizeof(XOVERLAPPED));
        }
    }
    RndTex *GetSnapshotTex(int idx) const {
        RndMat *mat = mLiveCamInput->GetSnapshot(idx);
        return mat ? mat->GetDiffuseTex() : nullptr;
    }
    bool InControllerMode() const { return mInControllerMode; }
    SkeletonRecoverer &Recoverer() { return mRecoverer; }
    int GetActiveSkeletonTrackingID() const { return mActiveSkelTrackingID; }
    void SetActiveSkeletonTrackingID(int id) { mActiveSkelTrackingID = id; }
    static float MaxRecoveryDistance() { return sMaxRecoveryDistance; }
    static float MinRecoveryTime() { return sMinRecoveryTime; }
    static float MaxRecoveryTime() { return sMaxRecoveryTime; }

    static bool sIdentityOpInProgress;
    static void Init();
    static void DebugInit();
    static void Terminate();

private:
    friend class HandRaisedGestureFilter;
    friend class StandingStillGestureFilter;
    GestureMgr();

    DataNode OnMsg(const KinectHardwareStatusMsg &);
    DataNode OnMsg(const KinectUserBindingChangedMsg &);

    static float sMaxRecoveryDistance;
    static float sMinRecoveryTime;
    static float sMaxRecoveryTime;
    static float sConfidenceLossThreshold;
    static float sConfidenceRegainThreshold;

    int unk30[NUM_SKELETONS]; // 0x30 - maybe this is SkeletonJoint?
    LiveCameraInput *mLiveCamInput; // 0x48
    Skeleton mSkeletons[NUM_SKELETONS]; // 0x4c
    IdentityInfo mIdentityInfos[NUM_SKELETONS]; // 0x4144
    SkeletonQualityFilter mFilters[NUM_SKELETONS]; // 0x41a4
    bool mTrackingAllSkeletons; // 0x424c
    SkeletonRecoverer mRecoverer; // 0x4250
    int mPauseOnSkeletonLossMode; // 0x425c - cycles 0/1/2 via TogglePauseOnSkeletonLoss
    int mActiveSkelTrackingID; // 0x4260 - active skeleton tracking ID
    int mPlayerSkeletonIDs[2]; // 0x4264
    bool mIDEnabled; // 0x426c
    bool mInControllerMode; // 0x426d
    bool mInVoiceMode; // 0x426e
    bool mGesturingWithVoice; // 0x426f
    bool mInDoubleUserMode; // 0x4270
    bool mInShellMode; // 0x4271 - true when not in gameplay panel
    RndDir *mDebugDir;
    XOVERLAPPED mOverlapped; // 0x4278
};

extern GestureMgr *TheGestureMgr;
