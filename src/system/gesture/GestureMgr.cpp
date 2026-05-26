#include "gesture/GestureMgr.h"
#include "GestureMgr.h"
#include "SkeletonViz.h"
#include "gesture/CameraTilt.h"
#include "gesture/DrawUtl.h"
#include "IdentityInfo.h"
#include "gesture/LiveCameraInput.h"
#include "gesture/Skeleton.h"
#include "gesture/SkeletonQualityFilter.h"
#include "gesture/SkeletonUpdate.h"
#include "gesture/SpeechMgr.h"
#include "gesture/WaveToTurnOnLight.h"
#include "hamobj/HamGameData.h"
#include "hamobj/HamPlayerData.h"
#include "math/Rot.h"
#include "obj/Dir.h"
#include "obj/DirLoader.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "os/System.h"
#include "rndobj/Anim.h"
#include "rndobj/Dir.h"
#include "rndobj/Rnd.h"
#include "ui/UILabel.h"
#include "utl/Loader.h"
#include "utl/MakeString.h"
#include "xdk/NUI.h"
#include <cmath>

float GestureMgr::sMaxRecoveryDistance = 0.3;
float GestureMgr::sMinRecoveryTime = 0.7;
float GestureMgr::sMaxRecoveryTime = 1;
float GestureMgr::sConfidenceLossThreshold = 12;
float GestureMgr::sConfidenceRegainThreshold = 16;

GestureMgr *TheGestureMgr;
bool GestureMgr::sIdentityOpInProgress;
static bool sAutoPauseOnCameraDisconnect;

GestureMgr::GestureMgr()
    : mLiveCamInput(LiveCameraInput::sInstance), mPauseOnSkeletonLossMode(2), mIDEnabled(1),
#ifdef HX_NATIVE
      mInControllerMode(1), // Native: always in controller mode (no Kinect)
#else
      mInControllerMode(0),
#endif
      mInVoiceMode(0), mGesturingWithVoice(0), mInDoubleUserMode(0),
      mInShellMode(0), mDebugDir(0) {
#ifndef HX_NATIVE
    MILO_ASSERT(mLiveCamInput, 0x40);
#endif
    mPlayerSkeletonIDs[0] = -1;
    mPlayerSkeletonIDs[1] = -1;
    int skeletonIdx = 0;
    Skeleton *skeleton = mSkeletons;
    SkeletonQualityFilter *qualityFilter = mFilters;
    int *perSkeletonState = unk30 - 1;
    int *identitySkeletonIndexSlot = (int *)((char *)mIdentityInfos - 4);
    while (skeletonIdx < 6) {
        skeleton->Init();
        qualityFilter->Init(sConfidenceLossThreshold, sConfidenceRegainThreshold);
        *(identitySkeletonIndexSlot += 4) = skeletonIdx;
        ++skeletonIdx;
        *++perSkeletonState = 0;
        ++skeleton;
        ++qualityFilter;
    }
    mTrackingAllSkeletons = false;
    SkeletonUpdateHandle handle = SkeletonUpdate::InstanceHandle();
    handle.AddCallback(this);
    memset(&mOverlapped, 0, sizeof(XOVERLAPPED));
}

GestureMgr::~GestureMgr() {
    SkeletonUpdateHandle handle = SkeletonUpdate::InstanceHandle();
    handle.RemoveCallback(this);
    RELEASE(mDebugDir);
}

BEGIN_HANDLERS(GestureMgr)
    HANDLE_EXPR(pause_on_skeleton_loss, mPauseOnSkeletonLossMode)
    HANDLE_EXPR(toggle_pause_on_skeleton_loss, TogglePauseOnSkeletonLoss())
