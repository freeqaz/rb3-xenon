#pragma once
#include "flow/FlowNode.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "obj/PropSync.h"
#include "obj/Task.h"
#include "rndobj/Anim.h"
#include "utl/BinStream.h"
#include "utl/PoolAlloc.h"

class FlowTimer : public FlowNode {
public:
    // Hmx::Object
    virtual ~FlowTimer();
    OBJ_CLASSNAME(FlowTimer)
    OBJ_SET_TYPE(FlowTimer)
    DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);

    // FlowNode
    virtual bool Activate();
    virtual void Deactivate(bool);
    virtual void ChildFinished(FlowNode *);
    virtual void RequestStop();
    virtual void RequestStopCancel();
    virtual void Execute(FlowNode::QueueState);
    virtual bool IsRunning();

    void OnKeyframe(FlowNode *);
    void OnTimerEnd();
    OBJ_MEM_OVERLOAD(0x17)
    NEW_OBJ(FlowTimer)

    int mStopMode; // 0x60
    ObjPtr<Task> mTask; // 0x64
    int mRate; // 0x70
    float mTotalTime; // 0x74

protected:
    FlowTimer();
};

class EventTask : public Task {
public:
    EventTask(FlowTimer *, ObjPtrVec<FlowNode> *, TaskUnits, float);
    virtual ~EventTask();
    OBJ_CLASSNAME(EventTask)
    virtual void Poll(float);

    POOL_OVERLOAD(EventTask, 0x12)

protected:
    ObjPtr<FlowTimer> mOwner; // 0x28
    ObjPtrVec<FlowNode> *mChildNodes; // 0x34
    ObjPtrVec<FlowNode>::iterator mCurNode; // 0x38
    float mDuration; // 0x3c
};
