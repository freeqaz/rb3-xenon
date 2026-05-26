#include "char/CharEyes.h"
#include "char/CharInterest.h"
#include "char/CharLookAt.h"
#include "char/CharWeightable.h"
#include "decomp.h"
#include "math/Easing.h"
#include "math/Rand.h"
#include "math/Rot.h"
#include "math/Utl.h"
#include "math/Vec.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "rndobj/Cam.h"
#include "rndobj/Graph.h"
#include "rndobj/Rnd.h"
#include "rndobj/Trans.h"
#include "ui/PanelDir.h"
#include "utl/BinStream.h"
#include "utl/Std.h"
#include "utl/Symbol.h"
#include "world/Dir.h"
#include <cmath>

void NormalizeScale(const Vector3 &, float, Vector3 &);

bool CharEyes::sDisableEyeDart;
bool CharEyes::sDisableEyeJitter;
bool CharEyes::sDisableInterestObjects;
bool CharEyes::sDisableProceduralBlink;
bool CharEyes::sDisableEyeClamping;
// CharLookAt::sDisableJitter is defined in CharLookAt.cpp

INIT_REVS(18, 0)

#if !defined(__EMSCRIPTEN__) && !defined(__APPLE__)
float pow(float base, float exp) { return std::pow(base, exp); }
#endif

CharEyes::CharEyes()
    : mEyes(this), mInterests(this), mFaceServo(this), mCamWeight(this), mTarget(0, 0, 0),
      mDefaultFilterFlags(0), mViewDirection(this), mHeadLookAt(this),
      mMaxExtrapolation(19.5), mMinTargetDist(35), mUpperLidTrackUp(1),
      mUpperLidTrackDown(1), mLowerLidTrackUp(0.75), mLowerLidTrackDown(0.75),
      mLowerLidTrackRotate(false), mInterestFilterFlags(0), mLastFacing(0, 0, 0),
      mLastLook(0), mLastBlinkWeight(0),
      mBlinkDetect(0), mBlinkActive(0), mCurrentInterest(this), mFocusInterest(this),
      mFocusTimer(-1), mNeedRecalc(0), mDartOffset(0, 1, 0), mDartTimer(0),
      mDartEnabled(0), mDartInterval(-1), mEyeClampCount(-1),
      mBlinkEnabled(0), mBlinkTimer(-1), mBlinkCount(0),
      mUpperBlinkAngle(-1), mLowerBlinkAngle(-1), mEnabled(0), mHeadIKActive(1) {
    mMaxEyeCang = std::cos(0.5235987715423107);
    mEyeStatusOverlay = RndOverlay::Find("eye_status", false);
}

CharEyes::~CharEyes() {}

void CharEyes::Enter() {
    mLastFacing.Zero();
    mLastLook = 0;
    mLastBlinkWeight = -1.0f;
    mLastCang = 1.0f;
    mBlinkDetect = false;
    mBlinkActive = false;
    mDartEnabled = false;
    mDartInterval = -1.0f;
    mEyeClampCount = -1;
    mBlinkEnabled = false;
    mBlinkTimer = -1.0f;
    mBlinkCount = 0;
    mUpperBlinkAngle = -1.0f;
    mLowerBlinkAngle = -1.0f;
    mInterestFilterFlags = mDefaultFilterFlags;
    mDartTimer = 0.0f;
    mEnabled = false;
    auto& _ref1 = mNeedRecalc;
    _ref1 = false;
    RndTransformable *head = GetHead();
    _ref1 = false;
    if (head) {
        mLastFacing = head->WorldXfm().m.y;
        Normalize(mLastFacing, mLastFacing);
    }
    for (ObjVector<EyeDesc>::iterator it = mEyes.begin(); it != mEyes.end(); ++it) {
#ifdef HX_NATIVE
        if (!it->mEye) continue;
#endif
        it->mEye->Enter();
    }
    for (ObjVector<CharInterestState>::iterator it = mInterests.begin();
         it != mInterests.end();
         ++it) {
        it->mRefractoryTime = -1.0f;
    }
    RndPollable::Enter();
}

void CharEyes::Exit() {
    mFocusInterest = 0;
    mFocusTimer = -1;
    mInterests.clear();
    for (ObjVector<EyeDesc>::iterator it = mEyes.begin(); it != mEyes.end(); ++it) {
        it->mEye->Exit();
    }
    RndPollable::Exit();
}

