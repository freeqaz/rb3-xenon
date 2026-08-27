#include "world/CameraShot.h"
#include "hamobj/HamWardrobe.h"
#include "math/Interp.h"
#include "math/Mtx.h"
#include "math/Rot.h"
#include "math/Utl.h"
#include "math/Vec.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "obj/PropSync.h"
#include "os/Debug.h"
#include "os/Platform.h"
#include "obj/Task.h"
#include "os/Timer.h"
#include "rndobj/Anim.h"
#include "rndobj/Cam.h"
#include "rndobj/DOFProc.h"
#include "rndobj/Draw.h"
#include "rndobj/MultiMesh.h"
#include "rndobj/MultiMeshProxy.h"
#include "rndobj/Trans.h"
#include "rndobj/Utl.h"
#include "rndobj/VelocityBuffer.h"
#include "utl/BinStream.h"
#include "utl/Loader.h"
#include "utl/Str.h"
#include "utl/Symbol.h"
#include "world/CameraManager.h"
#include "world/Crowd.h"
#include "world/FreeCamera.h"
#include "world/Dir.h"
#include "rndobj/TransProxy.h"
#include "ui/UI.h"
#include "utl/MakeString.h"
#include <cstdlib>
#include <cstring>

#ifdef HX_NATIVE
Hmx::Object *CamShot::sAnimTarget;
#endif

// RB3-360 retail: this TU's Load family reads the archive rev from a file-scope
// static halfword (lbl_82CC7494, `lhz`) that the outer CamShot::Load populates
// once — not from the BinStreamRev member. Mirror that so inner sub-Loads match.
static unsigned short sCamShotRev;

inline float ComputeFOVScale(float fov) {
    return 24.0f / (float(std::tan(fov / 2.0f)) * 2.0f);
}
inline float ScaleToFOV(float scale) {
    return float(std::atan(24.0f / (scale * 2.0f))) * 2.0f;
}

#pragma region AutoPrepTarget

bool AutoPrepTarget::sChanging = false;

AutoPrepTarget::AutoPrepTarget(CamShotFrame &frame)
    : mFrame(&frame), mShot(frame.mCamShot) {
    mShot->StartAnim();
    mOldFilter = mShot->mFilter;
    mOldCamHeight = mShot->mClampHeight;
    mOldZoomFov = mFrame->mZoomFOV;
    mFrame->mZoomFOV = 0;
    mShot->mFilter = 0.0f;
    mShot->mClampHeight = -1.0f;
    mShot->mLastShakeOffset.Set(0.0f, 0.0f, 0.0f);
    mShot->mLastShakeAngOffset.Set(0.0f, 0.0f, 0.0f);
    mShot->mLastDesiredShakeOffset.Set(0.0f, 0.0f, 0.0f);
    mShot->mLastDesiredShakeAngOffset.Set(0.0f, 0.0f, 0.0f);
    sChanging = true;
    mFrame->UpdateTarget();
    mShot->SetFrame(mFrame->mFrame, 1.0f);
}

AutoPrepTarget::~AutoPrepTarget() {
    mShot->SetPos(*mFrame, nullptr);
    mFrame->UpdateTarget();
    mShot->mFilter = mOldFilter;
    mShot->mClampHeight = mOldCamHeight;
    mFrame->mZoomFOV = mOldZoomFov;
    sChanging = false;
    mShot->EndAnim();
}

#pragma endregion
#pragma region CamShotFrame

CamShotFrame::CamShotFrame(Hmx::Object *owner)
    : mDuration(0), mBlend(0), mBlendEase(0), mBlendEaseMode(kBlendEaseInAndOut),
      mFrame(-1), mFOV(1.2217305f), mZoomFOV(0), mShakeNoiseFreq(0), mShakeNoiseAmp(0),
      mShakeMaxAngle(0, 0), mBlurDepth(0.35), mMaxBlur(1), mMinBlur(0),
      mFocusBlurMultiplier(0), mTargets(owner), mParent(owner), mFocalTarget(owner),
      mUseParentRotation(false), mParentFirstFrame(false),
      mCamShot(dynamic_cast<CamShot *>(owner)) {
    mWorldOffset.Reset();
    mScreenOffset.Zero();
    mLastTargetPos.x = kHugeFloat;
}

CamShotFrame::CamShotFrame(Hmx::Object *shotOwner, const CamShotFrame &other)
    : mDuration(other.mDuration), mBlend(other.mBlend), mBlendEase(other.mBlendEase),
      mBlendEaseMode(other.mBlendEaseMode), mFOV(other.mFOV), mZoomFOV(other.mZoomFOV),
      mWorldOffset(other.mWorldOffset), mScreenOffset(other.mScreenOffset),
      mShakeNoiseFreq(other.mShakeNoiseFreq), mShakeNoiseAmp(other.mShakeNoiseAmp),
      mShakeMaxAngle(other.mShakeMaxAngle), mBlurDepth(other.mBlurDepth),
      mMaxBlur(other.mMaxBlur), mMinBlur(other.mMinBlur),
      mFocusBlurMultiplier(other.mFocusBlurMultiplier), mTargets(other.mTargets),
      mParent(other.mParent), mFocalTarget(other.mFocalTarget),
      mUseParentRotation(other.mUseParentRotation), mParentFirstFrame(false) {
    mCamShot = dynamic_cast<CamShot *>(shotOwner);
}

void CamShotFrame::Save(BinStream &bs) const {
    bs << mDuration;
    bs << mBlend;
    bs << mBlendEase;
    bs << mBlendEaseMode;
    bs << mFOV;
    bs << mWorldOffset;
    bs << mScreenOffset;
    bs << mBlurDepth;
    bs << mMaxBlur;
    bs << mMinBlur;
    bs << mFocusBlurMultiplier;
    bs << mTargets;
    bs << mFocalTarget;
    bs << mParent;
    bs << mUseParentRotation;
    bs << mShakeNoiseAmp;
    bs << mShakeNoiseFreq;
    bs << mShakeMaxAngle;
    bs << mZoomFOV;
    bs << mParentFirstFrame;
}

RndTransformable *LoadSubPart(BinStreamRev &d, CamShot *shot) {
    if (sCamShotRev < 0x2B) {
        int dummy;
        d >> dummy;
    }
    String str;
    d >> str;
    Symbol sym;
    d >> sym;
    if (str.empty())
        return 0;
    RndTransformable *foundTrans =
        shot->Dir()->Find<RndTransformable>(str.c_str(), false);
    if (sym.Null()) {
        if (foundTrans)
            return foundTrans;
        MILO_LOG(
            "%s could not find %s, assuming character, attaching to base\n",
            PathName(shot),
            str
        );
    }
    char buf[256];
    strcpy(buf, sym.Str());
    char *buf_ptr = strchr(buf, '.');
    if (buf_ptr)
        *buf_ptr = '\0';
    else if (buf[0] == '\0') {
        strcpy(buf, "base");
    }
    const char *search = MakeString("%s_%s.tp", str, buf);
    RndTransProxy *proxy = shot->Dir()->Find<RndTransProxy>(search, false);
    if (!proxy) {
        proxy = Hmx::Object::New<RndTransProxy>();
        proxy->SetName(search, shot->Dir());
        proxy->SetProxy(dynamic_cast<ObjectDir *>(foundTrans));
        proxy->SetPart(sym);
    }
    return proxy;
}

void CamShotFrame::Load(BinStreamRev &d) {
    d >> mDuration;
    d >> mBlend;
    d >> mBlendEase;
    if (sCamShotRev > 0x2D) {
        d >> (int &)mBlendEaseMode;
    }
    d >> mFOV;
    d >> mWorldOffset;
    Transform zeroXfm;
    zeroXfm.Zero();
    if (zeroXfm == mWorldOffset) {
        mWorldOffset.Reset();
    }
    d >> mScreenOffset;
    d >> mBlurDepth;
    if (sCamShotRev < 0x17) {
        mBlurDepth = 1 - mBlurDepth;
        int x;
        d >> x;
    }
    if (sCamShotRev > 0x17) {
        d >> mMaxBlur;
    } else {
        mMaxBlur = 1;
    }
    if (sCamShotRev > 0x1C) {
        d >> mMinBlur;
    } else {
        mMinBlur = 0;
    }
    if (sCamShotRev > 0x14) {
        d >> mFocusBlurMultiplier;
    } else {
        mFocusBlurMultiplier = 0;
    }
    if (sCamShotRev < 0x17) {
        int x;
        d >> x;
    }
    if (sCamShotRev > 0x2B) {
        d >> mTargets;
    } else {
        int count;
        d >> count;
        mTargets.clear();
        for (int i = 0; i < count; i++) {
            RndTransformable *t = LoadSubPart(d, mCamShot);
            if (t) {
                mTargets.push_back(t);
            }
        }
    }
    if (sCamShotRev > 0x1A) {
        if (sCamShotRev > 0x2B) {
            d >> mFocalTarget;
        } else {
            mFocalTarget = LoadSubPart(d, mCamShot);
        }
    }
    if (sCamShotRev > 0x2B) {
        d >> mParent;
    } else {
        mParent = LoadSubPart(d, mCamShot);
    }
    d >> mUseParentRotation;
    if (sCamShotRev > 0x11) {
        d >> mShakeNoiseAmp;
        d >> mShakeNoiseFreq;
        d >> mShakeMaxAngle;
    }
    if (sCamShotRev > 0x15) {
        d >> mZoomFOV;
    }
    if (sCamShotRev > 0x28) {
        d >> mParentFirstFrame;
    }
}

