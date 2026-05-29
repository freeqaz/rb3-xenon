#include "flow/Flow.h"
#include "flow/FlowAnimate.h"
#include "flow/FlowCommand.h"
#include "flow/FlowDistance.h"
#include "flow/FlowEventListener.h"
#include "flow/FlowIf.h"
#include "flow/FlowLabel.h"
#include "flow/FlowManager.h"
#include "flow/FlowMultiSetProperty.h"
#include "flow/FlowNode.h"
#include "flow/FlowOnStop.h"
#include "flow/FlowOutPort.h"
#include "flow/FlowPickOne.h"
#include "flow/FlowQueueable.h"
#include "flow/FlowRun.h"
#include "flow/FlowSequence.h"
#include "flow/FlowSetProperty.h"
#include "flow/FlowSlider.h"
#include "flow/FlowSound.h"
#include "flow/FlowSwitch.h"
#include "flow/FlowSwitchCase.h"
#include "flow/FlowTimer.h"
#include "flow/FlowTrigger.h"
#include "flow/FlowValueCase.h"
#include "flow/FlowWhile.h"
#include "flow/PropertyEventProvider.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/DirLoader.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "obj/ObjPtr_p.h"
#include "os/Debug.h"
#include "os/File.h"
#include "utl/BinStream.h"
#include "utl/MakeString.h"
#include "utl/Symbol.h"
#include <list>

bool Flow::sReflectingProperty;

Flow::Flow()
    : mDynamicProperties(this), mFlowLabels(this), mFlowOutPorts(this), mObjects(this),
      mStartMode(0), mPrivate(1), mHardStop(0), mParamApplyCount(0) {}

Flow::~Flow() {
    if (!mRunningNodes.empty()) {
#ifdef HX_NATIVE
        if (ObjectDir::InDeleteObjects()) {
            // During cascade teardown, skip Deactivate (it sends messages to
            // potentially-destroyed listeners). Just clear the list — ObjPtrList
            // nodes' destructors are cascade-safe (skip ring unlinks).
            mRunningNodes.clear();
        } else
#endif
        if (mProxyFile.empty()) {
            FlowQueueable::Deactivate(true);
        }
    }
    TheFlowMgr->CancelCommand(this);
}

void Flow::Copy(const Hmx::Object *o, CopyType ty) {
    ObjectDir::Copy(o, ty);
    FlowQueueable::Copy(o, ty);
    const Flow *c = dynamic_cast<const Flow *>(o);
    if (c) {
        mDynamicProperties = c->mDynamicProperties;
        // Copy dynamic property values from source
        FOREACH (it, mDynamicProperties) {
            Symbol name(it->mName.c_str());
            const DataNode *prop = c->Property(name, false);
            SetProperty(name, *prop);
        }
        mStartMode = c->mStartMode;
        // Deactivate existing child nodes
        while (mChildNodes.begin() != mChildNodes.end()) {
            FlowNode *child = mChildNodes.begin()->Obj();
            if (child) {
                child->Deactivate(true);
            }
        }
        // Copy child nodes from source
        FOREACH (it, c->mChildNodes) {
            FlowNode *srcChild = it->Obj();
            FlowNode *newChild;
            if (dynamic_cast<Flow *>(srcChild)) {
                newChild = FlowNode::DuplicateChild(srcChild);
            } else {
                Symbol sym = srcChild->ClassName();
                Hmx::Object *newObj = Hmx::Object::NewObject(sym);
                newObj->InitObject();
                newChild = dynamic_cast<FlowNode *>(newObj);
                newChild->SetParent(this, true);
                newChild->Copy(srcChild, kCopyShallow);
            }
            newChild->SetParent(this, true);
            newChild->MoveIntoDir(Dir(), c->Dir());
        }
        mPrivate = c->mPrivate;
        mHardStop = c->mHardStop;
        RefreshPortLabelLists();
        if (!ProxyFile().empty()) {
            mStartMode = 5;
        }
    }
}

