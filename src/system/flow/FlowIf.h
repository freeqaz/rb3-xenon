#pragma once
#include "flow/FlowNode.h"

/** "a single case decision node" */
class FlowIf : public FlowNode {
public:
    // Hmx::Object
    virtual ~FlowIf();
    OBJ_CLASSNAME(FlowIf)
    OBJ_SET_TYPE(FlowIf)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, CopyType);
    virtual void Load(BinStream &);
    // FlowNode
    virtual bool Activate();

    OBJ_MEM_OVERLOAD(0x1B)
    NEW_OBJ(FlowIf)

protected:
    FlowIf();

    /** "left hand value in comparison" */
    DataNodeObjTrack mValue1; // 0x5c
    /** "right hand value in comparison" */
    DataNodeObjTrack mValue2; // 0x78
    /** "equality case to use for comparison" */
    OperatorType mOperator; // 0x94
};