BinStreamRev &operator>>(BinStreamRev &d, CamShotFrame &csf) {
    csf.Load(d);
    return d;
}

bool CamShotFrame::SameTargets(const CamShotFrame &other) const {
    if (mTargets.size() != other.mTargets.size())
        return false;
    FOREACH (it, mTargets) {
        ObjPtrList<RndTransformable>::iterator otherIt = other.mTargets.begin();
        for (; otherIt != other.mTargets.end(); ++otherIt) {
            if (*it == *otherIt)
                break;
        }
        if (otherIt == other.mTargets.end())
            return false;
    }
    return true;
}

void CamShotFrame::GetCurrentTargetPosition(Vector3 &v) const {
    v.Zero();
    int count = 0;
    ObjPtrList<RndTransformable>::iterator it = mTargets.begin();
    if (it != mTargets.end()) {
        do {
            RndTransformable *cur = *it;
            if (cur) {
                count++;
                Add(v, cur->WorldXfm().v, v);
            }
            ++it;
        } while (it != mTargets.end());
    }
    if (count > 0)
        v *= (1.0f / (float)count);
}

void CamShotFrame::ApplyScreenOffset(Transform &xfm, RndCam *cam) const {
    if (HasTargets()) {
#ifdef HX_NATIVE
        // Guard: LookAt normalizes direction; zero distance produces NaN
        Vector3 delta;
        Subtract(mLastTargetPos, xfm.v, delta);
        float distSq = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
        if (distSq > 1e-6f)
#endif
            xfm.LookAt(mLastTargetPos, xfm.m.z);
    }
    Vector3 v;
    Subtract(xfm.v, mLastTargetPos, v);
    float length = std::sqrt(v.y * v.y + v.z * v.z + v.x * v.x);
    Vector3 vother(
        -(mScreenOffset.x / cam->LocalProjectXfm().m.x.x) * length,
        0,
        (mScreenOffset.y / cam->LocalProjectXfm().m.z.y) * length
    );
    Multiply(vother, xfm, xfm.v);
}

void CamShotFrame::UpdateTarget() const {
    CamShotFrame *me = const_cast<CamShotFrame *>(this);
    GetCurrentTargetPosition(me->mLastTargetPos);
    if (mParent) {
        me->mTargetXfm = mParent->WorldXfm();
    }
}

bool CamShotFrame::OnSyncTargets(
    ObjPtrList<RndTransformable> &transList,
    DataNode &node,
    DataArray *prop,
    int i,
    PropOp op
) {
    bool synced;
    if (op != kPropGet && op != kPropSize) {
        AutoPrepTarget target(*this);
        synced = PropSync(transList, node, prop, i, op);
    } else
        synced = PropSync(transList, node, prop, i, op);
    return synced;
}

bool CamShotFrame::OnSyncParent(
    ObjPtr<RndTransformable> &parent, DataNode &node, DataArray *prop, int i, PropOp op
) {
    bool synced;
    if (op != kPropGet) {
        AutoPrepTarget target(*this);
        synced = PropSync(parent, node, prop, i, op);
    } else
        synced = PropSync(parent, node, prop, i, op);
    return synced;
}

bool CamShotFrame::HasTargets() const {
    FOREACH (it, mTargets) {
        if (*it)
            return true;
    }
    return false;
}

void CamShotFrame::BuildTransform(RndCam *cam, Transform &tf, bool b3) const {
    CamShotFrame *me = const_cast<CamShotFrame *>(this);

    // NB(rb3-xenon, idx233): declared here (not just before `if (mParent)`)
    // to match retail's register allocation -- retail's Ghidra decomp fetches
    // mParent as the very first field read after `this`, matching rb3-Wii's
    // BuildTransform which declares `parent` right after `me`.
    RndTransformable *parent = mParent;

    Vector3 targetPos;
    GetCurrentTargetPosition(targetPos);

#ifdef HX_NATIVE
    // Guard: if targetPos is zero (no valid targets) or camera projection
    // hasn't been initialized, WorldToScreen + subsequent math produces NaN/inf.
    // Skip target-dependent filtering and use raw offset.
    if (targetPos.x == 0.0f && targetPos.y == 0.0f && targetPos.z == 0.0f
        && mTargets.empty()) {
        me->mLastTargetPos = targetPos;
        tf = mWorldOffset;
        // (X2) no `Multiply(tf, mCamShot->WorldXfm(), tf)` -- retail CamShot has
        // no WorldXfm(); see the note at the end of BuildTransform.
        return;
    }
#endif

    Vector2 screenPos;
    cam->WorldToScreen(targetPos, screenPos);

#ifdef HX_NATIVE
    // Guard: WorldToScreen may produce NaN when projection is degenerate
    if (screenPos.x != screenPos.x || screenPos.y != screenPos.y) {
        screenPos.Set(0.0f, 0.0f);
    }
#endif

    screenPos.x = -((mScreenOffset.x + 1.0f) * 0.5f - screenPos.x);
    screenPos.y = -((1.0f - mScreenOffset.y) * 0.5f - screenPos.y);

    float dist = std::sqrt(screenPos.y * screenPos.y + screenPos.x * screenPos.x);
    dist = (1.0f - dist) < 0.0f ? 1.0f : dist;
    float filterDist = dist * mCamShot->mFilter;

    if (mLastTargetPos.x == kHugeFloat) {
        filterDist = 0.0f;
    } else {
        float dt = TheTaskMgr.DeltaSeconds();
        if (dt == 0.0f) {
            filterDist = 1e-11f;
        }
        if (filterDist != 0.0f) {
            ::Interp(mLastTargetPos, targetPos, filterDist, targetPos);
        }
    }
    me->mLastTargetPos = targetPos;

    MILO_ASSERT(mLastTargetPos.x != kHugeFloat, 0x7ce);

    if (mCamShot->mPath) {
        float pathFrame = mCamShot->mPathFrame;
        if (pathFrame < 0.0f) {
            if (0.0f < mCamShot->mDuration) {
                pathFrame = mCamShot->GetFrame() / mCamShot->mDuration;
            } else {
                pathFrame = 0.0f;
            }
        }
        RndTransAnim *path = mCamShot->mPath;
        float endFrame = path->EndFrame();
        path->MakeTransform(endFrame * pathFrame, tf, true, 1.0f);
        Multiply(mCamShot->mKeyframes[0].mWorldOffset, tf, tf);
    } else {
        tf = mWorldOffset;
    }

    if (parent) {
        bool useLiveParent;
        if (!mParentFirstFrame || mCamShot->mShotStarted) {
            useLiveParent = true;
        } else {
            useLiveParent = false;
        }

        const Transform *parentXfm;
        if (useLiveParent) {
            parentXfm = &parent->WorldXfm();
        } else {
            parentXfm = &mTargetXfm;
        }

        Transform localParent;
        localParent = *parentXfm;

        if (useLiveParent) {
            if (mCamShot->mFilter != 0.0f) {
                ::Interp(me->mTargetXfm.m, localParent.m, filterDist, localParent.m);
                ::Interp(me->mTargetXfm.v, localParent.v, filterDist, localParent.v);
            }
            me->mTargetXfm = localParent;
        }

        if (mUseParentRotation) {
            Multiply(tf, localParent, tf);
        } else {
            Add(tf.v, localParent.v, tf.v);
        }

        if (0.0f < mCamShot->mClampHeight && mTargets.size() == 1) {
            RndTransformable *target = mTargets.front();
            if (target) {
                float clampZ = mCamShot->mClampHeight + target->WorldXfm().v.z;
                if (clampZ > tf.v.z) {
                    tf.v.z = clampZ;
                }
            }
        }
    }

    // NB(rb3-xenon, X2): DC3's CamShot inherits RndTransformable and has a real
    // WorldXfm(); retail CamShot is `class CamShot : public RndAnimatable`
    // (CameraShot.h:144) and has none. The DC3-shaped
    // `Multiply(tf, mCamShot->WorldXfm(), tf)` that used to sit here was inside
    // `#ifdef HX_NATIVE`, i.e. it was live on the ONE side that could not
    // compile it -- latent only because world/ was not in the native build. It
    // is deleted rather than re-guarded: the identity world transform is the
    // correct native behaviour for a CamShot that has no transform at all, and
    // rb3-Wii's BuildTransform likewise has no such multiply. X360-neutral (the
    // removed text was already excluded when HX_NATIVE is undefined).

    // NB(rb3-xenon, idx233): retail RB3's BuildTransform has NO
    // ApplyDynamicOffsetPreLookAt/PostLookAt calls here -- both virtuals are a
    // DC3-era addition (absent from the rb3-Wii oracle's CamShot entirely, and
    // still empty-bodied stubs in CameraShot.h). Retail's Ghidra decomp goes
    // straight from the `if (mParent) {...}` block to the `if (b3)` block with
    // zero intervening calls -- same pattern as the ApplyFinalCamTransform fix
    // above in Interp().
    if (b3) {
        // NB(rb3-xenon, idx233): manually inlined -- ApplyScreenOffset has
        // exactly one call site and retail's Ghidra decomp shows its body
        // inlined here directly (HasTargets loop + LookAt + subtract/sqrt/
        // multiply, no `bl`), but our /Ob2 auto-inline heuristic left it as
        // a real out-of-line call, which was the dominant divergence (a
        // 39-instruction delete-only cluster).
        if (HasTargets()) {
            tf.LookAt(mLastTargetPos, tf.m.z);
        }
        Vector3 v;
        Subtract(tf.v, mLastTargetPos, v);
        float length = std::sqrt(v.y * v.y + v.z * v.z + v.x * v.x);
        Vector3 vother(
            -(mScreenOffset.x / cam->LocalProjectXfm().m.x.x) * length,
            0,
            (mScreenOffset.y / cam->LocalProjectXfm().m.z.y) * length
        );
        Multiply(vother, tf, tf.v);
    }
}

