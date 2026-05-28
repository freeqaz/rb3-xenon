#pragma once
#include "obj/Data.h"
#include "obj/Object.h"

/** A DataArray container to send to other objects for handling. */
class Message {
public:
    // Message(); // if there IS a void ctor for Msg i can't find it

    Message(Symbol type) {
        mData = new DataArray(2);
        mData->Node(1) = type;
    }

    Message(Symbol type, const DataNode &arg1) {
        mData = new DataArray(3);
        mData->Node(1) = type;
        mData->Node(2) = arg1;
    }

    Message(Symbol type, const DataNode &arg1, const DataNode &arg2) {
        mData = new DataArray(4);
        mData->Node(1) = type;
        mData->Node(2) = arg1;
        mData->Node(3) = arg2;
    }

    Message(
        Symbol type, const DataNode &arg1, const DataNode &arg2, const DataNode &arg3
    ) {
        mData = new DataArray(5);
        mData->Node(1) = type;
        mData->Node(2) = arg1;
        mData->Node(3) = arg2;
        mData->Node(4) = arg3;
    }

    Message(
        Symbol type,
        const DataNode &arg1,
        const DataNode &arg2,
        const DataNode &arg3,
        const DataNode &arg4
    ) {
        mData = new DataArray(6);
        mData->Node(1) = type;
        mData->Node(2) = arg1;
        mData->Node(3) = arg2;
        mData->Node(4) = arg3;
        mData->Node(5) = arg4;
    }

    Message(
        Symbol type,
        const DataNode &arg1,
        const DataNode &arg2,
        const DataNode &arg3,
        const DataNode &arg4,
        const DataNode &arg5
    ) {
        mData = new DataArray(7);
        mData->Node(1) = type;
        mData->Node(2) = arg1;
        mData->Node(3) = arg2;
        mData->Node(4) = arg3;
        mData->Node(5) = arg4;
        mData->Node(6) = arg5;
    }

    Message(
        Symbol type,
        const DataNode &arg1,
        const DataNode &arg2,
        const DataNode &arg3,
        const DataNode &arg4,
        const DataNode &arg5,
        const DataNode &arg6
    ) {
        mData = new DataArray(8);
        mData->Node(1) = type;
        mData->Node(2) = arg1;
        mData->Node(3) = arg2;
        mData->Node(4) = arg3;
        mData->Node(5) = arg4;
        mData->Node(6) = arg5;
        mData->Node(7) = arg6;
    }

    Message(
        Symbol type,
        const DataNode &arg1,
        const DataNode &arg2,
        const DataNode &arg3,
        const DataNode &arg4,
        const DataNode &arg5,
        const DataNode &arg6,
        const DataNode &arg7
    ) {
        mData = new DataArray(9);
        mData->Node(1) = type;
        mData->Node(2) = arg1;
        mData->Node(3) = arg2;
        mData->Node(4) = arg3;
        mData->Node(5) = arg4;
        mData->Node(6) = arg5;
        mData->Node(7) = arg6;
        mData->Node(8) = arg7;
    }

    Message(
        Symbol type,
        const DataNode &arg1,
        const DataNode &arg2,
        const DataNode &arg3,
        const DataNode &arg4,
        const DataNode &arg5,
        const DataNode &arg6,
        const DataNode &arg7,
        const DataNode &arg8
    ) {
        mData = new DataArray(10);
        mData->Node(1) = type;
        mData->Node(2) = arg1;
        mData->Node(3) = arg2;
        mData->Node(4) = arg3;
        mData->Node(5) = arg4;
        mData->Node(6) = arg5;
        mData->Node(7) = arg6;
        mData->Node(8) = arg7;
        mData->Node(9) = arg8;
    }

    Message(
        Symbol type,
        const DataNode &arg1,
        const DataNode &arg2,
        const DataNode &arg3,
        const DataNode &arg4,
        const DataNode &arg5,
        const DataNode &arg6,
        const DataNode &arg7,
        const DataNode &arg8,
        const DataNode &arg9
    ) {
        mData = new DataArray(11);
        mData->Node(1) = type;
        mData->Node(2) = arg1;
        mData->Node(3) = arg2;
        mData->Node(4) = arg3;
        mData->Node(5) = arg4;
        mData->Node(6) = arg5;
        mData->Node(7) = arg6;
        mData->Node(8) = arg7;
        mData->Node(9) = arg8;
        mData->Node(10) = arg9;
    }

    Message(DataArray *da) : mData(da) { da->AddRef(); }

    Message(int i) { mData = new DataArray(i + 2); }

    virtual ~Message() { mData->Release(); }

    DataArray *mData; // 0x0

    operator DataArray *() const { return mData; }
    DataArray *operator->() const { return mData; }
    DataArray *Data() const { return mData; }

    void SetType(Symbol type) { mData->Node(1) = type; }

    Symbol Type() const { return mData->Sym(1); }
    DataNode &operator[](int idx) { return mData->Node(idx + 2); }
};