void CharEyes::Highlight() {
#ifdef MILO_DEBUG
    if (GetHead()) {
        RndGraph *oneframe = RndGraph::GetOneFrame();
        RndTransformable *trans = 0;
        for (ObjVector<EyeDesc>::iterator it = mEyes.begin(); it != mEyes.end(); ++it) {
            if (it->mEye) {
                trans = it->mEye->GetSource();
                if (trans) {
                    const Transform &tf = trans->WorldXfm();
                    Vector3 v100(
                        tf.m.y.x * 3.0f + tf.v.x,
                        tf.m.y.y * 3.0f + tf.v.y,
                        tf.m.y.z * 3.0f + tf.v.z
                    );
                    if (it->mEye->mDisableRoll)
                        oneframe->AddLine(
                            trans->WorldXfm().v, v100, Hmx::Color(1.0f, 0.0f, 0.0f), true
                        );
                    else
                        oneframe->AddLine(
                            trans->WorldXfm().v, v100, Hmx::Color(0.0f, 1.0f, 0.0f), true
                        );
                }
            }
        }
        Vector3 headPos(GetHead()->WorldXfm().v);
        if (trans) {
            float f2 = mLastBlinkWeight;
            float f1 = mCurrentInterest ? mCurrentInterest->mMaxViewAngleCos : mMaxEyeCang;
            if (mDartEnabled) {
                oneframe->AddSphere(
                    mTarget, mData.mMaxRadius, Hmx::Color(0.9f, 0.9f, 0.9f)
                );
                Vector3 dartTarget(
                    mTarget.x + mCurrentDartOffsetX,
                    mTarget.y + mCurrentDartOffsetY,
                    mTarget.z + mCurrentDartOffsetZ
                );
                EnforceMinimumTargetDistance(headPos, dartTarget, dartTarget);
                oneframe->AddSphere(dartTarget, 0.5f, Hmx::Color(0.0f, 0.0f, 1.0f));
                oneframe->AddLine(
                    trans->WorldXfm().v,
                    dartTarget,
                    f2 < f1 ? Hmx::Color(1.0f, 0.0f, 0.0f) : Hmx::Color(0.2f, 0.2f, 1.0f),
                    true
                );
            } else {
                oneframe->AddLine(
                    trans->WorldXfm().v,
                    mTarget,
                    f2 < f1 ? Hmx::Color(1.0f, 0.0f, 0.0f) : Hmx::Color(1.0f, 1.0f, 1.0f),
                    true
                );
            }
            if (mBlinkEnabled) {
                oneframe->AddString3D(
                    "p blink!", trans->WorldXfm().v, Hmx::Color(1.0f, 1.0f, 1.0f)
                );
            }
        }
        if (mFocusInterest) {
            if (mFocusInterest != mCurrentInterest) {
                const char *nametouse = mCurrentInterest ? mCurrentInterest->Name() : "GENERATED";
                oneframe->AddString3D(
                    MakeString("focus = '%s' (looking at %s)", mFocusInterest->Name(), nametouse),
                    headPos,
                    Hmx::Color(1.0f, 0.0f, 0.0f)
                );
            } else {
                oneframe->AddString3D(
                    MakeString("focus = '%s'", mFocusInterest->Name()),
                    headPos,
                    Hmx::Color(0.0f, 1.0f, 0.0f)
                );
            }
        } else {
            if (mCurrentInterest) {
                oneframe->AddString3D(
                    MakeString("interest = '%s'", mCurrentInterest->Name()),
                    headPos,
                    Hmx::Color(0.0f, 1.0f, 0.0f)
                );
            }
        }
        if (mInterests.size() != 0) {
            RndTransformable *head = GetHead();
            const Transform &headxfm = head->WorldXfm();
            Vector3 headFwd(headxfm.m.y);
            Normalize(headFwd, headFwd);
            float sphereSize = 2.0f;
            for (ObjVector<CharInterestState>::iterator it = mInterests.begin();
                 it != mInterests.end();
                 ++it) {
                CharInterest *interest = it->mInterest;
                bool matchesFilter = interest->IsMatchingFilterFlags(mInterestFilterFlags);
                if (!matchesFilter && (mInterestFilterFlags != mDefaultFilterFlags ||
                    interest->mMaxViewAngleCos != 0)) {
                    continue;
                }

                if (interest == mCurrentInterest) {
                    oneframe->AddSphere(headxfm.v, sphereSize, Hmx::Color(1.0f, 0.0f, 0.0f));
                    if (interest->mMaxViewAngleCos != 0) {
                    }
                } else {
                    if (interest->IsWithinViewCone(headxfm.v, headFwd) &&
                        interest->IsWithinViewCone(headxfm.v, headFwd)) {
                        if (!matchesFilter) {
                        } else {
                            oneframe->AddSphere(interest->WorldXfm().v, sphereSize, Hmx::Color(0.3f, 0.3f, 1.0f));
                        }
                    } else if (!matchesFilter) {
                    } else {
                        oneframe->AddSphere(interest->WorldXfm().v, sphereSize, Hmx::Color(0.313f, 0.313f, 1.0f));
                    }
                }

                if (it->IsInRefractoryPeriod()) {
                    float refTime = it->RefractoryTimeRemaining();
                    if (interest->mMaxViewAngleCos != 0) {
                    }
                }
            }
        }
    }
#endif
}

DECOMP_FORCEACTIVE(CharEyes, "%s", "r=%f")

void CharEyes::UpdateOverlay() {
    if (mEyeStatusOverlay && mEyeStatusOverlay->Showing()) {
        *mEyeStatusOverlay << Dir()->Name() << ": ";
        if (mCurrentInterest) {
            if (mFocusInterest) {
                if (streq(mCurrentInterest->Name(), mFocusInterest->Name())) {
                    *mEyeStatusOverlay << "Look(FOC) ";
                    goto done_look;
                }
            }
            *mEyeStatusOverlay << "Look(" << mCurrentInterest->Name() << ") ";
        } else
            *mEyeStatusOverlay << "Look(GEN) ";
    done_look:
        if (mFocusInterest) {
            const Transform &headxfm = GetHead()->WorldXfm();
            Vector3 fwd(headxfm.m.y);
            Normalize(fwd, fwd);
            const char *str = mFocusInterest->IsWithinViewCone(headxfm.v, fwd) ? "t" : "f";
            *mEyeStatusOverlay << "Foc(" << mFocusInterest->Name() << " p(" << mFocusTimer << ") v(" << str << ")) ";
        } else
            *mEyeStatusOverlay << "Foc(NA) ";
        *mEyeStatusOverlay << "t(" << mLastLook << ") ";
        Vector3 headPos(GetHead()->WorldXfm().v);
        Vector3 diff;
        Vector3 target(mTarget);
        RndTransformable *tgt = GetTarget();
        if (tgt)
            target = tgt->WorldXfm().v;
        Subtract(target, headPos, diff);
        float len = Length(diff);
        *mEyeStatusOverlay << "Dist(" << len << ") ";
        if (mBlinkEnabled)
            *mEyeStatusOverlay << "P Blink! ";
        if (mDartEnabled)
            *mEyeStatusOverlay << "Dart! ";
        if (mBlinkActive)
            *mEyeStatusOverlay << "Close! ";
        *mEyeStatusOverlay << "\n";
    }
}

DECOMP_FORCEACTIVE(
    CharEyes,
    "no_lids",
    "eyes.disable_clamping",
    "eyes.debug_clamping",
    "eyes.disable_llidnorm",
    "cheat.disable_eye_darts",
    "cheat.disable_procedural_blinks",
    "cheat.disable_interest_objects",
    "ObjPtr_p.h",
    "f.Owner()",
    ""
)

BEGIN_HANDLERS(CharEyes)
    HANDLE(add_interest, OnAddInterest)
    HANDLE_ACTION(force_blink, ForceBlink())
    HANDLE(toggle_force_focus, OnToggleForceFocus)
    HANDLE(toggle_interest_overlay, OnToggleInterestOverlay)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_CUSTOM_PROPSYNC(CharEyes::EyeDesc)
    SYNC_PROP(eye, o.mEye)
    SYNC_PROP(upper_lid, o.mUpperLid)
    SYNC_PROP(lower_lid, o.mLowerLid)
    SYNC_PROP(upper_lid_blink, o.mUpperLidBlink)
    SYNC_PROP(lower_lid_blink, o.mLowerLidBlink)
END_CUSTOM_PROPSYNC

BEGIN_CUSTOM_PROPSYNC(CharEyes::CharInterestState)
    SYNC_PROP(interest, o.mInterest)
END_CUSTOM_PROPSYNC

