#include "flow/FlowOnStop.h"
#include "FlowOnStop.h"
#include "flow/FlowManager.h"
#include "flow/FlowNode.h"
#include "obj/Object.h"

FlowOnStop::FlowOnStop() : mMode(kAlways), mArmed(0) {}
FlowOnStop::~FlowOnStop() {}

BEGIN_HANDLERS(FlowOnStop)
    HANDLE_SUPERCLASS(FlowNode)
END_HANDLERS

BEGIN_PROPSYNCS(FlowOnStop)
    SYNC_PROP(mode, (int &)mMode)
    SYNC_SUPERCLASS(FlowNode)
END_PROPSYNCS

BEGIN_SAVES(FlowOnStop)
    SAVE_REVS(0, 0)
    SAVE_SUPERCLASS(FlowNode)
    bs << mMode;
END_SAVES

BEGIN_COPYS(FlowOnStop)
    COPY_SUPERCLASS(FlowNode)
    CREATE_COPY(FlowOnStop)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mMode)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(0, 0)

BEGIN_LOADS(FlowOnStop)
    LOAD_REVS(bs)
    ASSERT_REVS(0, 0)
    LOAD_SUPERCLASS(FlowNode)
    d >> (int &)mMode;
END_LOADS

bool FlowOnStop::Activate() {
    FLOW_LOG("Activate\n");
    mStopRequested = false;
    mArmed = true;
    return true;
}

void FlowOnStop::Deactivate(bool b1) {
    FLOW_LOG("Deactivated, which can cause this node to activate!\n");
    if (mArmed) {
        if (mMode != kRequestStopOnly) {
            FOREACH (it, mChildNodes) {
                ActivateChild(*it);
                if (mStopRequested)
                    break;
            }
            FlowNode::Deactivate(b1);
        }
        mArmed = false;
    }
}

void FlowOnStop::ChildFinished(FlowNode *n) {
    FLOW_LOG("Child Finished of class:%s\n", n->ClassName());
    mRunningNodes.remove(n);
    if (mRunningNodes.empty()) {
        mArmed = false;
        FLOW_LOG("Timed Release From Parent \n");
        Timer timer;
        timer.Reset();
        timer.Start();
        mFlowParent->ChildFinished(this);
        timer.Stop();
        TheFlowMgr->AddMs(timer.Ms());
    }
}

void FlowOnStop::RequestStop() {
    FLOW_LOG("RequestStop\n");
    if (mRunningNodes.empty()) {
        if (mMode != kDeactivateOnly) {
            mStopRequested = true;
            TheFlowMgr->QueueCommand(this, kQueue);
        } else {
            mArmed = false;
            mFlowParent->ChildFinished(this);
        }
    } else {
        FlowNode::RequestStop();
    }
}

void FlowOnStop::RequestStopCancel() {
    FLOW_LOG("RequestStopCancel\n");
    FlowNode::RequestStopCancel();
}

void FlowOnStop::Execute(QueueState qs) {
    if (qs == kQueue && mStopRequested) {
        mStopRequested = false;
        mArmed = false;
        FlowNode::Activate();
        if (mRunningNodes.empty()) {
            FLOW_TIMED_RELEASE_FROM_PARENT;
        }
    }
}

bool FlowOnStop::IsRunning() { return mArmed || FlowNode::IsRunning(); }
