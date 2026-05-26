#include "flow/FlowNode.h"
#include "flow/DrivenPropertyEntry.h"
#include "flow/FlowLabel.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/DirLoader.h"
#include "obj/Object.h"
#include "obj/Utl.h"
#include "os/Debug.h"
#include "flow/Flow.h"

float FlowNode::sIntensity = 1.0f;
bool FlowNode::sPushDrivenProperties = false;

#pragma region Hmx::Object

FlowNode::FlowNode()
    : mChildNodes(this, (EraseMode)0, kObjListNoNull), mRunningNodes(this),
      mFlowParent(nullptr), mDrivenPropEntries(this), mStopRequested(0) {
    mDebugOutput = false;
}

FlowNode::~FlowNode() {
    if (!mRunningNodes.empty()) {
#ifdef HX_NATIVE
        // During cascade, NullifyAllRefs already nullified ObjPtrs in
        // mRunningNodes. Deactivate would dereference nullptr nodes.
        if (ObjectDir::InDeleteObjects())
            mRunningNodes.clear();
        else
#endif
        Deactivate(true);
    }
    while (!mChildNodes.empty()) {
        FlowNode *cur = mChildNodes.front();
#ifdef HX_NATIVE
        if (!cur || ObjectDir::InDeleteObjects()) {
            // Null entry left by suppressed erase during ring walk, or
            // we're inside cascading ObjectDir::DeleteObjects — children
            // will be destroyed by the hash table iteration, so don't
            // double-delete them here.
            mChildNodes.erase(mChildNodes.begin());
            continue;
        }
#endif
        delete cur;
    }
}

BEGIN_HANDLERS(FlowNode)
    HANDLE_ACTION(activate, Activate());
    HANDLE_ACTION(deactivate, Deactivate(false));
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(FlowNode)
    SYNC_PROP_SET(comment, Note(), SetNote(_val.Str()))
    SYNC_PROP(debug_output, mDebugOutput)
    SYNC_PROP(debug_comment, mDebugComment)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(FlowNode)
    SAVE_REVS(2, 0)
    if (!dynamic_cast<Flow *>(this)) {
        SAVE_SUPERCLASS(Hmx::Object)
    }
    ObjPtrVec<FlowNode> flowNodes(this);
    FOREACH (it, mChildNodes) {
        if ((*it)->Dir() == Dir()) {
            flowNodes.push_back(*it);
        }
    }
    bs << flowNodes;
    bs << (int)mDrivenPropEntries.size();
    FOREACH (it, mDrivenPropEntries) {
        it->Save(bs);
    }
    bs << mDebugOutput;
    bs << mDebugComment;
END_SAVES

BEGIN_COPYS(FlowNode)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(FlowNode)
    BEGIN_COPYING_MEMBERS
        if (!dynamic_cast<Flow *>(this)) {
            FOREACH (it, c->mChildNodes) {
                FlowNode *n = DuplicateChild(*it);
                if (n) {
                    n->SetParent(this, true);
                }
            }
        }
        COPY_MEMBER(mDrivenPropEntries)
    END_COPYING_MEMBERS
END_COPYS

void FlowNode::Load(BinStream &bs) {
    int revs;
    bs >> revs;
    BinStreamRev d(bs, revs);

    static const unsigned short gRevs[4] = { 2, 0, 0, 0 };
    if (d.rev > 2) {
        MILO_FAIL(
            "%s can't load new %s version %d > %d",
            PathName(this),
            ClassName(),
            d.rev,
            gRevs[0]
        );
    }
    if (d.altRev > 0) {
        MILO_FAIL(
            "%s can't load new %s alt version %d > %d",
            PathName(this),
            ClassName(),
            d.altRev,
            gRevs[2]
        );
    }

    if (!dynamic_cast<Flow *>(this)) {
        Hmx::Object::Load(d.stream);
    }

    mChildNodes.Load(d.stream, true, nullptr);

    // Call SetParent on each loaded child node
    FOREACH (it, mChildNodes) {
        (*it)->SetParent(this, false);
    }

    int numEntries;
    d >> numEntries;
#ifdef HX_NATIVE
    if (numEntries < 0 || numEntries > 256) {
        fprintf(stderr, "FlowNode::Load ABORT: bad numEntries=%d for %s '%s'\n", numEntries, ClassName(), Name());
        abort();
    }
#endif
    mDrivenPropEntries.clear();
    mDrivenPropEntries.reserve(numEntries);
    for (int i = 0; i < numEntries; i++) {
        DrivenPropertyEntry entry(this);
        entry.Load(d.stream, this);
        mDrivenPropEntries.push_back(entry);
    }

    if (d.rev > 0) {
        bool unk;
        d >> unk;
        mDebugOutput = unk;
    }
    if (d.rev > 1) {
        String debugComment;
        d.stream >> debugComment;
        mDebugComment = debugComment;
    }
}

