#include "FlowSetProperty.h"
#include "obj/Msg.h"
#include "flow/DrivenPropertyEntry.h"
#include "flow/FlowManager.h"
#include "flow/FlowNode.h"
#include "flow/Flow.h"
#include "math/Easing.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/DirLoader.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "obj/Utl.h"
#include "utl/MakeString.h"
#include "utl/Str.h"
#include <cstdlib>

FlowSetProperty::~FlowSetProperty() { TheFlowMgr->CancelCommand(this); }

FlowSetProperty::FlowSetProperty()
    : PropertyEventListener(this), mTarget(this, nullptr), unk_0x98(0), mValue(0),
      mPersistent(0), mRate(0), mBlendTime(0), mChangePerUnit(0), unk_0xCC(this, nullptr),
      mEase(0), mEasePower(2), unk_0xE8(0), mStopMode(1) {}

PropertyTask::PropertyTask(Hmx::Object *obj, DataNode &prop, DataNode &val, TaskUnits units, float dur, EaseType t, float power, bool flag, Hmx::Object *listener)
    : mTarget(this, nullptr), mProperty(prop), mValue(val), mStartValue(0),
      mDuration(dur), mEasePower(power), mIsColorInterp(flag),
      mListener(this, nullptr), mElapsed(0.0f), mEaseFunc(gEaseFuncs[t]) {
    auto refsEnd = obj->Refs().end();
    MILO_ASSERT(obj, 0x4D);
    MILO_ASSERT(t >= kEaseLinear && t <= kEaseQuarterHalfStairstep, 0x16B);

    // Loop through target's refs to find existing PropertyTasks with same property
    for (ObjRef::iterator it = obj->Refs().begin(); it != refsEnd; ++it) {
        Hmx::Object *refOwner = it->RefOwner();
        if (refOwner != nullptr && refOwner->ClassName() == PropertyTask::StaticClassName()) {
            PropertyTask *task = static_cast<PropertyTask *>(refOwner);
            DataArray *myProp = mProperty.Array();
            DataArray *taskProp = task->mProperty.Array();
            if (taskProp->Size() == myProp->Size()) {
                bool match = true;
                for (int i = 0; i < myProp->Size(); i++) {
                    if (taskProp->Node(i) != myProp->Node(i)) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    delete task;
                    break;
                }
            }
        }
    }

    mListener = listener;
    mTarget = obj;
    mStartValue = *obj->Property(mProperty.Array(), true);

    // Handle string-to-int conversion for kDataString type
    if (mStartValue.Type() == kDataString) {
        mStartValue = atoi(mStartValue.Str());
    }
    if (mValue.Type() == kDataString) {
        mValue = atoi(mValue.Str());
    }

    TheTaskMgr.Start(this, units, 0.0f);
}

PropertyTask::~PropertyTask() {}

void PropertyTask::Poll(float ms) {
    float ratio = ms / mDuration;
    if (1.0f < ratio) {
        ObjPtr<PropertyTask> guard(this, nullptr);
        guard.SetObjConcrete(this);
        SetProperty(mValue);
        if (guard) {
            if (mListener) {
                static Message msg("on_anim_event", DataNode(Symbol("ended")));
                Hmx::Object *listener = mListener;
                mListener = nullptr;
                listener->Handle(msg, false);
            }
            delete this;
        }
    } else {
        float easedRatio = mEaseFunc(ratio, mEasePower, 0.0f);
        if (mIsColorInterp) {
            int startColor = mStartValue.Int(nullptr);

            float inv255 = 1.0f / 255.0f;
            float startG = (float)(unsigned char)(startColor >> 8) * inv255;
            float startB = (float)(unsigned char)(startColor) * inv255;
            float startR = (float)(unsigned char)(startColor >> 16) * inv255;

            int endColor = mValue.Int(nullptr);

            float endG = (float)(unsigned char)(endColor >> 8) * inv255;
            float endR = (float)(unsigned char)(endColor >> 16) * inv255;
            float endB = (float)(unsigned char)(endColor) * inv255;

            int interpG = (int)(((endG - startG) * easedRatio + startG) * 255.0f);
            int interpR = (int)(((endR - startR) * easedRatio + startR) * 255.0f);
            int interpB = (int)(((endB - startB) * easedRatio + startB) * 255.0f);

            int result = (interpB & 0xFF) | ((interpG & 0xFF) << 8) | ((interpR & 0xFF) << 16);
            DataNode colorNode(result);
            mTarget->SetProperty(mProperty.Array(), colorNode);
        } else {
            DataNode temp(mValue);
            float endVal = mValue.LiteralFloat(nullptr);
            float startVal = mStartValue.LiteralFloat(nullptr);
            float interpolated = (endVal - startVal) * easedRatio + startVal;
            if (temp.Type() != kDataInt && temp.Type() != kDataString) {
                DataNode floatNode(interpolated);
                temp = floatNode;
            } else {
                DataNode intNode((int)interpolated);
                temp = intNode;
            }
            SetProperty(temp);
        }
    }
}