BEGIN_PROPSYNCS(CharEyes)
    SYNC_PROP(eyes, mEyes)
    SYNC_PROP(view_direction, mViewDirection)
    SYNC_PROP(interests, mInterests)
    SYNC_PROP(face_servo, mFaceServo)
    SYNC_PROP(camera_weight, mCamWeight)
    SYNC_PROP_BITFIELD(default_interest_categories, mDefaultFilterFlags, 0x685)
    SYNC_PROP(head_lookat, mHeadLookAt)
    SYNC_PROP(max_extrapolation, mMaxExtrapolation)
    SYNC_PROP(disable_eye_dart, sDisableEyeDart)
    SYNC_PROP(disable_eye_jitter, sDisableEyeJitter)
    SYNC_PROP(disable_interest_objects, sDisableInterestObjects)
    SYNC_PROP(disable_procedural_blink, sDisableProceduralBlink)
    SYNC_PROP(disable_eye_clamping, sDisableEyeClamping)
    SYNC_PROP_BITFIELD(interest_filter_testing, mInterestFilterFlags, 0x68E)
    SYNC_PROP(min_target_dist, mMinTargetDist)
    SYNC_PROP(ulid_track_up, mUpperLidTrackUp)
    SYNC_PROP(ulid_track_down, mUpperLidTrackDown)
    SYNC_PROP(llid_track_up, mLowerLidTrackUp)
    SYNC_PROP(llid_track_down, mLowerLidTrackDown)
    SYNC_PROP(llid_track_rotate, mLowerLidTrackRotate)
    SYNC_SUPERCLASS(CharWeightable)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BinStream &operator<<(BinStream &bs, const CharEyes::EyeDesc &desc) {
    bs << desc.mEye;
    bs << desc.mUpperLid;
    bs << desc.mLowerLid;
    bs << desc.mUpperLidBlink;
    bs << desc.mLowerLidBlink;
    return bs;
}

inline BinStream &operator<<(BinStream &bs, const CharEyes::CharInterestState &state) {
    bs << state.mInterest;
    return bs;
}

BinStreamRev &operator>>(BinStreamRev &bs, CharEyes::EyeDesc &desc) {
    bs >> desc.mEye;
    bs >> desc.mUpperLid;
    if (bs.rev > 6)
        bs >> desc.mLowerLid;
    if (bs.rev > 0xF) {
        bs >> desc.mUpperLidBlink;
        bs >> desc.mLowerLidBlink;
    }
    return bs;
}

BinStream &operator>>(BinStream &bs, CharEyes::EyeDesc &desc) {
    bs >> desc.mEye;
    bs >> desc.mUpperLid;
    if (gRev > 6)
        bs >> desc.mLowerLid;
    if (gRev > 0xF) {
        bs >> desc.mUpperLidBlink;
        bs >> desc.mLowerLidBlink;
    }
    return bs;
}

BinStreamRev &operator>>(BinStreamRev &bs, CharEyes::CharInterestState &state) {
    bs >> state.mInterest;
    return bs;
}

BinStream &operator>>(BinStream &bs, CharEyes::CharInterestState &state) {
    bs >> state.mInterest;
    return bs;
}

BEGIN_SAVES(CharEyes)
    SAVE_REVS(18, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(CharWeightable)
    bs << mEyes;
    bs << mInterests;
    bs << mFaceServo;
    bs << mCamWeight;
    bs << mDefaultFilterFlags;
    bs << mViewDirection;
    bs << mHeadLookAt;
    bs << mMaxExtrapolation;
    bs << mMinTargetDist;
    bs << mUpperLidTrackUp;
    bs << mUpperLidTrackDown;
    bs << mLowerLidTrackUp;
    bs << mLowerLidTrackDown;
    bs << mLowerLidTrackRotate;
END_SAVES

BEGIN_LOADS(CharEyes)
    LOAD_REVS(bs)
    int gRev = d.rev;
    int gAltRev = d.altRev;
    ASSERT_REVS(0x12, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    if (gRev > 5)
        LOAD_SUPERCLASS(CharWeightable)
    if (gRev > 4)
        d >> mEyes;
    else {
        ObjPtrList<CharLookAt> pList(this, kObjListNoNull);
        d.stream >> pList;
        mEyes.resize(pList.size());
        int idx = 0;
        for (ObjPtrList<CharLookAt>::iterator it = pList.begin(); it != pList.end();
             ++it) {
            mEyes[idx].mEye = *it;
            mEyes[idx].mUpperLid = 0;
            mEyes[idx].mLowerLid = 0;
            mEyes[idx].mUpperLidBlink = 0;
            mEyes[idx].mLowerLidBlink = 0;
            idx++;
        }
    }
    if (gRev > 2 && gRev < 5) {
        ObjPtr<RndTransformable> tPtr(this);
        d.stream >> tPtr;
    }
    mInterests.clear();
    if (gRev > 3 && gRev <= 8) {
        ObjPtr<RndTransformable> tPtr(this);
        int cnt;
        d.stream >> cnt;
        for (int i = 0; i < cnt; i++) {
            d.stream >> tPtr;
            int x;
            d.stream >> x;
        }
    } else if (gRev > 8)
        d >> mInterests;
    if (gRev > 4)
        d.stream >> mFaceServo;
    else
        mFaceServo = 0;
    if (gRev > 7)
        d.stream >> mCamWeight;
    if (gRev > 9)
        d.stream >> mDefaultFilterFlags;
    if (gRev > 10)
        d.stream >> mViewDirection;
    if (gRev > 0xB)
        d.stream >> mHeadLookAt;
    if (gRev > 0xC)
        d.stream >> mMaxExtrapolation;
    if (gRev > 0xD)
        d.stream >> mMinTargetDist;
    if (gRev > 0xE) {
        d.stream >> mUpperLidTrackUp;
        d.stream >> mUpperLidTrackDown;
        d.stream >> mLowerLidTrackUp;
        if (gRev < 0x11) {
            int x, y;
            d.stream >> x;
            d.stream >> mLowerLidTrackDown;
            d.stream >> y;
        } else
            d.stream >> mLowerLidTrackDown;
    }
    if (gRev > 0x11)
        d >> mLowerLidTrackRotate;
END_LOADS

BEGIN_COPYS(CharEyes)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(CharWeightable)
    CREATE_COPY(CharEyes)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mEyes)
        COPY_MEMBER(mInterests)
        COPY_MEMBER(mFaceServo)
        COPY_MEMBER(mLastFacing)
        COPY_MEMBER(mLastLook)
        COPY_MEMBER(mCamWeight)
        COPY_MEMBER(mDefaultFilterFlags)
        COPY_MEMBER(mViewDirection)
        COPY_MEMBER(mHeadLookAt)
        COPY_MEMBER(mMaxExtrapolation)
        COPY_MEMBER(mMinTargetDist)
        COPY_MEMBER(mUpperLidTrackUp)
        COPY_MEMBER(mUpperLidTrackDown)
        COPY_MEMBER(mLowerLidTrackUp)
        COPY_MEMBER(mLowerLidTrackDown)
        COPY_MEMBER(mLowerLidTrackRotate)
    END_COPYING_MEMBERS
END_COPYS

void CharEyes::ForceBlink() {
    if (mHeadIKActive && !mBlinkEnabled) {
        mBlinkEnabled = true;
        mBlinkTimer = TheTaskMgr.Seconds(TaskMgr::kRealTime);
        mBlinkCount++;
    }
}

void CharEyes::SetEnableBlinks(bool b1, bool b2) {
    mHeadIKActive = b1;
    if (!b2 || b1 || !mBlinkEnabled || !mFaceServo)
        return;

    mFaceServo->SetProceduralBlinkWeight(0.0f);
    mBlinkEnabled = false;
    mTarget = mHeadForward;
}

bool CharEyes::SetFocusInterest(CharInterest *interest, int i) {
    if (mFocusInterest && mFocusTimer > i)
        return false;

    bool changed = interest != mFocusInterest;
    mFocusInterest = interest;
    mFocusTimer = i;
    if (changed)
        mNeedRecalc = true;
    if (!mFocusInterest)
        mFocusTimer = -1;

    return true;
}

CharInterest *CharEyes::GetCurrentInterest() {
    if (mFocusInterest)
        return mFocusInterest;
    if (mCurrentInterest)
        return mCurrentInterest;
    return 0;
}

void CharEyes::ToggleInterestsDebugOverlay() {
    if (mEyeStatusOverlay)
        mEyeStatusOverlay->SetShowing(!mEyeStatusOverlay->Showing());
}

bool CharEyes::IsHeadIKWeightIncreasing() {
    if (mHeadLookAt) {
        float weight = mHeadLookAt->Weight();
        return (weight > 0 && weight - mDartTimer > 0);
    }
    return false;
}

RndTransformable *CharEyes::GetHead() {
    if (mViewDirection)
        return mViewDirection;
    else if (!mEyes.empty() && mEyes[0].mEye) {
        RndTransformable *src = mEyes[0].mEye->GetSource();
        if (src)
            return src->TransParent();
    }
    return 0;
}

RndTransformable *CharEyes::GetTarget() {
    if (mEyes.empty() || !mEyes[0].mEye)
        return nullptr;
    else {
        return mEyes[0].mEye->mTarget;
    }
}

void CharEyes::ClearAllInterestObjects() { mInterests.clear(); }

void CharEyes::ListPollChildren(std::list<RndPollable *> &plist) const {
    for (ObjVector<EyeDesc>::const_iterator it = mEyes.begin(); it != mEyes.end(); ++it) {
        plist.push_back((*it).mEye);
    }
}

void CharEyes::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    for (ObjVector<CharInterestState>::iterator it = mInterests.begin();
         it != mInterests.end();
         ++it) {
        auto interestDir = it->mInterest->Dir();
        if (it->mInterest && Dir() == interestDir) {
            changedBy.push_back(it->mInterest);
        }
    }
    if (!mEyes.empty()) {
        changedBy.push_back(GetHead());
        change.push_back(GetTarget());
    }
    if (mHeadLookAt)
        changedBy.push_back(mHeadLookAt);
    if (mFaceServo)
        changedBy.push_back(mFaceServo);
}

