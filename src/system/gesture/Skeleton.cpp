#include "gesture/Skeleton.h"
#include "ArchiveSkeleton.h"
#include "IdentityInfo.h"
#include "gesture/GestureMgr.h"
#include "gesture/BaseSkeleton.h"
#include "gesture/SkeletonHistory.h"
#include "gesture/JointUtl.h"
#include "math/DoubleExponentialSmoother.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "os/System.h"
#include "xdk/NUI.h"
#include "xdk/XAPILIB.h"
#include <cmath>

Vector3DESmoother SkeletonFrame::sUpVectorSmoother;

#pragma region SkeletonFrame

float SkeletonFrame::TiltAngle() const { return (PI / 2) - (float)atan2(mFloorNormal.y, mFloorNormal.z); }

void SkeletonFrame::Init() {
    static Symbol kinect("kinect");
    static Symbol up_vector_smoothing("up_vector_smoothing");
    static Symbol smoothing("smoothing");
    static Symbol trend("trend");
    DataArray *cfg = SystemConfig(kinect, up_vector_smoothing);
    sUpVectorSmoother.SetSmoothParameters(
        cfg->FindFloat(smoothing), cfg->FindFloat(trend)
    );
    sUpVectorSmoother.ForceValue(Vector3(0, 1, 0));
}

void SkeletonFrame::Create(const NUI_SKELETON_FRAME &nui_frame, int elapsed) {
    mFrameNumber = nui_frame.dwFrameNumber;
    mElapsedMs = elapsed;

    sUpVectorSmoother.Smooth(
        Vector3(nui_frame.vNormalToGravity.x, nui_frame.vNormalToGravity.y, nui_frame.vNormalToGravity.z),
        elapsed * 0.001f,
        false
    );

    mFloorNormal = sUpVectorSmoother.Value();
    mFloorClipPlane.Set(
        nui_frame.vFloorClipPlane.x,
        nui_frame.vFloorClipPlane.y,
        nui_frame.vFloorClipPlane.z,
        nui_frame.vFloorClipPlane.w
    );

    // Pass smoothed floor normal (with w=0) to NuiTransformMatrixLevel
    XMVECTOR gravVec;
    gravVec.x = mFloorNormal.x;
    gravVec.y = mFloorNormal.y;
    gravVec.z = mFloorNormal.z;
    gravVec.w = 0.0f;
    XMMATRIX mat = NuiTransformMatrixLevel(gravVec);

    static const SkeletonTrackingState sTrackingMap[] = {
        kSkeletonNotTracked, kSkeletonPositionOnly, kSkeletonTracked
    };
    static const int sJointTrackingMap[] = { 0, 1, 2 };
    static const int sJointRemap[kNumJoints][2] = {
        {0, 0}, {1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}, {6, 6}, {7, 7},
        {8, 8}, {9, 9}, {10, 10}, {11, 11}, {12, 12}, {13, 13}, {14, 14},
        {18, 15}, {15, 16}, {16, 17}, {17, 18}, {19, 19}
    };

    // First pass: transform joint positions by gravity matrix
    XMVECTOR transformed[6 * kNumJoints];
    for (int s = 0; (unsigned int)s < 6; s++) {
        if (nui_frame.SkeletonData[s].eTrackingState == NUI_SKELETON_TRACKED) {
            for (int j = 0; j < kNumJoints; j++) {
                XMVECTOR pos = nui_frame.SkeletonData[s].SkeletonPositions[j];
                XMVECTOR sZ = __vspltw(pos, 2);
                XMVECTOR sY = __vspltw(pos, 1);
                XMVECTOR sX = __vspltw(pos, 0);
                XMVECTOR result = __vmaddfp(sZ, mat.r[2], mat.r[3]);
                result = __vmaddfp(sY, mat.r[1], result);
                result = __vmaddfp(sX, mat.r[0], result);
                transformed[s * kNumJoints + j] = result;
            }
        }
    }

    // Second pass: copy NUI data to SkeletonData
    for (int s = 0; s < (unsigned long)6; s++) {
        const NUI_SKELETON_DATA &nuiSkel = nui_frame.SkeletonData[s];
        SkeletonData &data = mSkeletonDatas[s];
        data.mTracking = sTrackingMap[nuiSkel.eTrackingState];
        if (data.mTracking == kSkeletonTracked) {
            data.mQualityFlags = nuiSkel.dwQualityFlags;
            for (int j = 0; j < kNumJoints; j++) {
                int dst = sJointRemap[j][0];
                int src = sJointRemap[j][1];
                data.mRawPositions[dst].z = nuiSkel.SkeletonPositions[src].z;
                data.mRawPositions[dst].y = nuiSkel.SkeletonPositions[src].y;
                data.mRawPositions[dst].x = nuiSkel.SkeletonPositions[src].x;
                data.mJointPositions[dst].y = transformed[s * kNumJoints + src].y;
                data.mJointPositions[dst].x = transformed[s * kNumJoints + src].x;
                data.mJointPositions[dst].z = transformed[s * kNumJoints + src].z;
                data.mJointTrackingState[dst] = sJointTrackingMap[nuiSkel.eSkeletonPositionTrackingState[src]];
            }
        }
        data.mTrackingID = nuiSkel.dwTrackingID;
        data.mClippedFlags = nuiSkel.dwEnrollmentIndex;

        // Transform hip center by gravity matrix
        XMVECTOR hipPos = nuiSkel.Position;
        XMVECTOR hZ = __vspltw(hipPos, 2);
        XMVECTOR hY = __vspltw(hipPos, 1);
        XMVECTOR hX = __vspltw(hipPos, 0);
        XMVECTOR hipResult = __vmaddfp(hZ, mat.r[2], mat.r[3]);
        hipResult = __vmaddfp(hY, mat.r[1], hipResult);
        hipResult = __vmaddfp(hX, mat.r[0], hipResult);
        data.mHipCenter.z = hipResult.z;
        data.mHipCenter.y = hipResult.y;
        data.mHipCenter.x = hipResult.x;
    }
}