bool PropertyTask::Replace(ObjRef *from, Hmx::Object *to) {
    auto& target = mTarget;
    if (from == static_cast<ObjRef *>(&target)) {
        target = to;
        if (target == nullptr) {
            delete this;
        }
        return true;
    }
    return Hmx::Object::Replace(from, to);
}

void PropertyTask::SetProperty(DataNode &val) {
    if (val.Type() == kDataString) {
        DataNode strNode(MakeString("%i", val));
        mTarget->SetProperty(mProperty.Array(), strNode);
    } else {
        mTarget->SetProperty(mProperty.Array(), val);
    }
}

BEGIN_PROPSYNCS(FlowSetProperty)
    SYNC_PROP_MODIFY(target, mTarget, OnTargetChanged())
    SYNC_PROP(value, mValue)
    SYNC_PROP(persistent, mPersistent)
    SYNC_PROP(rate, mRate)
    SYNC_PROP(blend_time, mBlendTime)
    SYNC_PROP(change_per_unit, mChangePerUnit)
    SYNC_PROP(ease, mEase)
    SYNC_PROP(ease_power, mEasePower)
    SYNC_PROP(stop_mode, mStopMode)
    SYNC_SUPERCLASS(FlowNode)
END_PROPSYNCS

BEGIN_HANDLERS(FlowSetProperty)
    HANDLE_EXPR(get_property_path, unk_0x98)
    HANDLE_ACTION(on_anim_event, OnAnimEvent(_msg->Sym(2)))
    HANDLE_EXPR(allow_blend, IsBlendable())
    HANDLE_ACTION(reactivate, ReActivate())
    HANDLE_SUPERCLASS(FlowNode)
END_HANDLERS

void FlowSetProperty::OnTargetChanged(void) {
    if (mTarget != nullptr) {
        if (unk_0x98.Type() == kDataArray && unk_0x98.Array()->Size() > 0) {
            const DataNode *props = mTarget->Property(unk_0x98.Array(), false);
            if (props == nullptr)
                unk_0x98 = 0;
        }
    }
    if (mTarget == nullptr) {
        unk_0x98 = 0;
    }
}

bool FlowSetProperty::IsBlendable(void) {
    if (mTarget != nullptr) {
        if (unk_0x98.NotNull()) {
            const DataNode *props = mTarget->Property(unk_0x98.Array(), false);
            if (props != nullptr) {
                if (props->Type() == kDataInt || props->Type() == kDataFloat) {
                    return true;
                }
                DrivenPropertyEntry *dpe = GetDrivenEntry("value");
                if (dpe != nullptr && !dpe->Empty()) {
                    return true;
                }
            }
        }
    }
    return false;
}

BEGIN_COPYS(FlowSetProperty)
    COPY_SUPERCLASS(FlowNode)
    CREATE_COPY(FlowSetProperty)
    BEGIN_COPYING_MEMBERS
        if (IsRunning()) {
            Deactivate(false);
        }
        UnregisterEvents(this);
        mTarget = c->mTarget;
        unk_0x98 = c->unk_0x98;
        mValue = c->mValue.Node();
        mRate = c->mRate;
        mBlendTime = c->mBlendTime;
        mChangePerUnit = c->mChangePerUnit;
        mEase = c->mEase;
        mEasePower = c->mEasePower;
        unk_0xE8 = c->unk_0xE8;
        mPersistent = c->mPersistent;
        mStopMode = c->mStopMode;
        GenerateAutoNames(this, true);
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(3, 0)

BEGIN_LOADS(FlowSetProperty)
    LOAD_REVS(bs)
    ASSERT_REVS(3, 0)
    LOAD_SUPERCLASS(FlowNode)
    if (d.rev < 2) {
        mTarget = mTarget.LoadFromMainOrDir(bs);
    } else {
        bs >> mTarget;
    }
    unk_0x98.Load(bs);
    if (d.rev < 1) {
        DataNode node;
        node.Load(bs);
        mValue = node;
    } else if (d.rev == 2) {
        int type;
        bs >> type;
        if (type == kDataObject) {
            Flow *owner = GetOwnerFlow();
            DirLoader *loader = owner->Loader();
            ObjectDir *dir = loader ? loader->ProxyDir() : owner->Dir();
            mValue = LoadObjectFromMainOrDir(bs, dir);
        } else {
            DataNode node;
            node.Load(bs);
            mValue = node;
        }
    } else {
        int type;
        bs >> type;
        if (type == kDataObject) {
            Flow *owner = GetOwnerFlow();
            if (!owner) {
                owner = dynamic_cast<Flow *>(this);
            }
            DirLoader *loader = owner->Loader();
            ObjectDir *dir = loader ? loader->ProxyDir() : owner->Dir();
            mValue = LoadObjectFromMainOrDir(bs, dir);
        } else {
            DataNode node;
            node.Load(bs);
            mValue = node;
        }
    }
    bs >> mRate;
    bs >> mBlendTime;
    bs >> mChangePerUnit;
    bs >> mEase;
    bs >> mEasePower;
    bs >> unk_0xE8;
    bs >> mPersistent;
    bs >> mStopMode;
