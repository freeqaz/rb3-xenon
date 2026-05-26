#include "flow/FlowSequence.h"
#include "flow/FlowNode.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "utl/MakeString.h"

FlowSequence::FlowSequence()
    : mItr(), mLooping(0), mRepeats(0), mRepeatCount(0), mStopMode(kStopImmediate),
      mIsAdvancing(0) {}

FlowSequence::~FlowSequence() {}

bool FlowSequence::Activate() {
    FLOW_LOG("Activate\n");
    mStopRequested = false;
    if (IsRunning()) {
        MILO_NOTIFY(
            "FlowSequence re-entrance error, activated when already running, deactivating and aborting, check your logic"
        );
        Deactivate(false);
        return false;
    }
    if (mRepeatCount == 0) {
        PushDrivenProperties();
    }
    mRepeatCount = 0;
    mItr = mChildNodes.begin();
    if (!mChildNodes.empty()) {
        mIsAdvancing = true;
        while (mItr != mChildNodes.end()) {
            if (!mRunningNodes.empty())
                break;
            ActivateChild(mItr->Obj());
            if (mStopRequested || !mRunningNodes.empty())
                break;
            ++mItr;
        }
        mIsAdvancing = false;
    }
    MILO_ASSERT(mRunningNodes.size() < 2, 0x50);
    if (mItr == mChildNodes.end()) {
        if (mRunningNodes.size() != 0)
            return true;
        if (!mLooping) {
            if (mRepeats == 0)
                return false;
        }
        MILO_NOTIFY_ONCE(
            "Instant looping sequence in %s! Stopping Sequence", FindPathName()
        );
        return mRunningNodes.size() > 0;
    }
    return true;
}

BEGIN_HANDLERS(FlowSequence)
    HANDLE_SUPERCLASS(FlowNode)
END_HANDLERS

BEGIN_PROPSYNCS(FlowSequence)
    SYNC_PROP(looping, mLooping)
    SYNC_PROP(repeats, mRepeats)
    SYNC_PROP(stop_mode, (int &)mStopMode)
    SYNC_SUPERCLASS(FlowNode)
END_PROPSYNCS

BEGIN_SAVES(FlowSequence)
    SAVE_REVS(1, 0)
    SAVE_SUPERCLASS(FlowNode)
    bs << mLooping;
    bs << mRepeats;
    bs << mStopMode;
END_SAVES

BEGIN_COPYS(FlowSequence)
    COPY_SUPERCLASS(FlowNode)
    CREATE_COPY(FlowSequence)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mLooping)
        COPY_MEMBER(mRepeats)
        COPY_MEMBER(mStopMode)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(1, 0)

BEGIN_LOADS(FlowSequence)
    LOAD_REVS(bs)
    ASSERT_REVS(1, 0)
    FlowNode::Load(bs);
    d >> mLooping;
    bs >> mRepeats;
    if (d.rev > 0)
        bs >> (int &)mStopMode;
END_LOADS

void FlowSequence::ChildFinished(FlowNode *node) {
    FLOW_LOG(
        "Child Finished of class:%s ; potential advance of iterator\n", node->ClassName()
    );
    mRunningNodes.remove(node);
    MILO_ASSERT(mRunningNodes.empty(), 0x74);
    if (mIsAdvancing)
        return;
    if (mStopRequested) {
        mStopRequested = false;
        FLOW_LOG("Releasing\n");
        mFlowParent->ChildFinished(this);
        return;
    }
    if (mItr != mChildNodes.end()) {
        ++mItr;
    }
    FLOW_LOG("Advancing sequence\n");
    mIsAdvancing = true;
    while (mItr != mChildNodes.end()) {
        ActivateChild(mItr->Obj());
        if (mStopRequested || !mRunningNodes.empty())
            break;
        ++mItr;
    }
    mIsAdvancing = false;
    if (!mStopRequested || !mRunningNodes.empty()) {
        if (mItr != mChildNodes.end())
            goto ret;
        if (!mLooping && mRepeatCount >= mRepeats - 1) {
            MILO_ASSERT(mRunningNodes.empty(), 0xA1);
            FLOW_LOG("Releasing\n");
        } else if (Activate()) {
            mRepeatCount++;
            goto ret;
        }
    }
    mFlowParent->ChildFinished(this);
ret:
    MILO_ASSERT(mRunningNodes.size() < 2, 0xA6);
}

void FlowSequence::RequestStop() {
    FLOW_LOG("RequestStop\n");
    if (mStopMode == kStopImmediate)
        FlowNode::RequestStop();
}

void FlowSequence::RequestStopCancel() {
    FLOW_LOG("RequestStop\n");
    FlowNode::RequestStopCancel();
}