void CamShotFrame::Interp(const CamShotFrame &other, float f1, float f2, RndCam *cam) {
    float blendT = f1;
    if (mBlendEase) {
        float easeOffset = 0;
        float easeEnd = 1.0f;
        switch (mBlendEaseMode) {
        case kBlendEaseIn:
            easeEnd = 2.0f;
            break;
        case kBlendEaseInAndOut:
            easeOffset = 0.0f;
            easeEnd = 1.0f;
            break;
        case kBlendEaseOut:
            easeOffset = -1.0f;
            break;
        default:
            MILO_NOTIFY("Invalid mBlendEaseMode: %d", mBlendEaseMode);
            break;
        }
        ATanInterpolator aint(easeOffset, easeEnd, easeOffset, easeEnd, mBlendEase);
        blendT = aint.Eval(f1);
    }

    // Interpolate FOV
    float interpFOV = ::Interp(mFOV, other.mFOV, blendT);
    float blendedFOV = ::Interp(cam->YFov(), interpFOV, f2);
    cam->SetFrustum(mCamShot->mNearPlane, mCamShot->mFarPlane, blendedFOV, 1.0f);

    bool hasTarget = HasTargets();
    bool thasTarget = other.HasTargets();
    bool sameTargets = SameTargets(other);

    // Build transforms for both keyframes
    Transform thisTf;
    BuildTransform(cam, thisTf, !sameTargets);
    Transform otherTf;
    other.BuildTransform(cam, otherTf, !sameTargets);

    // Interpolate position and rotation
    Transform resultTf;
    ::Interp(thisTf.v, otherTf.v, blendT, resultTf.v);
    ::Interp(thisTf.m, otherTf.m, blendT, resultTf.m);

    float targetDist;
    if (hasTarget || thasTarget) {
        if (sameTargets) {
            Transform thisLook(resultTf);
            Transform otherLook(resultTf);
            if (hasTarget) {
#ifdef HX_NATIVE
                Vector3 d1; Subtract(mLastTargetPos, resultTf.v, d1);
                if (d1.x * d1.x + d1.y * d1.y + d1.z * d1.z > 1e-6f)
#endif
                    thisLook.LookAt(mLastTargetPos, resultTf.m.z);
            }
            if (thasTarget) {
#ifdef HX_NATIVE
                Vector3 d2; Subtract(other.mLastTargetPos, resultTf.v, d2);
                if (d2.x * d2.x + d2.y * d2.y + d2.z * d2.z > 1e-6f)
#endif
                    otherLook.LookAt(other.mLastTargetPos, resultTf.m.z);
            }
            ::Interp(thisLook.m, otherLook.m, blendT, resultTf.m);
        }

        Vector2 screenOfs;
        if (hasTarget && !thasTarget) {
            targetDist = Distance(mLastTargetPos, resultTf.v);
            screenOfs = mScreenOffset;
        } else if (!hasTarget && thasTarget) {
            targetDist = Distance(other.mLastTargetPos, resultTf.v);
            screenOfs = other.mScreenOffset;
        } else {
            float otherDist = Distance(other.mLastTargetPos, resultTf.v);
            float thisDist = Distance(mLastTargetPos, resultTf.v);
            ::Interp(thisDist, otherDist, blendT, targetDist);
            ::Interp(mScreenOffset, other.mScreenOffset, blendT, screenOfs);
        }

        if (sameTargets) {
            Vector3 screenVec;
            screenVec.x = -(screenOfs.x / cam->LocalProjectXfm().m.x.x) * targetDist;
            screenVec.y = 0.0f;
            screenVec.z = (screenOfs.y / cam->LocalProjectXfm().m.z.y) * targetDist;
            Multiply(screenVec, resultTf, resultTf.v);
        }
    }

    // Interpolate zoom FOV and apply
    float zoomFOV;
    ::Interp(mZoomFOV, other.mZoomFOV, blendT, zoomFOV);
    cam->SetFrustum(mCamShot->mNearPlane, mCamShot->mFarPlane, blendedFOV + zoomFOV, 1.0f);

    // Depth of field
    RndTransformable *focus = mFocalTarget;
    RndTransformable *towardFocus = other.mFocalTarget;
    bool doDOF = mCamShot->mUseDepthOfField
        && (focus || hasTarget || towardFocus || thasTarget);
    if (doDOF) {
        float blurDepth;
        float focusMult;
        ::Interp(mBlurDepth, other.mBlurDepth, blendT, blurDepth);
        float maxBlur;
        ::Interp(mMaxBlur, other.mMaxBlur, blendT, maxBlur);
        float minBlur;
        ::Interp(mMinBlur, other.mMinBlur, blendT, minBlur);
        ::Interp(mFocusBlurMultiplier, other.mFocusBlurMultiplier, blendT, focusMult);

        float thisFocalDist = 0;
        float otherFocalDist = 0;
        if (focus) {
            thisFocalDist = Distance(focus->WorldXfm().v, resultTf.v);
        } else {
            if (hasTarget)
                thisFocalDist = Distance(mLastTargetPos, resultTf.v);
        }
        if (towardFocus) {
            otherFocalDist = Distance(towardFocus->WorldXfm().v, resultTf.v);
        } else {
            if (thasTarget)
                otherFocalDist = Distance(other.mLastTargetPos, resultTf.v);
        }
        if (!focus && !hasTarget) {
            MILO_ASSERT(towardFocus || thasTarget, 0x756);
            thisFocalDist = otherFocalDist;
        }
        if (!towardFocus && !thasTarget) {
            MILO_ASSERT(focus || hasTarget, 0x75c);
            otherFocalDist = thisFocalDist;
        }
        float focalDist = ::Interp(thisFocalDist, otherFocalDist, blendT);
        float scaledFocalDist = focalDist * focusMult;
        TheDOFProc->Set(
            cam, scaledFocalDist + focalDist, blurDepth, maxBlur, minBlur
        );
    } else {
        TheDOFProc->UnSet();
    }

    // Blend with current camera position
#ifdef HX_NATIVE
    // Guard: if camera's world transform is already poisoned with NaN/inf,
    // skip the blend to avoid propagating bad values
    {
        const Transform &cwx = cam->WorldXfm();
        if (cwx.v.x == cwx.v.x && cwx.v.x > -1e30f && cwx.v.x < 1e30f) {
            ::Interp(cwx.v, resultTf.v, f2, resultTf.v);
            ::Interp(cwx.m, resultTf.m, f2, resultTf.m);
        }
    }
#else
    ::Interp(cam->WorldXfm().v, resultTf.v, f2, resultTf.v);
    ::Interp(cam->WorldXfm().m, resultTf.m, f2, resultTf.m);
#endif

    // Shake
    float shakeAmp, shakeFreq;
    ::Interp(mShakeNoiseAmp, other.mShakeNoiseAmp, blendT, shakeAmp);
    ::Interp(mShakeNoiseFreq, other.mShakeNoiseFreq, blendT, shakeFreq);
    Vector2 shakeMaxAngle;
    ::Interp(mShakeMaxAngle, other.mShakeMaxAngle, blendT, shakeMaxAngle);
    Vector3 shakeOffset;
    Vector3 shakeAngOffset;
    mCamShot->Shake(shakeFreq, shakeAmp, shakeMaxAngle, shakeOffset, shakeAngOffset);
    Multiply(shakeOffset, resultTf, resultTf.v);
    Hmx::Matrix3 rotMtx;
    MakeRotMatrix(shakeAngOffset, rotMtx, true);
    Multiply(resultTf.m, rotMtx, resultTf.m);

    // NB(rb3-xenon): retail RB3 has NO ApplyFinalCamTransform call here — the
    // virtual is a DC3-era addition (absent from the rb3-Wii oracle entirely),
    // and retail's decomp goes straight from Multiply() to SetLocalXfm().
    cam->SetLocalXfm(resultTf);
}

Symbol FOV_to_LensSym(float fov) {
    float scaled = ComputeFOVScale(fov);
    if (NearlyEqual(scaled, 15.0f))
        return "15mm";
    else if (NearlyEqual(scaled, 20.0f))
        return "20mm";
    else if (NearlyEqual(scaled, 24.0f))
        return "24mm";
    else if (NearlyEqual(scaled, 28.0f))
        return "28mm";
    else if (NearlyEqual(scaled, 35.0f))
        return "35mm";
    else if (NearlyEqual(scaled, 50.0f))
        return "50mm";
    else if (NearlyEqual(scaled, 85.0f))
        return "85mm";
    else if (NearlyEqual(scaled, 135.0f))
        return "135mm";
    else if (NearlyEqual(scaled, 200.0f))
        return "200mm";
    else
        return "Custom";
}

