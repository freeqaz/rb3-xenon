#include "hamobj/HamSkeletonConverter.h"
#include "gesture/BaseSkeleton.h"
#include "gesture/JointUtl.h"
#include "gesture/Skeleton.h"
#include "gesture/SkeletonUpdate.h"
#include "hamobj/HamCharacter.h"
#include "math/Mtx.h"
#include "math/Rot.h"
#include "math/Utl.h"
#include "math/Vec.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Poll.h"
#include "rndobj/Rnd.h"
#include "rndobj/Trans.h"
#include "rndobj/Utl.h"
#include "utl/Str.h"

HamSkeletonConverter::HamSkeletonConverter()
    : mBones(this), unk28(0), mCharacter(this), mIsActive(0), unk751(0), unk754(0) {}

HamSkeletonConverter::~HamSkeletonConverter() {
    SkeletonUpdateHandle handle = SkeletonUpdate::InstanceHandle();
    if (handle.HasCallback(this)) {
        handle.RemoveCallback(this);
    }
}

BEGIN_HANDLERS(HamSkeletonConverter)
    HANDLE_ACTION(run_test, 0)
    HANDLE_SUPERCLASS(RndPollable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(HamSkeletonConverter)
    SYNC_PROP(bones, mBones)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(HamSkeletonConverter)
    SAVE_REVS(2, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mBones;
END_SAVES

BEGIN_COPYS(HamSkeletonConverter)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(HamSkeletonConverter)
    BEGIN_COPYING_MEMBERS
        mBones = c->mBones.Ptr();
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(2, 0)

BEGIN_LOADS(HamSkeletonConverter)
    LOAD_REVS(bs)
    ASSERT_REVS(2, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    d >> mBones;
END_LOADS

void HamSkeletonConverter::SetName(const char *name, ObjectDir *dir) {
    Hmx::Object::SetName(name, dir);
    mCharacter = dynamic_cast<HamCharacter *>(dir);
}

void HamSkeletonConverter::Enter() {
    SkeletonUpdateHandle handle = SkeletonUpdate::InstanceHandle();
    if (!handle.HasCallback(this)) {
        handle.AddCallback(this);
    }
    mPelvisMesh = mCharacter->Find<RndTransformable>("bone_pelvis.mesh", true);
    mPelvisInitialZ = mPelvisMesh->LocalXfm().v.z;
    mBoneMeshes.clear();
    mBoneMeshes.resize(kNumJoints);
    for (int i = 0; i < kNumJoints; i++) {
        RndTransformable *t =
            mCharacter->Find<RndTransformable>(MirrorBoneName((SkeletonJoint)i), true);
        mBoneMeshes[i] = t;
    }
    PaddedJointPos z;
    memcpy(&z, &mBoneMeshes[kJointHipLeft]->WorldXfm().m.z, sizeof(PaddedJointPos));
    mLeftHipZAxis = z;
    memcpy(&z, &mBoneMeshes[kJointHipRight]->WorldXfm().m.z, sizeof(PaddedJointPos));
    mRightHipZAxis = z;
    mLeftHipZAxisInit = mLeftHipZAxis;
    mRightHipZAxisInit = mRightHipZAxis;
}

void HamSkeletonConverter::Exit() {
    RndPollable::Exit();
    SkeletonUpdateHandle handle = SkeletonUpdate::InstanceHandle();
    if (handle.HasCallback(this)) {
        handle.RemoveCallback(this);
    }
}

void HamSkeletonConverter::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    change.push_back(mBones);
}

void HamSkeletonConverter::Highlight() {
    for (int i = 0; i < kNumJoints; i++) {
        Vector3 curV = mJointPositions[i];
        UtilDrawSphere(curV, 1.0f, Hmx::Color(0.0f, 0.0f, 1.0f), nullptr);
        Transform curXfm = mBoneTransforms[i];

        Vector3 scaledX;
        Scale(curXfm.m.x, 4.0f, scaledX);
        Vector3 scaledY;
        Scale(curXfm.m.y, 4.0f, scaledY);
        Vector3 scaledZ;
        Scale(curXfm.m.z, 4.0f, scaledZ);

        Vector3 lineEnd;
        Add(scaledX, curXfm.v, lineEnd);
        TheRnd.DrawLine(curXfm.v, lineEnd, Hmx::Color(1.0f, 0.0f, 0.0f), false);
        Add(scaledY, curXfm.v, lineEnd);
        TheRnd.DrawLine(curXfm.v, lineEnd, Hmx::Color(0.0f, 1.0f, 0.0f), false);
        Add(scaledZ, curXfm.v, lineEnd);
        TheRnd.DrawLine(curXfm.v, lineEnd, Hmx::Color(0.0f, 0.0f, 1.0f), false);
    }
}

void HamSkeletonConverter::PostUpdate(const SkeletonUpdateData *data) {
    if (mIsActive && data) {
        BaseSkeleton *skeleton = nullptr;
        for (int i = 0; i < 6; i++) {
            if (data->mSkeletonsLeft[i] && data->mSkeletonsLeft[i]->IsTracked()) {
                skeleton = data->mSkeletonsLeft[i];
                break;
            }
        }
        Set(skeleton);
    }
}

void HamSkeletonConverter::GetParentWorldXfm(
    RndTransformable *t, Transform &xfm, SkeletonJoint parent
) {
    RndTransformable *meshParent = t->TransParent();
    if (streq(meshParent->Name(), "bone_pelvis.mesh")) {
        xfm.m = mPelvisTransform.m;
        xfm.v = mPelvisTransform.v;
    } else if (IsSkeletonBone(meshParent->Name())) {
        MILO_ASSERT(streq(meshParent->Name(), CharBoneName(parent)), 0x2B2);
        xfm.m = mBoneTransforms[parent].m;
        xfm.v = mBoneTransforms[parent].v;
    } else {
        GetParentWorldXfm(meshParent, xfm, parent);
        Multiply(meshParent->LocalXfm(), xfm, xfm);
    }
}

void HamSkeletonConverter::SetQuatBoneValue(String s, Hmx::Quat q) {
    String str(s);
    if (str.find(".mesh") != FixedString::npos) {
        str = str.substr(0, s.length() - 5);
    }
    str += ".quat";
    Hmx::Quat *qPtr = (Hmx::Quat *)mBones->FindPtr(str.c_str());
    // this is stupid but hey if it matches lmao
    qPtr->w = q.w;
    qPtr->x = q.x;
    qPtr->y = q.y;
    qPtr->z = q.z;
}

void HamSkeletonConverter::SetRotzBoneValue(String s, float r) {
    String str(s);
    if (str.find(".mesh") != FixedString::npos) {
        str = str.substr(0, s.length() - 5);
    }
    str += ".rotz";
    float *rPtr = (float *)mBones->FindPtr(str.c_str());
    *rPtr = r;
}

void HamSkeletonConverter::SetPosBoneValue(String s, Vector3 v) {
    String str(s);
    if (str.find(".mesh") != FixedString::npos) {
        str = str.substr(0, s.length() - 5);
    }
    str += ".pos";
    Vector3 *vPtr = (Vector3 *)mBones->FindPtr(str.c_str());
    // this is stupid but hey if it matches lmao
    vPtr->x = v.x;
    vPtr->y = v.y;
    vPtr->z = v.z;
}

void HamSkeletonConverter::CalcQuatBone(
    SkeletonJoint joint, SkeletonJoint from, SkeletonJoint to
) {
    Vector3 dir;
    Subtract(mJointPositions[to], mJointPositions[from], dir);
    Normalize(dir, dir);

    RndTransformable *mesh = mBoneMeshes[from];
    Transform xfm = mesh->LocalXfm();

    Transform parentXfm;
    GetParentWorldXfm(mesh, parentXfm, joint);
    Multiply(xfm, parentXfm, xfm);

    Hmx::Quat q;
    MakeRotQuat(xfm.m.x, dir, q);

    Hmx::Matrix3 mat;
    Multiply(xfm.m.x, q, mat.x);
    Multiply(xfm.m.y, q, mat.y);
    Multiply(xfm.m.z, q, mat.z);
    Normalize(mat, mat);

    auto& boneXfm = mBoneTransforms[from];
    memcpy(&boneXfm.m, &mat, sizeof(Hmx::Matrix3));
    boneXfm.v = xfm.v;

    Invert(parentXfm, parentXfm);
    Multiply(boneXfm, parentXfm, xfm);

    q.Set(xfm.m);
    SetQuatBoneValue(String(CharBoneName(from)), q);
}

void HamSkeletonConverter::CalcRotzBone(
    SkeletonJoint joint, SkeletonJoint from, SkeletonJoint to
) {
    Vector3 dir1;
    Subtract(mJointPositions[from], mJointPositions[joint], dir1);
    Normalize(dir1, dir1);

    Vector3 dir2;
    Subtract(mJointPositions[to], mJointPositions[from], dir2);
    Normalize(dir2, dir2);

    float angle = acos(Dot(dir1, dir2));
    angle = -angle;
    int isNaN = (angle != angle) ? 1 : 0;
    if ((isNaN & 0xFF) == 0) {
        RndTransformable *mesh = mBoneMeshes[from];
        Hmx::Matrix3 mat;
        memcpy(&mat, &mesh->LocalXfm().m, sizeof(Hmx::Matrix3));
        MakeRotMatrixZ(angle, mat);

        Transform xfm;
        memcpy(&xfm.m, &mat, sizeof(Hmx::Matrix3));
        xfm.v = mesh->LocalXfm().v;

        Multiply(xfm, mBoneTransforms[joint], mBoneTransforms[from]);
    }
    SetRotzBoneValue(String(MirrorBoneName(from)), angle);
}

void HamSkeletonConverter::ScaleBone(
    SkeletonJoint joint1,
    SkeletonJoint joint2,
    SkeletonCoordSys coord,
    const Vector3 &pos1,
    const Vector3 &pos2,
    const Vector3 &basePos,
    Vector3 &result
) {
    RndTransformable *mesh2 = mBoneMeshes[joint2];
    RndTransformable *mesh1 = mBoneMeshes[joint1];

    float jointDist = Distance(pos1, pos2);

    float meshDist = Distance(mesh1->WorldXfm().v, mesh2->WorldXfm().v);
    float scale = meshDist / jointDist;

    Vector3 diff;
    Subtract(pos2, pos1, diff);
    Scale(diff, scale, diff);
    Add(basePos, diff, result);
}

void HamSkeletonConverter::SetArm(
    SkeletonJoint parent, SkeletonJoint shoulder, SkeletonJoint elbow, SkeletonJoint hand
) {
    Vector3 dir;
    Subtract(mJointPositions[elbow], mJointPositions[shoulder], dir);
    Normalize(dir, dir);

    Vector3 elbowToHand;
    Subtract(mJointPositions[elbow], mJointPositions[hand], elbowToHand);
    Vector3 cross1;
    Cross(dir, elbowToHand, cross1);
    Normalize(cross1, cross1);

    Vector3 cross2;
    Cross(cross1, dir, cross2);
    Normalize(cross2, cross2);

    Transform parentXfm;
    GetParentWorldXfm(mBoneMeshes[shoulder], parentXfm, parent);

    Vector3 worldPos;
    Multiply(mBoneMeshes[shoulder]->LocalXfm().v, parentXfm, worldPos);

    Hmx::Matrix3 mat;
    mat.x = dir;
    mat.y = cross2;
    mat.z = cross1;

    Transform xfm;
    memcpy(&xfm.m, &mat, sizeof(Hmx::Matrix3));
    xfm.v = worldPos;

    auto& shoulderXfm = mBoneTransforms[shoulder];
    memcpy(&shoulderXfm.m, &mat, sizeof(Hmx::Matrix3));
    shoulderXfm.v = worldPos;

    Transform invParent;
    Invert(parentXfm, invParent);
    Multiply(xfm, invParent, xfm);
    NormalizeAboutX(xfm.m);

    Hmx::Quat q;
    q.Set(xfm.m);
    SetQuatBoneValue(String(MirrorBoneName(shoulder)), q);

    CalcRotzBone(shoulder, elbow, hand);
}

void HamSkeletonConverter::SetLeg(
    SkeletonJoint parent,
    SkeletonJoint hip,
    SkeletonJoint knee,
    SkeletonJoint ankle,
    SkeletonJoint foot,
    const BaseSkeleton *skel,
    int side
) {
    Vector3 dir;
    auto& _sub0 = mJointPositions[hip];
    auto& kneePos = mJointPositions[knee];
    Subtract(kneePos, _sub0, dir);
    Vector3 dir2;
    Normalize(dir, dir);

    Subtract(mJointPositions[ankle], kneePos, dir2);
    Normalize(dir2, dir2);

    auto _tmp0 = Dot(dir, dir2);
    float angle = acos(_tmp0);
    angle = -angle;
    int isNaN = (angle != angle) ? 1 : 0;
    if ((isNaN & 0xFF) == 0) {
        RndTransformable *mesh = mBoneMeshes[hip];
        Transform parentXfm;
        GetParentWorldXfm(mesh, parentXfm, parent);

        Plane plane;
        plane.Set(_sub0, kneePos, mJointPositions[ankle]);

        int usePelvis = (angle < 0.2) ? 1 : 0;
        if (abs(usePelvis) != 0) {
            plane.a = mPelvisTransform.m.z.x;
            plane.b = mPelvisTransform.m.z.y;
            plane.c = mPelvisTransform.m.z.z;
            plane.d = -((plane.c * (_sub0.z - kneePos.z) + (plane.b * (_sub0.y - kneePos.y) + plane.a * (_sub0.x - kneePos.x))));
        }
        PaddedJointPos *hipZAxisInit = &mLeftHipZAxisInit + side;
        hipZAxisInit->x = plane.a * -1.0f;
        hipZAxisInit->y = plane.b * -1.0f;
        hipZAxisInit->z = plane.c * -1.0f;

        Vector3 worldPos;
        Multiply(mesh->LocalXfm().v, parentXfm, worldPos);

        Subtract(kneePos, _sub0, dir);
        Normalize(dir, dir);

        PaddedJointPos *hipZAxis = &mLeftHipZAxis + side;
        RotateTowards(*hipZAxis, *hipZAxisInit, 1000.0f, *hipZAxis);

        Vector3 cross1;
        Cross(dir, *hipZAxis, cross1);
        Normalize(cross1, cross1);

        Hmx::Matrix3 mat;
        mat.x = dir;
        mat.y = cross1;
        mat.z = *hipZAxis;

        Transform xfm;
        memcpy(&xfm.m, &mat, sizeof(Hmx::Matrix3));
        xfm.v = worldPos;

        auto& _sub1 = mBoneTransforms[hip];
        memcpy(&_sub1.m, &mat, sizeof(Hmx::Matrix3));
        _sub1.v = worldPos;

        Transform invParent;
        Invert(parentXfm, invParent);
        Multiply(xfm, invParent, xfm);
        NormalizeAboutX(xfm.m);

        Hmx::Quat q;
        q.Set(xfm.m);
        SetQuatBoneValue(String(MirrorBoneName(hip)), q);

        RndTransformable *kneeMesh = mBoneMeshes[knee];
        Hmx::Matrix3 kneeMat;
        memcpy(&kneeMat, &kneeMesh->LocalXfm().m, sizeof(Hmx::Matrix3));
        MakeRotMatrixZ(angle, kneeMat);

        Transform kneeXfm;
        memcpy(&kneeXfm.m, &kneeMat, sizeof(Hmx::Matrix3));
        kneeXfm.v = kneeMesh->LocalXfm().v;

        Multiply(kneeXfm, _sub1, mBoneTransforms[knee]);

        SetRotzBoneValue(String(MirrorBoneName(knee)), angle);
    }
}

void HamSkeletonConverter::Set(const BaseSkeleton *skel) {
    if (skel && skel->IsTracked()) {
        if (!unk751) {
            unk751 = true;
        }
        if (mCharacter) {
            unk40.m.x.Set(1.0f, 0.0f, 0.0f);
            unk40.m.y.Set(0.0f, 1.0f, 0.0f);
            unk40.m.z.Set(0.0f, 0.0f, 1.0f);
            unk40.v.Zero();

            Transform flipX;
            flipX.m.x.Set(1.0f, 0.0f, 0.0f);
            flipX.m.y.Set(0.0f, 0.0f, -1.0f);
            flipX.m.z.Set(0.0f, 1.0f, 0.0f);
            flipX.v.Zero();

            Transform flipZ;
            flipZ.m.x.Set(0.0f, -1.0f, 0.0f);
            flipZ.m.y.Set(0.0f, 0.0f, 1.0f);
            flipZ.m.z.Set(1.0f, 0.0f, 0.0f);
            flipZ.v.Zero();

            Multiply(flipX.m, unk40.m, unk40.m);
            Multiply(flipZ.m, unk40.m, unk40.m);

            Vector3 pelvisOffset;
            pelvisOffset.x = 0.0f;
            pelvisOffset.y = -2.0f;
            pelvisOffset.z = 0.0f;
            Multiply(pelvisOffset, unk40, pelvisOffset);

            unk40.v.z = mPelvisInitialZ;
            unk40.v.x = pelvisOffset.x;
            unk40.v.y = pelvisOffset.y * 39.37008f;

            Multiply(unk40, mCharacter->WorldXfm(), unk40);

            PaddedJointPos worldJoints[kNumJoints];
            for (int i = 0; i < kNumJoints; i++) {
                Vector3 camPos;
                skel->JointPos(kCoordCamera, (SkeletonJoint)i, camPos);
                Multiply(camPos, unk40, worldJoints[i]);
                Multiply(camPos, unk40, mJointPositions[i]);
            }

            mPelvisTransform.v.x = (worldJoints[kJointHipLeft].x + worldJoints[kJointHipRight].x) * 0.5f;
            mPelvisTransform.v.y = (worldJoints[kJointHipLeft].y + worldJoints[kJointHipRight].y) * 0.5f;
            mPelvisTransform.v.z = (worldJoints[kJointHipLeft].z + worldJoints[kJointHipRight].z) * 0.5f;

            Transform invCharXfm;
            Invert(mCharacter->WorldXfm(), invCharXfm);

            Vector3 pelvisLocal;
            Multiply(mPelvisTransform.v, invCharXfm, pelvisLocal);
            SetPosBoneValue(String("bone_pelvis.mesh"), pelvisLocal);

            float pelvisX = worldJoints[kJointHipCenter].x;
            float pelvisY = worldJoints[kJointHipCenter].y;
            float pelvisZ = worldJoints[kJointHipCenter].z;
            PaddedJointPos *curJoint = mJointPositions;
            for (int j = 0; j < kNumJoints; j++, curJoint++) {
                int parentJoint = JointParent((SkeletonJoint)j);
                if (parentJoint == -1) {
                    float pvx = mPelvisTransform.v.x;
                    float pvy = mPelvisTransform.v.y;
                    float pvz = mPelvisTransform.v.z;
                    float diffX = pelvisX - pvx;
                    float diffY = pelvisY - pvy;
                    float diffZ = pelvisZ - pvz;
                    float dist = Distance(mPelvisTransform.v, worldJoints[kJointHipCenter]);
                    RndTransformable *mesh0 = mBoneMeshes[0];
                    RndTransformable *pelvisMesh = mPelvisMesh;
                    float meshDist = Distance(mesh0->WorldXfm().v, pelvisMesh->WorldXfm().v);
                    float scale = meshDist / dist;
                    curJoint->x = pvx + diffX * scale;
                    curJoint->y = pvy + diffY * scale;
                    curJoint->z = pvz + diffZ * scale;
                } else {
                    ScaleBone((SkeletonJoint)parentJoint, (SkeletonJoint)j, kUnk5, worldJoints[parentJoint], worldJoints[j], mJointPositions[parentJoint], *curJoint);
                }
            }

            Vector3 hipAxis;
            Subtract(mJointPositions[kJointHipRight], mJointPositions[kJointHipLeft], hipAxis);

            Vector3 pelvisLateral;
            Subtract(mJointPositions[kJointHipCenter], mPelvisTransform.v, pelvisLateral);

            Normalize(hipAxis, hipAxis);
            Normalize(pelvisLateral, pelvisLateral);

            Vector3 pelvisFwd;
            Cross(hipAxis, pelvisLateral, pelvisFwd);
            Normalize(pelvisFwd, pelvisFwd);

            mPelvisTransform.m.x = pelvisLateral;
            mPelvisTransform.m.y = pelvisFwd;
            mPelvisTransform.m.z = hipAxis;
            Normalize(mPelvisTransform.m, mPelvisTransform.m);

            Transform pelvisLocalXfm;
            Multiply(mPelvisTransform, invCharXfm, pelvisLocalXfm);
            Hmx::Quat pelvisQ;
            pelvisQ.Set(pelvisLocalXfm.m);
            SetQuatBoneValue(String("bone_pelvis.mesh"), pelvisQ);

            CalcQuatBone(kJointHipCenter, kJointHipCenter, kJointSpine);
            CalcQuatBone(kJointHipCenter, kJointSpine, kJointShoulderCenter);
            CalcQuatBone(kJointSpine, kJointShoulderCenter, kJointHead);

            SetLeg(kJointHipCenter, kJointHipLeft, kJointKneeLeft, kJointAnkleLeft, kJointFootLeft, skel, 0);
            SetLeg(kJointHipCenter, kJointHipRight, kJointKneeRight, kJointAnkleRight, kJointFootRight, skel, 1);

            SetArm(kJointSpine, kJointShoulderLeft, kJointElbowLeft, kJointWristLeft);
            SetArm(kJointSpine, kJointShoulderRight, kJointElbowRight, kJointWristRight);
        }
    }
}

void HamSkeletonConverter::RotateTowards(
    const Vector3 &v1, const Vector3 &v2, float f, Vector3 &vout
) {
    if (v1 == v2)
        return;
    Hmx::Quat q50;
    q50.Reset();
    Hmx::Quat q40;
    MakeRotQuat(v1, v2, q40);
    float angle = acos(Dot(v1, v2));
    int isValid = (angle != angle) ? 1 : 0;
    if ((isValid & 0xFF) == 0) {
        float absAngle = fabs(angle);
        if (absAngle >= 1.0e-9) {
            float fabsed = fabsf(f / angle);
            if (fabsed >= 1.0f) {
                vout.x = v2.x;
                vout.y = v2.y;
                vout.z = v2.z;
            } else {
                Interp(q50, q40, fabsed, q40);
                Multiply(v1, q40, vout);
            }
        } else {
            vout.x = v1.x;
            vout.y = v1.y;
            vout.z = v1.z;
        }
    } else {
        vout.x = v1.x;
        vout.y = v1.y;
        vout.z = v1.z;
    }
}
