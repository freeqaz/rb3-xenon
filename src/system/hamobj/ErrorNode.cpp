#include "hamobj/ErrorNode.h"
#include "ErrorNode.h"
#include "hamobj/CharFeedback.h"
#include "gesture/BaseSkeleton.h"
#include "hamobj/DancerSkeleton.h"
#include "math/Vec.h"
#include "obj/Data.h"
#include "os/Debug.h"

namespace {
    int gSkeletonJointToErrorJoint[kNumJoints] = {
        kErrorJointHipCenter,  kErrorJointSpine,        kErrorJointShoulderCenter,
        kErrorJointHead,       kErrorJointShoulderLeft, kErrorJointElbowLeft,
        kErrorJointWristLeft,  kErrorJointHandLeft,     kErrorJointShoulderRight,
        kErrorJointElbowRight, kErrorJointWristRight,   kErrorJointHandRight,
        kErrorJointHipLeft,    kErrorJointKneeLeft,     kErrorJointAnkleLeft,
        kErrorJointHipRight,   kErrorJointKneeRight,    kErrorJointAnkleRight,
        kErrorJointFootLeft,   kErrorJointFootRight
    };
}

ErrorFrameInput::ErrorFrameInput(
    const SkeletonHistory *history,
    const DancerSkeleton &dancerSkeleton,
    const BaseSkeleton &baseSkeleton,
    float f1
)
    : mSkeleton(dancerSkeleton), mBaseSkeleton(baseSkeleton) {
    dancerSkeleton.CamBoneLengths(mBoneLengths);
    baseSkeleton.CamBoneLengths(mBaseBoneLengths);
    dancerSkeleton.CamJointPositions((Vector3 *)mJointPositions);
    baseSkeleton.CamJointPositions((Vector3 *)mBaseJointPositions);
    mDisplacements = false;
    int elapsedMs = dancerSkeleton.ElapsedMs();
    if (elapsedMs != -1) {
        int div = (int)(elapsedMs / f1);
        mDisplacements =
            baseSkeleton.Displacements(history, kCoordCamera, div, (Vector3 *)mBaseJointDisps, div);
    }
    dancerSkeleton.CamJointDisplacements((Vector3 *)mJointDisps);
}

void ErrorNodeInput::Set(const Vector3 &v, const Ham1NodeWeight *w) {
    mNodeComponentWeight = v;
    mNodeWeight = w;
}

#pragma region ErrorNode

ErrorNode::ErrorNode(ErrorNodeType e, const DataArray *cfg)
    : mType(e), mXErrorAxis(kJointHipCenter), mZErrorAxis(kJointHipCenter) {
    mNodeName = cfg->Sym(0);
    static Symbol joint("joint");
    static Symbol feedback_limbs("feedback_limbs");
    static Symbol xz_error_axis("xz_error_axis");
    mJoint = (SkeletonJoint)cfg->FindInt(joint);
    DataArray *limbsArr = cfg->FindArray(feedback_limbs, true);
    mFeedbackLimbs = kFeedbackNone;
    for (int i = 1; i < limbsArr->Size(); i++) {
        mFeedbackLimbs |= (FeedbackLimbs)limbsArr->Int(i);
    }
    DataArray *axisArr = cfg->FindArray(xz_error_axis, false);
    if (axisArr) {
        mXErrorAxis = (SkeletonJoint)axisArr->Int(1);
        mZErrorAxis = (SkeletonJoint)axisArr->Int(2);
    } else {
        mXErrorAxis = mZErrorAxis = kNumJoints;
    }
}

void ErrorNode::NormBoneLengths(
    const ErrorFrameInput &input,
    const SkeletonBone (&bones)[kMaxNumNormBones],
    float &totalBoneLengths,
    float &totalBaseBoneLengths
) const {
    totalBoneLengths = 0;
    totalBaseBoneLengths = 0;
    for (int i = 0; i < 3; i++) {
        SkeletonBone curBone = bones[i];
        if (curBone == kNumBones)
            return;
        totalBoneLengths += input.mBoneLengths[curBone];
        totalBaseBoneLengths += input.mBaseBoneLengths[curBone];
    }
}

bool ErrorNode::IsTypeJointMatch(int joint) const {
    return mType & joint && gSkeletonJointToErrorJoint[mJoint] & joint;
}

void ErrorNode::InitNormBones(
    const DataArray *cfg, SkeletonBone (&skelBones)[kMaxNumNormBones]
) {
    static Symbol norm_bones("norm_bones");
    DataArray *bones = cfg->FindArray(norm_bones, true);
    MILO_ASSERT(bones->Size() - 1 <= kMaxNumNormBones, 0x95);
    for (int i = 0; i < kMaxNumNormBones; i++) {
        if (i < bones->Size() - 1) {
            skelBones[i] = (SkeletonBone)bones->Int(i + 1);
        } else {
            skelBones[i] = kNumBones;
        }
    }
}

