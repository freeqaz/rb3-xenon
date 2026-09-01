#pragma once
#include "flow/FlowLabel.h"
#include "flow/FlowLabelProvider.h"
#include "flow/FlowOutPort.h"
#include "flow/FlowQueueable.h"
#include "flow/FlowTrigger.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "rndobj/Poll.h"
#include "utl/Str.h"

/** "Top level flow" */
class Flow : public FlowQueueable,
             public ObjectDir,
             public FlowLabelProvider,
             public RndPollable {
public:
    struct DynamicPropertyEntry {
        enum PropType {
            kInt,
            kFloat,
            kBool,
            kString,
            kColor,
            kObject,
            kSymbolList
        };
        DynamicPropertyEntry(Hmx::Object *);

        void SetName(DataNode &);
        void ResetDefaultValues();
        Symbol GetDefaultValueSymbol();
        void SetClassFilter(DataNode &);
        DataNode GetSymbolList();

        /** "Name for the property" */
        String mName; // 0x0
        /** "type of the property" */
        PropType mType; // 0xc
        DataNode mDefaultVal; // 0x10
        /** "Help string for the user" */
        String mHelp; // 0x18
        /** "Is this property exposed to the proxy using this flow?" */
        bool mExposed; // 0x24
        Symbol mObjectClass; // 0x28
        DataNode mSymbolList; // 0x2c
        Symbol mObjectType; // 0x34
        /** TU5 inserted ~80 bytes of additional members here (layout-correct
         *  placeholder; retail sizeof(DynamicPropertyEntry) == 0x88). */
        char mUnkTU5_0x30[0x50]; // 0x38
    };
    // Hmx::Object
    virtual ~Flow();
    OBJ_CLASSNAME(Flow)
    OBJ_SET_TYPE(Flow)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, CopyType);
    virtual void Load(BinStream &);
    virtual void PreSave(BinStream &);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    // ObjectDir
    virtual void SyncObjects();
    // RndPollable
    virtual void Enter();
    virtual void Exit();
    // FlowQueueable
    virtual bool Activate();
    virtual void Deactivate(bool);
    virtual void RequestStop();
    virtual void RequestStopCancel();
    virtual void Execute(QueueState);
    virtual Flow *GetOwnerFlow();
    virtual bool ActivateTrigger();
    virtual bool Activate(Hmx::Object *);
    virtual bool Activate(Hmx::Object *, DataArray *);

    OBJ_MEM_OVERLOAD(0x39)
    NEW_OBJ(Flow)

    int GetNumParams() const { return mParamApplyCount; }
    int GetStartMode() const { return mStartMode; }
    void RefreshPortLabelLists();
    FlowLabel *GetLabelForSym(Symbol);
    void ApplyParams(DataArray *, FlowTrigger *);

    void StartOnEnter(bool start) {
        if (start)
            mStartMode = 2;
        else
            mStartMode = 0;
    }

    void StartAfterGameCode(bool start) {
        if (mStartMode != 0)
            mStartMode = start ? 2 : 1;
    }

    friend class FlowNode;

protected:
    Flow();

    void ToggleRunning(int);
    void OnReflectedPropertyChanged(DataArray *);
    void OnInternalPropertyChanged(DataArray *);

    static bool sReflectingProperty;

    ObjVector<DynamicPropertyEntry> mDynamicProperties; // 0x114
    ObjPtrVec<FlowLabel> mFlowLabels; // 0x124
    ObjPtrVec<FlowOutPort> mFlowOutPorts; // 0x140
    ObjPtrVec<Hmx::Object> mObjects; // 0x15c
    int mStartMode; // 0x178
    /** "Are we hidden from run nodes?" */
    bool mPrivate; // 0x17c
    /** "force things to stop immediately?" */
    bool mHardStop; // 0x17d
    int mParamApplyCount; // 0x180
};

// FLOW_PROPANIM_COMMANDS_ENUM
// #define kFlowStart (0)
// #define kFlowStopImmediate (1)
// #define kFlowStopWhenAble (2)

void FlowInit();
