#pragma once
#include "flow/FlowNode.h"
#include "obj/Data.h"
#include "obj/Object.h"

class PropertyEventListener {
public:
    struct AutoPropEntry {
        AutoPropEntry(Hmx::Object *obj) : mPropertyArray(0), mProvider(obj) {}

        DataArray *mPropertyArray;
        ObjPtr<Hmx::Object> mProvider;
    };
    PropertyEventListener(Hmx::Object *);
    virtual ~PropertyEventListener() {}

protected:
    virtual void GenerateAutoNames(FlowNode *, bool);

    void RegisterEvents(FlowNode *);
    void UnregisterEvents(FlowNode *);

    friend class FlowSwitchCase;

    ObjVector<AutoPropEntry> mAutoPropEntries; // 0x4
    bool mEventsRegistered; // 0x14
};