#ifdef HX_NATIVE
    // Native: no Kinect camera — return safe defaults for LiveCameraInput queries
    if (!mLiveCamInput) {
        if (sym == "get_max_snapshots" || sym == "num_snapshots"
            || sym == "num_snapshot_batches" || sym == "get_snapshot_batch_index"
            || sym == "snapshot_tex") {
            return 0;
        }
        if (sym == "init_snapshots" || sym == "clear_snapshots"
            || sym == "start_snapshot_batch" || sym == "set_autoexposure_region"
            || sym == "set_autoexposure" || sym == "dump_camera_properties"
            || sym == "draw_skeletons" || sym == "set_tracked_skeletons") {
            return 0;
        }
        if (sym == "toggle_autoexposure_tweak" || sym == "using_autoexposure_tweak"
            || sym == "toggle_autoexposure") {
            return 0;
        }
    }
#endif
    HANDLE_EXPR(get_max_snapshots, mLiveCamInput->MaxSnapshots())
    HANDLE_ACTION(init_snapshots, mLiveCamInput->InitSnapshots(_msg->Int(2)))
    HANDLE_ACTION(clear_snapshots, mLiveCamInput->ClearSnapshots())
    HANDLE_EXPR(num_snapshots, mLiveCamInput->NumSnapshots())
    HANDLE_EXPR(snapshot_tex, GetSnapshotTex(_msg->Int(2)))
    HANDLE_ACTION(start_snapshot_batch, mLiveCamInput->StartSnapshotBatch())
    HANDLE_EXPR(num_snapshot_batches, mLiveCamInput->NumSnapshotBatches())
    HANDLE_EXPR(
        get_snapshot_batch_index,
        mLiveCamInput->GetSnapshotBatchStartingIndex(_msg->Int(2))
    )
    HANDLE_EXPR(
        toggle_autoexposure_tweak,
        mLiveCamInput->SetTweakedAutoexposure(!mLiveCamInput->GetTweakedAutoexposure())
    )
    HANDLE_EXPR(using_autoexposure_tweak, mLiveCamInput->GetTweakedAutoexposure())
    HANDLE_ACTION(
        set_autoexposure_region,
        mLiveCamInput->SetExposureRegion(
            _msg->Float(2), _msg->Float(3), _msg->Float(4), _msg->Float(5)
        )
    )
    HANDLE_ACTION(set_autoexposure, mLiveCamInput->SetAutoexposure(_msg->Int(2)))
    HANDLE_EXPR(
        toggle_autoexposure,
        mLiveCamInput->SetAutoexposure(!mLiveCamInput->GetAutoexposure())
    )
    HANDLE_EXPR(is_identification_enabled, mIDEnabled)
    HANDLE_ACTION(set_identification_enabled, SetIdentificationEnabled(_msg->Int(2)))
    HANDLE_ACTION(auto_tilt, AutoTilt())
    HANDLE_EXPR(get_player_index_by_tracking_id, mPlayerSkeletonIDs[1] == _msg->Int(2))
    HANDLE_EXPR(
        get_skeleton_index_by_tracking_id, GetSkeletonIndexByTrackingID(_msg->Int(2))
    )
    HANDLE_EXPR(get_tracking_id_by_skeleton_index, GetSkeleton(_msg->Int(2)).TrackingID())
    HANDLE_EXPR(is_tracked_skeleton_index, GetSkeleton(_msg->Int(2)).IsTracked())
    HANDLE_ACTION(set_tracked_skeletons, SetTrackedSkeletons(_msg->Int(2), _msg->Int(3)))
    HANDLE_ACTION(draw_skeletons, TheSkeletonViz->SetShowing(_msg->Int(2)))
    HANDLE_ACTION(dump_camera_properties, mLiveCamInput->DumpProperties())
    HANDLE_EXPR(get_max_recovery_dist, sMaxRecoveryDistance)
    HANDLE_ACTION(set_max_recovery_dist, sMaxRecoveryDistance = _msg->Float(2))
    HANDLE_EXPR(get_max_recovery_time, sMaxRecoveryTime)
    HANDLE_ACTION(set_max_recovery_time, sMaxRecoveryTime = _msg->Float(2))
    HANDLE_EXPR(get_min_recovery_time, sMinRecoveryTime)
    HANDLE_ACTION(set_min_recovery_time, sMinRecoveryTime = _msg->Float(2))
    HANDLE_ACTION(set_in_voice_mode, SetInVoiceMode(_msg->Int(2)))
    HANDLE_ACTION(set_gesturing_with_voice, SetGesturingWithVoice(_msg->Int(2)))
    HANDLE_ACTION(show_gesture_guide, ShowGestureGuide())
    HANDLE_ACTION(show_gesture_troubleshooter, XShowNuiTroubleshooterUI())
    HANDLE_MESSAGE(KinectHardwareStatusMsg)
    HANDLE_MESSAGE(KinectUserBindingChangedMsg)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