bool ErrorNode::XZErrorAxis(Vector3 &v, const DancerSkeleton &skeleton) const {
    if (mXErrorAxis == kNumJoints) {
        return false;
    } else {
        Subtract(skeleton.CamJointPos(mXErrorAxis), skeleton.CamJointPos(mZErrorAxis), v);
        return true;
    }
}

ErrorNode *ErrorNode::Create(const DataArray *cfg) {
    ErrorNodeType type = (ErrorNodeType)cfg->FindInt("type");
    if (type == kErrorHam1Euclidean) {
        return new Ham1EuclideanNode(type, cfg);
    } else if (type == kErrorHam1Displacement) {
        return new Ham1DisplacementNode(type, cfg);
    } else if (type == kErrorDisplacement) {
        return new DisplacementNode(type, cfg);
    } else if (type == kErrorPosition) {
        return new PositionNode(type, cfg);
    } else {
        MILO_FAIL("Could not create node of type %i", type);
        return nullptr;
    }
}

#pragma endregion
#pragma region Ham1EuclideanNode

Ham1EuclideanNode::Ham1EuclideanNode(ErrorNodeType e, const DataArray *cfg)
    : ErrorNode(e, cfg) {
    for (int i = 0; i < 3; i++) {
        // 0x24, 0x28 i = 0
        // 0x2c, 0x30 i = 1
        // 0x34, 0x38 i = 2
        mComponentWeightRanges[i][0] = 0;
        mComponentWeightRanges[i][1] = 0;
    }
    static Symbol coord_sys("coord_sys");
    mCoordSys = (SkeletonCoordSys)cfg->FindInt(coord_sys);
    static Symbol base_joint("base_joint");
    mBaseJoint = (SkeletonJoint)cfg->FindInt(base_joint);
    static Symbol component_weight_ranges("component_weight_ranges");
    DataArray *weightArr = cfg->FindArray(component_weight_ranges);
    for (int i = 1; i < 4; i++) {
        DataArray *arr = weightArr->Array(i);
        mComponentWeightRanges[i - 1][0] = arr->Float(0);
        mComponentWeightRanges[i - 1][1] = arr->Float(1);
    }
}

void Ham1EuclideanNode::CalcError(
    const ErrorFrameInput &frame_input, const ErrorNodeInput &node_input, Vector3 &vout
) const {
    MILO_ASSERT(node_input.mNodeWeight, 0x10E);
    Vector3 dancerVec;
    frame_input.mSkeleton.NormPos(mCoordSys, mJoint, dancerVec);
    Vector3 baseVec;
    frame_input.mBaseSkeleton.NormPos(mCoordSys, mJoint, baseVec);
    float diffX = dancerVec.x - baseVec.x;
    float diffY = dancerVec.y - baseVec.y;
    float diffZ = dancerVec.z - baseVec.z;
    Vector3 vToProcess;
    for (int i = 0; i < 3; i++) {
        float set = Max(mComponentWeightRanges[i][0], node_input.mNodeComponentWeight[i]);
        vToProcess[i] = Min(set, mComponentWeightRanges[i][1]);
    }
    ScaleOp op;
    op.mPerfectDist = node_input.mNodeWeight->mPerfectDist;
    op.mType = kErrorScaleDistSq;
    op.mRate = node_input.mNodeWeight->mRate;
    float px = vToProcess.x * diffX;
    float py = vToProcess.y * diffY;
    float pz = vToProcess.z * diffZ;
    vout.x = ScaleDistToError(op, std::sqrt(px * px + py * py + pz * pz));
}

#pragma endregion
#pragma region BaseDisplacementNode

BaseDisplacementNode::BaseDisplacementNode(ErrorNodeType e, const DataArray *cfg)
    : ErrorNode(e, cfg) {
    static Symbol base_joint("base_joint");
    DataArray *jointArr = cfg->FindArray(base_joint, false);
    if (jointArr) {
        mBaseJoint = (SkeletonJoint)jointArr->Int(1);
    } else {
        mBaseJoint = mJoint;
    }
    InitNormBones(cfg, mNormBones);
}

bool BaseDisplacementNode::Displacements(
    const ErrorFrameInput &frame_input, BaseDisplacementNode::DisplacementData &dispData
) const {
    if (frame_input.mDisplacements) {
        dispData.mJointDisplacement = frame_input.mJointDisps[mJoint];
        dispData.mBaseJointDisplacement = frame_input.mBaseJointDisps[mJoint];
        if (mBaseJoint != mJoint) {
            Subtract(
                dispData.mJointDisplacement,
                frame_input.mJointDisps[mBaseJoint],
                dispData.mJointDisplacement
            );
            Subtract(
                dispData.mBaseJointDisplacement,
                frame_input.mBaseJointDisps[mBaseJoint],
                dispData.mBaseJointDisplacement
            );
        }
        float boneLen, baseBoneLen;
        NormBoneLengths(frame_input, mNormBones, boneLen, baseBoneLen);
        if (baseBoneLen > 0) {
            Scale(
                dispData.mBaseJointDisplacement,
                boneLen / baseBoneLen,
                dispData.mBaseJointDisplacement
            );
            return true;
        }
    }
    dispData.mBaseJointDisplacement.Zero();
    dispData.mJointDisplacement.Zero();
    return false;
}

