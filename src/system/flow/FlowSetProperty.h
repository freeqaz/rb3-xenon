#pragma once
#include "flow/FlowNode.h"
#include "flow/FlowPtr.h"
#include "flow/PropertyEventListener.h"
#include "math/Easing.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "utl/PoolAlloc.h"

class PropertyTask : public Task {
public:
    PropertyTask(
        Hmx::Object *,
        DataNode &,
        DataNode &,
        TaskUnits,
        float,
        EaseType,
        float,
        bool,
        Hmx::Object *
    );
    virtual ~PropertyTask();
    OBJ_CLASSNAME(PropertyTask)
    virtual void Replace(ObjRef *, Hmx::Object *);
    virtual void Poll(float);

    POOL_OVERLOAD(PropertyTask, 0x17)

protected:
    void SetProperty(DataNode &);

    ObjOwnerPtr<Hmx::Object> mTarget; // 0x28
    DataNode mProperty; // 0x34
    DataNode mValue; // 0x3c
    DataNode mStartValue; // 0x44
    float mDuration; // 0x4c
    float mEasePower; // 0x50
    bool mIsColorInterp; // 0x54
    ObjPtr<Hmx::Object> mListener; // 0x58
    float mElapsed; // 0x64
    EaseFunc *mEaseFunc; // 0x68
};

class FlowSetProperty : public FlowNode, public PropertyEventListener {
protected:
    FlowSetProperty(void);
    u32 unk_0x74; // 0x78 - might be fake.
    FlowPtr<Hmx::Object> mTarget; // 0x7c

    DataNode unk_0x98; // 0x94 - "property_path"
    DataNodeObjTrack mValue; // 0x9c
    bool mPersistent; // 0xb0
    int mRate; // 0xb4
    f32 mBlendTime; // 0xb8
    f32 mChangePerUnit; // 0xbc
    ObjOwnerPtr<Task> unk_0xCC; // 0xc0
    int mEase; // 0xcc
    f32 mEasePower; // 0xd0
    u8 unk_0xE8; // 0xd4
    int mStopMode; // 0xd8

    void OnTargetChanged(void);
    void OnAnimEvent(Symbol);
    bool IsBlendable(void);

public:
    virtual ~FlowSetProperty();
    OBJ_CLASSNAME(FlowSetProperty)
    OBJ_SET_TYPE(FlowSetProperty)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, CopyType);
    virtual void Load(BinStream &);
    virtual void MoveIntoDir(ObjectDir *, ObjectDir *);
    virtual void Replace(ObjRef *from, Hmx::Object *to);

    virtual bool IsRunning(void);
    virtual bool Activate();
    virtual void Deactivate(bool);
    virtual void RequestStop();
    virtual void RequestStopCancel();
    virtual void Execute(QueueState);
    virtual void ChildFinished(FlowNode *);
    virtual void MiloPreRun();
    virtual void UpdateIntensity(void);

    void ReActivate(void);

    OBJ_MEM_OVERLOAD(0x20)
    NEW_OBJ(FlowSetProperty)
};
