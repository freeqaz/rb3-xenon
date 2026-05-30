#include "flow/FlowAnimate.h"
#include "flow/FlowLabel.h"
#include "flow/FlowManager.h"
#include "flow/FlowNode.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/Timer.h"
#include "rndobj/Anim.h"
#include "utl/MakeString.h"

FlowAnimate::FlowAnimate()
    : mAnimTask(this), mAnim(this), mStopMode(kStopLastFrame), mDeferredStopMode(0), mBlend(0), mWait(0),
      mDelay(0), mEnable(0), mRate(RndAnimatable::k30_fps), mStart(0), mEnd(0),
      mPeriod(0), mScale(1), mStopDeferred(0), mEase(kEaseLinear), mEasePower(2), mWrap(0),
      mImmediateRelease(0) {
    static Symbol range("range");
    mType = range;
}

FlowAnimate::~FlowAnimate() {
    TheFlowMgr->CancelCommand(this);
}

BEGIN_HANDLERS(FlowAnimate)
    HANDLE_ACTION(on_anim_event, OnAnimEvent(_msg->Sym(2)))
    HANDLE_ACTION(on_flow_finished, ChildFinished(_msg->Obj<FlowNode>(2)))
    HANDLE_SUPERCLASS(FlowNode)
END_HANDLERS

BEGIN_PROPSYNCS(FlowAnimate)
    SYNC_PROP_MODIFY(anim, mAnim, ResetAnim())
    SYNC_PROP(blend, mBlend)
    SYNC_PROP(wait, mWait)
    SYNC_PROP(delay, mDelay)
    SYNC_PROP(stop_mode, (int &)mStopMode)
    SYNC_PROP(enable, mEnable)
    SYNC_PROP(rate, (int &)mRate)
    SYNC_PROP(start, mStart)
    SYNC_PROP(end, mEnd)
    SYNC_PROP(scale, mScale)
    SYNC_PROP(period, mPeriod)
    SYNC_PROP(type, mType)
    SYNC_PROP(ease, (int &)mEase)
    SYNC_PROP(ease_power, mEasePower)
    SYNC_PROP(wrap, mWrap)
    SYNC_PROP(immediate_release, mImmediateRelease)
    SYNC_SUPERCLASS(FlowNode)
END_PROPSYNCS

BEGIN_SAVES(FlowAnimate)
    SAVE_REVS(3, 0)
    SAVE_SUPERCLASS(FlowNode)
    bs << mAnim << mBlend << mWait << mDelay;
    bs << mStopMode << mEnable;
    bs << mRate << mStart;
    bs << mEnd << mPeriod;
    bs << mType;
    bs << mScale << mEase << mEasePower;
    bs << mWrap;
    bs << mImmediateRelease;
END_SAVES

BEGIN_COPYS(FlowAnimate)
    COPY_SUPERCLASS(FlowNode)
    CREATE_COPY(FlowAnimate)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mAnim)
        COPY_MEMBER(mBlend)
        COPY_MEMBER(mDelay)
        COPY_MEMBER(mStopMode)
        COPY_MEMBER(mEnable)
        COPY_MEMBER(mRate)
        COPY_MEMBER(mStart)
        COPY_MEMBER(mEnd)
        COPY_MEMBER(mPeriod)
        COPY_MEMBER(mType)
        COPY_MEMBER(mScale)
        COPY_MEMBER(mEase)
        COPY_MEMBER(mEasePower)
        COPY_MEMBER(mWrap)
        COPY_MEMBER(mImmediateRelease)
    END_COPYING_MEMBERS
END_COPYS

void FlowAnimate::Load(BinStream &bs) {
    int revs;
    bs >> revs;
    BinStreamRev d(bs, revs);
    static const unsigned short gRevs[4] = { 3, 0, 0, 0 };
    if (d.rev > 3) {
        MILO_FAIL(
            "%s can't load new %s version %d > %d",
            PathName(this),
            ClassName(),
            d.rev,
            gRevs[0]
        );
    }
    if (d.altRev > 0) {
        MILO_FAIL(
            "%s can't load new %s alt version %d > %d",
            PathName(this),
            ClassName(),
            d.altRev,
            gRevs[2]
        );
    }

    FlowNode::Load(d.stream);

    if (d.rev < 3) {
        RndAnimatable *anim = mAnim.LoadFromMainOrDir(d.stream);
        mAnim = anim;
    } else {
        mAnim.LoadFromMainOrDir(d.stream);
    }

    d >> mBlend >> mWait >> mDelay;
    d >> (int&)mStopMode >> mEnable
      >> (int&)mRate >> mStart;
    d >> mEnd >> mPeriod;
    d >> mType;
    d >> mScale;
    if (d.rev > 0) {
        d >> (int &)mEase >> mEasePower;
        d >> mWrap;
    }
    if (d.rev > 1) {
        d >> mImmediateRelease;
    }
}