bool BaseDisplacementNode::Displacements(
    const ErrorFrameInput &frame_input,
    DisplacementData &dispData,
    Ham1DisplacementData &ham1Data
) const {
    Vector3 &proj = ham1Data.unk4;
    proj.x = 0.0f;
    proj.z = 0.0f;
    proj.y = 0.0f;
    ham1Data.unk14 = false;
    ham1Data.unk18 = 0.0f;
    ham1Data.unk0 = 0.0f;
    ham1Data.unk1c = 0.0f;
    bool ok = Displacements(frame_input, dispData);
    if (ok) {
        float jdLen = Length(dispData.mJointDisplacement);
        ham1Data.unk1c = jdLen;
        float nx, ny, nz;
        if (0.0f < jdLen) {
            float inv = 1.0f / jdLen;
            nx = dispData.mJointDisplacement.x * inv;
            ny = inv * dispData.mJointDisplacement.y;
            nz = dispData.mJointDisplacement.z * inv;
        } else {
            nx = 0.0f;
            ny = 0.0f;
            nz = 0.0f;
        }
        float dot = nx * dispData.mBaseJointDisplacement.x
            + ny * dispData.mBaseJointDisplacement.y
            + nz * dispData.mBaseJointDisplacement.z;
        proj.x = nx * dot;
        proj.y = ny * dot;
        proj.z = nz * dot;
        ham1Data.unk14 = (dot > 0.0f);
        float bjdLen = Length(dispData.mBaseJointDisplacement);
        ham1Data.unk0 = bjdLen;
        float bnx, bny, bnz;
        if (0.0f < bjdLen) {
            float inv = 1.0f / bjdLen;
            bnx = dispData.mBaseJointDisplacement.x * inv;
            bny = dispData.mBaseJointDisplacement.y * inv;
            bnz = inv * dispData.mBaseJointDisplacement.z;
        } else {
            bnx = 0.0f;
            bny = 0.0f;
            bnz = 0.0f;
        }
        float cosAngle = bnz * nz + bny * ny + bnx * nx;
        float clamped = -1.0f - cosAngle < 0.0f ? cosAngle : -1.0f;
        clamped = clamped - 1.0f < 0.0f ? clamped : 1.0f;
        ham1Data.unk18 = fabsf(acosf(clamped));
        return true;
    }
    return false;
}

void DistanceToErrors(const Vector3 &a, const Vector3 &b, const Vector3 &c, Vector3 &d) {
    Subtract(a, b, d);

    d.x *= c.x;
    d.z *= c.z;
    d.y *= c.y;

    for (int j = 0; j < 3; ++j) {
        float x = fabsf(d[j]);
        float y = -x;
        float z = (y >= 0.0f) ? 0.0f : x;
        float w = (z - 1.0f >= 0.0f) ? 1.0f : z;
        d[j] = w;
    }
}

void DisplacementNode::CalcError(
    const ErrorFrameInput &frame_input, const ErrorNodeInput &node_input, Vector3 &vout
) const {
    MILO_ASSERT(node_input.mNodeWeight == NULL, 0x1F8);
    DisplacementData dispData;
    if (Displacements(frame_input, dispData)) {
        DistanceToErrors(
            dispData.mJointDisplacement,
            dispData.mBaseJointDisplacement,
            node_input.mNodeComponentWeight,
            vout
        );
    } else {
        vout.Set(1, 1, 1);
    }
}

Ham1DisplacementNode::Ham1DisplacementNode(ErrorNodeType e, const DataArray *cfg)
    : BaseDisplacementNode(e, cfg) {
    static Symbol potential_angle_op("potential_angle_op");
    mPotentialAngleOp.Set(cfg->FindArray(potential_angle_op));
}

void Ham1DisplacementNode::CalcError(
    const ErrorFrameInput &frame_input, const ErrorNodeInput &node_input, Vector3 &vout
) const {
    MILO_ASSERT(node_input.mNodeWeight, 0x19D);
    ErrorData errData;
    DisplacementData dispData;
    Ham1DisplacementData ham1DispData;
    Errors(frame_input, node_input, errData, dispData, ham1DispData);
    vout.x = errData.unk4 * errData.unk8 + errData.unk0;
}