float LensSym_to_FOV(Symbol sym) {
    String lensStr(sym);
    unsigned int idx = lensStr.find("mm");
    if (idx != FixedString::npos) {
        float scale = std::atof(lensStr.substr(0, idx).c_str());
        return ScaleToFOV(scale);
    } else
        return -1;
}

BEGIN_CUSTOM_PROPSYNC(CamShotFrame)
    SYNC_PROP(duration, o.mDuration)
    SYNC_PROP(blend, o.mBlend)
    SYNC_PROP(blend_ease, o.mBlendEase)
    SYNC_PROP(blend_ease_mode, (int &)o.mBlendEaseMode)
    SYNC_PROP(world_offset, o.mWorldOffset)
    SYNC_PROP(screen_offset, o.mScreenOffset) {
        static Symbol _s("targets");
        if (sym == _s) {
            o.OnSyncTargets(o.mTargets, _val, _prop, _i + 1, _op);
            return true;
        }
    }
    {
        static Symbol _s("parent");
        if (sym == _s) {
            o.OnSyncParent(o.mParent, _val, _prop, _i + 1, _op);
            return true;
        }
    }
    SYNC_PROP(focal_target, o.mFocalTarget)
    SYNC_PROP(use_parent_rotation, o.mUseParentRotation)
    SYNC_PROP(parent_first_frame, o.mParentFirstFrame)
    SYNC_PROP_SET(field_of_view, o.mFOV * RAD2DEG, o.mFOV = _val.Float() * DEG2RAD)
    SYNC_PROP_SET(lens_mm, ComputeFOVScale(o.mFOV), o.mFOV = ScaleToFOV(_val.Float()))
    SYNC_PROP_SET(lens_preset, FOV_to_LensSym(o.mFOV), {
        float fov = LensSym_to_FOV(_val.Sym());
        if (fov != -1.0f)
            o.mFOV = fov;
        else
            o.mFOV += 0.00010011921f;
    })
    SYNC_PROP(blur_depth, o.mBlurDepth)
    SYNC_PROP(max_blur, o.mMaxBlur)
    SYNC_PROP(min_blur, o.mMinBlur)
    SYNC_PROP(focus_blur_multiplier, o.mFocusBlurMultiplier)
    SYNC_PROP(shake_noisefreq, o.mShakeNoiseFreq)
    SYNC_PROP(shake_noiseamp, o.mShakeNoiseAmp)
    SYNC_PROP(shake_maxangle, o.mShakeMaxAngle)
    SYNC_PROP_SET(zoom_fov, o.mZoomFOV * RAD2DEG, o.mZoomFOV = _val.Float() * DEG2RAD)
END_CUSTOM_PROPSYNC

#pragma endregion
#pragma region CamShotCrowd

CamShotCrowd::CamShotCrowd(Hmx::Object *owner)
    : mCrowd(owner), mCrowdRotate(kCrowdRotateNone),
      mCamShot(dynamic_cast<CamShot *>(owner)) {}

CamShotCrowd::CamShotCrowd(Hmx::Object *owner, const CamShotCrowd &other)
    : mCrowd(other.mCrowd), mCrowdRotate(other.mCrowdRotate), m3DCharIndices(other.m3DCharIndices),
      mCamShot(dynamic_cast<CamShot *>(owner)) {}

void CamShotCrowd::Save(BinStream &bs) const {
    bs << mCrowd;
    bs << mCrowdRotate;
    bs << m3DCharIndices;
    int num = -1;
    if (mCrowd) {
        num = mCrowd->GetModifyStamp();
    }
    bs << num;
}

void CamShotCrowd::Load(BinStream &bs) {
    bs >> mCrowd;
    bs >> (int &)mCrowdRotate;
    bs >> m3DCharIndices;
    int num;
    bs >> num;
    if (mCrowd && num != mCrowd->GetModifyStamp() || (!mCrowd && num != -1)) {
        m3DCharIndices.clear();
    }
}

BinStream &operator>>(BinStreamRev &d, CamShotCrowd &c) {
    c.Load(d.stream);
    return d.stream;
}

void CamShotCrowd::AddCrowdChars() {
    std::list<std::pair<RndMultiMesh *, std::list<RndMultiMesh::Instance>::iterator> >
        selectedCrowd;
    GetSelectedCrowd(selectedCrowd);
    if (selectedCrowd.empty()) {
        MILO_NOTIFY("No selected crowd members in this crowd");
    } else {
        AddCrowdChars(&selectedCrowd);
    }
}

void CamShotCrowd::SetCrowdChars() {
    std::list<std::pair<RndMultiMesh *, std::list<RndMultiMesh::Instance>::iterator> >
        selectedCrowd;
    GetSelectedCrowd(selectedCrowd);
    if (selectedCrowd.empty()) {
        MILO_NOTIFY("No selected crowd members in this crowd");
    } else {
        ClearCrowdChars();
        AddCrowdChars(&selectedCrowd);
    }
}

void CamShotCrowd::ClearCrowdChars() {
    m3DCharIndices.clear();
    if (!mCrowd) {
        MILO_NOTIFY("No crowd selected");
    }
    mCrowd->Set3DCharList(m3DCharIndices, mCamShot);
}

void CamShotCrowd::GetSelectedCrowd(
    std::list<std::pair<RndMultiMesh *, std::list<RndMultiMesh::Instance>::iterator> >
        &crowdChars
) {
    FOREACH (it, RndMultiMesh::ProxyPool()) {
        RndMultiMeshProxy *proxy = it->first;
        MILO_ASSERT(proxy, 0xA06);
        RndMultiMesh *multiMesh = proxy->MultiMesh();
        if (!proxy->Refs().empty() && multiMesh) {
            crowdChars.push_back(std::make_pair(multiMesh, proxy->Index()));
            {
                std::list<RndMultiMesh::Instance>::iterator dummy;
                proxy->SetMultiMesh(0, dummy);
            }
        }
    }
}

void CamShotCrowd::AddCrowdChars(
    const std::list<std::pair<RndMultiMesh *, std::list<RndMultiMesh::Instance>::iterator> > *
        crowdChars
) {
    if (!mCrowd) {
        MILO_NOTIFY("No crowd selected");
        return;
    }
    if (mCrowd->mForce3DCrowd)
        return;

    float fullness = mCrowd->mFlatFullness;
    mCrowd->Set3DCharList(std::vector<std::pair<int, int> >(), mCamShot);
    mCrowd->SetFullness(1, mCrowd->mCharFullness);

    if (!crowdChars) {
        ObjList<WorldCrowd::CharData>::iterator it = mCrowd->mCharacters.begin();
        int charIdx = 0;
        for (; it != mCrowd->mCharacters.end(); ++it, ++charIdx) {
            int instIdx = 0;
            RndMultiMesh::InstanceList &insts = it->mMMesh->Instances();
            for (RndMultiMesh::InstanceList::iterator instIt = insts.begin();
                 instIt != insts.end(); ++instIt, ++instIdx) {
                m3DCharIndices.push_back(std::make_pair(charIdx, instIdx));
            }
        }
        mCrowd->Set3DCharAll();
    } else {
        FOREACH_PTR (it, crowdChars) {
            RndMultiMesh *mmesh = it->first;
            int charIdx = 0;
            for (ObjList<WorldCrowd::CharData>::iterator it2 = mCrowd->mCharacters.begin();
                 it2 != mCrowd->mCharacters.end() && it2->mMMesh != mmesh; it2++) {
                charIdx++;
            }
            if (charIdx != mCrowd->mCharacters.size()) {
                int instIdx = 0;
                for (RndMultiMesh::InstanceList::iterator mmit =
                         mmesh->Instances().begin();
                     mmit != mmesh->Instances().end() && mmit != it->second;
                     ++mmit, ++instIdx)
                    ;
                MILO_ASSERT(instIdx != mmesh->Instances().size(), 0xBE1);
                std::pair<int, int> iPair = std::make_pair(charIdx, instIdx);
                if (std::find(m3DCharIndices.begin(), m3DCharIndices.end(), iPair)
                    == m3DCharIndices.end()) {
                    m3DCharIndices.push_back(iPair);
                }
            }
        }
        mCrowd->Set3DCharList(m3DCharIndices, mCamShot);
    }
    mCrowd->SetFullness(fullness, mCrowd->mCharFullness);
}

BEGIN_CUSTOM_PROPSYNC(CamShotCrowd)
    SYNC_PROP_MODIFY(crowd, o.mCrowd, o.m3DCharIndices.clear())
    SYNC_PROP(crowd_rotate, (int &)o.mCrowdRotate)
END_CUSTOM_PROPSYNC

#pragma endregion
#pragma region CamShot

// The string "camera_spew" occurs ZERO times in retail band.exe (Python scan of the
// whole image), so the DataVariable gate is dev-build-only across all 17 CAMERA_LOG
// sites in this TU. Retail still EVALUATES the log arguments - CamShot::ShotOk
// (0x824C0320) calls DataNode::Str() and discards the result, which is exactly what
// the comma-form MILO_LOG (Debug.h:299, ((void)(__VA_ARGS__))) produces once the
// inlined Name() is dead-eliminated. So gate the DataVariable test, not the log.
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
#define CAMERA_LOG(...)                                                                  \
    if (DataVariable("camera_spew") != 0) {                                              \
        MILO_LOG(__VA_ARGS__);                                                           \
    }