bool FlowAnimate::Activate() {
    FLOW_LOG("Activate\n");
    mStopRequested = false;
    PushDrivenProperties();
    RndAnimatable *anim = (RndAnimatable *)mAnim;
    if (anim) {
        if (mImmediateRelease) {
            mAnimTask = nullptr;
            if (mEnable) {
                float period = mPeriod;
                bool wrap = mWrap;
                int ease = mEase;
                Symbol type = mType;
                int rate = mRate;
                if (!(period == 0.0f)) {
                    float easePower = mEasePower;
                    float end = mEnd;
                    float start = mStart;
                    float delay = mDelay;
                    float blend = mBlend;
                    mAnim->Animate(
                        blend, mWait, delay, (RndAnimatable::Rate)rate, start, end,
                        period, 1.0f, type, this, (EaseType)ease, easePower, wrap
                    );
                } else {
                    float easePower = mEasePower;
                    float scale = mScale;
                    float end = mEnd;
                    float start = mStart;
                    float delay = mDelay;
                    float blend = mBlend;
                    mAnim->Animate(
                        blend, mWait, delay, (RndAnimatable::Rate)rate, start, end,
                        0.0f, scale, type, this, (EaseType)ease, easePower, wrap
                    );
                }
            } else {
                float delay = mDelay;
                float blend = mBlend;
                mAnim->Animate(blend, mWait, delay, this, kEaseLinear, 0.0f, false);
            }
        } else {
            if (mRunningNodes.empty()) {
                TheFlowMgr->QueueCommand(this, kQueue);
                return true;
            }
        }
    }
    return false;
}

void FlowAnimate::Execute(QueueState state) {
    FLOW_LOG("Execute: state = %i\n", state);
    if (IsRunning()) {
        if (mAnimTask && kIgnore == (int)state) {
            mAnimTask->mListener = NULL;
            if (mStopMode != kReleaseAndContinue) {
                AnimTask *task = mAnimTask;
                if (task) {
                    delete task;
                }
            }
            mAnimTask = nullptr;
            FLOW_LOG("Timed Release From Parent \n");
            Timer timer;
            timer.Reset();
            timer.Start();
            mFlowParent->ChildFinished(this);
            timer.Stop();
            TheFlowMgr->AddMs(timer.Ms());
        }
    } else {
        if (state == kQueue) {
            mStopDeferred = false;
            mDeferredStopMode = 0;
#ifdef HX_NATIVE
            if (!mAnim) {
                mFlowParent->ChildFinished(this);
                return;
            }
#endif
            Task *task;
            if (mEnable) {
                float period = mPeriod;
                bool wrap = mWrap;
                int ease = mEase;
                Symbol type = mType;
                int rate = mRate;
                if (!(period == 0.0f)) {
                    float easePower = mEasePower;
                    float end = mEnd;
                    float start = mStart;
                    float delay = mDelay;
                    float blend = mBlend;
                    task = mAnim->Animate(
                        blend, mWait, delay, (RndAnimatable::Rate)rate, start, end,
                        period, 1.0f, type, this, (EaseType)ease, easePower, wrap
                    );
                } else {
                    float easePower = mEasePower;
                    float scale = mScale;
                    float end = mEnd;
                    float start = mStart;
                    float delay = mDelay;
                    float blend = mBlend;
                    task = mAnim->Animate(
                        blend, mWait, delay, (RndAnimatable::Rate)rate, start, end,
                        0.0f, scale, type, this, (EaseType)ease, easePower, wrap
                    );
                }
            } else {
                float delay = mDelay;
                float blend = mBlend;
                task = mAnim->Animate(blend, mWait, delay, this, kEaseLinear, 0.0f, false);
            }
            mAnimTask = static_cast<AnimTask *>(task);
        } else if (state == kIgnore) {
            mFlowParent->ChildFinished(this);
        }
    }
}

void FlowAnimate::ChildFinished(FlowNode *node) {
    FLOW_LOG("Child Finished of class:%s\n", node->ClassName());
    mRunningNodes.remove(node);
    if (mRunningNodes.empty() && !mAnimTask && !mImmediateRelease) {
        FLOW_LOG("Timed Release From Parent \n");
        Timer timer;
        timer.Reset();
        timer.Start();
        mFlowParent->ChildFinished(this);
        timer.Stop();
        TheFlowMgr->AddMs(timer.Ms());
    }
}

