#include "obj/Msg.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "utl/Str.h"
#include "utl/Symbol.h"
#include "world/CameraShot.h"

Symbol MsgSinks::sCurrentExportEvent(gNullStr);

Symbol MsgSinks::GetPropSyncHandler(DataArray *arr) {
    if (mPropSyncHandlers) {
        int size = mPropSyncHandlers->Size();
        for (int i = 0; i < size; i += 2) {
            DataArray *array = mPropSyncHandlers->Array(i);
            if (array->Size() == arr->Size()) {
                bool ret = true;
                for (int j = 0; j < array->Size(); j++) {
                    if (array->UncheckedInt(j) != arr->UncheckedInt(j)) {
                        ret = false;
                        break;
                    }
                }
                if (ret)
                    return mPropSyncHandlers->Sym(i + 1);
            }
        }
    }
    return 0;
}

Symbol PathToEventName(DataArray *arr) {
    int val;
    StackString<100> str("on_");
    str += arr->Sym(0).Str();
    for (int i = 1; i < arr->Size(); i++) {
        if (arr->Type(i) == kDataSymbol) {
            str += arr->LiteralSym(i).Str();
        } else {
            val = arr->Int(i);
            str += MakeString("%i", (CamShotFrame::BlendEaseMode)val);
        }
    }
    str += "_change";
    return str.c_str();
}

bool MsgSinks::HasPropertySink(Hmx::Object *o, DataArray *a) {
    Symbol path = PathToEventName(a);
    if (mPropSyncHandlers) {
        for (int i = 1; i < mPropSyncHandlers->Size(); i += 2) {
            if (path == mPropSyncHandlers->Sym(i)) {
                return true;
            }
        }
    }
    return false;
}

bool MsgSinks::HasSink(Hmx::Object *o) const {
    for (ObjList<Sink>::const_iterator it = mSinks.begin(); it != mSinks.end(); ++it) {
        if (it->obj == o) {
            return true;
        }
    }
    return false;
}

MsgSinks::EventSinkElem &
MsgSinks::EventSinkElem::operator=(const EventSinkElem &other) {
    Sink::operator=(other);
    handler = other.handler;
    return *this;
}

void MsgSinks::ChainEventSinks(Hmx::Object *from, Hmx::Object *to) {
    for (ObjList<EventSink>::const_iterator it = mEventSinks.begin();
         it != mEventSinks.end();
         ++it) {
        if (it->chainProxy) {
            from->AddSink(to, it->event);
        }
    }
}

void MsgSinks::EventSink::Remove(Hmx::Object *o, bool exporting) {
    for (ObjList<EventSinkElem>::iterator it = sinks.begin(); it != sinks.end(); ++it) {
        if (it->obj == o) {
            it->obj = nullptr;
            // When exporting, null the pointer but keep the element in the list
            if (exporting) {
                return;
            }
            sinks.erase(it);
            return;
        }
    }
}

void MsgSinks::EventSink::Add(
    Hmx::Object *obj, Hmx::Object::SinkMode mode, Symbol s, bool b4
) {
    EventSinkElem elem(sinks.Owner());
    elem.obj.SetObjConcrete(obj);
    elem.mode = mode;
    elem.handler = s;
    if (b4) {
        sinks.push_front(elem);
    } else {
        sinks.push_back(elem);
    }
}

MsgSinks::~MsgSinks() {
    if (mPropSyncHandlers)
        mPropSyncHandlers->Release();
}

MsgSinks::MsgSinks(Hmx::Object *o)
    : mPropSyncHandlers(nullptr), mSinks(o), mEventSinks(o), mExporting(0), mOwner(o) {}

BEGIN_CUSTOM_PROPSYNC(MsgSinks::Sink)
    SYNC_PROP(obj, o.obj)
    SYNC_PROP(mode, (int &)o.mode)
END_CUSTOM_PROPSYNC