void CharEyes::DartUpdate() {
    static DataNode &dartCheat = DataVariable("cheat.disable_eye_darts");
    if (sDisableEyeDart || dartCheat.Int(NULL) != 0)
        return;
    mDartInterval -= TheTaskMgr.DeltaSeconds();
    if (mDartEnabled) {
        if (mDartInterval < 0) {
            mEyeClampCount--;
            if (mEyeClampCount < 0) {
                mDartEnabled = false;
                mDartInterval = RandomFloat(
                    mData.mMinSecsBetweenSequences,
                    mData.mMaxSecsBetweenSequences
                );
            } else {
                mDartInterval = RandomFloat(
                    mData.mMinSecsBetweenDarts,
                    mData.mMaxSecsBetweenDarts
                );
                *(Vector3 *)&mCurrentDartOffsetX = GenerateDartOffset();
            }
        }
    } else if (mDartInterval < 0 && EyesOnTarget(mData.mOnTargetAngleThresh)
               && !mBlinkEnabled) {
        mDartEnabled = true;
        mEyeClampCount = RandomInt(
            mData.mMinDartsPerSequence,
            mData.mMaxDartsPerSequence
        );
        mDartInterval = RandomFloat(
            mData.mMinSecsBetweenDarts,
            mData.mMaxSecsBetweenDarts
        );
        *(Vector3 *)&mCurrentDartOffsetX = GenerateDartOffset();
    }
}

bool CharEyes::CharInterestState::IsInRefractoryPeriod() {
    if (!mInterest || mRefractoryTime < 0)
        return false;
    else {
        float secs = TheTaskMgr.Seconds(TaskMgr::kRealTime) - mRefractoryTime;
        if (secs < mInterest->mRefractoryPeriod)
            return true;
        else
            return false;
    }
}

float CharEyes::CharInterestState::RefractoryTimeRemaining() {
    if (!mInterest || mRefractoryTime < 0)
        return 0.0f;
    else {
        float secs = TheTaskMgr.Seconds(TaskMgr::kRealTime) - mRefractoryTime;
        if (secs < mInterest->mRefractoryPeriod)
            return mInterest->mRefractoryPeriod - secs;
        else
            return 0.0f;
    }
}

void CharEyes::AddInterestObject(CharInterest *interest) {
    if (interest) {
        CharInterestState state(this);
        state.mInterest = interest;
        mInterests.push_back(state);
    }
}

bool CharEyes::EyesOnTarget(float f) {
    for (ObjVector<EyeDesc>::iterator it = mEyes.begin(); it != mEyes.end(); ++it) {
        CharLookAt *eye = it->mEye;
        if (eye) {
            RndTransformable *src = eye->GetSource();
            if (src) {
                Vector3 diff;
                Subtract(mTarget, src->WorldXfm().v, diff);
                Vector3 fwd(src->WorldXfm().m.y);
                Vector3 diff2d(diff);
                diff2d.z = 0;
                fwd.z = 0;
                float dot = Dot(fwd, diff2d);
                float angle = std::acos(Clamp<float>(-1, 1, dot / (Length(fwd) * Length(diff2d))));
                if (angle * 57.295776f > f) {
                    return false;
                }
            }
        }
    }
    return true;
}

void CharEyes::EnforceMinimumTargetDistance(
    const Vector3 &v1, const Vector3 &v2, Vector3 &vout
) {
    Vector3 diff;
    Subtract(v2, v1, diff);
    float vlen = Length(diff);
    mBlinkActive = false;
    float minDist;
    if (mCurrentInterest && mCurrentInterest->mOverridesMinTargetDist)
        minDist = mCurrentInterest->mMinTargetDistOverride;
    else
        minDist = mMinTargetDist;
    if (vlen < minDist) {
        Vector3 scaled;
        NormalizeScale(diff, minDist, scaled);
        Add(v1, scaled, vout);
        mBlinkActive = true;
    }
}