void GestureMgr_NativeInit();
void GestureMgr_NativeTerminate();
void GestureMgr_NativePoll(GestureMgr *);

void GestureMgr::Init() {
#ifdef HX_NATIVE
    TheGestureMgr = new GestureMgr();
    TheGestureMgr->SetName("gesture_mgr", ObjectDir::Main());
    TheDebug.AddExitCallback(GestureMgr::Terminate);
    GestureMgr_NativeInit();
    return;
#endif
    LiveCameraInput::PreInit();
    TheGestureMgr = new GestureMgr();
    TheGestureMgr->SetName("gesture_mgr", ObjectDir::Main());
    InitDrawUtl(*TheGestureMgr);
    ThePlatformMgr.AddSink(TheGestureMgr, "kinect_status_changed");
    ThePlatformMgr.AddSink(TheGestureMgr, "kinect_user_binding_changed");
    TheDebug.AddExitCallback(GestureMgr::Terminate);
}

void GestureMgr::DebugInit() {
    const char *debugStr = nullptr;
    if (SystemConfig("kinect")->FindData("gesture_debug", debugStr, false) && debugStr) {
        ObjectDir *dir = DirLoader::LoadObjects(debugStr, nullptr, nullptr);
        TheGestureMgr->mDebugDir = dynamic_cast<RndDir *>(dir);
        if (!TheGestureMgr->mDebugDir && dir) {
            delete dir;
        }
    }
}

void GestureMgr::Terminate() {
#ifdef HX_NATIVE
    GestureMgr_NativeTerminate();
#else
    TerminateDrawUtl();
    ThePlatformMgr.RemoveSink(TheGestureMgr);
#endif
    RELEASE(TheGestureMgr);
}

void GestureMgr::Poll() {
#ifdef HX_NATIVE
    GestureMgr_NativePoll(this);
#else
    if (TheSpeechMgr)
        TheSpeechMgr->Poll();
    TheCameraTilt->Poll();
    TheWaveToTurnOnLight->Poll();
#endif
}

LiveCameraInput *GestureMgr::GetLiveCameraInput() const { return mLiveCamInput; }

void GestureMgr::PostUpdate(const SkeletonUpdateData *data) {
    if (!data) {
        return;
    }

    if (!UsingCD() && !data->mCameraInput->IsConnected() && !sAutoPauseOnCameraDisconnect) {
        sAutoPauseOnCameraDisconnect = true;
        mPauseOnSkeletonLossMode = 1;
    }

    for (int i = 0; (unsigned int)i < 6; i++) {
        bool updateSkeleton = true;
        unk30[i] = 0;

        if (mTrackingAllSkeletons
            && data->mSkeletonsRight[i]->TrackingID() == mSkeletons[i].TrackingID()) {
            updateSkeleton = data->mSkeletonsRight[i]->TrackingState() != kSkeletonPositionOnly;
        }
        if (updateSkeleton) {
            mSkeletons[i] = *data->mSkeletonsRight[i];
        }

        mFilters[i].Update(mSkeletons[i], mInShellMode);
        mIdentityInfos[i].PostUpdate();
    }

    if (TheLoadMgr.EditMode()) {
        int idx = -1;
        if (mActiveSkelTrackingID > 0) {
            for (int i = 0; i < 6; i++) {
                if (mSkeletons[i].TrackingID() == mActiveSkelTrackingID) {
                    idx = i;
                    break;
                }
            }
        }
        if (idx < 0) {
            for (int i = 0; i < 6; i++) {
                Skeleton &skel = GetSkeleton(i);
                if (skel.IsTracked()) {
                    mActiveSkelTrackingID = skel.TrackingID();
                    break;
                }
            }
        }
    }

    if (mTrackingAllSkeletons) {
        int leftID = mPlayerSkeletonIDs[0];
        int rightID = mPlayerSkeletonIDs[1];
        int leftIdx = GetSkeletonIndexByTrackingID(leftID);
        int rightIdx = GetSkeletonIndexByTrackingID(rightID);

        int nextLeft = leftID;
        int start = 0, end = 5;
        if (leftIdx != -1) {
            start = leftIdx + 1;
            end = leftIdx + 5;
        }
        for (int i = start; i <= end; i++) {
            int candidate = GetSkeleton(i % 6).TrackingID();
            if (candidate > 0 && candidate != rightID) {
                nextLeft = candidate;
                break;
            }
        }

        int nextRight = rightID;
        start = 0;
        end = 5;
        if (rightIdx != -1) {
            start = rightIdx + 1;
            end = rightIdx + 5;
        }
        for (int i = start; i <= end; i++) {
            int candidate = GetSkeleton(i % 6).TrackingID();
            if (candidate > 0 && candidate != nextLeft && candidate != leftID) {
                nextRight = candidate;
                break;
            }
        }

        mPlayerSkeletonIDs[0] = nextLeft;
        mPlayerSkeletonIDs[1] = nextRight;
        UpdateTrackedSkeletons();
    }

    mRecoverer.Poll();
}