BEGIN_CUSTOM_PROPSYNC(MsgSinks::EventSinkElem)
    SYNC_PROP(handler, o.handler)
    SYNC_PROP(obj, o.obj)
    SYNC_PROP(mode, (int &)o.mode)
END_CUSTOM_PROPSYNC

BEGIN_CUSTOM_PROPSYNC(MsgSinks::EventSink)
    SYNC_PROP(event, o.event)
    SYNC_PROP(sinks, o.sinks)
END_CUSTOM_PROPSYNC

bool PropSync(MsgSinks &o, DataNode &_val, DataArray *_prop, int _i, PropOp _op) {
    if (_i == _prop->Size())
        return true;
    else {
        Symbol sym = _prop->Sym(_i);
        SYNC_PROP(sinks, o.Sinks())
        SYNC_PROP(event_sinks, o.mEventSinks)
        return false;
    }
}

void MsgSinks::AddSink(
    Hmx::Object *s, Symbol ev, Symbol handler, Hmx::Object::SinkMode mode, bool chainProxy
) {
    MILO_ASSERT(s, 0x9C);
    if (ev.Null() && !chainProxy) {
        MILO_NOTIFY("%s can't have chainProxy false with no event", PathName(mOwner));
    }
    RemoveSink(s, ev);
    if (ev.Null()) {
        MILO_ASSERT(s != mOwner, 0xA6);
        Sink sink(mOwner);
        sink.obj.SetObjConcrete(s);
        sink.mode = mode;
        if (mExporting != 0) {
            mSinks.push_front(sink);
        } else {
            mSinks.push_back(sink);
        }
    } else {
        if (handler.Null())
            handler = ev;
        MILO_ASSERT((s != mOwner) || (handler != ev), 0xB9);
        ObjList<EventSink>::iterator found;
        for (found = mEventSinks.begin(); found != mEventSinks.end(); ++found) {
            if (found->event == ev) {
                if (chainProxy != found->chainProxy) {
                    MILO_NOTIFY("%s mismatched proxy chain for %s", PathName(mOwner), ev);
                }
                found->Add(s, mode, handler, mExporting);
                return;
            }
        }
        mEventSinks.push_back();
        mEventSinks.back().event = ev;
        mEventSinks.back().chainProxy = chainProxy;
        mEventSinks.back().Add(s, mode, handler, mExporting);
    }
}

void MsgSinks::AddPropertySink(Hmx::Object *o, DataArray *a, Symbol s) {
    GetPropSyncHandler(a);
    Symbol path = PathToEventName(a);
    if (!mPropSyncHandlers) {
        mPropSyncHandlers = new DataArray(2);
    } else {
        mPropSyncHandlers->Resize(mPropSyncHandlers->Size() + 2);
    }
    mPropSyncHandlers->Node(mPropSyncHandlers->Size() - 2) = DataNode(a->Clone(true, false, 0), kDataArray);
    mPropSyncHandlers->Node(mPropSyncHandlers->Size() - 2).LiteralArray()->Release();
    mPropSyncHandlers->Node(mPropSyncHandlers->Size() - 1) = path;
    AddSink(o, path, s, Hmx::Object::kHandle, false);
}

void MsgSinks::Sink::Export(DataArray *arr) {
    switch (mode) {
    case Hmx::Object::kHandle:
        obj->Handle(arr, false);
        break;
    case Hmx::Object::kExport:
        obj->Export(arr, false);
        break;
    case Hmx::Object::kType:
        obj->HandleType(arr);
        break;
    case Hmx::Object::kExportType:
        obj->Export(arr, true);
        break;
    }
}