#pragma endregion
#pragma region Skeleton

Skeleton::Skeleton() : mTracking(kSkeletonNotTracked), mTrackingID(-1), unkac4(0) {
    Init();
}

void Skeleton::JointPos(SkeletonCoordSys cs, SkeletonJoint joint, Vector3 &pos) const {
    MILO_ASSERT((0) <= (cs) && (cs) < (kNumCoordSys), 0xDA);
    MILO_ASSERT((0) <= (joint) && (joint) < (kNumJoints), 0xDB);
    pos = mTrackedJoints[joint].mJointPos[cs];
}

bool Skeleton::Displacement(
    const SkeletonHistory *history,
    SkeletonCoordSys cs,
    SkeletonJoint joint,
    int i4,
    Vector3 &disp,
    int &iref
) const {
    ArchiveSkeleton archiveSkeleton;
    if (PrevTrackedSkeleton(history, i4, iref, archiveSkeleton)) {
        Vector3 v3;
        archiveSkeleton.JointPos(cs, joint, v3);
        Subtract(mTrackedJoints[joint].mJointPos[cs], v3, disp);
        return true;
    } else {
        disp.Zero();
        return false;
    }
}

bool Skeleton::Displacements(
    const SkeletonHistory *history,
    SkeletonCoordSys cs,
    int i4,
    Vector3 *disps,
    int &iref
) const {
    FOREACH (it, mCamDisplacements) {
        if (it->unk0 == i4) {
            memcpy(disps, it->unk8, sizeof(it->unk8));
            iref = it->unk4;
            return 0 < (unsigned int)(iref + 1);
        }
    }

    CameraDisplacement camDisp;
    camDisp.unk0 = i4;
    ArchiveSkeleton archiveSkeleton;
    bool ok = PrevTrackedSkeleton(history, i4, iref, archiveSkeleton);
    if (ok) {
        for (int i = 0; kNumJoints > i; i++) {
            Vector3 prevPos;
            archiveSkeleton.JointPos(cs, (SkeletonJoint)i, prevPos);
            Subtract(mTrackedJoints[i].mJointPos[cs], prevPos, disps[i]);
        }
    } else {
        memset(disps, 0, sizeof(camDisp.unk8));
    }
    camDisp.unk4 = iref;
    memcpy(camDisp.unk8, disps, sizeof(camDisp.unk8));
    mCamDisplacements.push_back(camDisp);
    return ok;
}

