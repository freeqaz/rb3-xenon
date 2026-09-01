#pragma once
#include "flow/FlowNode.h"
#include "flow/FlowPtr.h"
#include "rndobj/Trans.h"

/** "Runs children when two trans objects are within a range" */
class FlowDistance : public FlowNode {
public:
    // Hmx::Object
    virtual ~FlowDistance();
    OBJ_CLASSNAME(FlowDistance)
    OBJ_SET_TYPE(FlowDistance)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, CopyType);
    virtual void Load(BinStream &);
    // FlowNode
    virtual bool Activate();
    virtual void Deactivate(bool);
    virtual void ChildFinished(FlowNode *);
    virtual void RequestStop();
    virtual void RequestStopCancel();
    virtual void Execute(QueueState);
    virtual bool IsRunning();
    virtual void UpdateIntensity();

    // laneAT-f4 opt-out: the retail bytes show FlowDistance's operator new was kept
    // OUT OF LINE and ICF-folded (its `new` site is a single
    // `bl ??2<folded>@@SAPAXI@Z` with NO StaticClassName call), unlike the
    // OBJ_MEM_OVERLOAD majority which retail inlined. Classified from the
    // CTOR relocation, not the symbol name -- see
    // /home/free/tmp/laneAT/f4/newobj_classify.py.
    MEM_OVERLOAD(FlowDistance, 0x1D)
    NEW_OBJ(FlowDistance)

protected:
    FlowDistance();

    /** "First object to compare" */
    FlowPtr<RndTransformable> mObj1; // 0x60
    /** "Second object to compare" */
    FlowPtr<RndTransformable> mObj2; // 0x78
    /** "Distance for comparison" */
    float mDistance; // 0x90
    /** "Is the node persistent?" */
    bool mPersistent; // 0x94
    bool mPolling; // 0x95
    bool mOutOfRange; // 0x96
    /** "Run children when closer than distance value" */
    bool mRunInRange; // 0x97
    /** "Applies current distance to flow intensity, closer being higher intensity" */
    bool mDriveIntensity; // 0x98
    float mIntensityScale; // 0x9c
    // Retail RB3 sizes FlowNode-derived objects 0x10 larger than dc3's (newer)
    // engine layout; the extra 16 bytes sit in the trailing virtual-Hmx::Object
    // region and are not referenced by any accessor (all field functions match).
    // NewObject/??_G size immediates (0xdc) confirm this via the sizeof oracle.
    char _retailTrailingPad[16]; // 0xa0
};