Vector3 CharEyes::GenerateDartOffset() {
    Vector3 vout;
    float start = mData.mMinRadius;
    float end = mData.mMaxRadius;
    if (mData.mScaleWithDistance && mData.mReferenceDistance > 0.1f) {
        const Vector3 &v = GetHead()->WorldXfm().v;
        Vector3 diff(mTarget.x - v.x, mTarget.y - v.y, mTarget.z - v.z);
        float len = Length(diff);
        start *= len / mData.mReferenceDistance;
        end *= len / mData.mReferenceDistance;
    }
    float mult = RandomFloat(0, 1) > 0.5f ? 1.0f : -1.0f;
    vout[0] = RandomFloat(start, end) * mult;
    mult = RandomFloat(0, 1) > 0.5f ? 1.0f : -1.0f;
    vout[1] = RandomFloat(start, end) * mult;
    mult = RandomFloat(0, 1) > 0.5f ? 1.0f : -1.0f;
    vout[2] = RandomFloat(start, end) * mult;
    return vout;
}

bool CharEyes::Replace(ObjRef *ref, Hmx::Object *obj) {
    EyeDesc *eyeEnd = mEyes.end();
    EyeDesc *eyeBegin = mEyes.begin();
    int eyeCount = (int)((char *)eyeEnd - (char *)eyeBegin) / (int)sizeof(EyeDesc);
    if (eyeCount != 0) {
        int eyeOff = (int)((char *)ref - (char *)eyeBegin);
        if (eyeOff >= 0) {
            int eyeTotal = eyeCount * (int)sizeof(EyeDesc);
            if ((unsigned)eyeOff < (unsigned)eyeTotal) {
                int eyeIdx = eyeOff / (int)sizeof(EyeDesc);
                if (eyeOff == eyeIdx * (int)sizeof(EyeDesc)) {
                    EyeDesc *desc = eyeBegin + eyeIdx;
                    if (desc != mEyes.end()) {
                        if (!desc->mEye.SetObj(obj))
                            mEyes.erase(mEyes.begin() + eyeIdx);
                        return true;
                    }
                }
            }
        }
    }
    CharInterestState *stateEnd = mInterests.end();
    CharInterestState *stateBegin = mInterests.begin();
    int stateCount = (int)((char *)stateEnd - (char *)stateBegin) / (int)sizeof(CharInterestState);
    if (stateCount != 0) {
        int stateOff = (int)((char *)ref - (char *)stateBegin);
        if (stateOff >= 0) {
            int stateTotal = stateCount * (int)sizeof(CharInterestState);
            if ((unsigned)stateOff < (unsigned)stateTotal) {
                int stateIdx = stateOff / (int)sizeof(CharInterestState);
                if (stateOff == stateIdx * (int)sizeof(CharInterestState)) {
                    CharInterestState *state = stateBegin + stateIdx;
                    if (state != mInterests.end()) {
                        if (!state->mInterest.SetObj(obj))
                            mInterests.erase(mInterests.begin() + stateIdx);
                        return true;
                    }
                }
            }
        }
    }
    return CharWeightable::Replace(ref, obj);
}

void CharEyes::NextLook() {
    auto& _ref0 = mTarget;
    Vector3 oldTarget = _ref0;

    RndTransformable *head = GetHead();
    const Transform &headXfm = head->WorldXfm();

    Vector3 facingDir(headXfm.m.y);
    Normalize(facingDir, facingDir);

    if (mFocusInterest) {
        _ref0 = mFocusInterest->WorldXfm().v;
        mCurrentInterest = mFocusInterest;
        const CharEyeDartRuleset *dartOverride = mCurrentInterest->GetDartRulesetOverride();
        if (dartOverride) {
            memcpy(&mData, &dartOverride->mData, sizeof(mData));
        } else {
            mData.ClearToDefaults();
        }
    } else {
        const Vector3 &lastFacing = mLastFacing;
        float dz = (facingDir.z - lastFacing.z) * 45.0f;
        float dx = (facingDir.x - lastFacing.x) * 45.0f;
        float dy = (facingDir.y - lastFacing.y) * 45.0f;

        float extrapMag = std::sqrt(dy * dy + (dx * dx + dz * dz));
        float maxExtrap = std::tan(mMaxExtrapolation * 0.017453292f);

        if (extrapMag > maxExtrap) {
            float scale = maxExtrap / extrapMag;
            dx = scale * dx;
            dy = dy * scale;
            dz = dz * scale;
        }

        float newFacingX = facingDir.x + dx;
        float newFacingY = dy + facingDir.y;
        float newFacingZ = dz + facingDir.z;

        float dist = RandomFloat(20.0f, 100.0f);
        dist *= 12.0f;

        float projX = dist * newFacingX;
        float projY = newFacingY * dist;
        float projZ = newFacingZ * dist;

        _ref0.x = headXfm.v.x + projX;
        _ref0.y = projY + headXfm.v.y;
        _ref0.z = headXfm.v.z + projZ;

        auto _tmp0 = Dir();
        RndTransformable *dirTrans = dynamic_cast<RndTransformable *>(_tmp0);
        if (dirTrans) {
            const Vector3 &dirPos = dirTrans->WorldXfm().v;
            if (_ref0.z < dirPos.z) {
                float scale = (dirPos.z - headXfm.v.z) / (_ref0.z - headXfm.v.z);
                float sx = projX * scale;
                float sy = projY * scale;
                float sz = projZ * scale;
                _ref0.x = headXfm.v.x + sx;
                _ref0.y = sy + headXfm.v.y;
                _ref0.z = headXfm.v.z + sz;
            }
        }

        static DataNode &interestCheat = DataVariable("cheat.disable_interest_objects");

        if (mInterests.size() > 0 && !sDisableInterestObjects) {
            if (interestCheat.Int(0) == 0) {
                float maxDistSq = -1.0f;
                float bestScore = maxDistSq;
                for (ObjVector<CharInterestState>::iterator it = mInterests.begin();
                     it != mInterests.end();
                     ++it) {
                    const Vector3 &intPos = it->mInterest->WorldXfm().v;
                    float fy = intPos.y - headXfm.v.y;
                    float fx = intPos.x - headXfm.v.x;
                    float fz = intPos.z - headXfm.v.z;
                    float distSq = (fz * fz + (fx * fx + fy * fy));
                    if (distSq > maxDistSq)
                        maxDistSq = distSq;
                }

                if (maxDistSq > 0.0f) {
                    CharInterestState *bestState = 0;
                    Vector3 targetDir;
                    Subtract(_ref0, headXfm.v, targetDir);
                    Normalize(targetDir, targetDir);

                    float inverseDist = 1.0f / maxDistSq;

                    for (ObjVector<CharInterestState>::iterator it = mInterests.begin();
                         it != mInterests.end();
                         ++it) {
                        if (it->mInterest != mCurrentInterest) {
                            if (!it->IsInRefractoryPeriod()) {
                                float score = it->mInterest->ComputeScore(
                                    headXfm.m.y,
                                    headXfm.v,
                                    targetDir,
                                    inverseDist,
                                    mInterestFilterFlags,
                                    mDefaultFilterFlags == mInterestFilterFlags
                                );
                                if (score >= 0.0f && score > bestScore) {
                                    bestScore = score;
                                    bestState = &*it;
                                }
                            }
                        }
                    }

                    if (bestState) {
                        _ref0 = bestState->mInterest->WorldXfm().v;
                        mCurrentInterest = bestState->mInterest;
                        const CharEyeDartRuleset *dartOverride =
                            mCurrentInterest->GetDartRulesetOverride();
                        if (dartOverride) {
                            memcpy(&mData, &dartOverride->mData, sizeof(mData));
                        } else {
                            mData.ClearToDefaults();
                        }
                        bestState->mRefractoryTime =
                            TheTaskMgr.Seconds(TaskMgr::kRealTime);
                    } else {
                        mCurrentInterest = 0;
                        mData.ClearToDefaults();
                    }

                    mDartOffset = targetDir;
                    goto stateReset;
                }
            }
        }

        mCurrentInterest = 0;
        mData.ClearToDefaults();
    }

stateReset:
    mLastLook = 0.0f;
    mAvDelta = 0.0f;
    mEnabled = false;
    mNeedRecalc = false;
    mLastCang = 1e30f;
    mDartEnabled = false;
    mDartInterval = 0.2f;
    mEyeClampCount = -1;

    static DataNode &blinkCheat = DataVariable("cheat.disable_procedural_blinks");

    if (!sDisableProceduralBlink && !blinkCheat.NotNull() && !mBlinkEnabled && mFaceServo
        && mBlinkCount < 25
        && TheTaskMgr.Seconds(TaskMgr::kRealTime) - mLowerBlinkAngle > 0.6f
        && mLastBlinkWeight < 0.5f) {
        Vector3 oldDir(
            oldTarget.x - headXfm.v.x,
            oldTarget.y - headXfm.v.y,
            oldTarget.z - headXfm.v.z
        );
        Normalize(oldDir, oldDir);

        Vector3 newDir(
            _ref0.x - headXfm.v.x,
            _ref0.y - headXfm.v.y,
            _ref0.z - headXfm.v.z
        );
        Normalize(newDir, newDir);

        auto _tmp1 = Dot(newDir, oldDir);
        if (_tmp1 < 0.984808f) {
            ForceBlink();
            mHeadForward = _ref0;
            _ref0 = oldTarget;
        }
    }
}