const char *FlowNode::FindPathName() {
    ObjectDir *dir = dynamic_cast<ObjectDir *>(this);
    if (dir) {
        return dir->Hmx::Object::FindPathName();
    } else {
        Flow *flow = GetOwnerFlow();
        return MakeString("%s:%s:%s", Name(), ClassName(), flow->FindPathName());
    }
}

#pragma endregion
#pragma region FlowNode

void FlowNode::SetParent(class FlowNode *new_parent, bool b) {
    if (mFlowParent != new_parent) {
        if (mFlowParent != nullptr) {
            mFlowParent->mChildNodes.remove(this);
        }
        mFlowParent = new_parent;
        if (new_parent != nullptr && b) {
            new_parent->mChildNodes.push_back(this);
        }
    }
}

bool FlowNode::Activate() {
    FLOW_LOG("Activating Children\n");
    mStopRequested = false;
    FOREACH (it, mChildNodes) {
        ActivateChild(*it);
        if (mStopRequested)
            break;
    }
    return !mRunningNodes.empty();
}

void FlowNode::Deactivate(bool b1) {
    FLOW_LOG("Deactivated\n");
    // Manually iterate with pre-increment to handle iterator invalidation
    // when ChildFinished() is called during node->Deactivate()
    auto it = mRunningNodes.begin();
    while (it != mRunningNodes.end()) {
        auto node = *it;
        ++it;
        node->Deactivate(b1);
    }
    mRunningNodes.clear();
}

void FlowNode::ChildFinished(FlowNode *node) {
    FLOW_LOG("Child Finished of class:%s\n", node->ClassName());
    mRunningNodes.remove(node);
    if (mRunningNodes.empty()) {
        FLOW_LOG("Releasing\n");
        if (mFlowParent)
            mFlowParent->ChildFinished(this);
    }
}

void FlowNode::RequestStop() {
    FLOW_LOG("RequestStop\n");
    mStopRequested = true;
    auto it = mRunningNodes.begin();
    while (it != mRunningNodes.end()) {
        auto next_it = it;
        next_it++;
        (*it)->RequestStop();
        it = next_it;
    }
}

void FlowNode::RequestStopCancel() {
    FLOW_LOG("RequestStopCancel\n");
    mStopRequested = false;
    FOREACH (it, mRunningNodes) {
        (*it)->RequestStopCancel();
    }
}

Flow *FlowNode::GetOwnerFlow() {
    ObjectDir *dir = Dir();
    if (dir != 0)
        return static_cast<Flow *>(dir);
    return 0;
}

void FlowNode::MiloPreRun() {
    FOREACH (it, mChildNodes) {
        (*it)->MiloPreRun();
    }
}

void FlowNode::MoveIntoDir(ObjectDir *from, ObjectDir *to) {
    if (Dir() == NULL || Dir() == to) {
        String suffix("a");
        suffix[0] = rand() % 25 + 'a';
        const char *name = NextName(MakeString("%s", (char *)suffix.c_str()), from);

        if (to) {
            while (!strcmp(to->Name(), name) || !strcmp(from->Name(), name)) {
                suffix[0] = rand() % 25 + 'a';
                name = MakeString("%s%s", name, suffix.c_str());
            }
        }

        SetName(NextName(name, from), from);

        FOREACH (it, mChildNodes) {
            (*it)->MoveIntoDir(from, to);
        }

        FOREACH (it, mDrivenPropEntries) {
            DrivenPropertyEntry &entry = *it;
            FOREACH (it2, entry.mMathOps) {
                FlowMathOp &op = *it2;
                if (op.mDrivenObj == (Hmx::Object *)to) {
                    op.mDrivenObj = (Hmx::Object *)from;
                }
            }
        }
    }
}