#else
#define CAMERA_LOG(...) MILO_LOG(__VA_ARGS__)
#endif

CamShot::CamShot()
    : mKeyframes(this), mLooping(false), mLoopKeyframe(0),
      mNearPlane(RndCam::DefaultNearPlane()),
      mFarPlane(mNearPlane * RndCam::MaxFarNearPlaneRatio()), mUseDepthOfField(true),
      mFilter(0.9), mClampHeight(-1), mCrowdStateOverride(gNullStr), mAnims(this),
      mPath(this), mPathFrame(-1),
      mPlatform(kPlatformNone), mHideList(this), mShowList(this), mGenHideList(this),
      mParentDir(this), mDrawOverrides(this), mPostProcOverrides(this), mCrowds(this),
      mPS3PerPixel(true), mGlowSpot(this), mFlags(0),
      mEndHideList(this), mEndShowList(this), mLastDesiredShakeOffset(0, 0, 0),
      mLastDesiredShakeAngOffset(0, 0, 0), mLastShakeOffset(0, 0, 0),
      mLastShakeAngOffset(0, 0, 0), mShakeVelocity(0, 0, 0), mShakeAngVelocity(0, 0, 0), mLastNext(0),
      mLastPrev(0), mDuration(0), mDisabled(0), mShotStarted(1), mShotOver(0), mHidden(0),
      mSetFrameActive(0) {}

CamShot::~CamShot() {}

DataNode CamShot::OnGetOccluded(DataArray *) { return 0; }
DataNode CamShot::OnSetAllCrowdChars3D(DataArray *) { return 0; }

BEGIN_HANDLERS(CamShot)
    HANDLE(has_targets, OnHasTargets)
    HANDLE(set_pos, OnSetPos)
    HANDLE_EXPR(duration_seconds, GetDurationSeconds())
    HANDLE(set_3d_crowd, OnSetCrowdChars)
    HANDLE(add_3d_crowd, OnAddCrowdChars)
    HANDLE(clear_3d_crowd, OnClearCrowdChars)
#ifdef RB3_KEEP_DC3_ONLY_HANDLERS
    // RB3-360 retail does not carry this handler. Two independent oracles over
    // orig/45410914/band.exe agree that fn_824C4A98 builds exactly 12 Symbols
    // and "get_crowd_dir" is not among them; the literal appears in no
    // non-executable section of the image at all. DC3 (newer engine) added it.
#endif
    HANDLE_EXPR(gen_hide_list, 0)
    HANDLE_EXPR(clear_hide_list, 0)
    HANDLE(get_occluded, OnGetOccluded)
    HANDLE_EXPR(platform_ok, PlatformOk())
    HANDLE(set_all_to_3D, OnSetAllCrowdChars3D)
    HANDLE(radio, OnRadio)
    HANDLE_SUPERCLASS(RndAnimatable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

WorldDir *CamShot::GetCrowdDir() const {
    ObjectDir *dir = mParentDir.Ptr() ? mParentDir.Ptr() : Dir();
    return dynamic_cast<WorldDir *>(dir);
}

#define SYNC_PROP_LIST(s, member)                                                        \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s) {                                                                 \
            if (!(_op & (kPropGet | kPropSize)))                                         \
                UnHide();                                                                \
            if (PropSync(member, _val, _prop, _i + 1, _op))                              \
                return true;                                                             \
            else                                                                         \
                return false;                                                            \
        }                                                                                \
    }

BEGIN_PROPSYNCS(CamShot)
    SYNC_PROP_MODIFY(keyframes, mKeyframes, CacheFrames())
    SYNC_PROP(looping, mLooping)
    SYNC_PROP(loop_keyframe, mLoopKeyframe)
    SYNC_PROP_SET(category, mCategory, mCategory = _val.ForceSym())
    SYNC_PROP(filter, mFilter)
    SYNC_PROP(clamp_height, mClampHeight)
    SYNC_PROP(near_plane, mNearPlane)
    SYNC_PROP(far_plane, mFarPlane) {
        static Symbol _s("duration");
        if (sym == _s && _op & kPropGet)
            return PropSync(mDuration, _val, _prop, _i + 1, _op);
    }
    SYNC_PROP(use_depth_of_field, mUseDepthOfField)
    SYNC_PROP(path, mPath)
    SYNC_PROP(path_frame, mPathFrame)
    SYNC_PROP(platform_only, (int &)mPlatform)
    SYNC_PROP_LIST(hide_list, mHideList)
    SYNC_PROP_LIST(show_list, mShowList)
    SYNC_PROP_LIST(gen_hide_list, mGenHideList)
    SYNC_PROP(draw_overrides, mDrawOverrides)
    SYNC_PROP(postproc_overrides, mPostProcOverrides)
    SYNC_PROP(glow_spot, mGlowSpot)
    SYNC_PROP(crowds, mCrowds)
#ifdef RB3_KEEP_DC3_ONLY_HANDLERS
    // Retail fn_824C6D80 builds exactly 25 Symbols and "crowd_state_override"
    // is not one of them (both oracles agree; the literal is absent from every
    // non-executable section of band.exe). The member itself IS real -- it is
    // still saved/copied/loaded below -- only the propsync arm is DC3-only.
    SYNC_PROP(crowd_state_override, mCrowdStateOverride)
#endif
    SYNC_PROP(ps3_per_pixel, mPS3PerPixel)
    SYNC_PROP_BITFIELD(flags, mFlags, 0xB94)
    SYNC_PROP_SET(disabled, mDisabled, )
    SYNC_PROP(anims, mAnims)
    SYNC_SUPERCLASS(RndAnimatable)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(CamShot)
    SAVE_REVS(0x32, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndAnimatable)
    bs << mKeyframes;
    bs << mLooping;
    bs << mLoopKeyframe;
    bs << mNearPlane;
    bs << mFarPlane;
    bs << mUseDepthOfField;
    bs << mFilter;
    bs << mClampHeight;
    bs << mPath;
    bs << mCategory;
    bs << mPlatform;
    bs << mHideList;
#ifdef HX_NATIVE
    MILO_ASSERT(mGenHideVector.empty(), 0x3CE);
#endif
    if (bs.Cached()) {
        FOREACH (it, mHideList) {
            mGenHideList.remove(*it);
        }
        if (bs.GetPlatform() == kPlatformXBox) {
            mGenHideList.clear();
        }
    }
    bs << mGenHideList;
    bs << mShowList;
    bs << mGlowSpot;
    bs << mDrawOverrides;
    bs << mPostProcOverrides;
    bs << mPS3PerPixel;
    bs << mFlags;
    bs << mCrowds;
    bs << mAnims;
END_SAVES

BEGIN_COPYS(CamShot)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndAnimatable)
    CREATE_COPY(CamShot)
    BEGIN_COPYING_MEMBERS
        mKeyframes.clear();
        for (int i = 0; i != c->mKeyframes.size(); i++) {
            mKeyframes.push_back(CamShotFrame(this, c->mKeyframes[i]));
        }
        mCrowds.clear();
        for (int i = 0; i != c->mCrowds.size(); i++) {
            mCrowds.push_back(CamShotCrowd(this, c->mCrowds[i]));
        }
        COPY_MEMBER(mCrowdStateOverride)
        COPY_MEMBER(mNearPlane)
        COPY_MEMBER(mFarPlane)
        COPY_MEMBER(mUseDepthOfField)
        COPY_MEMBER(mFilter)
        COPY_MEMBER(mClampHeight)
        COPY_MEMBER(mPath)
        COPY_MEMBER(mPlatform)
        COPY_MEMBER(mCategory)
        COPY_MEMBER(mHideList)
        COPY_MEMBER(mGenHideList)
#ifdef HX_NATIVE
        COPY_MEMBER(mGenHideVector)
#endif
        COPY_MEMBER(mShowList)
        COPY_MEMBER(mLooping)
        COPY_MEMBER(mLoopKeyframe)
        COPY_MEMBER(mGlowSpot)
        COPY_MEMBER(mDrawOverrides)
        COPY_MEMBER(mPostProcOverrides)
        COPY_MEMBER(mPS3PerPixel)
        COPY_MEMBER(mFlags)
        COPY_MEMBER(mAnims)
        CacheFrames();
    END_COPYING_MEMBERS
END_COPYS

void LoadDrawables(BinStream &bs, std::vector<RndDrawable *> &draws, ObjectDir *dir) {
    MILO_ASSERT(dir, 0x3AC);
    draws.clear();
    uint count;
    bs >> count;
    draws.reserve(count);
    while (count != 0) {
        char name[0x80];
        bs.ReadString(name, 0x80);
        RndDrawable *draw =
            dynamic_cast<RndDrawable *>(dir->FindObject(name, false));
        if (draw) {
            draws.push_back(draw);
        }
        count--;
    }
}

INIT_REVS(0x34, 0)

