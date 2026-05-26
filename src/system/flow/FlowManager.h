#pragma once
#include "obj/Object.h"
#include "flow/FlowNode.h"
#include "rndobj/Overlay.h"
#include <map>

class FlowManager : public Hmx::Object {
public:
    FlowManager();
    virtual ~FlowManager();

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
    std::map<FlowNode *, FlowNode::QueueState> mFlowQueue; // 0x30
    ObjPtrVec<FlowNode> mPollables; // 0x48
    std::map<Symbol, DataNode> mEventTimes; // 0x64
    float mFrameTimeAccumulator; // 0x7c
    float mPeakFrameTime; // 0x80
    RndOverlay *mFlowOverlay; // 0x84
    RndOverlay *mFlowPeakOverlay; // 0x88
    RndOverlay *mFlowTaskOverlay; // 0x8c
    RndOverlay *mFlowEventOverlay; // 0x90
    int mFrameCounterModulo; // 0x94
    float mFrameTimeSamples[60]; // 0x98
    float mAvgFrameTime; // 0x188
    float mLastFrameTime; // 0x18c
    float mElapsedTime; // 0x190
    DataNode mPeakFrameInfo; // 0x194
};

extern FlowManager *TheFlowMgr;