void FlowAnimate::OnAnimEvent(Symbol sym) {
    FLOW_LOG("Event: %s\n", (char *)sym.Str());

    FOREACH (it, mChildNodes) {
        FlowNode *child = it->Obj();
        if (child->ClassName() == FlowLabel::StaticClassName()
            && ((FlowLabel *)child)->Label() == sym) {
            ActivateLabel((FlowLabel *)child);
            break;
        }
    }

    static Symbol sEnded("ended");
    static Symbol sStop("stop");
    static Symbol sNoStop("no_stop");
    static Symbol sInterrupted("interrupted");
    static Symbol sLooped("looped");

    if (sym == sInterrupted) {
        mAnimTask = nullptr;
        if (mRunningNodes.empty() && !mImmediateRelease) {
            FLOW_LOG("Timed Release From Parent \n");
            Timer timer;
            timer.Reset();
            timer.Start();
            mFlowParent->ChildFinished(this);
            timer.Stop();
            TheFlowMgr->AddMs(timer.Ms());
        }
    }

    if (sym == sEnded) {
        if (mAnimTask) {
            mAnimTask->mListener = NULL;
        }
        mAnimTask = nullptr;
        if (mRunningNodes.empty() && !mImmediateRelease) {
            FLOW_LOG("Timed Release From Parent \n");
            Timer timer;
            timer.Reset();
            timer.Start();
            mFlowParent->ChildFinished(this);
            timer.Stop();
            TheFlowMgr->AddMs(timer.Ms());
        }
    } else {
        if (sym == sStop) {
            if (mDeferredStopMode == kStopOnMarker || mDeferredStopMode == kStopBetweenMarkers) {
                TheFlowMgr->QueueCommand(this, kIgnore);
            }
            mDeferredStopMode = 0;
            mBetweenStopMarkers = true;
            return;
        }
        if (sym == sNoStop) {
            mDeferredStopMode = 0;
        } else {
            if (sym != sLooped) {
                return;
            }
            if (mStopDeferred && mAnimTask) {
                AnimTask *task = mAnimTask;
                task->mListener = NULL;
                delete (AnimTask *)mAnimTask;
                FLOW_LOG("Timed Release From Parent \n");
                Timer timer;
                timer.Reset();
                timer.Start();
                mFlowParent->ChildFinished(this);
                timer.Stop();
                TheFlowMgr->AddMs(timer.Ms());
                return;
            }
        }
        mBetweenStopMarkers = false;
    }
}

bool FlowAnimate::Replace(ObjRef *ref, Hmx::Object *obj) {
    if (RefIs(ref, mAnimTask)) {
        if (mAnimTask) {
            AnimTask *task = mAnimTask;
            if (task->mListener == (Hmx::Object *)this) {
                OnAnimEvent("interrupted");
            }
        }
        mAnimTask = nullptr;
        return true;
    }
    return Hmx::Object::Replace(ref, obj);
}

bool FlowAnimate::IsRunning() {
    if (!FlowNode::IsRunning()) {
        return mAnimTask != 0;
    }
    return true;
}

void FlowAnimate::RequestStopCancel() {
    FLOW_LOG("RequestStopCancel\n");
    TheFlowMgr->QueueCommand(this, kQueue);
    if (mStopDeferred) {
        mStopDeferred = false;
    }
    FlowNode::RequestStopCancel();
}

void FlowAnimate::RequestStop() {
    if (mAnimTask) {
        switch (mStopMode) {
        case kStopImmediate:
            TheFlowMgr->QueueCommand(this, kIgnore);
            break;
        case kStopLastFrame:
            mStopDeferred = true;
            break;
        case kStopOnMarker:
            mDeferredStopMode = kStopOnMarker;
            mStopDeferred = true;
            break;
        case kStopBetweenMarkers:
            if (!mBetweenStopMarkers) {
                mStopDeferred = true;
                mDeferredStopMode = kStopBetweenMarkers;
                break;
            }
            TheFlowMgr->QueueCommand(this, kIgnore);
            break;
        case kReleaseAndContinue:
            TheFlowMgr->QueueCommand(this, kIgnore);
            break;
        default:
            MILO_NOTIFY_ONCE("Bad Stop Mode value in animate case!");
            break;
        }
    }
    FlowNode::RequestStop();
}

void FlowAnimate::Deactivate(bool b) {
    FLOW_LOG("Deactivate\n");
    if (mAnimTask) {
        mAnimTask->mListener = NULL;
        AnimTask *task = mAnimTask;
        mAnimTask = NULL;
        delete task;
    }
    TheFlowMgr->CancelCommand(this);
    FlowNode::Deactivate(b);
}

void FlowAnimate::ResetAnim() {
    if (mAnim && !FlowNode::sPushDrivenProperties) {
        mRate = mAnim->GetRate();
        mStart = mAnim->StartFrame();
        mEnd = mAnim->EndFrame();
        mEase = kEaseLinear;
        mWrap = false;
        mPeriod = 0;
        mScale = 1;
        mEasePower = 2;
        static Symbol range("range");
        static Symbol loop("loop");
        mType = mAnim->Loop() ? loop : range;
    }
}
