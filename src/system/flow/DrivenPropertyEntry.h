#pragma once
#include "obj/Data.h"
#include "obj/Object.h"
#include "flow/DrivenPropertyMathOps.h"
#include "utl/BinStream.h"

class FlowNode;

class DrivenPropertyEntry {
    friend class FlowNode;

public:
    DrivenPropertyEntry(Hmx::Object *);
    ~DrivenPropertyEntry();
    void Save(BinStream &);
    void Load(BinStream &, FlowNode *);

    bool Empty() { return mMathOps.empty(); }
    const DataNode &Node() const { return mNode; }
    const ObjVector<FlowMathOp> &MathOps() const { return mMathOps; }

protected:
    DataNode mNode; // 0x0
    ObjVector<FlowMathOp> mMathOps; // 0x8
};