#define DECLARE_MESSAGE(classname, type)                                                 \
    class classname : public Message {                                                   \
    public:                                                                              \
        classname(DataArray *da) : Message(da) {}                                        \
        virtual ~classname() {}                                                          \
        static Symbol Type() {                                                           \
            static Symbol t(type);                                                       \
            return t;                                                                    \
        }

#define DECLARE_MESSAGE_NOINLINE_DTOR(classname, type)                                   \
    class classname : public Message {                                                   \
    public:                                                                              \
        classname(DataArray *da) : Message(da) {}                                        \
        virtual ~classname();                                                            \
        static Symbol Type() {                                                           \
            static Symbol t(type);                                                       \
            return t;                                                                    \
        }

#define END_MESSAGE                                                                      \
    }                                                                                    \
    ;

#include "obj/Object.h"
#include "utl/MemMgr.h"
#include <list>

class ObjRef;

/** "Exports messages to other objects called sinks" */
class MsgSource : public virtual Hmx::Object {
public:
    enum SinkMode {
        kHandle = 0,
        kExport = 1,
        kType = 2,
        kExportType = 3,
    };

    struct Sink {
        Sink() {}
        Sink(Hmx::Object *o, SinkMode m) : obj(o), mode(m) {}
        Hmx::Object *obj;   // 0x0
        SinkMode mode;      // 0x4
        void Export(DataArray *);
    };

    struct EventSinkElem : public Sink {
        EventSinkElem() {}
        EventSinkElem(Hmx::Object *o, SinkMode m, Symbol s) : Sink(o, m), handler(s) {}
        Symbol handler;     // 0x8
    };

    struct EventSink {
        EventSink() {}
        EventSink(Symbol s) : ev(s) {}
        Symbol ev;          // 0x0
        std::list<EventSinkElem> sinks; // 0x4
        void Add(Hmx::Object *, SinkMode, Symbol, bool);
        void Remove(Hmx::Object *, MsgSource *, bool);
    };

    MsgSource();
    OBJ_CLASSNAME(MsgSource);
    OBJ_SET_TYPE(MsgSource);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual ~MsgSource();
    virtual void Replace(Hmx::Object *, Hmx::Object *);
    virtual void Export(DataArray *, bool);

    void ChainSource(MsgSource *, MsgSource *);
    void AddSink(Hmx::Object *, Symbol = Symbol(), Symbol = Symbol(), SinkMode = kHandle);
    void RemoveSink(Hmx::Object *, Symbol = Symbol());
    void MergeSinks(MsgSource *);
    DataNode OnAddSink(DataArray *);
    DataNode OnRemoveSink(DataArray *);

    NEW_OBJ(MsgSource)
    static void Init() { REGISTER_OBJ_FACTORY(MsgSource) }

    std::list<Sink> mSinks;        // 0x8
    std::list<EventSink> mEventSinks; // 0x10
    int mExporting;                // 0x18
};

class MsgSinks {
    friend bool PropSync(MsgSinks &, DataNode &, DataArray *, int, PropOp);

public:
    struct Sink {
        Sink(Hmx::Object *owner) : obj(owner, nullptr) {}
        ~Sink() {}

        Sink &operator=(const Sink &s) {
            obj.SetObjConcrete(s.obj);
            mode = s.mode;
            return *this;
        }

        void Export(DataArray *);

        /** "Object to sink to" */
        ObjOwnerPtr<Hmx::Object> obj; // 0x0
        /** "the mode" */
        Hmx::Object::SinkMode mode; // 0x14
    };
    struct EventSinkElem : public Sink {
        EventSinkElem(Hmx::Object *owner) : Sink(owner) {}
        EventSinkElem &operator=(const EventSinkElem &);

        /** "Name of the handler to use" */
        Symbol handler; // 0x18
    };
    struct EventSink {
        EventSink(Hmx::Object *owner) : sinks(owner) {}
        void Add(Hmx::Object *, Hmx::Object::SinkMode, Symbol, bool);
        void Remove(Hmx::Object *, bool exporting);

        /** "the event to send down" */
        Symbol event; // 0x0
        bool chainProxy; // 0x4
        /** "the objects, with modes and handlers to send this event to" */
        ObjList<EventSinkElem> sinks; // 0x8
    };

    MsgSinks(Hmx::Object *owner);
    ~MsgSinks();
    bool Replace(ObjRef *from, Hmx::Object *to);
    void RemovePropertySink(Hmx::Object *, DataArray *);
    bool HasPropertySink(Hmx::Object *, DataArray *);
    void RemoveSink(Hmx::Object *s, Symbol event);
    void AddSink(
        Hmx::Object *s,
        Symbol ev,
        Symbol handler = Symbol(),
        Hmx::Object::SinkMode mode = Hmx::Object::kHandle,
        bool chainProxy = true
    );
    void AddPropertySink(Hmx::Object *, DataArray *, Symbol);
    void MergeSinks(Hmx::Object *from);
    Symbol GetPropSyncHandler(DataArray *);
    void Export(DataArray *);
    bool HasSink(Hmx::Object *) const;
    void ChainEventSinks(Hmx::Object *from, Hmx::Object *to);

    ObjList<Sink> &Sinks() { return mSinks; }
    static Symbol CurrentExportEvent() { return sCurrentExportEvent; }

    MEM_OVERLOAD(MsgSinks, 0xAF);

    friend bool PropSync(MsgSinks &, DataNode &, DataArray *, int, PropOp);

private:
    DataArray *mPropSyncHandlers; // 0x0 - array of {DataArray*, Symbol} pairs for property sync export handlers
    ObjList<Sink> mSinks; // 0x4
    /** "Event specific sinks, each particular event is sent to these guys" */
    ObjList<EventSink> mEventSinks; // 0x10
    /** The number of messages that are exporting. */
    int mExporting; // 0x1c
    Hmx::Object *mOwner; // 0x20

    static Symbol sCurrentExportEvent;
};

#include "obj/PropSync.h"
bool PropSync(MsgSinks &, DataNode &, DataArray *, int, PropOp);