JointConfidence Skeleton::JointConf(SkeletonJoint joint) const {
    MILO_ASSERT((0) <= (joint) && (joint) < (kNumJoints), 0xE1);
    return mTrackedJoints[joint].mJointConf;
}

bool Skeleton::IsTracked() const { return mTracking == kSkeletonTracked; }
int Skeleton::QualityFlags() const { return mQualityFlags; }
int Skeleton::ElapsedMs() const { return mElapsedMs; }

void Skeleton::CameraToPlayerXfm(SkeletonCoordSys cs, Transform &playerXfm) const {
    MILO_ASSERT((kCoordLeftArm) <= (cs) && (cs) < (kNumCoordSys), 0x127);
    playerXfm = mPlayerXfms[cs - 1];
}

void Skeleton::CamJointPositions(Vector3 *positions) const {
    for (int i = 0; i < kNumJoints; i++) {
        *positions++ = mTrackedJoints[i].mJointPos[kCoordCamera];
    }
}

void Skeleton::CamBoneLengths(float *lens) const {
    memcpy(lens, mCamBoneLengths, sizeof(mCamBoneLengths));
}

float Skeleton::BoneLength(SkeletonBone bone, SkeletonCoordSys cs) const {
    if (cs == kCoordCamera) {
        MILO_ASSERT((0) <= (bone) && (bone) < (kNumBones), 0x12F);
        return mCamBoneLengths[bone];
    } else
        return BaseSkeleton::BoneLength(bone, cs);
}

bool Skeleton::IsValid() const {
    if (mSkeletonIdx >= 0) {
        return TheGestureMgr->IsSkeletonValid(mSkeletonIdx);
    } else
        return false;
}

bool Skeleton::IsSitting() const {
    if (mSkeletonIdx >= 0) {
        return TheGestureMgr->IsSkeletonSitting(mSkeletonIdx);
    } else
        return false;
}

bool Skeleton::IsSideways() const {
    if (mSkeletonIdx >= 0) {
        return TheGestureMgr->IsSkeletonSideways(mSkeletonIdx);
    } else
        return false;
}

const TrackedJoint &Skeleton::HandJoint(SkeletonSide side) const {
    return mTrackedJoints[side == kSkeletonLeft ? kJointHandLeft : kJointHandRight];
}

const TrackedJoint &Skeleton::ElbowJoint(SkeletonSide side) const {
    return mTrackedJoints[side == kSkeletonLeft ? kJointElbowLeft : kJointElbowRight];
}

const TrackedJoint &Skeleton::ShoulderJoint(SkeletonSide side) const {
    return mTrackedJoints[side == kSkeletonLeft ? kJointShoulderLeft : kJointShoulderRight];
}

const TrackedJoint &Skeleton::HipJoint(SkeletonSide side) const {
    return mTrackedJoints[side == kSkeletonLeft ? kJointHipLeft : kJointHipRight];
}

const TrackedJoint &Skeleton::KneeJoint(SkeletonSide side) const {
    return mTrackedJoints[side == kSkeletonLeft ? kJointKneeLeft : kJointKneeRight];
}

void Skeleton::ScreenPos(SkeletonJoint joint, Vector2 &pos) const {
    if (mTracking == kSkeletonTracked) {
        JointScreenPos(mTrackedJoints[joint], pos);
    } else
        pos.Zero();
}