BEGIN_HANDLERS(Flow)
    HANDLE_ACTION(activate, Activate(false))
    HANDLE_ACTION(on_reflected_property_changed, OnReflectedPropertyChanged(_msg))
    HANDLE_ACTION(on_internal_property_changed, OnInternalPropertyChanged(_msg))
    HANDLE_ACTION(request_stop, RequestStop())
    HANDLE_EXPR(is_running, IsRunning())
    HANDLE_SUPERCLASS(FlowQueueable)
    HANDLE_SUPERCLASS(ObjectDir)
END_HANDLERS

BEGIN_CUSTOM_PROPSYNC(Flow::DynamicPropertyEntry)
    SYNC_PROP_SET(name, o.mName, o.SetName(_val))
    SYNC_PROP_MODIFY(type, (int &)o.mType, o.ResetDefaultValues())
    SYNC_PROP_SET(default_int, o.mDefaultVal.Int(), o.mDefaultVal = _val)
    SYNC_PROP_SET(default_bool, o.mDefaultVal.Int(), o.mDefaultVal = _val)
    SYNC_PROP_SET(default_color, o.mDefaultVal.Int(), o.mDefaultVal = _val)
    SYNC_PROP_SET(default_float, o.mDefaultVal.Float(), o.mDefaultVal = _val)
    SYNC_PROP_SET(
        default_string,
        o.mDefaultVal.Type() == kDataString ? o.mDefaultVal.Str() : "",
        o.mDefaultVal = _val
    )
    SYNC_PROP_SET(
        default_object,
        o.mDefaultVal.Type() == kDataObject ? o.mDefaultVal.GetObj() : NULL_OBJ,
        o.mDefaultVal = _val
    )
    SYNC_PROP_SET(default_symbol, o.GetDefaultValueSymbol(), o.mDefaultVal = _val)
    SYNC_PROP_SET(object_class, o.mObjectClass, o.SetClassFilter(_val))
    SYNC_PROP(object_type, o.mObjectType)
    SYNC_PROP(help, o.mHelp)
    SYNC_PROP(exposed, o.mExposed)
    SYNC_PROP_SET(get_symbol_list, o.GetSymbolList(), )
END_CUSTOM_PROPSYNC

BEGIN_PROPSYNCS(Flow)
    SYNC_PROP(dynamic_properties, mDynamicProperties)
    SYNC_PROP_SET(start_on_enter, mStartMode != 0, StartOnEnter(_val.Int()))
    SYNC_PROP_SET(start_after_game_code, mStartMode == 2, StartAfterGameCode(_val.Int()))
    SYNC_PROP(hard_stop, mHardStop)
    SYNC_PROP(intensity, FlowNode::sIntensity)
    SYNC_PROP(private, mPrivate)
    SYNC_PROP_SET(toggle_running, 0, ToggleRunning(_val.Int()))
    SYNC_SUPERCLASS(FlowQueueable)
    SYNC_SUPERCLASS(ObjectDir)
END_PROPSYNCS

BinStream &operator<<(BinStream &bs, const Flow::DynamicPropertyEntry &entry) {
    int type = entry.mType;
    bs << entry.mName;
    bs << type;
    entry.mDefaultVal.Save(bs);
    bs << entry.mHelp;
    bs << entry.mObjectClass;
    bool exposed = entry.mExposed;
    bs << exposed;
    entry.mSymbolList.Save(bs);
    bs << entry.mObjectType;
    return bs;
}

BinStreamRev &operator>>(BinStreamRev &d, Flow::DynamicPropertyEntry &entry) {
    d.stream >> entry.mName;
    d.stream >> (int &)entry.mType;
    BinStream &bs = d.stream;
    entry.mDefaultVal.Load(bs);
    bs >> entry.mHelp;
    bs >> entry.mObjectClass;
    if (d.rev > 1) {
        BinStreamRev &d2 = (d >> entry.mExposed);
        entry.mSymbolList.Load(d2.stream);
    }
    if (d.rev > 5) {
        d.stream >> entry.mObjectType;
    }
    return d;
}