BEGIN_LOADS(CamShot)
    LOAD_REVS(bs)
    ASSERT_REVS(0x34, 0)
    sCamShotRev = d.rev;
    bool hidden = mHidden;
    if (hidden) {
        UnHide();
    }
    float oldRevFloat = 0;
    if (sCamShotRev > 0) {
        Hmx::Object::Load(bs);
        RndAnimatable::Load(bs);
    }
    // NB(rb3-xenon): retail CamShot does NOT inherit RndTransformable, so the
    // serialized RndTransformable block is absent — rev > 0x32 still holds in
    // the file format but loads nothing for it here. (rb3-Wii equivalent in
    // ../rb3/src/system/world/CameraShot.cpp also has no RndTransformable::Load.)
    if (sCamShotRev > 0xC) {
        d >> mKeyframes;
        d >> mLooping;
        if (sCamShotRev > 0x1E) {
            d >> mLoopKeyframe;
        } else {
            mLoopKeyframe = false;
        }
        if (sCamShotRev < 0x28) {
            d >> oldRevFloat;
        }
        d >> mNearPlane;
        d >> mFarPlane;
        d >> mUseDepthOfField;
        d >> mFilter;
        d >> mClampHeight;
    } else {
        mLooping = false;
        mLoopKeyframe = false;

        float fov1, fov2;
        d >> fov1;
        d >> fov2;
        if (sCamShotRev < 9) {
            fov1 = ConvertFov(fov1, 0.75f);
            fov2 = ConvertFov(fov2, 0.75f);
        }
        Transform tf1;
        Transform tf2;
        d >> tf1;
        d >> tf2;
        Vector2 vec1;
        Vector2 vec2;
        d >> vec1;
        d >> vec2;
        if (sCamShotRev < 0x28)
            d >> oldRevFloat;

        float blendDuration;
        d >> blendDuration;
        d >> mNearPlane;
        d >> mFarPlane;
        d >> mUseDepthOfField;
        float blurDepth = 1.0f;
        if (sCamShotRev > 9) {
            float newBlurDepth;
            float dummyFloat1, dummyFloat2;
            d >> newBlurDepth;
            d >> dummyFloat1;
            d >> dummyFloat2;
            blurDepth = 1.0f - newBlurDepth;
        }
        if (sCamShotRev < 4) {
            bool ratebool;
            d >> ratebool;
            SetRate((Rate)!ratebool);
        }
        d >> mFilter;
        if (sCamShotRev < 7)
            mFilter = 0.9f;
        d >> mClampHeight;
        ObjPtrList<RndTransformable> targetList(this);
        ObjPtr<RndTransformable> parentPtr(this);
        int targetCount;
        d >> targetCount;
        for (int i = 0; i < targetCount; i++) {
            RndTransformable *subpart = LoadSubPart(d, this);
            if (subpart)
                targetList.push_back(subpart);
        }
        parentPtr = LoadSubPart(d, this);
        bool useParentRotation = false;
        if (sCamShotRev > 10)
            d >> useParentRotation;
        CamShotFrame frame1(this);
        CamShotFrame frame2(this);
        if (blendDuration > 0.0f) {
            frame1.mDuration = 0.0f;
            frame1.mBlend = blendDuration;
            frame1.mWorldOffset = tf1;
            frame1.mScreenOffset = vec1;
            frame1.mFOV = fov1;
            frame1.mBlurDepth = blurDepth;
            frame1.mMaxBlur = 1;
            frame1.mMinBlur = 0;
            frame1.mFocusBlurMultiplier = 0.0f;
            frame1.mTargets = targetList;
            frame1.mParent = parentPtr;
            frame1.mUseParentRotation = useParentRotation;
            mKeyframes.push_back(frame1);
        }
        frame2.mDuration = 0.0f;
        frame2.mBlend = 0.0f;
        frame2.mWorldOffset = tf2;
        frame2.mScreenOffset = vec2;
        frame2.mFOV = fov2;
        frame2.mBlurDepth = blurDepth;
        frame2.mMaxBlur = 1;
        frame2.mMinBlur = 0;
        frame2.mFocusBlurMultiplier = 0.0f;
        frame2.mTargets = targetList;
        frame2.mParent = parentPtr;
        frame2.mUseParentRotation = useParentRotation;
        mKeyframes.push_back(frame2);
    }

    d >> mPath;
    if (sCamShotRev > 1 && sCamShotRev < 0x2D) {
        float unusedPathFloat;
        d >> unusedPathFloat;
    }
    if (sCamShotRev > 2) {
        d >> mCategory;
        if (sCamShotRev < 0x26) {
            float unusedCategoryFloat;
            d >> unusedCategoryFloat;
        }
    }
    if (sCamShotRev > 0x22) {
        d >> (int &)mPlatform;
    } else if (sCamShotRev > 0x21) {
        int platformState;
        d >> platformState;
        if (platformState == 1) {
            mPlatform = kPlatformXBox;
        } else if (platformState == 2) {
            mPlatform = kPlatformPS3;
        } else {
            mPlatform = kPlatformNone;
        }
    }
    if (sCamShotRev < 1) {
        RndAnimatable::Load(bs);
    }
    CamShotCrowd crowdData(this);

    if (sCamShotRev > 4 && sCamShotRev < 42) {
        d >> crowdData.m3DCharIndices;
    }
    int crowdModifyStamp = -1;
    if (sCamShotRev >= 8 && sCamShotRev < 42)
        d >> crowdModifyStamp;
    if (sCamShotRev > 5) {
#ifdef HX_NATIVE
        mGenHideVector.clear();
#endif
        mGenHideList.clear();
        mHideList.clear();
        if (sCamShotRev <= 0x2F || (bs.Cached() && sCamShotRev < 0x32)) {
            mHideList.Load(bs, false);
        } else {
            mHideList.Load(bs, false);
            std::vector<RndDrawable *> tempDraws;
            LoadDrawables(bs, tempDraws, Dir());
        }
    }
    if (sCamShotRev > 0x1B) {
        mShowList.Load(bs, false);
    }

    if (sCamShotRev > 0xB) {
        if (sCamShotRev < 0x2A)
            d >> crowdData.mCrowd;
    } else {
        const DataNode *prop = Property("hide_crowd", false);
        if (!prop || prop->Int() == 0) {
            ObjDirItr<WorldCrowd> iter(Dir(), true);
            if (iter) {
                crowdData.mCrowd = iter;
            }
        }
    }
    if (sCamShotRev > 32 && sCamShotRev < 42)
        d >> (int &)crowdData.mCrowdRotate;
    if (sCamShotRev >= 8 && sCamShotRev < 42) {
        if (crowdData.mCrowd) {
            if (crowdModifyStamp != crowdData.mCrowd->GetModifyStamp())
                crowdData.m3DCharIndices.clear();
        } else if (crowdModifyStamp != -1)
            crowdData.m3DCharIndices.clear();
    }
    if (sCamShotRev == 0xE) {
        float unused1, unused2, unused3;
        d >> unused1;
        d >> unused2;
        d >> unused3;
    }

    if (sCamShotRev > 15 && sCamShotRev < 18) {
        float shakeFreq, shakeAmp;
        bs >> shakeFreq;
        bs >> shakeAmp;
        for (int i = 0; i != mKeyframes.size(); i++) {
            mKeyframes[i].mShakeNoiseAmp = shakeAmp;
            mKeyframes[i].mShakeNoiseFreq = shakeFreq;
        }
    }
    if (sCamShotRev > 0x10 && sCamShotRev < 0x12) {
        Vector2 shakeAngle;
        bs >> shakeAngle;
        for (int i = 0; i != mKeyframes.size(); i++) {
            mKeyframes[i].mShakeMaxAngle = shakeAngle;
        }
    }
    if (sCamShotRev > 0x13)
        d >> mGlowSpot;
    if (sCamShotRev > 0x1D)
        d >> mDrawOverrides;
    if (sCamShotRev > 0x1F)
        d >> mPostProcOverrides;
    if (sCamShotRev > 0x23 && !(sCamShotRev >= 47 && sCamShotRev <= 48)) {
        d >> mPS3PerPixel;
    }
    if (sCamShotRev > 0x24)
        d >> mFlags;
    Symbol oldAnimSym;
    if (sCamShotRev > 39 && sCamShotRev < 43)
        d >> oldAnimSym;
    if (sCamShotRev < 0x2A) {
        if (crowdData.mCrowd)
            mCrowds.push_back(crowdData);
    } else
        d >> mCrowds;
    if (sCamShotRev > 0x33) {
        d >> mCrowdStateOverride;
    } else {
        static Symbol none("none");
        mCrowdStateOverride = none;
    }
    if (sCamShotRev > 0x2A)
        d >> mAnims;

    if (!oldAnimSym.Null()) {
        mAnims.push_back(Dir()->Find<RndAnimatable>(oldAnimSym.Str(), false));
    }
    CacheFrames();
    if (hidden)
        DoHide();
END_LOADS

void CamShot::StartAnim() {
    START_AUTO_TIMER("cam_switch");
    CAMERA_LOG("** %s CamShot::StartAnim() start\n", Name());
    static Message msg("start_shot");
    WorldDir *crowdDir = GetCrowdDir();
    Export(msg, true);
    mLastNext = 0;
    if (crowdDir) {
        crowdDir->SetCrowds(mCrowds);
        TheHamWardrobe->ForceCrowdAnimationStart(mCrowdStateOverride);
    }
    mShotOver = false;
    mLastPrev = 0;
    mShotStarted = true;
    mLastDesiredShakeOffset.Zero();
    mLastShakeOffset.Zero();
    mShakeVelocity.Zero();
    mLastDesiredShakeAngOffset.Zero();
    mLastShakeAngOffset.Zero();
    mShakeAngVelocity.Zero();
    StartAnims(mAnims);
    for (int i = 0; i != mCrowds.size(); i++) {
        CamShotCrowd &cur = mCrowds[i];
        if (cur.mCrowd) {
            cur.mCrowd->Set3DCharList(cur.m3DCharIndices, cur.mCamShot);
        }
    }
    RndVelocityBuffer::Singleton().ResetFrame();
    DoHide();
    CAMERA_LOG("** %s CamShot::StartAnim() stop\n", Name());
}