bool Skeleton::PrevTrackedSkeleton(
    const SkeletonHistory *history, int i2, int &iref, ArchiveSkeleton &archiveSkeleton
) const {
    MILO_ASSERT(history, 0x169);
    if (mTracking == kSkeletonTracked
        && history->PrevSkeleton(*this, i2, archiveSkeleton, iref)) {
        return archiveSkeleton.IsTracked();
    } else
        return false;
}

bool Skeleton::Velocity(
    const SkeletonHistory &history,
    SkeletonCoordSys cs,
    SkeletonJoint joint,
    int i4,
    Vector3 &velocity,
    int &iref
) const {
    if (Displacement(&history, cs, joint, i4, velocity, iref)) {
        float scale = 1.0f / (iref * 0.001f);
        velocity.x = velocity.x * scale;
        velocity.y = velocity.y * scale;
        velocity.z = velocity.z * scale;
        return true;
    } else {
        velocity.Zero();
        return false;
    }
}


void Skeleton::Init() {
    mTracking = kSkeletonNotTracked;
    mSkeletonIdx = -1;
    mQualityFlags = 0;
    unkab0.Zero();
    for (int i = 0; i < 5; i++) {
        mPlayerXfms[i].Reset();
    }
    for (int i = 0; i < kNumJoints; i++) {
        for (int j = 0; j < kNumCoordSys; j++) {
            mTrackedJoints[i].mJointPos[j].Zero();
        }
        mTrackedJoints[i].mJointConf = kConfidenceNotTracked;
        mTrackedJoints[i].mSmoothedPos.Zero();
    }
    memset(mCamBoneLengths, 0, sizeof(mCamBoneLengths));
    mCamDisplacements.clear();
}

bool Skeleton::ProfileMatched() const {
    IdentityInfo *info = TheGestureMgr->GetIdentityInfo(mSkeletonIdx);
    return info ? info->ProfileMatched() : false;
}

int Skeleton::GetEnrollmentIndex() const {
    IdentityInfo *info = TheGestureMgr->GetIdentityInfo(mSkeletonIdx);
    return info ? info->EnrollmentIndex() : -1;
}

bool Skeleton::NeedIdentify() const {
    return GetEnrollmentIndex() == -1 || GetEnrollmentIndex() == -5;
}

void Skeleton::Poll(int skel_idx, const SkeletonFrame &frame) {
    MILO_ASSERT((0) <= (skel_idx) && (skel_idx) < (6), 0x1F8);
    if (mSkeletonIdx != skel_idx && TheGestureMgr) {
        IdentityInfo *identityInfo = TheGestureMgr->GetIdentityInfo(skel_idx);
        MILO_ASSERT(identityInfo, 0x1FC);
        identityInfo->Reset(skel_idx);
    }

    mSkeletonIdx = skel_idx;
    mElapsedMs = frame.mElapsedMs;
    const SkeletonData &data = frame.mSkeletonDatas[skel_idx];
    mTrackingID = data.mTrackingID;
    unkab0 = data.mHipCenter;
    mTracking = data.mTracking;
    if (mTracking != kSkeletonNotTracked) {
        if (mTracking == kSkeletonTracked) {
        mQualityFlags = data.mQualityFlags;
        if (TheGestureMgr) {
            IdentityInfo *identityInfo = TheGestureMgr->GetIdentityInfo(skel_idx);
            MILO_ASSERT(identityInfo, 0x211);
            if (identityInfo->EnrollmentIndex() != data.mClippedFlags) {
                identityInfo->SetEnrollmentIndex(data.mClippedFlags);
            }
        }

        {
            const Vector3 &floorNormal = frame.mFloorNormal;
            for (int i = 1; i < kNumCoordSys; i++) {
                BaseSkeleton::MakeCameraToPlayerXfm(
                    (SkeletonCoordSys)i,
                    mPlayerXfms[i - 1],
                    (const Vector3 *)data.mJointPositions,
                    floorNormal
                );
            }
        }

        for (int i = 0; i < kNumJoints; i++) {
            auto& _sub0 = mTrackedJoints[i];
            for (int j = 0; j < kNumCoordSys; j++) {
                if (j == 0) {
                    _sub0.mJointPos[0] = data.mJointPositions[i];
                } else {
                    MultiplyTranspose(
                        data.mJointPositions[i],
                        mPlayerXfms[j - 1],
                        _sub0.mJointPos[j]
                    );
                }
            }
            _sub0.mJointConf = (JointConfidence)data.mJointTrackingState[i];
            _sub0.mSmoothedPos = data.mRawPositions[i];
        }

        for (int i = 0; i < kNumBones; i++) {
            mCamBoneLengths[i] = BaseSkeleton::BoneLength((SkeletonBone)i, kCoordCamera);
        }

        mCamDisplacements.clear();

        Vector4 clipPlane = frame.mFloorClipPlane;
        unkac4 = (mTrackedJoints[kJointHipRight].mJointPos[kCoordCamera].y
                  + mTrackedJoints[kJointHipLeft].mJointPos[kCoordCamera].y)
            * 0.5f + clipPlane.w;
        }
    } else {
        Init();
    }
}