IdentityInfo *GestureMgr::GetIdentityInfo(int idx) {
    if (idx >= 0 && idx < 6)
        return &mIdentityInfos[idx];
    else
        return nullptr;
}

int GestureMgr::GetSkeletonIndexByTrackingID(int id) const {
    if (id > 0) {
        for (int i = 0; i < 6; i++) {
            if (mSkeletons[i].TrackingID() == id)
                return i;
        }
    }
    return -1;
}

Skeleton *GestureMgr::GetSkeletonByTrackingID(int id) {
    if (id > 0) {
        for (int i = 0; i < 6; i++) {
            if (mSkeletons[i].TrackingID() == id
                && mSkeletons[i].TrackingState() != kSkeletonNotTracked)
                return &mSkeletons[i];
        }
    }
    return nullptr;
}

Skeleton *GestureMgr::GetSkeletonByEnrollmentIndex(int idx) {
    if (idx >= 0) {
        for (int i = 0; i < 6; i++) {
            if (mSkeletons[i].GetEnrollmentIndex() == idx) {
                return &mSkeletons[i];
            }
        }
    }
    return nullptr;
}

Skeleton *GestureMgr::GetActiveSkeleton() {
    return GetSkeletonByTrackingID(mActiveSkelTrackingID);
}

Skeleton &GestureMgr::GetSkeleton(int idx) {
    MILO_ASSERT((0) <= (idx) && (idx) < (6), 0x99);
    return mSkeletons[idx];
}

const Skeleton &GestureMgr::GetSkeleton(int idx) const {
    MILO_ASSERT((0) <= (idx) && (idx) < (6), 0x9F);
    return mSkeletons[idx];
}

SkeletonQualityFilter &GestureMgr::GetSkeletonQualityFilter(int idx) {
    MILO_ASSERT((0) <= (idx) && (idx) < (6), 0x122);
    return mFilters[idx];
}

int GestureMgr::GetActiveSkeletonIndex() const {
    int i;
    if (mActiveSkelTrackingID > 0) {
        for (i = 0; i < 6; i++) {
            if (mSkeletons[i].TrackingID() == mActiveSkelTrackingID)
                goto done;
        }
    }
    i = -1;
done:
    return i;
}

int GestureMgr::GetSecondarySkeletonIndex(bool requireValid) const {
    for (int i = 0; i < NUM_SKELETONS; i++) {
        if (i != GetActiveSkeletonIndex()) {
            if (GetSkeleton(i).IsTracked()) {
                if (!requireValid) {
                    return i;
                }
                if (GetSkeleton(i).IsValid()) {
                    return i;
                }
            }
        }
    }
    return -1;
}

