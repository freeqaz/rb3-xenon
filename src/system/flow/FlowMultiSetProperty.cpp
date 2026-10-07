#include "flow/FlowMultiSetProperty.h"
#include "flow/DrivenPropertyEntry.h"
#include "obj/ObjPtrVec_impl.h"
#include "flow/FlowNode.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/DirLoader.h"
#include "obj/Object.h"
#include "char/CharClipSet.h"

FlowMultiSetProperty::FlowMultiSetProperty()
    : mTargets(this, (EraseMode)1, kObjListNoNull) {}

FlowMultiSetProperty::~FlowMultiSetProperty() {}

BEGIN_HANDLERS(FlowMultiSetProperty)
    HANDLE_SUPERCLASS(FlowNode)
END_HANDLERS

BEGIN_PROPSYNCS(FlowMultiSetProperty)
    SYNC_PROP_MODIFY(targets, mTargets, (mTargets.sort(ObjNameSort()), mTargets.unique()))
    SYNC_PROP(value, mProperty)
    SYNC_SUPERCLASS(FlowNode)
END_PROPSYNCS

BEGIN_SAVES(FlowMultiSetProperty)
    SAVE_REVS(0, 0)
    SAVE_SUPERCLASS(FlowNode)
    bs << mTargets;
    bs << mProperty << mPropertyValue;
END_SAVES

BEGIN_COPYS(FlowMultiSetProperty)
    COPY_SUPERCLASS(FlowNode)
    CREATE_COPY_AS(FlowMultiSetProperty, c)
    BEGIN_COPYING_MEMBERS_FROM(c)
        COPY_MEMBER(mTargets)
        COPY_MEMBER(mProperty)
        COPY_MEMBER(mPropertyValue)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(0, 0)

BEGIN_LOADS(FlowMultiSetProperty)
    LOAD_REVS(bs)
    ASSERT_REVS(0, 0)
    LOAD_SUPERCLASS(FlowNode)
    mTargets.Load(bs, true, Dir());
    bs >> mProperty >> mPropertyValue;
END_LOADS

bool FlowMultiSetProperty::Activate() {
    FLOW_LOG("Activate\n");
    mStopRequested = false;
    if (!mTargets.empty()) {
        DrivenPropertyEntry *node = GetDrivenEntry("value");
        if (node != nullptr) {
            mPropertyValue = mTargets[0]->Property(mProperty.Array(), true)->Evaluate();
        }
    }
    FlowNode::PushDrivenProperties();
    FOREACH (it, mTargets) {
        if (*it) {
            (*it)->SetProperty(mProperty.Array(), mPropertyValue);
        }
    }
    return false;
}