void CharEyes::LidTrackAndClampingUpdate(EyeDesc &desc, float blinkWeight) {
    if (DataVariable("no_lids").Int(0))
        return;
    if (!mFaceServo)
        return;
    if (!mFaceServo->mClips)
        return;
    if (!mFaceServo->mBaseClip)
        return;

    RndTransformable *source = desc.mEye->GetSource();
    if (!source)
        return;

    RndTransformable *lowerLid = desc.mLowerLid;
    RndTransformable *upperLid = desc.mUpperLid;

    float dist = -1.0f;
    float maxDot = dist;
    if (lowerLid) {
        Vector3 srcPos = source->WorldXfm().v;
        Vector3 lidPos = lowerLid->WorldXfm().v;
        float dx = lidPos.x - srcPos.x;
        float dy = lidPos.y - srcPos.y;
        float dz = lidPos.z - srcPos.z;
        dist = std::sqrt((dy * dy + (dx * dx + dz * dz)));
    }

    float eyeRot = (1.0f - blinkWeight) * source->LocalXfm().m.y.x;
    float negEyeRot = -eyeRot;

    if (upperLid) {
        float angle =
            (0.0f <= eyeRot ? mUpperLidTrackUp : mUpperLidTrackDown) * negEyeRot;
        bool isNaN = (angle != angle);
        if (!isNaN) {
            Transform &xfm = upperLid->DirtyLocalXfm();
            RotateAboutZ(xfm.m, angle, xfm.m);
        }
    }

    if (lowerLid) {
        if (mLowerLidTrackRotate) {
            float angle =
                (eyeRot >= 0.0f ? mLowerLidTrackUp : mLowerLidTrackDown) * negEyeRot;
            bool isNaN = (angle != angle);
            if (!isNaN) {
                Transform &xfm = lowerLid->DirtyLocalXfm();
                RotateAboutZ(xfm.m, angle, xfm.m);
            }
        } else {
            float offset =
                (eyeRot >= 0.0f ? mLowerLidTrackUp : mLowerLidTrackDown) * eyeRot;
            lowerLid->DirtyLocalXfm().v.x += offset;
        }
    }

    RndTransformable *lowerBlink = desc.mLowerLidBlink;
    RndTransformable *upperBlink = desc.mUpperLidBlink;
    if (lowerBlink && upperBlink) {
        Vector3 sourcePos = source->WorldXfm().v;
        Vector3 upperBlinkPos = upperBlink->WorldXfm().v;
        Vector3 lowerBlinkPos = lowerBlink->WorldXfm().v;

        Vector3 upperDir(
            upperBlinkPos.x - sourcePos.x,
            upperBlinkPos.y - sourcePos.y,
            upperBlinkPos.z - sourcePos.z
        );
        Normalize(upperDir, upperDir);

        Vector3 lowerDir(
            lowerBlinkPos.x - sourcePos.x,
            lowerBlinkPos.y - sourcePos.y,
            lowerBlinkPos.z - sourcePos.z
        );
        Normalize(lowerDir, lowerDir);

        Vector3 cross;
        Cross(lowerDir, upperDir, cross);

        const Transform &srcXfm = source->WorldXfm();
        bool lidsOK =
            cross.x * srcXfm.m.x.x + cross.y * srcXfm.m.x.y + cross.z * srcXfm.m.x.z
            <= 0.0f;

        if (!sDisableEyeClamping) {
            DataNode &clampCheat = DataVariable("disable_clamping");
            if (!clampCheat.Int(0) && !lidsOK) {
                float midX =
                    (upperBlinkPos.x - lowerBlinkPos.x) * 0.5f + lowerBlinkPos.x;
                float midY =
                    (upperBlinkPos.y - lowerBlinkPos.y) * 0.5f + lowerBlinkPos.y;
                float midZ =
                    (upperBlinkPos.z - lowerBlinkPos.z) * 0.5f + lowerBlinkPos.z;

                float clampOffX = midX - lowerBlinkPos.x;
                float clampOffY = midY - lowerBlinkPos.y;
                float clampOffZ = midZ - lowerBlinkPos.z;

                Vector3 newLowerPos = lowerLid->WorldXfm().v;
                newLowerPos.x += clampOffX;
                newLowerPos.y += clampOffY;
                newLowerPos.z += clampOffZ;
                lowerLid->SetWorldPos(newLowerPos);

                const Vector3 &ulidPos = upperLid->WorldXfm().v;
                Vector3 origDir(
                    upperBlinkPos.x - ulidPos.x,
                    upperBlinkPos.y - ulidPos.y,
                    upperBlinkPos.z - ulidPos.z
                );
                Normalize(origDir, origDir);

                const Vector3 &ulidPos2 = upperLid->WorldXfm().v;
                Vector3 newDir(
                    midX - ulidPos2.x,
                    midY - ulidPos2.y,
                    midZ - ulidPos2.z
                );
                Normalize(newDir, newDir);

                float dot = Dot(newDir, origDir);
                maxDot = maxDot - dot >= 0.0f ? maxDot : dot;
                float clamped = maxDot - 1.0f >= 0.0f ? 1.0f : maxDot;
                float angle = std::acos(clamped);

                bool isNaN = (angle != angle);
                if (!isNaN) {
                    Transform &xfm = upperLid->DirtyLocalXfm();
                    RotateAboutZ(xfm.m, -angle, xfm.m);
                }
            }
        }

        DataNode &drawCheat = DataVariable("debug_clamping");
        if (drawCheat.Int(0)) {
            RndGraph *graph = RndGraph::GetOneFrame();

            if (!(lidsOK)) {
                graph->AddSphere(
                    upperBlinkPos, 0.05f, Hmx::Color(1.0f, 0.0f, 0.0f, 1.0f)
                );
            } else {
                graph->AddSphere(
                    upperBlinkPos, 0.05f, Hmx::Color(0.0f, 0.0f, 1.0f, 1.0f)
                );
            }
            if (!(lidsOK)) {
                graph->AddSphere(
                    lowerBlinkPos, 0.05f, Hmx::Color(1.0f, 0.0f, 0.0f, 1.0f)
                );
            } else {
                graph->AddSphere(
                    lowerBlinkPos, 0.05f, Hmx::Color(0.0f, 0.0f, 1.0f, 1.0f)
                );
            }
            graph->AddSphere(sourcePos, 0.05f, Hmx::Color(0.0f, 0.0f, 1.0f, 1.0f));

            Hmx::Color cyanColor(0.0f, 1.0f, 1.0f, 1.0f);
            graph->AddLine(sourcePos, upperBlinkPos, cyanColor, false);
            graph->AddLine(sourcePos, lowerBlinkPos, cyanColor, false);

            Normalize(cross, cross);
            Vector3 normalEnd(
                cross.x + sourcePos.x, cross.y + sourcePos.y, cross.z + sourcePos.z
            );
            if (!(lidsOK)) {
                graph->AddLine(
                    sourcePos, normalEnd, Hmx::Color(1.0f, 0.0f, 0.0f, 1.0f), false
                );
            } else {
                graph->AddLine(
                    sourcePos, normalEnd, Hmx::Color(0.0f, 1.0f, 0.0f, 1.0f), false
                );
            }

            const Transform &srcXfm2 = source->WorldXfm();
            Vector3 facingEnd(
                srcXfm2.m.x.x + sourcePos.x,
                srcXfm2.m.x.y + sourcePos.y,
                srcXfm2.m.x.z + sourcePos.z
            );
            graph->AddLine(
                sourcePos, facingEnd, Hmx::Color(1.0f, 1.0f, 0.0f, 1.0f), false
            );

            if (!lidsOK) {
                Vector3 mid2(
                    (upperBlinkPos.x - lowerBlinkPos.x) * 0.5f + lowerBlinkPos.x,
                    (upperBlinkPos.y - lowerBlinkPos.y) * 0.5f + lowerBlinkPos.y,
                    (upperBlinkPos.z - lowerBlinkPos.z) * 0.5f + lowerBlinkPos.z
                );
                graph->AddSphere(
                    mid2, 0.03125f, Hmx::Color(1.0f, 0.0f, 1.0f, 1.0f)
                );
            }
        }
    }

    if (!DataVariable("disable_llidnorm").Int(0) && !mLowerLidTrackRotate && 0.0f < dist) {
        Vector3 srcPos = source->WorldXfm().v;
        Vector3 lidPos = lowerLid->WorldXfm().v;
        Vector3 dir(
            lidPos.x - srcPos.x, lidPos.y - srcPos.y, lidPos.z - srcPos.z
        );
        Normalize(dir, dir);
        Vector3 clampedPos(
            dir.x * dist + srcPos.x, dir.y * dist + srcPos.y, dir.z * dist + srcPos.z
        );
        lowerLid->SetWorldPos(clampedPos);
    }
}

