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
    virtual bool Replace(ObjRef *, Hmx::Object *);
    virtual void Poll(float);

    POOL_OVERLOAD(PropertyTask, 0x17)

protected:
    void SetProperty(DataNode &);

    ObjOwnerPtr<Hmx::Object> mTarget; // 0x2C
    DataNode mProperty; // 0x40
    DataNode mValue; // 0x48
    DataNode mStartValue; // 0x50
    float mDuration; // 0x58
    float mEasePower; // 0x5C
    bool mIsColorInterp; // 0x60
    ObjPtr<Hmx::Object> mListener; // 0x64
    float mElapsed; // 0x78
    EaseFunc *mEaseFunc; // 0x7C
};

class FlowSetProperty : public FlowNode, public PropertyEventListener {
protected:
    FlowSetProperty(void);
    u32 unk_0x74; // 0x74 - might be fake.
    FlowPtr<Hmx::Object> mTarget; // 0x78

    DataNode unk_0x98; // 0x98 - "property_path"
    DataNodeObjTrack mValue; // 0xA0
    bool mPersistent; // 0xBC
    int mRate; // 0xC0
    f32 mBlendTime; // 0xC4
    f32 mChangePerUnit; // 0xC8
    ObjOwnerPtr<Task> unk_0xCC; // 0xcc
    int mEase; // 0xE0
    f32 mEasePower; // 0xE4
    u8 unk_0xE8; // 0xe8
    int mStopMode; // 0xEC

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
    virtual bool Replace(ObjRef *from, Hmx::Object *to);

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