void Ham1DisplacementNode::Errors(
    const ErrorFrameInput &frame_input,
    const ErrorNodeInput &node_input,
    ErrorData &errData,
    DisplacementData &dispData,
    Ham1DisplacementData &ham1Data
) const {
    bool ok = Displacements(frame_input, dispData, ham1Data);
    if (!ok) {
        errData.unk8 = 1.0f;
        errData.unk0 = 1.0f;
        errData.unk4 = 1.0f;
        return;
    }
    float angle = ham1Data.unk18;
    ScaleOp op;
    op.mType = kErrorScaleDistSq;
    op.mPerfectDist = node_input.mNodeWeight->mPerfectDist2;
    op.mRate = node_input.mNodeWeight->mRate2;
    float projLen = Length(ham1Data.unk4);
    errData.unk4 = ScaleDistToError(mPotentialAngleOp, ham1Data.unk0);
    errData.unk4 = errData.unk4 - 1.0f < 0.0f ? errData.unk4 : 1.0f;
    errData.unk8 = ScaleDistToError(op, angle);
    float ratio = 1.0f;
    if (0.0f < ham1Data.unk1c) {
        ratio = projLen / ham1Data.unk1c;
    }
    float magInput;
    if (ham1Data.unk14) {
        magInput = fabsf(1.0f - ratio);
    } else {
        magInput = ratio + 1.0f;
    }
    op.mType = kErrorScaleDistSq;
    op.mPerfectDist = node_input.mNodeWeight->mPerfectDist;
    op.mRate = node_input.mNodeWeight->mRate;
    errData.unk0 = ScaleDistToError(op, magInput);
}

PositionNode::PositionNode(ErrorNodeType e, const DataArray *cfg) : ErrorNode(e, cfg) {
    static Symbol base_joint("base_joint");

    mBaseJoint = (SkeletonJoint)cfg->FindInt(base_joint);
    InitNormBones(cfg, mNormBones);
}

void PositionNode::CalcError(
    const ErrorFrameInput &frame_input, const ErrorNodeInput &node_input, Vector3 &vout
) const {
    MILO_ASSERT(node_input.mNodeWeight == NULL, 0x21C);
    Vector3 jointDiff;
    Vector3 baseJointDiff;
    float desired_bone_len;
    float base_bone_len;
    Vector3 scaledBaseDiff;

    Subtract(
        frame_input.mBaseJointPositions[mJoint],
        frame_input.mBaseJointPositions[mBaseJoint],
        baseJointDiff
    );
    Subtract(
        frame_input.mJointPositions[mJoint],
        frame_input.mJointPositions[mBaseJoint],
        jointDiff
    );
    NormBoneLengths(frame_input, mNormBones, desired_bone_len, base_bone_len);
    MILO_ASSERT(desired_bone_len > 0, 0x22C);

    if (base_bone_len <= 0) {
        vout.Set(1, 1, 1);
    } else {
        Scale(baseJointDiff, desired_bone_len / base_bone_len, scaledBaseDiff);
        DistanceToErrors(jointDiff, scaledBaseDiff, node_input.mNodeComponentWeight, vout);
    }
}

float ScaleDistToError(const ScaleOp &op, float dist) {
    if (op.mPerfectDist == -1.0f)
        return dist;
    if (dist < 0.0f) {
        MILO_NOTIFY("%f distance is less than zero (%f, %f)", dist, op.mPerfectDist, op.mRate);
        return 1.0f;
    }
    if (dist > op.mPerfectDist) {
        float excess = dist - op.mPerfectDist;
        if (op.mType == kErrorScaleDistSq) {
            excess = excess * excess;
        }
        return op.mRate * excess;
    }
    return 0.0f;
}

float ScaleFullErrorDist(const ScaleOp &op) {
    if (op.mPerfectDist == -1.0f)
        return 1.0f;
    float invRate;
    if (op.mType == kErrorScaleDist) {
        invRate = 1.0f / op.mRate;
    } else {
        invRate = sqrtf(1.0f / op.mRate);
    }
    return invRate + op.mPerfectDist;
}

void XZErrorWeight(const Vector3 &v, float &xzWeight, float &yWeight) {
    Vector3 flat;
    flat = v;
    flat.y = 0;
    Normalize(flat, flat);
    static Vector3 up(0, 0, 1);
    float dot = fabs(up.z * flat.z + up.x * flat.x + up.y * flat.y);
    float angle = acosf(dot);
    xzWeight = angle * (2.0f / PI);
    yWeight = 1.0f - xzWeight;
}

void ScaleOp::Set(const DataArray *cfg) {
    static Symbol type("type");
    static Symbol rate("rate");
    static Symbol perfect_dist("perfect_dist");
    mType = (ErrorScaleType)cfg->FindInt(type);
    cfg->FindData(perfect_dist, mPerfectDist);
    cfg->FindData(rate, mRate);
}