void CharEyes::ProceduralBlinkUpdate() {
    static DataNode &disableCheat = DataVariable("cheat.disable_procedural_blinks");

    if (sDisableProceduralBlink)
        return;
    if (disableCheat.Int(0))
        return;
    if (!mHeadIKActive && !mBlinkEnabled)
        return;

    mUpperBlinkAngle = mUpperBlinkAngle - TheTaskMgr.DeltaSeconds();
    if (mUpperBlinkAngle < 0.0f) {
        mBlinkCount = 0;
        mUpperBlinkAngle = 15.0f;
    }

    if (!mFaceServo)
        return;
    if (!mBlinkEnabled)
        return;

    float elapsed = TheTaskMgr.Seconds(TaskMgr::kRealTime) - mBlinkTimer;
    if (elapsed < 0.115f) {
        // Closing phase
        float t = Clamp(0.0f, 1.0f, elapsed * 8.695652f);
        auto blinkWeight = EaseInExp(t);
        mFaceServo->SetProceduralBlinkWeight(blinkWeight);
    } else if (elapsed < 0.3f) {
        // Opening phase
        float t = Clamp(0.0f, 1.0f, 1.0f - (elapsed - 0.115f) * 5.405405f);
        auto blinkWeight = EaseSigmoid(t, 0.0f, 0.0f);
        mFaceServo->SetProceduralBlinkWeight(blinkWeight);
        mTarget = mHeadForward;
    } else {
        // Blink complete
        mFaceServo->SetProceduralBlinkWeight(0.0f);
        mBlinkEnabled = false;
        mTarget = mHeadForward;
    }
}

