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

// Retail transforms lookDir/sourceFilter with the COLUMN-VECTOR product
// (out.i = Dot(v, m.i), i.e. each output is a dot of one matrix ROW with v),
// which Mtx.h has no overload for -- Mtx.h only supplies the transpose
// (Vector3, Matrix3) form, whose out.x reads the matrix COLUMN {0x0,0x10,0x20}.
// Retail's asm at va 0x823ba738 reads rows {0x0,0x4,0x8} / {0x10,0x14,0x18} /
// {0x20,0x24,0x28}, so the two call sites below need this overload. rb3-Wii has
// it in its own Mtx.h; its body is hand-unrolled into accx/accy/accz temporaries
// which is MWCC-shaped and costs MSVC 6 extra instructions here (aliasing
// reloads, since `out` may alias `v`) -- the Dot() spelling is what matches.
// NOTE: this belongs in math/Mtx.h, but adding it there is a header change with
// engine-wide blast radius, so it is kept TU-local pending its own A/B.
inline void Multiply(const Hmx::Matrix3 &m, const Vector3 &v, Vector3 &out) {
    out.Set(Dot(v, m.x), Dot(v, m.y), Dot(v, m.z));
}

// Same product as Mtx.h's Multiply(Vector3, Matrix3, Vector3), but with each
// product written VECTOR-first (v.x * m.x.x) instead of Mtx.h's matrix-first
// (m.x.x * v.x). MSVC keeps that operand order, and retail is vector-first.
// Mtx.h (and dc3's) are matrix-first; fixing that in the header is a candidate
// engine-wide force multiplier, but needs a whole-binary A/B, hence the local
// copy under a distinct name.
inline void MultiplyVM(const Vector3 &v, const Hmx::Matrix3 &m, Vector3 &vout) {
    vout.Set(
        v.x * m.x.x + v.y * m.y.x + v.z * m.z.x,
        v.x * m.x.y + v.y * m.y.y + v.z * m.z.y,
        v.x * m.x.z + v.y * m.y.z + v.z * m.z.z
    );
}

// RB3-360 retail rev storage. Retail's LOAD_REVS keeps NO BinStreamRev: it splits
// the packed rev into two mutable file-scope shorts, and ASSERT_REVS emits nothing.
// The two words must live in ONE aligned(4) aggregate (altRev +0, rev +4) -- MSVC
// does not lay .bss out in declaration order, so two separate statics get other
// globals interleaved between them and will not fold onto one base register.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_CharLookAt;
#define gAltRev gRevs_CharLookAt.altRev
#define gRev gRevs_CharLookAt.rev

const float sMaxThreshold = 80;
bool CharLookAt::sDisableJitter = false;

CharLookAt::CharLookAt()
    : mSource(this), mPivot(this), mTarget(this), mMaxYaw(80),
      mMinPitch(-80), mMaxPitch(sMaxThreshold), mMinWeightYaw(-1), mWeightYawSpeed(10000),
      mMaxWeightYaw(-1), mPivotLookTarget(kHugeFloat, 0, 0), mPivotLookWeight(1), mSourceRadius(0),
      unka4(0, 0, 0), mShowRange(false), mAllowRoll(true), mDisableRoll(false), mEnableJitter(false),
      mYawJitterLimit(0), mPitchJitterLimit(0) {
    mHalfTime = 0;
    mMinYaw = -80;
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
    // retail (MILO_DEBUG off) has no property-sync exposure for jitter/test_range
    // members (rb3-Wii gates these under #ifdef MILO_DEBUG in BEGIN_PROPSYNCS),
    // and does not double-sync the Hmx::Object superclass.
    SYNC_SUPERCLASS(CharWeightable)
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

BEGIN_LOADS(CharLookAt)
    int rev;
    bs >> rev;
    gRev = getHmxRev(rev);
    gAltRev = getAltRev(rev);
    Hmx::Object::Load(bs);
    CharWeightable::Load(bs);
    bs >> mSource;
    bs >> mPivot;
    bs >> mTarget;
    bs >> mHalfTime;
    bs >> mMinYaw;
    bs >> mMaxYaw;
    bs >> mMinPitch;
    bs >> mMaxPitch;
    if (gRev > 1) {
        bs >> mMinWeightYaw;
        bs >> mMaxWeightYaw;
        bs >> mWeightYawSpeed;
    }
    if (gRev < 3)
        mAllowRoll = true;
    else
        bs >> mAllowRoll;
    if (gRev < 4) {
        mEnableJitter = false;
        mPitchJitterLimit = 0;
        mYawJitterLimit = 0;
    } else {
        bs >> mEnableJitter;
        bs >> mPitchJitterLimit;
        bs >> mYawJitterLimit;
    }
    if (gRev > 4)
        bs >> mSourceRadius;
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
                    // LengthSquared(sourceFilter), but spelled with the component
                    // temporaries rb3-Wii's LengthSquared() carries and ours does
                    // not. The DECLARATION order is load-bearing: MSVC emits the
                    // three fsubs rotated one position left of the decl order, so
                    // (y,z,x) here reproduces retail's (z,x,y). Vec.h's plain
                    // "v.x*v.x + v.y*v.y + v.z*v.z" gives (y,z,x) and cannot be
                    // fixed by reordering the summands -- /fp:fast reassociates
                    // and canonicalizes the sum, so only the temporaries move it.
                    float fsy = sourceFilter.y;
                    float fsz = sourceFilter.z;
                    float fsx = sourceFilter.x;
                    float filterSq = fsx * fsx + fsy * fsy + fsz * fsz;
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
                    MultiplyVM(pivotXfm.m.y, rotMat, lookDir);
                } else
                    Normalize(lookDir, lookDir);
                Multiply(mPivot->TransParent()->WorldXfm().m, lookDir, lookDir);
                Normalize(lookDir, lookDir);
                mDisableRoll = mLookLimits.Clamp(lookDir);
                Normalize(lookDir, lookDir);
                if (mPivotLookTarget.x != kHugeFloat && mHalfTime != 0.0f) {
                    Interp(mPivotLookTarget, lookDir, deltasecs / (deltasecs + mHalfTime), lookDir);
                }
                mPivotLookTarget = lookDir;
                // retail (this build/version, vanilla 45410914) has NEITHER the
                // rb3-Wii debug-only mTestRange preview branch NOR the mShowRange
                // preview switch -- both absent from the compiled retail Poll().
                // Do not reintroduce those two. The eye-jitter block below IS
                // present in retail, minus rb3-Wii's later-added sDisableJitter/
                // "cheat.disable_eye_jitter" dev guards (retail's asm gates on
                // mEnableJitter && deltasecs>0.0f only -- verified via va
                // 0x823ba738 Ghidra decomp + objdiff instruction-level match).
                if (mEnableJitter && deltasecs > 0.0f) {
                    float yawJitter = RandomFloat(-mYawJitterLimit, mYawJitterLimit);
                    float pitchJitter = RandomFloat(-mPitchJitterLimit, mPitchJitterLimit);
                    lookDir.x += pitchJitter * DEG2RAD;
                    lookDir.z += yawJitter * DEG2RAD;
                }
                if (mSourceRadius > 0.0f) {
                    Multiply(mPivot->TransParent()->WorldXfm().m, sourceFilter, sourceFilter);
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