void Skeleton::PostUpdate() {}

bool Skeleton::RequestIdentity() {
    MILO_ASSERT(!GestureMgr::sIdentityOpInProgress, 0x2A9);
    IdentityInfo *info = TheGestureMgr->GetIdentityInfo(mSkeletonIdx);
    if (info) {
        HRESULT hr = NuiIdentityIdentify(mTrackingID, 0, IdentityCallback, info);
        MILO_ASSERT(hr != E_INVALIDARG, 0x2B1);
        if (hr < 0) {
            if (hr != (HRESULT)0x8000000A)
                return false;
        }
        if (hr == 0) {
            info->SetIdentified(true);
        } else {
            GestureMgr::sIdentityOpInProgress = true;
        }
        return true;
    } else {
        return false;
    }
}

bool Skeleton::EnrollIdentity(int enrollmentIdx) {
    MILO_ASSERT(!GestureMgr::sIdentityOpInProgress, 0x25D);
    IdentityInfo *info = TheGestureMgr->GetIdentityInfo(mSkeletonIdx);
    if (!info) {
        return false;
    }

    DWORD flags = 1;
    if (enrollmentIdx != -1) {
        flags = 0x21;
    }
    if (enrollmentIdx == -3) {
        enrollmentIdx = -2;
    }

    HRESULT hr = NuiIdentityEnroll(mTrackingID, enrollmentIdx, flags, IdentityCallback, info);
    MILO_ASSERT(hr != E_INVALIDARG, 0x26E);

    if (hr < 0) {
        if (hr != (HRESULT)0x8000000A) {
            return false;
        }
    }

    if (hr == 0) {
        info->SetIdentified(true);
    } else {
        GestureMgr::sIdentityOpInProgress = true;
    }
    return true;
}

int Skeleton::IdentityCallback(void *pvContext, NUI_IDENTITY_MESSAGE *pMessage) {
    IdentityInfo *info = (IdentityInfo *)pvContext;
    MILO_ASSERT(pvContext != NULL, 0x280);
    MILO_ASSERT(pMessage != NULL, 0x281);

    if ((unsigned int)pMessage->MessageId < NUI_IDENTITY_MESSAGE_ID_COMPLETE) {
        // frame processed
    } else if (!(pMessage->MessageId == NUI_IDENTITY_MESSAGE_ID_COMPLETE)) {
        MILO_ASSERT(false, 0x297);
    } else {
        info->SetIdentified(true);
        info->SetProfileMatched(pMessage->Data.Complete.bProfileMatched != 0);
        if ((unsigned int)info->EnrollmentIndex()
            != pMessage->Data.Complete.dwEnrollmentIndex) {
            info->SetEnrollmentIndex(pMessage->Data.Complete.dwEnrollmentIndex);
        }
    }

    if (!TheGestureMgr->IDEnabled()) {
        MILO_LOG("An identification operation that was in progress was canceled.\n");
        GestureMgr::sIdentityOpInProgress = false;
        return 0;
    }
    return 1;
}
