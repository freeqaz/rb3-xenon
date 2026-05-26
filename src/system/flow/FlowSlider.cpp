#include "flow/FlowSlider.h"
#include "flow/FlowDistance.h"
#include "obj/ObjPtrVec_impl.h"
#include "flow/FlowManager.h"
#include "flow/FlowNode.h"
#include "flow/FlowValueCase.h"
#include "flow/PropertyEventListener.h"
#include "flow/Flow.h"
#include "math/Easing.h"
#include "obj/Object.h"
#include "os/Debug.h"

void FlowDistance::RequestStop() {
    FLOW_LOG("RequestStop\n");
    FlowNode::RequestStop();
}

void FlowDistance::RequestStopCancel() {
    FLOW_LOG("RequestStopCancel\n");
    FlowNode::RequestStopCancel();
}

bool SliderChildSort(FlowNode *a, FlowNode *b) {
    return static_cast<FlowValueCase *>(a)->Value() < static_cast<FlowValueCase *>(b)->Value();
}

FlowSlider::FlowSlider()
    : PropertyEventListener(this), mPersistent(1), mAlwaysRun(0), mValue(0),
      mEaseType(kEasePolyOut), mEasePower(2) {
    UpdateEase();
}

FlowSlider::~FlowSlider() {}

BEGIN_HANDLERS(FlowSlider)
    HANDLE_ACTION(reactivate, ReActivate())
    HANDLE_SUPERCLASS(FlowNode)
END_HANDLERS

BEGIN_PROPSYNCS(FlowSlider)
    SYNC_PROP(persistent, mPersistent)
    SYNC_PROP(always_run, mAlwaysRun)
    SYNC_PROP_MODIFY(ease, (int &)mEaseType, UpdateEase())
    SYNC_PROP(ease_power, mEasePower)
    SYNC_PROP_MODIFY(value, mValue, UpdateActivations())
    SYNC_SUPERCLASS(FlowNode)
END_PROPSYNCS

BEGIN_SAVES(FlowSlider)
    SAVE_REVS(0, 0)
    SAVE_SUPERCLASS(FlowNode)
    bs << mPersistent;
    bs << mAlwaysRun;
    bs << mValue;
    bs << mEaseType;
    bs << mEasePower;
END_SAVES

BEGIN_COPYS(FlowSlider)
    COPY_SUPERCLASS(FlowNode)
    CREATE_COPY_AS(FlowSlider, c)
    BEGIN_COPYING_MEMBERS_FROM(c)
        COPY_MEMBER(mPersistent)
        COPY_MEMBER(mAlwaysRun)
        COPY_MEMBER(mValue)
        COPY_MEMBER(mEaseType)
        COPY_MEMBER(mEasePower)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(0, 0)

BEGIN_LOADS(FlowSlider)
    LOAD_REVS(bs)
    ASSERT_REVS(0, 0)
    LOAD_SUPERCLASS(FlowNode)
    d >> mPersistent;
    d >> mAlwaysRun;
    bs >> mValue;
    bs >> (int &)mEaseType;
    bs >> mEasePower;
    GenerateAutoNames(this, true);
    mChildNodes.sort(SliderChildSort);
    UpdateEase();
END_LOADS

bool FlowSlider::Activate() {
    FLOW_LOG("Activate\n");
    mStopRequested = false;
    if (IsRunning()) {
        MILO_NOTIFY(
            "FlowSlider re-entrance error, activated when already running, deactivating and aborting, check your logic"
        );
        Deactivate(false);
        return false;
    } else {
        PushDrivenProperties();
        if (mPersistent) {
            RegisterEvents(this);
            mEventsRegistered = true;
        }
        UpdateActivations();
        if (mAlwaysRun) {
            return true;
        } else {
            return IsRunning();
        }
    }
}

void FlowSlider::Deactivate(bool b) {
    FLOW_LOG("Deactivated\n");
    if (mEventsRegistered)
        PropertyEventListener::UnregisterEvents(this);
    FlowNode::Deactivate(b);
}