void GestureMgr::SetTrackedSkeletons(int i1, int i2) {
    mPlayerSkeletonIDs[0] = i1;
    mPlayerSkeletonIDs[1] = i2;
    UpdateTrackedSkeletons();
}

void GestureMgr::SetIdentificationEnabled(bool enabled) {
    if (enabled != mIDEnabled) {
        if (!enabled) {
            NuiIdentityAbort();
        }
        mIDEnabled = enabled;
    }
}

void GestureMgr::SetInControllerMode(bool mode) {
#ifdef HX_NATIVE
    // Native: always stay in controller mode (no Kinect gesture input).
    // DTA scripts call exit_controller_mode during screen transitions,
    // but enter_controller_mode is never called back on native.
    mInControllerMode = true;
#else
    mInControllerMode = mode;
#endif
}
void GestureMgr::SetInVoiceMode(bool mode) { mInVoiceMode = mode; }
void GestureMgr::SetGesturingWithVoice(bool gesturing) {
    mGesturingWithVoice = gesturing;
}
void GestureMgr::SetInDoubleUserMode(bool mode) { mInDoubleUserMode = mode; }

void GestureMgr::StartTrackAllSkeletons() {
    mTrackingAllSkeletons = true;
    for (int i = 0; i < 6; i++) {
        mFilters[i].Init(10, 12);
        mFilters[i].SetSidewaysCutoffThreshold(0.9);
    }
}

void GestureMgr::CancelTrackAllSkeletons() {
    mTrackingAllSkeletons = false;
    for (int i = 0; i < 6; i++) {
        mFilters[i].Init(sConfidenceLossThreshold, sConfidenceRegainThreshold);
        mFilters[i].RestoreDefaultSidewaysCutoffThreshold();
    }
}

bool GestureMgr::IsTrackingAllSkeletons() const { return mTrackingAllSkeletons; }

bool GestureMgr::IsSkeletonValid(int idx) const {
    MILO_ASSERT((0) <= (idx) && (idx) < (6), 0x128);
    return mFilters[idx].Valid();
}

bool GestureMgr::IsSkeletonSitting(int idx) const {
    MILO_ASSERT((0) <= (idx) && (idx) < (6), 0x12E);
    return mFilters[idx].Sitting();
}

bool GestureMgr::IsSkeletonSideways(int idx) const {
    MILO_ASSERT((0) <= (idx) && (idx) < (6), 0x134);
    return mFilters[idx].Sideways();
}

void GestureMgr::UpdateTrackedSkeletons() {
    LiveCameraInput *cameraInput = mLiveCamInput;
    MILO_ASSERT(cameraInput, 0x14F);
    cameraInput->SetTrackedSkeletons(mPlayerSkeletonIDs[0], mPlayerSkeletonIDs[1]);
}

int GestureMgr::GetPlayerSkeletonID(int playerIndex) {
    MILO_ASSERT((0) <= (playerIndex) && (playerIndex) < (2), 0x2DA);
    return mPlayerSkeletonIDs[playerIndex];
}

void GestureMgr::SetPlayerSkeletonID(int playerIndex, int id) {
    MILO_ASSERT((0) <= (playerIndex) && (playerIndex) < (2), 0x2E0);
    mPlayerSkeletonIDs[playerIndex] = id;
    UpdateTrackedSkeletons();
}

int GestureMgr::GetPlayerFilteredSkeletonID(int playerIndex, bool b2) {
    MILO_ASSERT((0) <= (playerIndex) && (playerIndex) < (2), 0x2E8);
    int id = GetPlayerSkeletonID(playerIndex);
    if (id >= 0) {
        Skeleton *skeleton = GetSkeletonByTrackingID(id);
        if (!skeleton || (b2 && skeleton->IsSitting())) {
            id = -1;
        }
    }
    return id;
}

DataNode GestureMgr::OnMsg(const KinectHardwareStatusMsg &msg) {
    if (msg->Int(2) == 1) {
        MILO_ASSERT(mLiveCamInput, 0x21b);
        mLiveCamInput->SetAutoexposure(true);
    }
    return 1;
}