void CharEyes::Poll() {
    if (mEyes.empty())
        return;

    RndTransformable *head = GetHead();
    if (!head)
        return;

    float dt = TheTaskMgr.DeltaSeconds();
    if (dt < 0.0f) {
        Exit();
        return;
    }

    float camWeight = 0.0f;
    if (mCamWeight) {
        camWeight = mCamWeight->Weight();
    }

    mLastLook += TheTaskMgr.DeltaSeconds();

    float blinkWeight;
    if (mFaceServo) {
        blinkWeight = mFaceServo->BlinkWeightLeft();
    } else {
        blinkWeight = 0.0f;
    }

    bool blinkDetected = false;
    if (blinkWeight < 0.3f) {
        mBlinkDetect = true;
    } else {
        if (mBlinkDetect && mLastBlinkWeight > 0.8f && blinkWeight < mLastBlinkWeight) {
            mBlinkDetect = false;
            mBlinkCount++;
            blinkDetected = true;
            mLowerBlinkAngle = TheTaskMgr.Seconds(TaskMgr::kRealTime);
        }
    }
    mLastBlinkWeight = blinkWeight;

    const Transform &headXfm = head->WorldXfm();
    const Vector3 &headPos = headXfm.v;

    Vector3 targetDir;
    Subtract(mTarget, headPos, targetDir);
    Normalize(targetDir, targetDir);

    Vector3 facingDir(headXfm.m.y);
    Normalize(facingDir, facingDir);

    float cang = Dot(facingDir, targetDir);
    cang = Clamp(-1.0f, 1.0f, cang);

    if (mLastCang != 1e+30f) {
        TheTaskMgr.Seconds(TaskMgr::kRealTime);
        CharInterest *interest = mCurrentInterest;
        mAvDelta = (cang - mLastCang - mAvDelta) * 0.1f + mAvDelta;

        float minLookTime = interest ? interest->mMinLookTime : 1.0f;
        float maxLookTime = interest ? interest->mMaxLookTime : 3.0f;
        float viewAngleCos = interest ? interest->mMaxViewAngleCos : mMaxEyeCang;

        bool canSeeTarget = cang >= viewAngleCos;

        if (mLastLook <= maxLookTime && !mNeedRecalc
            && (mFocusInterest == 0 || interest == (CharInterest *)mFocusInterest
                || ((mLastLook <= 0.4f
                     || !mFocusInterest->IsWithinViewCone(headPos, facingDir))
                    && !IsHeadIKWeightIncreasing()))
            && (!mEnabled || mLastLook <= 0.25f)) {
            if (mLastLook <= minLookTime)
                goto storeState;
            if (!blinkDetected) {
                if (canSeeTarget) {
                    bool anyEyeClamped = false;
                    auto eyesEnd = mEyes.end();
                    for (ObjVector<EyeDesc>::iterator it = mEyes.begin(); it != eyesEnd;
                         ++it) {
                        if (it->mEye && it->mEye->mDisableRoll) {
                            anyEyeClamped = true;
                            break;
                        }
                    }
                    if (!anyEyeClamped)
                        goto storeState;
                }
                if (mAvDelta >= 0.0f)
                    goto storeState;
            }
        }

        if (camWeight == 0.0f) {
            NextLook();
        }
    }

storeState:
    mLastCang = cang;
    mLastFacing = facingDir;

    float headLookWeight = 0.0f;
    if (mHeadLookAt) {
        headLookWeight = mHeadLookAt->Weight();
    }
    mDartTimer = headLookWeight;

    DartUpdate();

    if (mCurrentInterest) {
        if (!mBlinkEnabled) {
            mTarget = mCurrentInterest->WorldXfm().v;
        } else {
            mHeadForward = mCurrentInterest->WorldXfm().v;
        }
        EnforceMinimumTargetDistance(headPos, mTarget, mTarget);
    }

    RndTransformable *eyeTarget = 0;
    if (!mEyes.empty() && mEyes[0].mEye) {
        eyeTarget = mEyes[0].mEye->mTarget;
    }

    if (eyeTarget) {
        float weight = Weight();
        const Vector3 *interpTarget;
        Transform xfm;
        Vector3 localTarget;
        float interpWeight;
        if (camWeight > 0.0f) {
            RndCam *cam = 0;
            if (TheWorld)
                cam = TheWorld->Cam();
            if (!cam)
                cam = RndCam::Current();
            if (!cam)
                cam = TheRnd.GetDefaultCam();
            if (!cam)
                goto skipInterp;
            xfm = eyeTarget->WorldXfm();
            interpTarget = &cam->WorldXfm().v;
            interpWeight = camWeight;
        } else {
            localTarget = mTarget;
            if (mDartEnabled) {
                localTarget.x += mCurrentDartOffsetX;
                localTarget.y += mCurrentDartOffsetY;
                localTarget.z += mCurrentDartOffsetZ;
                EnforceMinimumTargetDistance(headPos, localTarget, localTarget);
            }
            xfm = eyeTarget->WorldXfm();
            interpTarget = &localTarget;
            interpWeight = weight;
        }
        Interp(xfm.v, *interpTarget, interpWeight, xfm.v);
        eyeTarget->SetWorldXfm(xfm);
    }
skipInterp:

    ProceduralBlinkUpdate();

    CharLookAt::sDisableJitter = sDisableEyeJitter;
    for (ObjVector<EyeDesc>::iterator it = mEyes.begin(); it != mEyes.end(); ++it) {
#ifdef HX_NATIVE
        if (!it->mEye) continue;
#endif
        it->mEye->Poll();
        LidTrackAndClampingUpdate(*it, blinkWeight);
    }
    CharLookAt::sDisableJitter = false;

    UpdateOverlay();
}

DataNode CharEyes::OnToggleForceFocus(DataArray *da) {
    if (mFocusInterest)
        SetFocusInterest(0, 0);
    else
        SetFocusInterest(mCurrentInterest, 0);
    return 0;
}

DataNode CharEyes::OnToggleInterestOverlay(DataArray *da) {
    ToggleInterestsDebugOverlay();
    return 0;
}

DataNode CharEyes::OnAddInterest(DataArray *arr) {
    mInterests.push_back(CharInterestState(arr->Obj<CharInterest>(1)));
    return 0;
}

// NormalizeScale body guarded: defining it here causes IPA — the compiler
// sees NormalizeScale doesn't touch r4/r6, so EnforceMinimumTargetDistance
// skips callee-saved GPR saves (r29-r31) that the target uses. Result:
// structurally incompatible prologue (91.3% -> 57.8%). AT_LIMIT.
// Native build: NormalizeScale is provided inline in src/system/math/Vec.h.
