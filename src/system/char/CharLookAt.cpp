#include "char/CharLookAt.h"
#include "char/Char.h"
#include "char/CharWeightable.h"
#include "math/Mtx.h"
#include "math/Rand.h"
#include "math/Rot.h"
#include "math/Utl.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "rndobj/Graph.h"
#include "rndobj/Poll.h"

const float sMaxThreshold = 80;
bool CharLookAt::sDisableJitter = false;

CharLookAt::CharLookAt()
    : mSource(this), mPivot(this), mTarget(this), mHalfTime(0), mMinYaw(-80), mMaxYaw(80),
      mMinPitch(-80), mMaxPitch(sMaxThreshold), mMinWeightYaw(-1), mMaxWeightYaw(-1),
      mWeightYawSpeed(10000), mPivotLookTarget(kHugeFloat, 0, 0), mPivotLookWeight(1), mSourceRadius(0),
      unka4(0, 0, 0), mShowRange(false), mTestRange(false), mTestRangePitch(0.5),
      mTestRangeYaw(0.5), mAllowRoll(true), mDisableRoll(false), mEnableJitter(false),
      mYawJitterLimit(0), mPitchJitterLimit(0) {
    SyncLimits();
}

CharLookAt::~CharLookAt() {}

BEGIN_HANDLERS(CharLookAt)
    HANDLE_SUPERCLASS(CharPollable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(CharLookAt)
    SYNC_PROP(source, mSource)
    SYNC_PROP(pivot, mPivot)
    SYNC_PROP(target, mTarget)
    SYNC_PROP(half_time, mHalfTime)
    SYNC_PROP_SET(min_yaw, mMinYaw, SetMinYaw(_val.Float()))
    SYNC_PROP_SET(max_yaw, mMaxYaw, SetMaxYaw(_val.Float()))
    SYNC_PROP_SET(min_pitch, mMinPitch, SetMinPitch(_val.Float()))
    SYNC_PROP_SET(max_pitch, mMaxPitch, SetMaxPitch(_val.Float()))
    SYNC_PROP(min_weight_yaw, mMinWeightYaw)
    SYNC_PROP(max_weight_yaw, mMaxWeightYaw)
    SYNC_PROP(weight_yaw_speed, mWeightYawSpeed)
    SYNC_PROP(allow_roll, mAllowRoll)
    SYNC_PROP(show_range, mShowRange)
    SYNC_PROP(source_radius, mSourceRadius)
    SYNC_PROP(enable_jitter, mEnableJitter)
    SYNC_PROP(yaw_jitter_limit, mYawJitterLimit)
    SYNC_PROP(pitch_jitter_limit, mPitchJitterLimit)
    SYNC_PROP(test_range, mTestRange)
    SYNC_PROP(test_range_pitch, mTestRangePitch)
    SYNC_PROP(test_range_yaw, mTestRangeYaw)
    SYNC_SUPERCLASS(CharWeightable)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(CharLookAt)
    SAVE_REVS(5, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(CharWeightable)
    bs << mSource;
    bs << mPivot;
    bs << mTarget;
    bs << mHalfTime;
    bs << mMinYaw;
    bs << mMaxYaw;
    bs << mMinPitch;
    bs << mMaxPitch;
    bs << mMinWeightYaw;
    bs << mMaxWeightYaw;
    bs << mWeightYawSpeed;
    bs << mAllowRoll;
    bs << mEnableJitter;
    bs << mPitchJitterLimit;
    bs << mYawJitterLimit;
    bs << mSourceRadius;
END_SAVES

BEGIN_COPYS(CharLookAt)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(CharWeightable)
    CREATE_COPY(CharLookAt)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mSource)
        COPY_MEMBER(mPivot)
        COPY_MEMBER(mTarget)
        COPY_MEMBER(mHalfTime)
        COPY_MEMBER(mMinYaw)
        COPY_MEMBER(mMaxYaw)
        COPY_MEMBER(mMinPitch)
        COPY_MEMBER(mMaxPitch)
        COPY_MEMBER(mMinWeightYaw)
        COPY_MEMBER(mMaxWeightYaw)
        COPY_MEMBER(mWeightYawSpeed)
        COPY_MEMBER(mAllowRoll)
        COPY_MEMBER(mSourceRadius)
        COPY_MEMBER(mEnableJitter)
        COPY_MEMBER(mYawJitterLimit)
        COPY_MEMBER(mPitchJitterLimit)
    END_COPYING_MEMBERS
    SyncLimits();
END_COPYS

INIT_REVS(5, 0)

BEGIN_LOADS(CharLookAt)
    LOAD_REVS(bs)
    ASSERT_REVS(5, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(CharWeightable)
    d >> mSource;
    d >> mPivot;
    d >> mTarget;
    d >> mHalfTime;
    d >> mMinYaw;
    d >> mMaxYaw;
    d >> mMinPitch;
    d >> mMaxPitch;
    if (d.rev > 1) {
        d >> mMinWeightYaw;
        d >> mMaxWeightYaw;
        d >> mWeightYawSpeed;
    }
    if (d.rev < 3)
        mAllowRoll = true;
    else
        d >> mAllowRoll;
    if (d.rev < 4) {
        mEnableJitter = false;
        mPitchJitterLimit = 0;
        mYawJitterLimit = 0;
    } else {
        d >> mEnableJitter;
        d >> mPitchJitterLimit;
        d >> mYawJitterLimit;
    }
    if (d.rev > 4)
        d >> mSourceRadius;
    SyncLimits();
END_LOADS

void CharLookAt::Enter() {
    mPivotLookTarget.Set(kHugeFloat, 0, 0);
    if (mPivot) {
        mPivot->DirtyLocalXfm().m.Identity();
    }
    RndPollable::Enter();
}

void CharLookAt::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    changedBy.push_back(GetSource());
    changedBy.push_back(mTarget);
    change.push_back(mPivot);
}

void CharLookAt::SetMinYaw(float yaw) {
    mMinYaw = yaw;
    SyncLimits();
}

void CharLookAt::SetMaxYaw(float yaw) {
    mMaxYaw = yaw;
    SyncLimits();
}

void CharLookAt::SetMinPitch(float pitch) {
    mMinPitch = pitch;
    SyncLimits();
}

void CharLookAt::SetMaxPitch(float pitch) {
    mMaxPitch = pitch;
    SyncLimits();
}

void CharLookAt::Poll() {
    RndTransformable *source = GetSource();
    float deltasecs = TheTaskMgr.DeltaSeconds();
    if (mTarget && mPivot) {
        if (!mPivot->TransParent() || !source || deltasecs < 0)
            return;
        else {
            Vector3 lookDir;
            Subtract(mTarget->WorldXfm().v, source->WorldXfm().v, lookDir);
            float charWeight = Weight();
            if (mMinWeightYaw >= 0.0f) {
                Vector3 srcFwd(source->WorldXfm().m.y);
                Normalize(srcFwd, srcFwd);
                Vector3 lookDir2d(lookDir);
                lookDir2d.z = 0;
                srcFwd.z = 0;
                float dot = Dot(srcFwd, lookDir2d);
                float clamped = Clamp<float>(-1.0f, 1.0f, dot / (Length(srcFwd) * Length(lookDir2d)));
                float acosDeg = (float)std::acos(clamped) * RAD2DEG;
                float autoWeight = Clamp<float>(
                    0.0f,
                    1.0f,
                    (mMaxWeightYaw - acosDeg) / (mMaxWeightYaw - mMinWeightYaw)
                );
                float autoWeightDelta = (autoWeight - mPivotLookWeight) / deltasecs;
                if (MinEq(autoWeightDelta, mWeightYawSpeed)) {
                    autoWeight = autoWeightDelta * deltasecs + mPivotLookWeight;
                }
                charWeight *= autoWeight;
                mPivotLookWeight = autoWeight;
            }
            if (charWeight != 0.0f) {
                Vector3 sourceFilter(0.0f, 0.0f, 0.0f);
                if (mSourceRadius > 0.0f) {
                    if (TheTaskMgr.DeltaSeconds() > 0.0f) {
                        Interp(unka4, source->WorldXfm().m.y, 0.1f, unka4);
                    }
                    Subtract(source->WorldXfm().m.y, unka4, sourceFilter);
                    float filterSq = LengthSquared(sourceFilter);
                    float srcRad = mSourceRadius * DEG2RAD;
                    if (filterSq > srcRad * srcRad) {
                        float sqrtFilter = std::sqrt(filterSq);
                        sourceFilter *= srcRad / sqrtFilter;
                    }
                }
                if (source != mPivot) {
                    Transform pivotXfm(mPivot->WorldXfm());
                    Hmx::Quat rotQuat;
                    MakeRotQuat(source->WorldXfm().m.y, lookDir, rotQuat);
                    Hmx::Matrix3 rotMat;
                    MakeRotMatrix(rotQuat, rotMat);
                    Multiply(pivotXfm.m, rotMat, pivotXfm.m);
                    mPivot->SetWorldXfm(pivotXfm);
                    Subtract(mTarget->WorldXfm().v, source->WorldXfm().v, lookDir);
                    MakeRotQuat(source->WorldXfm().m.y, lookDir, rotQuat);
                    MakeRotMatrix(rotQuat, rotMat);
                    Multiply(pivotXfm.m.y, rotMat, lookDir);
                } else
                    Normalize(lookDir, lookDir);
                Multiply(lookDir, mPivot->TransParent()->WorldXfm().m, lookDir);
                Normalize(lookDir, lookDir);
                mDisableRoll = mLookLimits.Clamp(lookDir);
                Normalize(lookDir, lookDir);
                if (mPivotLookTarget.x != kHugeFloat && mHalfTime != 0.0f) {
                    Interp(mPivotLookTarget, lookDir, deltasecs / (deltasecs + mHalfTime), lookDir);
                }
                mPivotLookTarget = lookDir;
                if (mTestRange) {
                    float interpYaw, interpPitch;
                    Interp(mLookLimits.mMin.z, mLookLimits.mMax.z, mTestRangeYaw, interpYaw);
                    Interp(mLookLimits.mMin.x, mLookLimits.mMax.x, mTestRangePitch, interpPitch);
                    lookDir.Set(interpPitch, mLookLimits.mMin.y, interpYaw);
                } else if (mShowRange) {
                    charWeight = 1.0f;
                    switch (((int)TheTaskMgr.Seconds(TaskMgr::kRealTime)) & 7) {
                    case 0:
                        lookDir.Set(mLookLimits.mMin.x, mLookLimits.mMin.y, mLookLimits.mMin.z);
                        break;
                    case 1:
                        lookDir.Set(0.0f, mLookLimits.mMin.z, mLookLimits.mMax.x);
                        break;
                    case 2:
                        lookDir.Set(mLookLimits.mMax.x, mLookLimits.mMin.y, mLookLimits.mMin.z);
                        break;
                    case 3:
                        lookDir.Set(mLookLimits.mMax.x, mLookLimits.mMin.y, 0.0f);
                        break;
                    case 4:
                        lookDir.Set(mLookLimits.mMax.x, mLookLimits.mMin.y, mLookLimits.mMax.z);
                        break;
                    case 5:
                        lookDir.Set(0.0f, mLookLimits.mMin.y, mLookLimits.mMax.z);
                        break;
                    case 6:
                        lookDir.Set(mLookLimits.mMin.x, mLookLimits.mMin.y, mLookLimits.mMax.z);
                        break;
                    case 7:
                        lookDir.Set(mLookLimits.mMin.x, mLookLimits.mMin.y, 0.0f);
                        break;
                    default:
                        break;
                    }
                }
                static DataNode &disable = DataVariable("cheat.disable_eye_jitter");
                if (mEnableJitter && !sDisableJitter && !disable && deltasecs > 0.0f) {
                    auto _tmp4 = RandomFloat(-mPitchJitterLimit, mPitchJitterLimit);
                    lookDir.Set(
                        lookDir[0]
                            + _tmp4
                                * DEG2RAD,
                        lookDir[1],
                        lookDir[2] + RandomFloat(-mYawJitterLimit, mYawJitterLimit) * DEG2RAD
                    );
                }
                if (mSourceRadius > 0.0f) {
                    Multiply(sourceFilter, mPivot->TransParent()->WorldXfm().m, sourceFilter);
                    lookDir -= sourceFilter;
                }
                if (mAllowRoll) {
                    Hmx::Quat rotQuat;
                    MakeRotQuat(mPivot->LocalXfm().m.y, lookDir, rotQuat);
                    FastInterp(Hmx::Quat(0, 0, 0, 1.0f), rotQuat, charWeight, rotQuat);
                    Hmx::Matrix3 rotMat;
                    MakeRotMatrix(rotQuat, rotMat);
                    if (rotMat.x.x < -2.0f || rotMat.x.x > 2.0f) {
                        MILO_NOTIFY_ONCE(
                            "%s has m.x.x %g, character or target scaled or NAN",
                            PathName(this),
                            rotMat.x.x
                        );
                        rotMat.Identity();
                    }
                    Multiply(mPivot->LocalXfm().m, rotMat, mPivot->DirtyLocalXfm().m);
                } else {
                    Hmx::Matrix3 &dirtyMat = mPivot->DirtyLocalXfm().m;
                    Interp(dirtyMat.y, lookDir, charWeight, dirtyMat.y);
                    dirtyMat.z.Set(-1.0f, 0.0f, 0.0f);
                    Normalize(dirtyMat.y, dirtyMat.y);
                    Cross(dirtyMat.y, dirtyMat.z, dirtyMat.x);
                    Normalize(dirtyMat.x, dirtyMat.x);
                    Cross(dirtyMat.x, dirtyMat.y, dirtyMat.z);
                    if (dirtyMat.x.x < -2.0f || dirtyMat.x.x > 2.0f) {
                        MILO_NOTIFY_ONCE(
                            "%s has m.x.x %g, character or target scaled or NAN",
                            PathName(this),
                            dirtyMat.x.x
                        );
                        dirtyMat.Identity();
                    }
                }
            }
        }
    }
}

static void DrawBounds(Vector3 lookDir, const Hmx::Matrix3 &rotMat, const Vector3 &pos, RndGraph *graph) {
    Normalize(lookDir, lookDir);
    Vector3 result;
    Multiply(lookDir, rotMat, result);
    Hmx::Color green(0, 1, 0, 1);
    result *= 10.0f;
    result += pos;
    graph->AddLine(pos, result, green, false);
}

void CharLookAt::Highlight() {
    if (mSource && mTarget) {
        RndTransformable *source = GetSource();
        RndTransformable *target = mTarget;
        RndGraph *graph = RndGraph::GetOneFrame();
        Hmx::Color red(1, 0, 0, 1);
        graph->AddLine(source->WorldXfm().v, target->WorldXfm().v, red, false);
        RndTransformable *parent = mPivot->TransParent();
        Transform parentXfm(parent->WorldXfm());
        const Vector3 &pivotPos = mPivot->WorldXfm().v;
        auto _tmp0 = Vector3(mLookLimits.mMin.x, mLookLimits.mMin.y, 0);
        DrawBounds(_tmp0, parentXfm.m, pivotPos, graph);
        DrawBounds(Vector3(mLookLimits.mMax.x, mLookLimits.mMin.y, 0), parentXfm.m, pivotPos, graph);
        DrawBounds(Vector3(0, mLookLimits.mMin.y, mLookLimits.mMin.z), parentXfm.m, pivotPos, graph);
        DrawBounds(Vector3(0, mLookLimits.mMin.y, mLookLimits.mMax.z), parentXfm.m, pivotPos, graph);
    }
}

void CharLookAt::SyncLimits() {
    ClampEq(mMinYaw, -sMaxThreshold, sMaxThreshold);
    ClampEq(mMaxYaw, -sMaxThreshold, sMaxThreshold);
    ClampEq(mMinPitch, -sMaxThreshold, sMaxThreshold);
    ClampEq(mMaxPitch, -sMaxThreshold, sMaxThreshold);
    float yaw = Max<float>(fabsf(mMinYaw), fabsf(mMaxYaw));
    float pitch = Max<float>(fabsf(mMinPitch), fabsf(mMaxPitch));
    mLookLimits.mMin.y = (float)std::cos(Max<float>(yaw, pitch) * DEG2RAD);
    mLookLimits.mMax.y = kHugeFloat;
    mLookLimits.mMin.z = (float)std::tan(mMinYaw * DEG2RAD) * mLookLimits.mMin.y;
    mLookLimits.mMax.z = (float)std::tan(mMaxYaw * DEG2RAD) * mLookLimits.mMin.y;
    mLookLimits.mMin.x = (float)std::tan(mMinPitch * DEG2RAD) * mLookLimits.mMin.y;
    mLookLimits.mMax.x = (float)std::tan(mMaxPitch * DEG2RAD) * mLookLimits.mMin.y;
}
