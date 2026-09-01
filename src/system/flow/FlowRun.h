#pragma once
#include "flow/Flow.h"
#include "flow/FlowNode.h"
#include "flow/FlowPtr.h"
#include "obj/Dir.h"
#include "utl/Str.h"

/** "Run or stop another Flow" */
class FlowRun : public FlowNode {
public:
    // Hmx::Object
    virtual ~FlowRun();
    OBJ_CLASSNAME(FlowRun)
    OBJ_SET_TYPE(FlowRun)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, CopyType);
    virtual void Load(BinStream &);
    // FlowNode
    virtual bool Activate();
    virtual void ChildFinished(FlowNode *);
    virtual void RequestStop();
    virtual void RequestStopCancel();

    // laneAT-f4 opt-out: the retail bytes show FlowRun's operator new was kept
    // OUT OF LINE and ICF-folded (its `new` site is a single
    // `bl ??2<folded>@@SAPAXI@Z` with NO StaticClassName call), unlike the
    // OBJ_MEM_OVERLOAD majority which retail inlined. Classified from the
    // CTOR relocation, not the symbol name -- see
    // /home/free/tmp/laneAT/f4/newobj_classify.py.
    MEM_OVERLOAD(FlowRun, 0x17)
    NEW_OBJ(FlowRun)

    void ResolveTarget();

protected:
    FlowRun();

    void OnTargetDirChange();
    void OnTargetChange();

    /** "Allows you to target flows inside of proxies" */
    FlowPtr<ObjectDir> mTargetDir; // 0x60
    /** "Flow to start or stop" */
    FlowPtr<Flow> mTarget; // 0x78
    String mTargetName; // 0x90
    /** "Stop instead of starting the target flow?" */
    bool mStop; // 0x9c
    /** "If true, we don't track the running state of the target flow" */
    bool mImmediateRelease; // 0x9d
};