void FlowNode::UpdateIntensity() {
    FOREACH (it, mRunningNodes) {
        (*it)->UpdateIntensity();
    }
}

FlowNode *FlowNode::DuplicateChild(FlowNode *child) {
    Flow *childFlow = dynamic_cast<Flow *>(child);
    if (!(!childFlow)) {
        Symbol flowSym = Flow::StaticClassName();
        Hmx::Object *newObj = Hmx::Object::NewObject(flowSym);
        Flow *newFlow = dynamic_cast<Flow *>(newObj);

        // Copy the proxy file from old flow to new flow
        newFlow->SetProxyFile(childFlow->ProxyFile(), false);

        // Copy dynamic property values from old flow to new flow
        Flow::DynamicPropertyEntry *it2 = newFlow->mDynamicProperties.begin();
        while (it2 != newFlow->mDynamicProperties.end()) {
            {
                DataArrayPtr arr(new DataArray(1));
                arr->Node(0) = DataNode(Symbol(it2->mName.c_str()));
                const DataNode *prop =
                    childFlow->Property(Symbol(it2->mName.c_str()), false);
                if (prop) {
                    newFlow->SetProperty(arr, *prop);
                }
            }
            it2++;
        }

        // Duplicate FlowLabel children that are not in the old flow's directory
        FOREACH (it, childFlow->mChildNodes) {
            if ((*it)->ClassName() == FlowLabel::StaticClassName()
                && (*it)->Dir() != static_cast<ObjectDir *>(childFlow)) {
                Symbol labelSym = FlowLabel::StaticClassName();
                Hmx::Object *labelObj = Hmx::Object::NewObject(labelSym);
                FlowLabel *newLabel = dynamic_cast<FlowLabel *>(labelObj);
                newLabel->InitObject();
                newLabel->Copy((FlowNode *)(*it), kCopyDeep);
                newLabel->SetParent(newFlow, true);
                Hmx::Object *labelBase = newLabel;
                ObjectDir *dir = child->Dir();
                const char *name = NextName("l", dir);
                labelBase->SetName(name, dir);
            }
        }

        return newFlow;
    } else {
        Symbol sym = child->ClassName();
        Hmx::Object *newObj = Hmx::Object::NewObject(sym);
        newObj->InitObject();
        FlowNode *newNode = dynamic_cast<FlowNode *>(newObj);
        newNode->Copy(child, kCopyDeep);
        Hmx::Object *nodeBase = newNode;
        ObjectDir *dir = child->Dir();
        const char *name = NextName("n", dir);
        nodeBase->SetName(name, dir);
        return newNode;
    }
}

void FlowNode::PushDrivenProperties() {
    sPushDrivenProperties = true;
    FOREACH (it, mDrivenPropEntries) {
        DrivenPropertyEntry &entry = *it;
        ObjVector<FlowMathOp> &mathOps =
            const_cast<ObjVector<FlowMathOp> &>(entry.MathOps());
        DataNode targetValue(0);

        FlowMathOp &firstOp = mathOps[0];
        Hmx::Object *drivenObj = firstOp.DrivenObj();

        if (drivenObj) {
            DataArray *propPath = firstOp.Rhs().Array(NULL);
            const DataNode *prop = drivenObj->Property(propPath, false);
            if (prop) {
                targetValue = *prop;
            } else {
                targetValue = DataNode(firstOp.Default());
            }
        } else {
            targetValue = DataNode(firstOp.Default());
        }

        if (&*mathOps.end() == &mathOps[0] + 1) {
            SetProperty(entry.Node().Array(NULL), targetValue);
        } else {
            if (targetValue.CompatibleType(kDataFloat)) {
                FlowMathOp *op = &mathOps[0];
                float val = targetValue.LiteralFloat(NULL);
                auto opEnd = mathOps.end();
                while (op++, op != opEnd) {
                    val = op->Apply(val);
                }
                targetValue = DataNode(val);
            }

            const DataNode *existingProp = Property(entry.Node().Array(NULL), true);

            if (existingProp->Type() == targetValue.Type()) {
                SetProperty(entry.Node().Array(NULL), targetValue);
            } else if (targetValue.Type() == kDataFloat
                       || targetValue.Type() == kDataInt) {
                if (existingProp->Type() == kDataFloat) {
                    SetProperty(entry.Node().Array(NULL), targetValue);
                } else {
                    float fVal = targetValue.LiteralFloat(NULL);
                    int iVal;
                    if (fVal > 0.0) {
                        iVal = (int)(fVal + 0.5f);
                    } else {
                        iVal = (int)(fVal - 0.5f);
                    }
                    SetProperty(entry.Node().Array(NULL), DataNode(iVal));
                }
            }
        }
    }
    sPushDrivenProperties = false;
}

