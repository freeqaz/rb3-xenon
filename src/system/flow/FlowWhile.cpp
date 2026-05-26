#include "flow/FlowWhile.h"
#include "flow/Flow.h"
#include "flow/FlowManager.h"
#include "flow/FlowNode.h"
#include "flow/FlowSwitch.h"
#include "flow/FlowSwitchCase.h"
#include "flow/PropertyEventListener.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/Timer.h"

FlowWhile::FlowWhile() : PropertyEventListener(this), mEntryCount(0) {}

FlowWhile::~FlowWhile() {}

BEGIN_HANDLERS(FlowWhile)
    HANDLE_ACTION(reactivate, ReActivate())
    HANDLE_SUPERCLASS(FlowSwitch)
END_HANDLERS

BEGIN_PROPSYNCS(FlowWhile)
    SYNC_SUPERCLASS(FlowSwitch)
END_PROPSYNCS

BEGIN_SAVES(FlowWhile)
    SAVE_REVS(0, 0)
    SAVE_SUPERCLASS(FlowSwitch)
END_SAVES

BEGIN_COPYS(FlowWhile)
    COPY_SUPERCLASS(FlowSwitch)
    CREATE_COPY(FlowWhile)
    BEGIN_COPYING_MEMBERS
        if (IsRunning()) {
            Deactivate(false);
        }
        UnregisterEvents(this);
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(0, 0)

BEGIN_LOADS(FlowWhile)
    LOAD_REVS(bs)
    ASSERT_REVS(0, 0)
    LOAD_SUPERCLASS(FlowSwitch)
    GenerateAutoNames(this, true);
END_LOADS

bool FlowWhile::Activate() {
    FLOW_LOG("Activated \n");
    mStopRequested = false;
    if (IsRunning()) {
        MILO_NOTIFY(
            "FlowWhile re-entrance error, activated when already running, forcing stop, check your logic"
        );
        Deactivate(false);
        return false;
    } else if (mDrivenPropEntries.empty())
        return false;
    else {
        if (!mEventsRegistered) {
            RegisterEvents(this);
        }
        PushDrivenProperties();
        if (mValue.NotNull()) {
            if (mPreviousValue.Type() != mValue.Type()) {
                mPreviousValue = mValue;
            }
        } else if (mValue.Type() == kDataObject) {
            mPreviousValue = NULL_OBJ;
        } else {
            mPreviousValue = 0;
        }
        DataNode n(mPreviousValue);
        mPreviousValue = mValue;
        ActivateValueCases(mValue, n);
        if (mEventsRegistered) {
            return true;
        } else {
            return FlowNode::IsRunning();
        }
    }
}

void FlowWhile::Deactivate(bool b) {
    if (!b)
        PropertyEventListener::UnregisterEvents(this);
    mEventsRegistered = false;
    FlowNode::Deactivate(b);
}

void FlowWhile::ChildFinished(FlowNode *n) {
    FLOW_LOG("Child Finished of class:%s\n", n->ClassName());
    if (!mEventsRegistered) {
        FlowNode::ChildFinished(n);
    } else {
        PushDrivenProperties();
        mRunningNodes.remove(n);
        if (n && static_cast<FlowSwitchCase *>(n)->Op() == kTransition) {
            if (mValue != mPreviousValue) {
                DataNode dupe(mPreviousValue);
                mPreviousValue = mValue;
                if (!ActivateTransitionCases(mValue, dupe)) {
                    ActivateValueCases(mValue, dupe);
                }
            } else {
                ActivateValueCases(mValue, mValue);
            }
        } else {
            if (mValue != mPreviousValue) {
                DataNode dupe(mPreviousValue);
                mPreviousValue = mValue;
                if (!ActivateTransitionCases(mValue, dupe)) {
                    ActivateValueCases(mValue, dupe);
                }
            }
        }
    }
}

void FlowWhile::RequestStop() {
    UnregisterEvents(this);
    mEventsRegistered = false;
    if (!FlowNode::IsRunning()) {
        mFlowParent->ChildFinished(this);
    } else {
        FlowNode::RequestStop();
    }
}

void FlowWhile::RequestStopCancel() {
    FlowNode::RequestStopCancel();
    if (!mEventsRegistered)
        PropertyEventListener::RegisterEvents(this);
}

