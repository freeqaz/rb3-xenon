#include "flow/FlowSwitch.h"
#include "FlowSwitch.h"
#include "FlowSwitchCase.h"
#include "flow/DrivenPropertyEntry.h"
#include "flow/DrivenPropertyMathOps.h"
#include "flow/FlowNode.h"
#include "obj/Data.h"
#include "obj/Object.h"

FlowSwitch::FlowSwitch() : mFirstValidCaseOnly(1) { mPreviousValue = DataNode(kDataUndef, 0); }
FlowSwitch::~FlowSwitch() {}

BEGIN_HANDLERS(FlowSwitch)
    HANDLE_SUPERCLASS(FlowNode)
END_HANDLERS

BEGIN_PROPSYNCS(FlowSwitch)
    SYNC_PROP(value, mValue)
    SYNC_PROP(first_valid_case_only, mFirstValidCaseOnly)
    SYNC_SUPERCLASS(FlowNode)
END_PROPSYNCS

BEGIN_SAVES(FlowSwitch)
    SAVE_REVS(0, 0)
    SAVE_SUPERCLASS(FlowNode)
    bs << mFirstValidCaseOnly;
END_SAVES

BEGIN_COPYS(FlowSwitch)
    COPY_SUPERCLASS(FlowNode)
    CREATE_COPY(FlowSwitch)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mFirstValidCaseOnly)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(0, 0)

BEGIN_LOADS(FlowSwitch)
    LOAD_REVS(bs)
    ASSERT_REVS(0, 0)
    FlowNode::Load(bs);
    d >> mFirstValidCaseOnly;
    VerifyTypes();
    PushDrivenProperties();
    mPreviousValue = mValue;
END_LOADS

bool FlowSwitch::Activate() {
    FLOW_LOG("Activate\n");
    mStopRequested = false;
    if (IsRunning()) {
        MILO_NOTIFY(
            "FlowSwitch re-entrance error, activated when already running, deactivating and aborting, check your logic"
        );
        Deactivate(false);
        return false;
    } else {
        PushDrivenProperties();
        if (mValue.NotNull()) {
            if (mPreviousValue.Type() != mValue.Type()) {
                mPreviousValue = mValue;
            }
        } else {
            if (mValue.Type() == kDataObject) {
                mPreviousValue = NULL_OBJ;
            } else {
                mPreviousValue = 0;
            }
        }
        if (!ActivateTransitionCases(mValue, mPreviousValue)) {
            ActivateValueCases(mValue, mPreviousValue);
        }
        mPreviousValue = mValue;
        return FlowNode::IsRunning();
    }
}

void FlowSwitch::ChildFinished(FlowNode *n) {
    FLOW_LOG("Child Finished of class:%s\n", n->ClassName());
    PushDrivenProperties();
    FlowSwitchCase *switchCase = static_cast<FlowSwitchCase *>(n);
    if (switchCase && switchCase->Op() == kTransition) {
        mRunningNodes.remove(n);
        if (mValue != mPreviousValue) {
            DataNode dupe(mPreviousValue);
            mPreviousValue = mValue;
            if (!ActivateTransitionCases(mValue, dupe)) {
                ActivateValueCases(mValue, dupe);
            }
        } else {
            ActivateValueCases(mValue, mPreviousValue);
        }
        if (!FlowNode::IsRunning()) {
            mFlowParent->ChildFinished(this);
        }
    } else {
        FlowNode::ChildFinished(n);
    }
}

void FlowSwitch::VerifyTypes() {
    DrivenPropertyEntry *entry = GetDrivenEntry(("value"));
    if (!entry)
        return;
    const DataNode *currentValue = Property(entry->Node().Array(), true);
    DataNode currentCopy(*currentValue);

    DataNode targetValue(0);
    FlowMathOp &firstOp =
        const_cast<ObjVector<FlowMathOp> &>(entry->MathOps())[0];
    Hmx::Object *drivenObj = firstOp.DrivenObj();

    if (drivenObj) {
        const DataNode *drivenVal = drivenObj->Property(firstOp.Rhs().Array(NULL), false);
        if (drivenVal) {
            targetValue = *drivenVal;
        } else {
            targetValue = DataNode(firstOp.Default());
        }
    } else {
        targetValue = DataNode(firstOp.Default());
    }

    if (!targetValue.CompatibleType(currentCopy.Type())) {
        SetProperty(entry->Node().Array(NULL), targetValue);
    }
}

void FlowSwitch::ActivateValueCases(DataNode &value, DataNode &previousValue) {
    FLOW_LOG("Activating Value Cases\n");
    int matchCount = 0;
    bool pending = false;
    FOREACH (it, mChildNodes) {
        FlowSwitchCase *cur = static_cast<FlowSwitchCase *>(it->Obj());
        if (!cur->IsRunning() && cur->Op() != kTransition) {
            bool valid = cur->IsValidCase(this, &value, &previousValue, false);
            if (valid) {
                matchCount++;
                if (!cur->HasChildren()) {
                    pending = true;
                } else {
                    ActivateChild(cur);
                }
            } else {
                if (pending && cur->HasChildren()) {
                    matchCount++;
                    ActivateChild(cur);
                    pending = false;
                }
            }
            if (valid && mFirstValidCaseOnly && !pending)
                return;
            if (cur->Op() == kDefault && matchCount == 0) {
                ActivateChild(cur);
            }
            if (mStopRequested)
                return;
        }
    }
}

bool FlowSwitch::ActivateTransitionCases(DataNode &n1, DataNode &n2) {
    FOREACH (it, mChildNodes) {
        FlowSwitchCase *cur = static_cast<FlowSwitchCase *>(it->Obj());
        if (cur->Op() == kTransition && cur->IsValidCase(this, &n1, &n2, true)) {
            ActivateChild(cur);
            if (mStopRequested) {
                return !FlowNode::IsRunning();
            }
            if (FlowNode::IsRunning()) {
                return true;
            }
        }
    }
    return false;
}