void CamShot::EndAnim() {
    CAMERA_LOG("** %s CamShot::EndAnim() start\n", Name());
    UnHide();
    static Message msg("stop_shot");
    TheHamWardrobe->ForceCrowdAnimationEnd();
    Export(msg, true);
    EndAnims(mAnims);
    CAMERA_LOG("** %s CamShot::EndAnim() stop\n", Name());
}

void CamShot::SetFrame(float frame, float blend) {
    START_AUTO_TIMER("camera");
    if (mSetFrameActive)
        return;
#ifdef HX_NATIVE
    // Guard: reject NaN/inf frame values that would poison camera transforms
    if (frame != frame || frame > 1e15f || frame < -1e15f)
        return;
#endif
    RndAnimatable::SetFrame(frame, blend);
    RndCam *cam = GetCam();
    if (!cam)
        return;
    FOREACH (it, mAnims) {
        (*it)->SetFrame(frame, 1);
    }
    if (mKeyframes.empty())
        return;
    mSetFrameActive = true;
    mPathFrame = -1;
    EndFrame();
    static CamShotFrame nullFrame(nullptr);
    nullFrame.mCamShot = this;
    CamShotFrame *frame4c = nullptr;
    CamShotFrame *frame50 = nullptr;
    float f48 = 1.0f;
    GetKey(frame, frame4c, frame50, f48);
    if (mDisabled != 0) {
        frame50->UpdateTarget();
        if (frame4c)
            frame4c->UpdateTarget();
        mSetFrameActive = false;
    } else {
        if (frame50 != mLastNext) {
            frame50->UpdateTarget();
        }
        if (!frame4c) {
            nullFrame.Interp(*frame50, 1.0f, blend, cam);
        } else {
            if (frame4c != mLastPrev) {
                if (frame4c != mLastNext) {
                    frame4c->UpdateTarget();
                }
                mLastPrev = frame4c;
            }
            frame4c->Interp(*frame50, f48, blend, cam);
        }
        mLastNext = frame50;
        if (CheckShotStarted()) {
            static Message msg("shot_started");
            HandleType(msg);
            mShotStarted = false;
        }
        if (CheckShotOver(frame)) {
            SetShotOver();
        }
        mSetFrameActive = false;
    }
}

void CamShot::ListAnimChildren(std::list<RndAnimatable *> &children) const {
    FOREACH (it, mAnims) {
        children.push_back(*it);
    }
}

void CamShot::Init() {
    REGISTER_OBJ_FACTORY(CamShot)
    sAnimTarget = Hmx::Object::New<Hmx::Object>();
}

void CamShot::Disable(bool disable, int mask) {
    if (disable)
        mDisabled |= mask;
    else
        mDisabled &= ~mask;
}

bool CamShot::CheckShotStarted() { return mShotStarted; }

bool CamShot::CheckShotOver(float f) { return !mShotOver && !mLooping && f >= mDuration; }

bool CamShot::PlatformOk() const {
    if (TheLoadMgr.EditMode() || mPlatform == kPlatformNone
        || TheLoadMgr.GetPlatform() == kPlatformNone)
        return true;
    Platform plat = TheLoadMgr.GetPlatform();
    if (TheLoadMgr.GetPlatform() == kPlatformPC)
        plat = kPlatformXBox;
    return plat == mPlatform;
}

float CamShot::GetDurationSeconds() const {
    if (Units() == kTaskBeats) {
        return 0.0f;
    } else {
        MILO_ASSERT(Units() == kTaskSeconds, 0x5cc);
        return mDuration / 30.0f;
    }
}

void CamShot::CacheFrames() {
    float frames = 0.0f;
    for (int i = 0; i != mKeyframes.size(); i++) {
        CamShotFrame &curframe = mKeyframes[i];
        curframe.SetFrame(frames);
        frames += curframe.GetDuration() + curframe.GetBlend();
    }
    mDuration = frames;
}

void CamShot::GetKey(float frame, CamShotFrame *&prev, CamShotFrame *&next, float &keyBlend) {
    MILO_ASSERT(!mKeyframes.empty(), 0x256);
    if (frame <= 0 || mDuration <= 0) {
        prev = nullptr;
        next = mKeyframes.begin();
        keyBlend = 1.0f;
        return;
    }
    if (frame >= mKeyframes.back().mFrame) {
        if (mLooping && (mLoopKeyframe < mKeyframes.size() && mLoopKeyframe >= 0)) {
            if (frame >= mDuration) {
                float duration = mDuration - mKeyframes[mLoopKeyframe].mFrame;
                frame -= mDuration;
                MILO_ASSERT(duration > 0, 0x26A);
                float remainder = std::fmod(frame, duration);
                frame = remainder + mKeyframes[mLoopKeyframe].mFrame;
            }
            if (frame >= mKeyframes.back().mFrame) {
                if (mKeyframes.back().mBlend <= 0) {
                    prev = nullptr;
                    next = &mKeyframes.back();
                    keyBlend = 1.0f;
                    return;
                }
                float holdEnd = mKeyframes.back().mFrame + mKeyframes.back().mDuration;
                if (frame > holdEnd) {
                    MILO_ASSERT(mKeyframes.back().mBlend > 0, 0x27F);
                    prev = &mKeyframes.back();
                    next = &mKeyframes[mLoopKeyframe];
                    keyBlend = (frame - holdEnd) / mKeyframes.back().mBlend;
                    return;
                }
                prev = nullptr;
                next = &mKeyframes.back();
                keyBlend = 1.0f;
                return;
            }
        } else {
            prev = nullptr;
            next = &mKeyframes.back();
            keyBlend = 1.0f;
            return;
        }
    }
    int before = 0;
    int after = mKeyframes.size() - 1;
    while (after > before + 1) {
        int avg = (before + after) >> 1;
        float curFrame = mKeyframes[avg].mFrame;
        if (frame == curFrame) {
            prev = nullptr;
            next = &mKeyframes[avg];
            keyBlend = 1.0f;
            return;
        }
        if (frame > curFrame) {
            before = avg;
        }
        if (!(frame > curFrame)) {
            after = avg;
        }
    }
    MILO_ASSERT(frame >= mKeyframes[before].mFrame && frame < mKeyframes[after].mFrame, 0x2AF);
    float holdEnd = mKeyframes[before].mFrame + mKeyframes[before].mDuration;
    if (frame > holdEnd) {
        MILO_ASSERT(mKeyframes[before].mBlend > 0, 0x2B4);
        prev = &mKeyframes[before];
        next = &mKeyframes[after];
        keyBlend = (frame - holdEnd) / mKeyframes[before].mBlend;
    } else {
        prev = nullptr;
        next = &mKeyframes[before];
        keyBlend = 1.0f;
    }
}

void CamShot::Shake(float freq, float amp, const Vector2 &maxAngle, Vector3 &offset, Vector3 &angOffset) {
    if (TheTaskMgr.DeltaSeconds() > 0 && !AutoPrepTarget::sChanging) {
        Vector2 localAng = maxAngle;
        localAng *= DEG2RAD;
        if (RandomFloat() < freq) {
            float angle = RandomFloat(0.0f, 6.2831855f);
            float randAmp = amp * RandomFloat();
            float cosVal = randAmp * Cosine(angle);
            mLastDesiredShakeOffset.x += cosVal;
            mLastDesiredShakeOffset.y += cosVal * 0.333f;
            mLastDesiredShakeOffset.z += randAmp * Sine(angle);
            mLastDesiredShakeAngOffset.x += RandomFloat(-localAng.x, localAng.x);
            mLastDesiredShakeAngOffset.y = 0;
            mLastDesiredShakeAngOffset.z += RandomFloat(-localAng.y, localAng.y);
        }
        float lenamp = Length(mLastDesiredShakeOffset) - amp;
        if (lenamp > 0) {
            Normalize(mLastDesiredShakeOffset, mLastDesiredShakeOffset);
            mLastDesiredShakeOffset *= amp - lenamp;
        }
        float fabs1 = std::fabs(mLastDesiredShakeAngOffset.x) - localAng.x;
        if (fabs1 > 0) {
            if (mLastDesiredShakeAngOffset.x > 0) {
                fabs1 *= -1.0f;
            }
            mLastDesiredShakeAngOffset.x += fabs1;
        }
        float fabs2 = std::fabs(mLastDesiredShakeAngOffset.z) - localAng.y;
        if (fabs2 > 0) {
            if (mLastDesiredShakeAngOffset.z > 0) {
                fabs2 *= -1.0f;
            }
            mLastDesiredShakeAngOffset.z += fabs2;
        }

        Vector3 spring;
        Subtract(mLastDesiredShakeOffset, mLastShakeOffset, spring);
        bool usePPFPS = false;
        if (RndPostProc::Current() && RndPostProc::Current()->EmulateFPS() > 0)
            usePPFPS = true;
        float emulateFPS = usePPFPS ? RndPostProc::Current()->EmulateFPS() : 60.0f;
        float fps = 60.0f / emulateFPS;
        spring *= 0.02f;
        Vector3 vel = mShakeVelocity;
        vel *= fps;
        ::Add(mLastShakeOffset, vel, mLastShakeOffset);
        ::Add(mShakeVelocity, spring, mShakeVelocity);
        ::Add(mLastShakeOffset, spring, mLastShakeOffset);
        float powed = std::pow(0.9f, fps);
        mShakeVelocity *= powed;

        Subtract(mLastDesiredShakeAngOffset, mLastShakeAngOffset, spring);
        spring *= 0.02f;
        Vector3 angVel = mShakeAngVelocity;
        angVel *= fps;
        ::Add(mLastShakeAngOffset, angVel, mLastShakeAngOffset);
        ::Add(mShakeAngVelocity, spring, mShakeAngVelocity);
        ::Add(mLastShakeAngOffset, spring, mLastShakeAngOffset);
        mShakeAngVelocity *= powed;
    }
    offset = mLastShakeOffset;
    angOffset = mLastShakeAngOffset;
}