void MsgSinks::Export(DataArray *arr) {
    mExporting++;
    for (ObjList<Sink>::iterator it = mSinks.begin(); it != mSinks.end();) {
        if (it->obj != nullptr) {
            it->Export(arr);
            ++it;
        } else {
            if (mExporting == 1) {
                it = mSinks.erase(it);
            } else {
                ++it;
            }
        }
    }

    Symbol oldExportEvent = sCurrentExportEvent;
    sCurrentExportEvent = arr->Sym(1);
    for (ObjList<EventSink>::iterator evIt = mEventSinks.begin();
         evIt != mEventSinks.end(); ++evIt) {
        if (evIt->event == arr->Sym(1)) {
            DataNode origNode = arr->Node(1);
            for (ObjList<EventSinkElem>::iterator sinkIt = evIt->sinks.begin();
                 sinkIt != evIt->sinks.end();) {
                if (sinkIt->obj != nullptr) {
                    arr->Node(1) = DataNode(sinkIt->handler);
                    sinkIt->Export(arr);
                    ++sinkIt;
                } else {
                    if (mExporting == 1) {
                        sinkIt = evIt->sinks.erase(sinkIt);
                    } else {
                        ++sinkIt;
                    }
                }
            }
            arr->Node(1) = origNode;
            if (evIt->sinks.empty()) {
                mEventSinks.erase(evIt);
            }
            break;
        }
    }

    mExporting--;
    sCurrentExportEvent = oldExportEvent;
}

void MsgSinks::RemoveSink(Hmx::Object *obj, Symbol ev) {
    MILO_ASSERT(obj, 0x10A);
    for (ObjList<Sink>::iterator it = mSinks.begin(); it != mSinks.end(); ++it) {
        if (it->obj == obj) {
            if (!ev.Null()) {
                MILO_WARN(
                    "%s: removing global to %s for event %s, all other events will be wiped out",
                    PathName(mOwner), obj->Name(), ev
                );
            }
            it->obj = nullptr;
            if (!mExporting)
                mSinks.erase(it);
            return;
        }
    }
    if (ev.Null()) {
        for (ObjList<EventSink>::iterator it = mEventSinks.begin();
             it != mEventSinks.end(); ++it) {
            it->Remove(obj, mExporting != 0);
        }
    } else {
        for (ObjList<EventSink>::iterator it = mEventSinks.begin();
             it != mEventSinks.end(); ++it) {
            if (it->event == ev) {
                it->Remove(obj, mExporting != 0);
                return;
            }
        }
    }
}

void MsgSinks::MergeSinks(Hmx::Object *o) {
    MsgSinks *from = o->Sinks();
    if ((int)from) {
        FOREACH (it, from->mSinks) {
            AddSink(it->obj, Symbol(), Symbol(), it->mode);
        }
        FOREACH (it, from->mEventSinks) {
            FOREACH (elemIt, it->sinks) {
                AddSink(elemIt->obj, it->event, elemIt->handler, elemIt->mode);
            }
        }
    }
}

void MsgSinks::RemovePropertySink(Hmx::Object *o, DataArray *a) {
    Symbol path = PathToEventName(a);
    RemoveSink(o, path);
    if (mPropSyncHandlers) {
        for (int i = 1; i < mPropSyncHandlers->Size(); i += 2) {
            if (path == mPropSyncHandlers->Sym(i)) {
                mPropSyncHandlers->Remove(i);
                mPropSyncHandlers->Remove(i - 1);
                return;
            }
        }
    }
    MILO_NOTIFY_ONCE("Property Sink not in the list! %s -> %s", PathName(mOwner), PathName(o));
}

bool MsgSinks::Replace(ObjRef *ref, Hmx::Object *obj) {
    for (ObjList<Sink>::iterator it = mSinks.begin(); it != mSinks.end(); ++it) {
        if (RefIs(ref, it->obj)) {
            mSinks.erase(it);
            return true;
        }
    }
    for (ObjList<EventSink>::iterator evIt = mEventSinks.begin();
         evIt != mEventSinks.end(); ++evIt) {
        for (ObjList<EventSinkElem>::iterator sinkIt = evIt->sinks.begin();
             sinkIt != evIt->sinks.end(); ++sinkIt) {
            if (&sinkIt->obj == ref) {
                evIt->sinks.erase(sinkIt);
                return true;
            }
        }
    }
    return false;
}
