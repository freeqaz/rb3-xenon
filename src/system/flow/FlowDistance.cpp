// ⛔ DO NOT try to "fix" ??0FlowDistance@@IAA@XZ from source -- lane DJ-2d,
// 2026-08-03.  The row reads 46.47% mpn / 172 B but it is a MAP DEFECT:
// scripts/target_symbol_map.json maps it to retail 0x82574938, which is
// ??0NextSongPanel@@.  RTTI is decisive: the two vtables that function installs
// (0x8209DC94 / 0x8209DC3C) decode via their ??_R4 Complete Object Locators to
// .?AVNextSongPanel@@ at subobject offsets 0x0 and 0xb4.  NextSongPanel has no
// ctor row anywhere in the map -- this address is the one it is missing.
// ⚠ The map-neighbour heuristic does NOT catch this one: the ctor COMDAT sits
// ~0xCF000 from the rest of NextSongPanel, so its neighbours are unrelated.
// Only the RTTI decode fires.  (tools/map_class_neighbour_audit.py documents
// this recall hole.)
//
// The unit's OTHER foreign block is a different defect: the pin
// 0x823C1798..0x823C17DC sits exactly in the hole between CharIKHead.cpp's own
// ...end:0x823C1794 and start:0x823C17E0 blocks, and CharIKHead's own map
// symbols bracket it (0x823C12A8 before, 0x823C17E0 after).  So the row
// ??_ECharIKHead@@UAAPAXI@Z is CORRECT and the SPLITS PIN is wrong -- a
// boundary MOVE (donor FlowDistance -> receiver CharIKHead), not a map fix.
//
// ⚠ Do NOT remove _retailTrailingPad[16] in FlowDistance.h on the strength of
// the ctor diff.  That pad rests on a separate and still-valid oracle:
// ?NewObject@FlowDistance@@SAPAVObject@Hmx@@XZ matches retail at 100% and
// allocates sizeof(FlowDistance)==0xdc.  Our layout is right; only the ctor row
// is misattributed.
#include "flow/FlowDistance.h"
#include "flow/Flow.h"
#include "flow/FlowManager.h"
#include "flow/FlowNode.h"
#include "math/Utl.h"
#include "math/Vec.h"
#include "obj/Object.h"
#include "utl/BinStream.h"

FlowDistance::FlowDistance()
    : mObj1(this, nullptr), mObj2(this, nullptr), mDistance(10), mPersistent(0), mPolling(0),
      mOutOfRange(0), mRunInRange(1), mDriveIntensity(0), mIntensityScale(0) {}

FlowDistance::~FlowDistance() {}

BEGIN_HANDLERS(FlowDistance)
    HANDLE_SUPERCLASS(FlowNode)
END_HANDLERS

BEGIN_PROPSYNCS(FlowDistance)
    SYNC_PROP(one, mObj1)
    SYNC_PROP(two, mObj2)
    SYNC_PROP(distance, mDistance)
    SYNC_PROP(persistent, mPersistent)
    SYNC_PROP(run_in_range, mRunInRange)
    SYNC_PROP(drive_intensity, mDriveIntensity)
    SYNC_SUPERCLASS(FlowNode)
END_PROPSYNCS

BEGIN_SAVES(FlowDistance)
    SAVE_REVS(0, 0)
    SAVE_SUPERCLASS(FlowNode)
    bs << mObj1;
    bs << mObj2;
    bs << mPersistent;
    bs << mDistance;
    bs << mRunInRange;
    bs << mDriveIntensity;
END_SAVES

BEGIN_COPYS(FlowDistance)
    COPY_SUPERCLASS(FlowNode)
    CREATE_COPY(FlowDistance)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mObj1)
        COPY_MEMBER(mObj2)
        COPY_MEMBER(mPersistent)
        COPY_MEMBER(mDistance)
        COPY_MEMBER(mRunInRange)
        COPY_MEMBER(mDriveIntensity)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(0, 0)

BEGIN_LOADS(FlowDistance)
    LOAD_REVS(bs)
    ASSERT_REVS(0, 0)
    LOAD_SUPERCLASS(FlowNode)
    mObj1.LoadFromMainOrDir(bs);
    mObj2.LoadFromMainOrDir(bs);
    d >> mPersistent;
    d >> mDistance;
    d >> mRunInRange;
    d >> mDriveIntensity;
END_LOADS

bool FlowDistance::Activate() {
    FLOW_LOG("Activated\n");
    mStopRequested = false;
    PushDrivenProperties();
    mStopRequested = false;
    if (mObj1 && mObj2) {
        if (mPersistent) {
            TheFlowMgr->AddPollable(this);
            mPolling = true;
        }
        Vector3 diff;
        Subtract(mObj1->WorldXfm().v, mObj2->WorldXfm().v, diff);
        mOutOfRange = Length(diff) > mDistance;
        Execute(kWhenAble);
        if (mPersistent) {
            return true;
        } else
            return FlowNode::IsRunning();
    } else {
        return false;
    }
}

void FlowDistance::Deactivate(bool b) {
    FLOW_LOG("Deactivated\n");
    TheFlowMgr->RemovePollable(this);
    mPolling = false;
    FlowNode::Deactivate(b);
}

void FlowDistance::ChildFinished(FlowNode *n) {
    FLOW_LOG("Child Finished of class:%s\n", n->ClassName());
    mRunningNodes.remove(n);
    if (mRunningNodes.empty()) {
        if (mPolling) {
            TheFlowMgr->RemovePollable(this);
        }
        if (!mPersistent || mStopRequested) {
            mPolling = false;
            mFlowParent->ChildFinished(this);
        }
    }
}

// RequestStop and RequestStopCancel are in FlowSlider.cpp (cross-unit)

void FlowDistance::Execute(QueueState qs) {
    bool shouldStop = false;
    bool shouldActivate = false;
    Vector3 diff;
    Subtract(mObj1->WorldXfm().v, mObj2->WorldXfm().v, diff);
    float dist = Length(diff);
    if (mDriveIntensity && mRunInRange) {
        float oldScale = mIntensityScale;
        float intensity = Clamp<float>(0.0f, 1.0f, 1.0f - dist / mDistance);
        mIntensityScale = intensity;
        if (intensity != oldScale) {
            UpdateIntensity();
        }
    }
    if (mOutOfRange) {
        if ((double)dist > (double)mDistance) {
            mOutOfRange = false;
            if (mRunInRange) {
                shouldStop = true;
            } else {
                shouldActivate = true;
            }
        }
    } else {
        if ((double)dist <= (double)mDistance) {
            mOutOfRange = true;
            if (mRunInRange) {
                shouldActivate = true;
            } else {
                shouldStop = true;
            }
        }
    }
    if (shouldStop) {
        FlowNode::RequestStop();
    } else if (shouldActivate) {
        FlowNode::Activate();
    }
}

bool FlowDistance::IsRunning() {
    if (mPersistent && mPolling)
        return true;
    return FlowNode::IsRunning();
}

void FlowDistance::UpdateIntensity() {
    float oldIntensity = FlowNode::sIntensity;
    FlowNode::sIntensity *= mIntensityScale;
    FlowNode::UpdateIntensity();
    FlowNode::sIntensity = oldIntensity;
}