END_LOADS

void FlowSetProperty::MoveIntoDir(ObjectDir *r4, ObjectDir *r5) {
    FlowNode::MoveIntoDir(r4, r5);
    if (mTarget == r5) {
        Hmx::Object *obj;
        if (r4 == nullptr) {
            obj = nullptr;
        } else {
            obj = r4;
        }
        mTarget = obj;
    }
}

bool FlowSetProperty::Activate() {
    FLOW_LOG("Activate\n");
    mStopRequested = false;
    if (mTarget != nullptr) {
        if (unk_0x98.Type() == kDataArray && unk_0x98.Array()->Size() > 0) {
            if (mPersistent && !mEventsRegistered) {
                RegisterEvents(this);
            }
            const auto *value = GetDrivenEntry("value");
            if (value != nullptr) {
                mValue = mTarget->Property(unk_0x98.Array(), true)->Evaluate();
            }
            PushDrivenProperties();

            if (mBlendTime == 0.0f && mChangePerUnit == 0.0f) {
                FLOW_LOG("Setting Value on %s\n", mTarget->Name())
                mTarget->SetProperty(unk_0x98.Array(), mValue.Node());
                return mEventsRegistered - 0;
            }
            if (mTarget->Property(unk_0x98.Array(), true)->Evaluate()
                != mValue.Node().Evaluate()) {
                FLOW_LOG("Queueing\n")
                TheFlowMgr->QueueCommand(this, kQueue);
                return 1;
            }
        }
    }

    return mEventsRegistered;
}

void FlowSetProperty::ReActivate() {
    FLOW_LOG("Reactivate\n");
    Timer t;
    t.Restart();
    PushDrivenProperties();
    auto& target = mTarget;
    if (0.0f == mBlendTime && mChangePerUnit == 0.0f) {
        FLOW_LOG("Setting Value on %s\n", target->Name())
        target->SetProperty(unk_0x98.Array(), mValue.Node());
        return;
    }
    if (target->Property(unk_0x98.Array(), true)->Evaluate()
        != mValue.Node().Evaluate()) {
        FLOW_LOG("Queueing\n")
        TheFlowMgr->QueueCommand(this, kQueue);
    }
    t.Stop();

    FlowNode *flow;
    for (flow = GetTopFlow(); flow->GetParent() != nullptr;
         flow = flow->GetParent()->GetTopFlow()) {
    }
    Symbol sym = MakeString("%s: %s->%s", ClassName(), flow->Dir()->Name(), flow->Name());
    TheFlowMgr->AddEventTime(sym, t.Ms());
}

void FlowSetProperty::Execute(QueueState qs) {
    FLOW_LOG("Execute: state = %i\n", qs);

    if (IsRunning()) {
        if (qs == kIgnore) {
            FLOW_LOG("RequestStop: Stopping\n");
            if (mEventsRegistered) {
                UnregisterEvents(this);
            }
            if (unk_0xCC != nullptr) {
                auto *task = unk_0xCC.Ptr();
                unk_0xCC = nullptr;
                delete task;
            }
        }
        if (qs == kQueue) {
            float durationTime = mBlendTime;

            if (mChangePerUnit != 0.0f && !unk_0xE8) {
                const DataNode *node = mTarget->Property(unk_0x98.Array(), true);
                int propType = node->Type();

                if (propType == kDataFloat) {
                    durationTime = (mValue.Node().Float() - node->Float()) / mChangePerUnit;
                    if (durationTime < 0.0f) {
                        durationTime = -durationTime;
                    }
                } else if (propType == kDataInt) {
                    durationTime = (float)(mValue.Node().Int() - node->Int()) / mChangePerUnit;
                    if (durationTime < 0.0f) {
                        durationTime = -durationTime;
                    }
                } else {
                    StackString<32> ss;
                    DataNode valueNode = mValue.Node();
                    valueNode.Print(ss, false, 0);
                    MILO_NOTIFY(
                        "%s has bad property %s for %s",
                        PathName(this),
                        ss,
                        PrintPropertyPath(unk_0x98.Array())
                    );
                    durationTime = 0.0f;
                }
            }

            if (durationTime > 0.0f) {
                Hmx::Object *target = mTarget;
                DataNode valueNode = mValue.Node();
                FLOW_LOG("Spawning Ramp Task on %s\n", target->Name())
                PropertyTask *task = new PropertyTask(
                    target,
                    unk_0x98,
                    valueNode,
                    (TaskUnits)mRate,
                    durationTime,
                    (EaseType)mEase,
                    mEasePower,
                    unk_0xE8,
                    this
                );
                unk_0xCC = task;
            } else {
                FLOW_LOG("Setting Value on %s\n", mTarget->Name())
                mTarget->SetProperty(unk_0x98.Array(), mValue.Node());
                if (mEventsRegistered) {
                    return;
                }
                FLOW_LOG("Releasing\n")
                mFlowParent->ChildFinished(this);
            }
        }
    } else {
        if (qs == kIgnore) {
            mFlowParent->ChildFinished(this);
        }
    }
}

