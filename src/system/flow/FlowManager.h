#pragma once
#include "obj/Object.h"
#include "flow/FlowNode.h"
#include "rndobj/Overlay.h"
#include <map>

class FlowManager : public Hmx::Object {
public:
    FlowManager();

    void AddPollable(FlowNode *);
    void RemovePollable(FlowNode *);
    void Poll();
    void AddEventTime(Symbol, float);
    void QueueCommand(FlowNode *, FlowNode::QueueState);
    void CancelCommand(FlowNode *);
    void AddMs(float ms) { mFrameTimeAccumulator += ms; }

protected:
    bool unk2c;
    bool mExecuting;
    std::map<FlowNode *, FlowNode::QueueState> mFlowQueue; // 0x2c
    ObjPtrVec<FlowNode> mPollables; // 0x44
    std::map<Symbol, DataNode> mEventTimes; // 0x60
    float mFrameTimeAccumulator; // 0x78
    float mPeakFrameTime; // 0x7c
    RndOverlay *mFlowOverlay; // 0x80
    RndOverlay *mFlowPeakOverlay; // 0x84
    RndOverlay *mFlowTaskOverlay; // 0x88
    RndOverlay *mFlowEventOverlay; // 0x8c
    int mFrameCounterModulo; // 0x90
    float mFrameTimeSamples[60]; // 0x94
    float mAvgFrameTime; // 0x184
    float mLastFrameTime; // 0x188
    float mElapsedTime; // 0x18c
    DataNode mPeakFrameInfo; // 0x190
};

extern FlowManager *TheFlowMgr;