void FlowSlider::ChildFinished(FlowNode *n) {
    FLOW_LOG("Child Finished of class:%s\n", n->ClassName());
    mRunningNodes.remove(n);
    if (mRunningNodes.empty()) {
        if (mEventsRegistered && mStopRequested) {
            UnregisterEvents(this);
            mEventsRegistered = false;
            mStopRequested = false;
        } else if (mEventsRegistered) {
            return;
        }
        mFlowParent->ChildFinished(this);
    }
}

void FlowSlider::RequestStop() {
    FLOW_LOG("RequestStop\n");
    FlowNode::RequestStop();
}

void FlowSlider::RequestStopCancel() {
    FLOW_LOG("RequestStopCancel\n");
    FlowNode::RequestStopCancel();
}

bool FlowSlider::IsRunning() {
    if (mEventsRegistered)
        return true;
    return FlowNode::IsRunning();
}

void FlowSlider::UpdateIntensity() {
    PushDrivenProperties();
    UpdateActivations();
}

__declspec(noinline) void FlowSlider::UpdateEase() {
    EaseType e = mEaseType;
    MILO_ASSERT(e >= kEaseLinear && e <= kEaseQuarterHalfStairstep, 0x16b);
    mEaseFunc = gEaseFuncs[e];
}

void FlowSlider::UpdateActivations() {
    float savedIntensity = FlowNode::sIntensity;

    auto cur = mChildNodes.begin();
    auto prev = mChildNodes.begin();
    auto next = mChildNodes.begin();

    float one = 1.0f;
    float zero = 0.0f;

    while (cur != mChildNodes.end()) {
        ++next;
        FlowValueCase *prevCase = static_cast<FlowValueCase *>(prev->Obj());
        FlowValueCase *curCase = static_cast<FlowValueCase *>(cur->Obj());
        FlowValueCase *nextCase = static_cast<FlowValueCase *>(next->Obj());
        prev = cur;

        float t;
        float intensity;

        if (next != mChildNodes.end()) {
            float curPos = curCase->Value();
            if (mValue >= curPos) {
                float nextPos = nextCase->Value();
                if (mValue <= nextPos) {
                    if (curPos != nextPos) {
                        t = (mValue - curPos) / (nextPos - curPos);
                    } else {
                        t = zero;
                    }
                    t = one - t;
                    goto ease;
                }
            }
        }

        {
            if (mValue <= curCase->Value() && mValue >= prevCase->Value()) {
                float curPos = curCase->Value();
                float prevPos = prevCase->Value();
                if (curPos != prevPos) {
                    t = (mValue - prevPos) / (curPos - prevPos);
                } else {
                    t = zero;
                }
                goto ease;
            }
        }

        {
            float curPos = curCase->Value();
            if (next == mChildNodes.end() && mValue > curPos) {
                intensity = one;
            } else if (cur == mChildNodes.begin() && mValue < curPos) {
                intensity = one;
            } else {
                intensity = zero;
            }
        }
        goto apply;

    ease:
        FlowNode::sIntensity = t;
        intensity = (float)mEaseFunc((double)t, (double)mEasePower, (double)one);

    apply:
        FlowNode::sIntensity = intensity * savedIntensity;

        if (curCase->IsRunning() > 0) {
            curCase->UpdateIntensity();
            if (FlowNode::sIntensity == 0.0f && mAlwaysRun == 0) {
                mRunningNodes.remove(curCase);
                curCase->Deactivate(false);
            }
        } else {
            if (mAlwaysRun > 0 || FlowNode::sIntensity != 0.0f) {
                ActivateChild(curCase);
            }
        }

        ++cur;
    }

    FlowNode::sIntensity = savedIntensity;
}

void FlowSlider::ReActivate() {
    Timer timer;
    timer.Restart();
    UpdateIntensity();
    timer.Stop();
    FlowNode *flow;
    for (flow = GetTopFlow(); flow->GetParent() != nullptr;
         flow = flow->GetParent()->GetTopFlow()) {
    }
    Symbol sym = MakeString("%s: %s->%s", ClassName(), flow->Dir()->Name(), flow->Name());
    TheFlowMgr->AddEventTime(sym, timer.Ms());
}