void FlowNode::ActivateChild(FlowNode *child) {
#ifdef HX_NATIVE
    if (!child) return;
#endif
    mRunningNodes.push_back(child);
    if (!child->Activate()) {
        FLOW_LOG(
            "Activated Child %s, which ran in full immediately.\n", child->ClassName()
        );
        mRunningNodes.remove(child);
    }
}

bool FlowNode::HasRunningNode(FlowNode *node) {
    return mRunningNodes.find(node) != mRunningNodes.end();
}

DrivenPropertyEntry *FlowNode::GetDrivenEntry(Symbol s) {
    DataArrayPtr ptr(new DataArray(1));
    ptr->Node(0) = s;
    return GetDrivenEntry(ptr);
}

DrivenPropertyEntry *FlowNode::GetDrivenEntry(DataArray *a) {
    FOREACH (it, mDrivenPropEntries) {
        if (it->Node().Type() == kDataArray) {
            DataArray *curArr = it->Node().Array();
            if (curArr->Size() == a->Size()) {
                bool b1 = true;
                for (int i = 0; i < curArr->Size(); i++) {
                    if (curArr->Node(i) != a->Node(i)) {
                        b1 = false;
                    }
                }
                if (b1) {
                    return &(*it);
                }
            }
        }
    }
    return nullptr;
}

Flow *FlowNode::GetTopFlow() {
    Flow *pFlow = GetOwnerFlow();
    if (!pFlow)
        return static_cast<Flow *>(this);
    for (; pFlow->GetOwnerFlow() && pFlow->GetOwnerFlow() != pFlow;
         pFlow = pFlow->GetOwnerFlow())
        ;
    return pFlow;
}

void FlowNode::ActivateLabel(FlowLabel *label) {
    FLOW_LOG("Activating Label:%s\n", label->Label());
    mStopRequested = false;
    mRunningNodes.push_back(label);
    if (!label->Activate(this)) {
        mRunningNodes.remove(label);
    }
}

static ObjectDir *FlowParentDir(Flow *flow) {
    if (flow->Loader()) {
        return flow->Loader()->ProxyDir();
    }
    return flow->Dir();
}

Hmx::Object *FlowNode::LoadObjectFromMainOrDir(BinStream &bs, ObjectDir *dir) {
    Symbol sym;
    bs >> sym;
    if (sym == "")
        return nullptr;

    // Try main dir first
    Hmx::Object *obj = ObjectDir::Main()->Find<Hmx::Object>(sym.Str(), false);
    if (obj == 0)
        obj = dir->Find<Hmx::Object>(sym.Str(), false);
    if (obj == 0) {
        Flow *flow = dynamic_cast<Flow *>(dir);
        if (flow) {
            ObjectDir *parentDir = FlowParentDir(flow);
            if (parentDir) {
                obj = FlowParentDir(flow)->Find<Hmx::Object>(sym.Str(), false);
                if (obj == 0) {
                    Flow *parentFlow = dynamic_cast<Flow *>(FlowParentDir(flow));
                    if (parentFlow) {
                        ObjectDir *gpDir = FlowParentDir(parentFlow);
                        if (gpDir) {
                            obj = FlowParentDir(parentFlow)->Find<Hmx::Object>(sym.Str(), false);
                        }
                    }
                }
            }
        }
    }

    return obj;
}