bool CamShot::SetPos(CamShotFrame &frame, RndCam *cam) {
    RndCam *c = cam ? cam : GetCam();
    if (!c) {
        return false;
    } else {
        cam = c;
        frame.mWorldOffset = cam->WorldXfm();
        if (frame.HasTargets()) {
            Vector3 targetPos;
            frame.GetCurrentTargetPosition(targetPos);
            cam->WorldToScreen(targetPos, frame.mScreenOffset);
            frame.mScreenOffset += Vector2(-0.5f, -0.5f);
            frame.mScreenOffset.x *= 2.0f;
            frame.mScreenOffset.y *= -2.0f;

            Vector3 camToTarget;
            Subtract(targetPos, frame.mWorldOffset.v, camToTarget);
            Vector3 yComponent(cam->WorldXfm().m.y);
            yComponent *= Dot(cam->WorldXfm().m.y, camToTarget);
            Vector3 projectedCamPos;
            ::Add(yComponent, cam->WorldXfm().v, projectedCamPos);
            Vector3 targetOffset;
            Subtract(targetPos, projectedCamPos, targetOffset);
            ::Add(frame.mWorldOffset.v, targetOffset, frame.mWorldOffset.v);
        } else {
            frame.mScreenOffset.Zero();
        }

        frame.mFOV = cam->YFov();

        RndTransformable *frameParent = frame.mParent;
        if (frameParent) {
            Transform parentXfm(frameParent->WorldXfm());
            if (!frame.mUseParentRotation) {
                parentXfm.m.Identity();
            }
            Transform invParent;
            FastInvert(parentXfm, invParent);
            Multiply(frame.mWorldOffset, invParent, frame.mWorldOffset);
        }

        if (mPath && &mKeyframes[0] == &frame) {
            Transform pathXfm;
            mPath->MakeTransform(0, pathXfm, true, 1.0f);
            frame.mWorldOffset.v -= pathXfm.v;
            if (!frame.HasTargets()) {
                frame.mWorldOffset.m.Identity();
            }
        }

        return true;
    }
}

DataNode CamShot::OnHasTargets(DataArray *da) {
    return mKeyframes[da->Int(2)].HasTargets();
}

DataNode CamShot::OnRadio(DataArray *da) {
    int i2 = da->Int(2);
    int i3 = da->Int(3);
    if (mFlags & i2) {
        mFlags &= ~i3;
        mFlags |= i2;
    }
    return 0;
}

bool CamShot::ShotOk(CamShot *shot) {
    static Message msg("shot_ok", 0);
    msg[0] = shot;
    DataNode handled = HandleType(msg);
    if (handled.Type() != kDataUnhandled) {
        if (handled.Type() == kDataString) {
            CAMERA_LOG("Shot %s rejected: %s.\n", Name(), handled.Str());
            return false;
        } else if (handled.Int() == 0) {
            CAMERA_LOG("Shot %s rejected: not ok.\n", Name());
            return false;
        } else {
            return true;
        }
    } else
        return true;
}

DataNode CamShot::OnSetPos(DataArray *da) {
    int idx = da->Int(2);
    return SetPos(mKeyframes[idx], RndCam::Current());
}

DataNode CamShot::OnClearCrowdChars(DataArray *da) {
    int idx = da->Int(2);
    MILO_ASSERT(idx < mCrowds.size(), 0xb03);
    mCrowds[idx].ClearCrowdChars();
    return 0;
}

DataNode CamShot::OnAddCrowdChars(DataArray *da) {
    int idx = da->Int(2);
    MILO_ASSERT(idx < mCrowds.size(), 0xb0b);
    mCrowds[idx].AddCrowdChars();
    return 0;
}

DataNode CamShot::OnSetCrowdChars(DataArray *da) {
    int idx = da->Int(2);
    MILO_ASSERT(idx < mCrowds.size(), 0xb13);
    mCrowds[idx].SetCrowdChars();
    return 0;
}

void CamShot::StartAnims(ObjPtrList<RndAnimatable> &anims) {
    FOREACH (it, anims) {
        (*it)->StartAnim();
    }
}

void CamShot::EndAnims(ObjPtrList<RndAnimatable> &anims) {
    FOREACH (it, anims) {
        (*it)->EndAnim();
    }
}

void CamShot::SetFrames(ObjPtrList<RndAnimatable> &anims, float frame) {
    FOREACH (it, anims) {
        (*it)->SetFrame(frame, 1);
    }
}

void CamShot::SetShotOver() {
    static Message msg("shot_over");
    HandleType(msg);
    mShotOver = true;
}

void CamShot::AddAnim(RndAnimatable *anim) {
    if (mAnims.find(anim) == mAnims.end()) {
        mAnims.push_back(anim);
    }
}

void CamShot::DoHide() {
    CAMERA_LOG("** %s CamShot::DoHide() start\n", Name());
    if (!mHidden) {
        mEndHideList.clear();
        mEndShowList.clear();
        FOREACH (it, mHideList) {
            RndDrawable *cur = *it;
            if (cur->Showing()) {
                cur->SetShowing(false);
                mEndShowList.push_back(cur);
                CAMERA_LOG("   ** %s hide from mHideList\n", cur->Name());
            }
        }
        FOREACH (it, mGenHideList) {
            RndDrawable *cur = *it;
            if (cur->Showing()) {
                cur->SetShowing(false);
                mEndShowList.push_back(cur);
                CAMERA_LOG("   ** %s hide from mEndShowList\n", cur->Name());
            }
        }
#ifdef HX_NATIVE
        FOREACH (it, mGenHideVector) {
            RndDrawable *cur = *it;
            if (cur->Showing()) {
                cur->SetShowing(false);
                CAMERA_LOG("   ** %s hide from mGenHideVector\n", cur->Name());
                mEndShowList.push_back(cur);
            }
        }
#endif
        FOREACH (it, mShowList) {
            RndDrawable *cur = *it;
            if (!cur->Showing()) {
                cur->SetShowing(true);
                mEndHideList.push_back(cur);
                CAMERA_LOG("   ** %s show from mEndShowList\n", cur->Name());
            }
        }
        mHidden = true;
    }
    CAMERA_LOG("** %s CamShot::DoHide() stop\n", Name());
}

void CamShot::UnHide() {
    CAMERA_LOG(" ** %s CamShot::UnHide() start\n", Name());
    if (mHidden) {
        FOREACH (it, mEndHideList) {
            (*it)->SetShowing(false);
            CAMERA_LOG("   ** %s hide from mEndHideList\n", (*it)->Name());
        }
        FOREACH (it, mEndShowList) {
            (*it)->SetShowing(true);
            CAMERA_LOG("   ** %s show from mEndShowList\n", (*it)->Name());
        }
        mEndHideList.clear();
        mEndShowList.clear();
        mHidden = false;
    }
    CAMERA_LOG(" ** %s CamShot::UnHide() stop\n", Name());
}

RndCam *CamShot::GetCam() {
    RndCam *ret = 0;
    WorldDir *crowdDir = GetCrowdDir();
    if (crowdDir) {
        ret = crowdDir->Cam();
        if (ret == 0) {
            MILO_NOTIFY_ONCE("%s: paneldir but no cam", PathName(crowdDir));
        }
    } else {
        MILO_NOTIFY_ONCE("%s: no worlddir, so no cam", PathName(this));
    }
    return ret;
}

void CamShot::ClearCrowds() {
    ObjVector<CamShotCrowd>::iterator it = mCrowds.begin();
    while (it != mCrowds.end()) {
        if (!it->mCrowd) {
            mCrowds.erase(it);
        } else {
            ++it;
        }
    }
}

bool CamShot::AddCrowd(CamShotCrowd &crowd) {
    bool ret = true;
    FOREACH (it, mCrowds) {
        if (it->mCrowd == crowd.mCrowd) {
            ret = false;
            break;
        }
    }
    if (ret) {
        mCrowds.push_back(crowd);
    }
    return ret;
}

// sw2 scatter-include (default/CameraShot <- hamobj/DanceRemixer.cpp)
#define gRev gRev_DanceRemixer
#define gAltRev gAltRev_DanceRemixer
#include "hamobj/DanceRemixer.cpp"
#undef gRev
#undef gAltRev
