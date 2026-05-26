#include "flow/FlowManager.h"
#include "flow/FlowNode.h"
#include "obj/ObjPtrVec_impl.h"
#include "obj/Data.h"
#include "os/Timer.h"
#include "rndobj/Env.h"
#include "rndobj/Overlay.h"
#include "rndobj/Trans.h"
#include "utl/MakeString.h"

template Hmx::Object *ObjPtrVec<RndTransformable, ObjectDir>::Node::RefOwner() const;
template ObjPtrVec<FlowNode, ObjectDir>::iterator
ObjPtrVec<FlowNode, ObjectDir>::erase(ObjPtrVec<FlowNode, ObjectDir>::iterator);
template void ObjPtrVec<RndEnviron, ObjectDir>::Set(
    ObjPtrVec<RndEnviron, ObjectDir>::iterator, RndEnviron *);

FlowManager *TheFlowMgr;

FlowManager::FlowManager() : unk2c(0), mExecuting(0), mPollables(this) {
    mFlowQueue.clear();
    mFrameCounterModulo = 0;
    mFrameTimeAccumulator = 0;
    mPeakFrameTime = 0;
    mLastFrameTime = 0;
    mElapsedTime = 0;
    for (int i = 0; i < 60; i++) {
        mFrameTimeSamples[i] = 0.0f;
    }
    mFlowOverlay = RndOverlay::Find("flow", false);
    mFlowPeakOverlay = RndOverlay::Find("flow_peak", false);
    mFlowTaskOverlay = RndOverlay::Find("flow_task", false);
    mFlowEventOverlay = RndOverlay::Find("flow_event", false);
}

FlowManager::~FlowManager() {}

void FlowManager::AddPollable(FlowNode *n) { mPollables.push_back(n); }
void FlowManager::RemovePollable(FlowNode *n) { mPollables.remove(n); }

void FlowManager::QueueCommand(FlowNode *n, FlowNode::QueueState q) {
    if (mExecuting && q != FlowNode::kQueueOne) {
        n->Execute(q);
    } else
        mFlowQueue[n] = q;
}

void FlowManager::CancelCommand(FlowNode *n) { mFlowQueue[n] = FlowNode::kImmediate; }

void FlowManager::AddEventTime(Symbol s, float f1) {
    float fsub = f1 - mElapsedTime;
    if (mEventTimes.find(s) != mEventTimes.end()) {
        DataNode &n = mEventTimes[s];
        float f7 = n.Array()->Float(0);
        int i5 = n.Array()->Int(1);
        float f8 = n.Array()->Float(2) + mElapsedTime;
        i5++;
        n.Array()->Node(0) = f7 + fsub;
        n.Array()->Node(1) = i5;
        n.Array()->Node(2) = f8;
    } else {
        DataArrayPtr ptr(fsub, 1, mElapsedTime);
        mEventTimes[s] = ptr;
    }
    mElapsedTime = 0;
}

