// ⛔ DO NOT try to "fix" ??0FlowRun@@IAA@XZ from source -- lane DJ-2d,
// 2026-08-03.  The row reads 48.96% mpn / 292 B and looks like a nearly-there
// ctor, but it is a MAP DEFECT: scripts/target_symbol_map.json maps it to
// retail 0x82618BA8, which is ??0CustomizePanel@@ -- a different class.
// Three independent channels agree:
//   1. RTTI  -- the vtables that function installs (0x820C3F0C / 0x820C3ECC /
//      0x820C3E74) decode via their ??_R4 Complete Object Locators to
//      .?AVCustomizePanel@@ at subobject offsets 0x0 / 0x3c / 0xb8.
//   2. Neighbours -- 0x82618BA8 is flanked in the map by
//      ?StoreFocusComponent@CustomizePanel@@ and ??_GCustomizePanel@@.
//   3. Gap -- CustomizePanel has NO ctor row anywhere in the map; this address
//      is exactly the row it is missing.
// The splits pin 0x82618BA8..0x82618CCC is also byte-exactly the hole between
// CustomizePanel.cpp's own ...end:0x82618BA8 and start:0x82618CCC blocks.
// Remedy is a coupled map+splits move owned by the map lane, NOT source.
// ⚠ FlowRun.cpp is a SINGLE-function unit: draining this block requires
// deleting its whole splits.txt entry in the same edit, or report.json
// hard-fails on the 42-byte empty obj (see CLAUDE.md "Build wiring").
#include "flow/FlowRun.h"
#include "FlowRun.h"
#include "flow/Flow.h"
#include "flow/FlowNode.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "os/Debug.h"

FlowRun::FlowRun()
    : mTargetDir(this), mTarget(this), mTargetName(""), mStop(false),
      mImmediateRelease(false) {}

FlowRun::~FlowRun() {}

BEGIN_HANDLERS(FlowRun)
    HANDLE_ACTION(on_flow_finished, ChildFinished(_msg->Obj<FlowNode>(2)))
    HANDLE_SUPERCLASS(FlowNode)
END_HANDLERS

BEGIN_PROPSYNCS(FlowRun)
    SYNC_PROP_MODIFY(target_dir, mTargetDir, OnTargetDirChange())
    SYNC_PROP_MODIFY(target, mTarget, OnTargetChange())
    SYNC_PROP(stop, mStop)
    SYNC_PROP(immediate_release, mImmediateRelease)
    SYNC_SUPERCLASS(FlowNode)
END_PROPSYNCS

BEGIN_SAVES(FlowRun)
    SAVE_REVS(2, 0)
    SAVE_SUPERCLASS(FlowNode)
    bs << mTargetDir;
    ResolveTarget();
    bs << mTargetName;
    bs << mStop;
    bs << mImmediateRelease;
END_SAVES

void FlowRun::Copy(const Hmx::Object *o, Hmx::Object::CopyType ty) {
    FlowNode::Copy(o, ty);
    const FlowRun *c = dynamic_cast<const FlowRun *>(o);
    if (c) {
        mTargetDir = c->mTargetDir;
        mTargetName = c->mTargetName;
        mTarget = c->mTarget;
        mStop = c->mStop;
        mImmediateRelease = c->mImmediateRelease;
    }
}

INIT_REVS(2, 0)

BEGIN_LOADS(FlowRun)
    LOAD_REVS(bs)
    ASSERT_REVS(2, 0)
    LOAD_SUPERCLASS(FlowNode)
    if (d.rev < 2) {
        Hmx::Object *obj = FlowNode::LoadObjectFromMainOrDir(bs, Dir());
        if (obj) {
            mTargetDir = dynamic_cast<ObjectDir *>(obj);
        }
        mTarget = mTarget.LoadFromMainOrDir(bs);
    } else {
        mTargetDir.LoadFromMainOrDir(bs);
        bs >> mTargetName;
        mTarget = (Flow *)0;
    }
    d >> mStop;
    d >> mImmediateRelease;
END_LOADS

bool FlowRun::Activate() {
    FLOW_LOG("Activate\n");
    mStopRequested = false;
    PushDrivenProperties();
    ResolveTarget();
    Flow *target = mTarget;
    if (target) {
        if (mStop) {
            mTarget->RequestStop();
        } else if (mImmediateRelease) {
            mTarget->Activate(nullptr);
        } else {
            Flow *t = mTarget;
            mRunningNodes.push_back(t);
            bool running = mTarget->Activate(this);
            if (running) {
                return true;
            }
            Flow *t2 = mTarget;
            mRunningNodes.remove(t2);
        }
    }
    return false;
}


void FlowRun::ResolveTarget() {
    if (mTarget)
        return;
    if (!mTargetName.c_str()[0])
        return;
    ObjectDir *targetDir = mTargetDir;
    if (!targetDir) {
        Flow *ownerFlow = GetOwnerFlow();
        DirLoader *loader = ownerFlow->Loader();
        if (loader) {
            targetDir = loader->ProxyDir();
        } else {
            targetDir = ownerFlow->Dir();
        }
        MILO_ASSERT(targetDir, 0x72);
    }
    mTarget = targetDir->Find<Flow>(mTargetName.c_str(), false);
}

void FlowRun::ChildFinished(FlowNode *node) {
    FLOW_LOG("Child Finished of class:%s\n", node->ClassName());
    if (!mRunningNodes.empty()) {
        FlowNode::ChildFinished(node);
    }
}

void FlowRun::RequestStop() {
    FLOW_LOG("RequestStop\n");
    mStopRequested = true;
    mTarget->RequestStop();
}

void FlowRun::RequestStopCancel() {
    FLOW_LOG("RequestStopCancel\n");
    mStopRequested = false;
    mTarget->RequestStopCancel();
}

void FlowRun::OnTargetDirChange() {
    mTarget = (Flow *)0;
    mTargetName = "";
}

void FlowRun::OnTargetChange() {
    if (mTarget)
        mTargetName = mTarget->Name();
    else
        mTargetName = "";
    return;
}