DataNode GestureMgr::OnMsg(const KinectUserBindingChangedMsg &msg) {
    static SkeletonEnrollmentChangedMsg enrollmentChangedMsg;
    Export(enrollmentChangedMsg, true);
    return 0;
}

void GestureMgr::DrawSkeletonKinectData() {
    Vector2 textPos(0.15f, 0.2f);

    auto& debugDir = mDebugDir;
    for (int i = 0; (unsigned int)i < NUM_SKELETONS; i++) {
        const Skeleton &skel = GetSkeleton(i);
        SkeletonTrackingState tracking = skel.TrackingState();

        if (debugDir == NULL) {
            const char *statusStr;
            if (tracking == kSkeletonTracked) {
                statusStr = "tracked";
            } else if (tracking == kSkeletonPositionOnly) {
                statusStr = "position only";
            } else {
                statusStr = "not tracked";
            }

            int trackingID = skel.TrackingID();
            const char *activeStr = (i == GetActiveSkeletonIndex()) ? "(active)" : "";

            Hmx::Color white(1.0f, 1.0f, 1.0f, 1.0f);
            Vector2 &result = TheRnd.DrawString(
                MakeString("%i: %s %s %d", i, statusStr, activeStr, trackingID),
                textPos, white, true
            );
            textPos.y = result.y;
        } else {
            RndDir *marker =
                debugDir->Find<RndDir>(MakeString("marker%d", i), false);
            if (marker != NULL) {
                marker->SetShowing(tracking != kSkeletonNotTracked);
                if (tracking != kSkeletonNotTracked) {
                    const Vector3 &root = skel.GetUnkab0();
                    Vector3 markerPos(root.x, -root.z, 0.0f);
                    marker->SetLocalPos(markerPos);

                    UILabel *idLabel =
                        marker->Find<UILabel>("id.lbl", false);
                    if (idLabel) {
                        idLabel->SetInt(skel.TrackingID(), false);
                    }

                    UILabel *indexLabel =
                        marker->Find<UILabel>("index.lbl", false);
                    if (indexLabel) {
                        indexLabel->SetInt(i, false);
                    }

                    RndAnimatable *activeAnim =
                        marker->Find<RndAnimatable>("active.anim", false);
                    if (activeAnim) {
                        float frame =
                            (i == GetActiveSkeletonIndex()) ? 1.0f : 0.0f;
                        activeAnim->SetFrame(frame, 1.0f);
                    }

                    RndAnimatable *trackedAnim =
                        marker->Find<RndAnimatable>("tracked.anim", false);
                    if (trackedAnim) {
                        trackedAnim->SetFrame(
                            (float)(tracking == kSkeletonTracked), 1.0f
                        );
                    }

                    RndAnimatable *colorAnim = marker->Find<RndAnimatable>(
                        "player_color.anim", false
                    );
                    if (colorAnim) {
                        int playerIdx = -1;
                        for (int j = 0; j < 2; j++) {
                            if (skel.TrackingID()
                                == TheGameData->Player(j)
                                       ->GetSkeletonTrackingID()) {
                                playerIdx = j;
                            }
                        }
                        colorAnim->SetFrame((float)playerIdx, 1.0f);
                    }

                    float rotation = 0.0f;
                    if (tracking == kSkeletonTracked) {
                        Vector3 leftShoulder, rightShoulder;
                        skel.JointPos(
                            kCoordCamera, kJointShoulderLeft, leftShoulder
                        );
                        skel.JointPos(
                            kCoordCamera, kJointShoulderRight, rightShoulder
                        );
                        Vector3 diff;
                        Subtract(rightShoulder, leftShoulder, diff);
                        rotation = (float)std::atan(diff.z / diff.x) * RAD2DEG;
                    }

                    RndAnimatable *rotAnim = marker->Find<RndAnimatable>(
                        "rotation.anim", false
                    );
                    if (rotAnim) {
                        rotAnim->SetFrame(rotation, 1.0f);
                    }
                }
            }
        }
    }

    if (debugDir) {
        debugDir->DrawShowing();
    }
}