BEGIN_SAVES(Flow)
    SAVE_REVS(7, 2)
    DataArrayPtr ptr;
    FOREACH (it, mDynamicProperties) {
        Symbol name = it->mName.c_str();
        const DataNode *prop = Property(name, false);
        if (prop && prop->Type() == kDataObject) {
            DataArrayPtr ptr2(new DataArray(2));
            ptr2->Node(0) = name;
            ptr2->Node(1) = prop->Evaluate();
            ptr->Resize(ptr->Size() + 1);
            ptr->Node(ptr->Size() - 1) = ptr2;
#ifdef HX_NATIVE
            mTypeProps->ClearKeyValue(name);
#else
            mTypeProps.ClearKeyValue(name);
#endif
        }
    }
#ifdef HX_NATIVE
    if (mTypeProps && mTypeProps->Map()) {
        DataArray *props = mTypeProps->Map();
#else
    if (mTypeProps.HasProps() && mTypeProps.Map()) {
        DataArray *props = mTypeProps.Map();
#endif
        for (int i = 0; i < props->Size(); i += 2) {
            Symbol sym = props->Sym(i);
            if (props->Type(i + 1) == kDataObject) {
                DataArrayPtr ptr3(new DataArray(2));
                ptr3->Node(0) = sym;
                ptr3->Node(1) = props->Evaluate(i + 1);
                ptr->Resize(ptr->Size() + 1);
                ptr->Node(ptr->Size() - 1) = ptr3;
#ifdef HX_NATIVE
                mTypeProps->ClearKeyValue(sym);
#else
                mTypeProps.ClearKeyValue(sym);
#endif
            }
        }
    }
    SAVE_SUPERCLASS(ObjectDir)
    for (int i = 0; i < ptr->Size(); i++) {
#ifdef HX_NATIVE
        mTypeProps->SetKeyValue(ptr->Array(i)->Sym(0), ptr->Array(i)->Node(1), true);
#else
        mTypeProps.SetKeyValue(ptr->Array(i)->Sym(0), ptr->Array(i)->Node(1), true);
#endif
    }
    if (IsProxy()) {
        bs << mDynamicProperties.size();
        FOREACH (it, mDynamicProperties) {
            bs << Symbol(it->mName.c_str());
            const DataNode *prop = Property(it->mName.c_str(), false);
            if (prop) {
                if (prop->Type() == kDataObject) {
                    bs << *prop;
                } else {
                    bs << prop->Type();
                    bs << *prop;
                }
            } else {
                bs << it->mDefaultVal.Type();
                bs << it->mDefaultVal;
            }
        }

    } else {
        SAVE_SUPERCLASS(FlowQueueable)
        bs << mHardStop;
        bs << mDynamicProperties;
        bs << mStartMode;
        bs << mPrivate;
    }
END_SAVES

BEGIN_LOADS(Flow)
    PreLoad(bs);
    PostLoad(bs);
END_LOADS

void Flow::PreSave(BinStream &bs) {
    if (ProxyFile().empty() || !IsProxy()) {
        SetInlineProxyType(kInlineAlways);
    } else {
        SetInlineProxyType(kInlineCached);
    }
}

INIT_REVS(7, 2)

void Flow::PreLoad(BinStream &bs) {
    LOAD_REVS(bs)
    ASSERT_REVS(7, 2)
    ObjectDir::PreLoad(bs);
    if (d.altRev < 2 && !gLoadingProxyFromDisk) {
        if (strstr(mProxyFile.c_str(), "/flow/")
            && !strstr(mProxyFile.c_str(), "/run/flow/")) {
            mProxyFile.Set(
                FileRoot(), MakeString("flow/%s", FileGetName(mProxyFile.c_str()))
            );
        }
    }
    d.PushRev(this);
}

void Flow::PostLoad(BinStream &bs) {
    BinStreamRev d(bs, bs.PopRev(this));
    ObjectDir::PostLoad(d.stream);
    if (IsProxy()) {
        int numDynProps = 0;
        d.stream.ReadEndian(&numDynProps, 4);
        if (d.rev < 5) {
            for (int i = 0; i < numDynProps; i++) {
                Symbol propName;
                d.stream >> propName;

                DataNode node;
                node.Load(d.stream);

                const DataNode *existingProp = Property(propName, false);
                if (!existingProp) {
                    if (propName != "") {
                        SetProperty(propName, node);
                    }
                }
            }
        } else {
            for (int i = 0; i < numDynProps; i++) {
                Symbol propName;
                d.stream >> propName;

                DataNode node;
                int nodeType = 0;
                d.stream.ReadEndian(&nodeType, 4);
                if (nodeType == kDataObject) {
                    ObjectDir *dir = Dir();
                    if (dir) {
                        if (dir->Loader()) {
                            dir = dir->Loader()->GetDir();
                        } else {
                            dir = dir->Dir();
                        }
                    }
                    node = FlowNode::LoadObjectFromMainOrDir(d.stream, dir);
                } else {
                    DataNode tmpNode;
                    tmpNode.Load(d.stream);
                    node = tmpNode;
                }

                const DataNode *existingProp = Property(propName, false);
                if (!existingProp) {
                    if (propName != "") {
                        SetProperty(propName, node);
                    }
                }
                if (node.Type() == kDataArray) {
                    node.UncheckedArray()->Release();
                }
            }
        }
    } else {
        if (d.rev < 3) {
            int oldRev = 0;
            d.stream.ReadEndian(&oldRev, 4);
            FlowQueueable::Load(d.stream);
            if (oldRev < 1) {
                bool loadEventProvider;
                d >> loadEventProvider;
                if (loadEventProvider) {
                    ObjPtr<Hmx::Object> eventProvider(this, 0);
                    eventProvider = FlowNode::LoadObjectFromMainOrDir(d.stream, Dir());
                }
            } else {
                ObjPtr<Hmx::Object> eventProvider(this, 0);
                eventProvider.Load(d.stream, true, 0);
            }
            std::list<Symbol> triggerEvents;
            std::list<Symbol> stopEvents;
            d >> triggerEvents;
            d >> stopEvents;
            if (triggerEvents.size() > 0 || stopEvents.size() > 0) {
                MILO_NOTIFY("Flow with trigger events found, removing: %s", PathName(this));
            }
            d >> mHardStop;
            if (oldRev > 0) {
                ObjList<FlowTrigger::PropTriggerDefn> triggerProperties(this);
                ObjList<FlowTrigger::PropTriggerDefn> stopProperties(this);
                d >> triggerProperties;
                d >> stopProperties;
                if (triggerProperties.size() > 0 || stopProperties.size() > 0) {
                    MILO_NOTIFY("Flow with trigger events found, removing: %s", PathName(this));
                }
            }
        } else {
            FlowQueueable::Load(d.stream);
            d >> mHardStop;
        }
        d >> mDynamicProperties;
        if (d.rev < 7) {
            bool startOnEnter;
            d >> startOnEnter;
            if (startOnEnter) {
                mStartMode = 2;
            } else {
                mStartMode = 0;
            }
        } else {
            d.stream.ReadEndian(&mStartMode, 4);
        }
        if (d.rev > 0) {
            d >> mPrivate;
        } else {
            mPrivate = false;
        }
    }
    if (mStartMode != 0) {
        mPrivate = true;
    }
    RefreshPortLabelLists();
    if (Loader() && Loader()->ProxyDir()) {
        if (Loader()->ProxyDir()->InlineProxyType() == kInlineAlways) {
            mStartMode = 5;
        }
    }
}

void Flow::SyncObjects() {
    ObjectDir::SyncObjects();

    FlowNode *node = this;
    Flow *topLevelFlow;
    for (topLevelFlow = node->GetTopFlow(); topLevelFlow != nullptr;) {
        if (topLevelFlow->GetParent() == nullptr) break;
        node = topLevelFlow->GetParent();
        topLevelFlow = node->GetTopFlow();
    }
    ObjectDir *flow = topLevelFlow->Dir();
    if (dynamic_cast<Flow *>(flow)) {
        DirLoader *loader = topLevelFlow->Loader();
        if (loader) {
            flow = loader->ProxyDir();
        } else {
            flow = topLevelFlow->Dir();
        }
    }
    if (flow && flow != this) {
        FOREACH (it, mDynamicProperties) {
            if (it->mExposed) {
                Symbol name(it->mName.c_str());
                DataArrayPtr ptr(new DataArray(1));
                ptr->Node(0) = name;
                if (flow->HasPropertySink(this, ptr)) {
                    flow->RemovePropertySink(this, ptr);
                }
                if (HasPropertySink(this, ptr)) {
                    RemovePropertySink(this, ptr);
                }
                if (!flow->Property(ptr, false)) {
                    if (Property(ptr, false)) {
                        flow->SetProperty(ptr, *Property(ptr, true));
                    } else {
                        flow->SetProperty(ptr, it->mDefaultVal);
                    }
                } else {
                    SetProperty(ptr, *flow->Property(ptr, true));
                }
                flow->AddPropertySink(this, ptr, "on_reflected_property_changed");
                AddPropertySink(this, ptr, "on_internal_property_changed");
            }
        }
    }
}

bool Flow::Activate() {
    FLOW_LOG("Activate\n");
    mStopRequested = false;
    if (ProxyFile().empty()) {
        Timer timer;
        timer.Restart();
        PushDrivenProperties();
        bool activate = FlowQueueable::Activate(nullptr);
        timer.Stop();
        TheFlowMgr->AddEventTime(Name(), timer.Ms());
        return activate;
    } else {
        PushDrivenProperties();
        return ActivateTrigger();
    }
}

void Flow::Deactivate(bool b1) { FlowQueueable::Deactivate(b1); }

void Flow::Enter() {
    FlowQueueable *q = this;
    if (ProxyFile().empty() && mStartMode > 0) {
        if (mStartMode == 1) {
            q->Execute(kQueue);
        } else {
            TheFlowMgr->QueueCommand(q, kQueue);
        }
    }
}

void Flow::Exit() {
    if (IsRunning() && ProxyFile().empty()) {
        if (mHardStop) {
            Deactivate(false);
        } else {
            RequestStop();
        }
    }
}

void Flow::RequestStop() {
    FLOW_LOG("RequestStop\n");
    if (!mStopRequested) {
        mStopRequested = true;
        auto it = mRunningNodes.begin();
        while (it != mRunningNodes.end()) {
            auto next_it = it;
            next_it++;
            if ((*it)->ClassName() != FlowLabel::StaticClassName()) {
                (*it)->RequestStop();
            }
            it = next_it;
        }
        mStopRequested = false;
    }
}

void Flow::RequestStopCancel() {
    if (mStopRequested) {
        mStopRequested = false;
        FOREACH (it, mRunningNodes) {
            if ((*it)->ClassName() != FlowLabel::StaticClassName()) {
                (*it)->RequestStopCancel();
            }
        }
    }
}

void Flow::Execute(FlowNode::QueueState q) {
    FLOW_LOG("Start on Enter\n");
    if (IsRunning()) {
        Deactivate(false);
    }
    Activate();
}

Flow *Flow::GetOwnerFlow() {
    const FilePath &file = ProxyFile();
    return !file.empty() ? dynamic_cast<Flow *>(Dir()) : nullptr;
}

bool Flow::ActivateTrigger() {
    mStopRequested = false;
    FOREACH (it, mChildNodes) {
#ifdef HX_NATIVE
        Hmx::Object *obj = it->Obj();
        if (obj && obj->ClassName() != FlowLabel::StaticClassName()) {
#else
        if (it->Obj()->ClassName() != FlowLabel::StaticClassName()) {
#endif
            ActivateChild(*it);
        }
        if (mStopRequested)
            break;
    }
    return FlowNode::IsRunning();
}

bool Flow::Activate(Hmx::Object *obj) {
    FLOW_LOG("activate with listener");
    mStopRequested = false;
    Timer timer;
    timer.Restart();
    PushDrivenProperties();
    bool activate = FlowQueueable::Activate(obj);
    timer.Stop();
    TheFlowMgr->AddEventTime(Name(), timer.Ms());
    return activate;
}

bool Flow::Activate(Hmx::Object *obj, DataArray *) { return Activate(obj); }

Flow::DynamicPropertyEntry::DynamicPropertyEntry(Hmx::Object *obj)
    : mName(""), mType(kInt), mHelp(""), mExposed(false) {
    Symbol temp("Object");
    mObjectClass = temp;
    mObjectType = Symbol();
}

void Flow::DynamicPropertyEntry::SetClassFilter(DataNode &filter) {
    if (filter.Sym() != mObjectClass) {
        mObjectClass = filter.Sym();
        mObjectType = Symbol();
    }
}

void Flow::DynamicPropertyEntry::SetName(DataNode &name) {
    String str(name.Str());
    str.ReplaceAll(' ', '_');
    str.ToLower();
    if (str == "name" || str == "note" || str == "intensity" || str == "hard_stop"
        || str == "private" || str == "interrupt") {
        str.insert(0, "_");
    }
    mName = str;
}

void Flow::DynamicPropertyEntry::ResetDefaultValues() { mDefaultVal = 0; }

Symbol Flow::DynamicPropertyEntry::GetDefaultValueSymbol() {
    if (mDefaultVal.Type() == kDataSymbol) {
        return mDefaultVal.Sym();
    } else {
        return Symbol();
    }
}

DataNode Flow::DynamicPropertyEntry::GetSymbolList() {
    if (mSymbolList.Type() == kDataArray) {
        return mSymbolList.Array();
    } else {
        DataArrayPtr ptr(new DataArray(1));
        ptr->Node(0) = Symbol();
        return ptr;
    }
}

void Flow::ToggleRunning(int type) {
    switch (type) {
    case 0:
        if (!IsRunning()) {
            FlowQueueable::Activate(nullptr);
        }
        break;
    case 1:
        Deactivate(false);
        break;
    case 2:
        RequestStop();
        break;
    default:
        MILO_FAIL("Unknown toggle running type: %d\n", type);
        break;
    }
}

FlowLabel *Flow::GetLabelForSym(Symbol sym) {
    FOREACH (it, mFlowLabels) {
        FlowLabel *cur = it->Obj();
        if (cur->Label() == sym) {
            return cur;
        }
    }
    return nullptr;
}

void ScanForOutPorts(ObjPtrVec<FlowOutPort> &outPorts, FlowNode *node, Flow *flow) {
    FOREACH (it, node->mChildNodes) {
        FlowOutPort *port = dynamic_cast<FlowOutPort *>(it->Obj());
        if (port && port->GetOwnerFlow() == flow) {
            outPorts.push_back(port);
        }
        ScanForOutPorts(outPorts, it->Obj(), flow);
    }
}

void Flow::RefreshPortLabelLists() {
    mFlowOutPorts.clear();
    mFlowLabels.clear();
    ScanForOutPorts(mFlowOutPorts, this, this);
    FOREACH (it, mChildNodes) {
        if (it->Obj()->ClassName() == "FlowLabel") {
            mFlowLabels.push_back(static_cast<FlowLabel *>(it->Obj()));
        }
    }
}

void Flow::OnReflectedPropertyChanged(DataArray *a) {
    if (!sReflectingProperty) {
        Symbol sym = a->Array(2)->Sym(0);
        DataArray *propArr = a->Array(2);
        Flow *topFlow = GetTopFlow();
        ObjectDir *flowDir = topFlow->Dir();
        FOREACH (it, mDynamicProperties) {
            if (it->mExposed && sym == it->mName.c_str()) {
                sReflectingProperty = true;
                SetProperty(propArr, *flowDir->Property(propArr, true));
                sReflectingProperty = false;
            }
        }
    }
}

void Flow::OnInternalPropertyChanged(DataArray *a) {
    if (!sReflectingProperty) {
        Symbol sym = a->Array(2)->Sym(0);
        DataArray *propArr = a->Array(2);
        Flow *topFlow = GetTopFlow();
        ObjectDir *flowDir = topFlow->Dir();
        FOREACH (it, mDynamicProperties) {
            if (it->mExposed && sym == it->mName.c_str()) {
                sReflectingProperty = true;
                flowDir->SetProperty(propArr, *Property(propArr, true));
                sReflectingProperty = false;
            }
        }
    }
}

void Flow::ApplyParams(DataArray *msg, FlowTrigger *trigger) {
    mParamApplyCount++;
    if (msg && msg->Size() >= 3) {
        DataArray *def = trigger->GetEventEditorDef(MsgSinks::CurrentExportEvent());
        if (def) {
            def = def->FindArray("editor");
            for (int i = 1; i < def->Size(); i++) {
                MILO_ASSERT(def->Type(i) == kDataArray, 0x24F);
                Symbol prop = def->Array(i)->Sym(0);
                MILO_ASSERT(msg->Size() > i + 1, 0x251);
                SetProperty(prop, msg->Evaluate(i + 1));
            }
        }
    }
}

void FlowInit() {
    TheFlowMgr = new FlowManager();
    REGISTER_OBJ_FACTORY(FlowNode);
    REGISTER_OBJ_FACTORY(FlowEventListener);
    REGISTER_OBJ_FACTORY(FlowSequence);
    REGISTER_OBJ_FACTORY(FlowAnimate);
    REGISTER_OBJ_FACTORY(FlowPickOne);
    REGISTER_OBJ_FACTORY(Flow);
    REGISTER_OBJ_FACTORY(FlowSwitch);
    REGISTER_OBJ_FACTORY(FlowSwitchCase);
    REGISTER_OBJ_FACTORY(FlowSound);
    REGISTER_OBJ_FACTORY(FlowRun);
    REGISTER_OBJ_FACTORY(FlowTimer);
    REGISTER_OBJ_FACTORY(FlowValueCase);
    REGISTER_OBJ_FACTORY(FlowOutPort);
    REGISTER_OBJ_FACTORY(FlowSetProperty);
    REGISTER_OBJ_FACTORY(FlowMultiSetProperty);
    REGISTER_OBJ_FACTORY(FlowWhile);
    REGISTER_OBJ_FACTORY(FlowLabel);
    REGISTER_OBJ_FACTORY(FlowCommand);
    REGISTER_OBJ_FACTORY(FlowIf);
    REGISTER_OBJ_FACTORY(FlowOnStop);
    REGISTER_OBJ_FACTORY(FlowSlider);
    REGISTER_OBJ_FACTORY(FlowDistance);
    REGISTER_OBJ_FACTORY(PropertyEventProvider);
    if (streq(FileRoot(), FileSystemRoot())) {
        PropertyEventProvider *prov = Hmx::Object::New<PropertyEventProvider>();
        prov->SetType("example");
        prov->SetName("exampleData", ObjectDir::Main());
    }
}