void FlowSetProperty::ChildFinished(FlowNode *child) {
    FLOW_LOG("Child Finished of class:%s\n", child->ClassName());
    mRunningNodes.remove(child);
    if (mRunningNodes.empty() && unk_0xCC == nullptr && !mEventsRegistered) {
        FLOW_LOG("Timed Release From Parent \n");
        Timer timer;
        timer.Reset();
        timer.Start();
        mFlowParent->ChildFinished(this);
        timer.Stop();
        TheFlowMgr->AddMs(timer.Ms());
    }
}

bool FlowSetProperty::Replace(ObjRef *from, Hmx::Object *to) {
    if (from == static_cast<ObjRef *>(&unk_0xCC)) {
        unk_0xCC = nullptr;
        OnAnimEvent("interrupted");
        return true;
    } else {
        return Hmx::Object::Replace(from, to);
    }
}

BEGIN_SAVES(FlowSetProperty)
    SAVE_REVS(3, 0)
    SAVE_SUPERCLASS(FlowNode)
    bs << mTarget;
    bs << unk_0x98;
    if (mValue.Node().Type() == kDataObject) {
        mValue.Node().Save(bs);
    } else {
        bs << mValue.Node().Type();
        mValue.Node().Save(bs);
    }
    bs << mRate;
    bs << mBlendTime;
    bs << mChangePerUnit;
    bs << mEase;
    bs << mEasePower;
    bs << unk_0xE8;
    bs << mPersistent;
    bs << mStopMode;
END_SAVES

void FlowSetProperty::OnAnimEvent(Symbol) {
    FLOW_LOG("PropertyRampEnded\n");
    unk_0xCC = nullptr;
    if (mRunningNodes.size() == 0 && !mEventsRegistered) {
        FLOW_LOG("Timed Release From Parent \n");
        Timer t;
        t.Reset();
        t.Start();
        mFlowParent->ChildFinished(this);
        t.Stop();
        TheFlowMgr->AddMs(t.Ms());
    }
}

void FlowSetProperty::Deactivate(bool b) {
    FLOW_LOG("Deactivated\n");
    if (!b) {
        UnregisterEvents(this);
    }
    TheFlowMgr->CancelCommand(this);
    if (unk_0xCC != nullptr) {
        auto *idiot = unk_0xCC.Ptr();
        unk_0xCC = nullptr;
        delete idiot;
    }
    FlowNode::Deactivate(b);
}

bool FlowSetProperty::IsRunning() {
    if (!mEventsRegistered) {
        if (mRunningNodes.size() == 0) {
            return unk_0xCC.Ptr();
        }
    }
    return true;
}

void FlowSetProperty::RequestStop() {
    FLOW_LOG("RequestStop\n");
    mStopRequested = true;
    if (mStopMode == 0 || unk_0xCC == nullptr) {
        TheFlowMgr->QueueCommand(this, kIgnore);
    }
    FlowNode::RequestStop();
}

void FlowSetProperty::RequestStopCancel() {
    FLOW_LOG("RequestStopCancel\n");
    mStopRequested = false;
    if (mStopMode != 0) {
        TheFlowMgr->QueueCommand(this, kQueue);
    }
    FlowNode::RequestStopCancel();
}

void FlowSetProperty::MiloPreRun() {
    UnregisterEvents(this);
    GenerateAutoNames(this, 1);
    FlowNode::MiloPreRun();
}

void FlowSetProperty::UpdateIntensity(void) {
    if (mPersistent)
        Activate();
}