bool FlowWhile::IsRunning() {
    return (mEventsRegistered || FlowNode::IsRunning()) ? true : false;
}

void FlowWhile::MiloPreRun() {
    if (!IsRunning()) {
        UnregisterEvents(this);
        GenerateAutoNames(this, true);
    }
    FlowNode::MiloPreRun();
}

void FlowWhile::GenerateAutoNames(FlowNode *n, bool b) {
    DrivenPropertyEntry *entry = GetDrivenEntry("value");
    if (entry && mChildNodes.size()) {
        PropertyEventListener::GenerateAutoNames(this, true);
        FOREACH (it, mChildNodes) {
            PropertyEventListener::GenerateAutoNames(*it, false);
        }
    }
    //       puVar1 = Symbol::Symbol(aSStack_30,"value");
    //   pDVar2 = FlowNode::GetDrivenEntry(this + -0x70,*puVar1);
    //   if ((pDVar2 != 0x0) && ((*(this + -0x54) - *(this + -0x58)) / 0x14 != 0)) {
    //     PropertyEventListener::GenerateAutoNames(this,this + -0x70,true);
    //     pFVar5 = this + -0x58;
    //     iVar4 = 0;
    //     if (*(this + -0x58) != *(this + -0x54)) {
    //       iVar4 = *pFVar5;
    //     }
    //     while( true ) {
    //       iVar3 = 0;
    //       if (*pFVar5 != *(this + -0x54)) {
    //         iVar3 = *pFVar5;
    //       }
    //       if (iVar4 == ((*(this + -0x54) - *pFVar5) / 0x14) * 0x14 + iVar3) break;
    //       PropertyEventListener::GenerateAutoNames(this,*(iVar4 + 0xc),false);
    //       iVar4 = iVar4 + 0x14;
    //     }
    //   }
}

void FlowWhile::ReActivate() {
    FLOW_LOG("Reactivate\n");
    Timer timer;
    timer.Restart();
    PushDrivenProperties();
    mEntryCount++;
    if ((int)mEntryCount > 8) {
        char *path = (char *)PathName(Dir());
        MILO_NOTIFY(
            "While reentrance count > 8 in flow %s, did you mean to use a switch? Aborting while node behavior",
            path
        );
        mEntryCount--;
        return;
    }

    if (!FlowNode::IsRunning()) {
        if (mPreviousValue.Equal(mValue, nullptr, true)) {
            mEntryCount--;
            return;
        }
        if (!ActivateTransitionCases(mValue, mPreviousValue)) {
            ActivateValueCases(mValue, mPreviousValue);
        }
        mPreviousValue = mValue;
    } else if (!(!mFirstValidCaseOnly)) {
        FlowSwitchCase *running =
            static_cast<FlowSwitchCase *>(mRunningNodes.front());
        if (running->Op() != kTransition) {
            FlowSwitchCase *validCase = nullptr;
            FOREACH (it, mChildNodes) {
                FlowSwitchCase *item = static_cast<FlowSwitchCase *>(it->Obj());
                if (item->IsValidCase(this, &mValue, &mValue, true)) {
                    validCase = item;
                    break;
                }
            }
            if (running != validCase) {
                running->RequestStop();
            }
        }
    } else {
        FOREACH (it, mChildNodes) {
            FlowSwitchCase *child = static_cast<FlowSwitchCase *>(it->Obj());
            if (!(child->IsValidCase(this, &mValue, &mPreviousValue, true))) {
                if (child->IsRunning()) {
                    child->RequestStop();
                }
            } else {
                if (child->IsRunning()) {
                    child->RequestStopCancel();
                } else {
                    ActivateChild(child);
                    if (mStopRequested)
                        break;
                }
            }
        }
    }

    mEntryCount--;
    timer.Stop();
    FlowNode *flow = GetTopFlow();
    while (flow->GetParent()) {
        flow = flow->GetParent()->GetTopFlow();
    }
    Symbol sym =
        MakeString("%s: %s->%s", ClassName(), flow->Dir()->Name(), flow->Name());
    TheFlowMgr->AddEventTime(sym, timer.Ms());
}