void FlowManager::Poll() {
    mLastFrameTime = 0;
    Timer timer;
    timer.Reset();
    timer.Start();

    mExecuting = true;

    for (std::map<FlowNode *, FlowNode::QueueState>::iterator it = mFlowQueue.begin();
         it != mFlowQueue.end();
         ++it) {
        if (it->second != FlowNode::kImmediate) {
            it->first->Execute(it->second);
        }
    }
    mFlowQueue.clear();

    ObjPtrVec<FlowNode> polls(mPollables);
    FOREACH (it, polls) {
        (*it)->Execute(FlowNode::kWhenAble);
    }

    mExecuting = false;
    timer.Stop();
    unk2c = false;
    float timerMs = timer.Ms() - mLastFrameTime;

    float eventTimeSum = 0.0f;
    Symbol peakSym(NULL);
    Symbol peakElapsedSym(NULL);
    float maxElapsedTime = -1.0f;
    float maxEventTime = -1.0f;

    if (!mEventTimes.empty()) {
        maxEventTime = maxElapsedTime;
        if (mFlowEventOverlay->Showing()) {
            *mFlowEventOverlay << "\n\n\n\n\n\n\n\n\n\n";
        }

        for (std::map<Symbol, DataNode>::iterator it = mEventTimes.begin();
             it != mEventTimes.end();
             ++it) {
            DataNode node(it->second);
            float eventTime = node.Array()->Float(0);
            float elapsedTime = node.Array()->Float(2);

            eventTimeSum += eventTime;

            if (eventTime >= maxEventTime) {
                maxEventTime = eventTime;
                peakSym = it->first;
            }
            if (elapsedTime >= maxElapsedTime) {
                maxElapsedTime = elapsedTime;
                peakElapsedSym = it->first;
            }

            if (mFlowEventOverlay->Showing()) {
                float f2 = node.Array()->Float(2);
                float f0 = node.Array()->Float(0);
                int count = node.Array()->Int(1);
                *mFlowEventOverlay
                    << MakeString("%s    count: %i   time: %.3f ms   task: %.3f ms\n", it->first.Str(), count, f0, f2);
            }
        }

        if (mFlowOverlay->Showing()) {
            *mFlowOverlay << MakeString(
                "Worst:   FlowTime: %s  %.3f    TaskTime: %s  %.3f\n",
                peakSym.Str(),
                maxEventTime,
                peakElapsedSym.Str(),
                maxElapsedTime
            );
        }
    }

    float total = timerMs + eventTimeSum + mFrameTimeAccumulator;

    if (mFlowOverlay->Showing()) {
        *mFlowOverlay << MakeString(
            "Events: %.3f ms  %i Commands in %.3f ms  Release: %.3f ms  Tasks: %.3f ms\n",
            total,
            (int)mEventTimes.size(),
            maxEventTime,
            maxElapsedTime,
            timerMs
        );
    }

    mFrameTimeSamples[mFrameCounterModulo] = total;
    mFrameCounterModulo++;

    if (total > mPeakFrameTime) {
        mPeakFrameTime = total;
        DataArrayPtr ptr(peakSym, maxEventTime, peakElapsedSym, maxElapsedTime, total);
        DataNode dn(ptr);
        mPeakFrameInfo = dn;
    }

    if (mFrameCounterModulo >= 60) {
        mAvgFrameTime = 0;
        for (int i = 0; i < 60; i++) {
            mAvgFrameTime += mFrameTimeSamples[i];
        }
        mPeakFrameTime = 0;
        mFrameCounterModulo = 0;
        mAvgFrameTime *= (1.0f / 60.0f);

        if (mPeakFrameInfo.Type() == kDataArray) {
            DataArray *arr = mPeakFrameInfo.Array();
            float peakTime = arr->Float(1);
            if (peakTime > 0 && mFlowPeakOverlay->Showing()) {
                float pt = arr->Float(1);
                Symbol s = arr->Sym(0);
                *mFlowPeakOverlay << MakeString("%s %.1f", s, pt);
            }
            float peakElapsed = arr->Float(3);
            if (peakElapsed > 0 && mFlowTaskOverlay->Showing()) {
                float pe = arr->Float(3);
                Symbol s2 = arr->Sym(2);
                *mFlowTaskOverlay << MakeString("%s %.1f", s2, pe);
            }
        }
    }

    if (mFlowOverlay->Showing()) {
        *mFlowOverlay
            << MakeString("Average: %.3f ms   Peak: %.3f ms    Frame: %.3f ms\n", mAvgFrameTime, mPeakFrameTime, total);
    }

    Symbol flowSym("flow");
    Timer *autoTimer = AutoTimer::GetTimer(flowSym);
    if (autoTimer) {
        autoTimer->SetLastMs(total);
    }

    mEventTimes.clear();
    mFrameTimeAccumulator = 0;
    mLastFrameTime = 0;
}
