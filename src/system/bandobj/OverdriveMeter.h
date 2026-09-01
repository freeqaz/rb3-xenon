#pragma once
#include "obj/ObjMacros.h"
#include "rndobj/Dir.h"
#include "rndobj/EventTrigger.h"
#include "rndobj/Group.h"

class OverdriveMeter : public RndDir {
public:
    enum State {
        kFilling = 1,
        kReady = 2,
        kDeploying = 3,
    };

    OverdriveMeter();
    OBJ_CLASSNAME(OverdriveMeterDir);
    OBJ_SET_TYPE(OverdriveMeterDir);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual ~OverdriveMeter();
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    virtual void SyncObjects();

    void Reset();
    void SetEnergy(float, State, Symbol, float, bool);
    void MiloReset();
    void EnergyReady(Symbol, bool, float);
    void Deploy();
    void StopDeploy();
    void SetNoOverdrive();

    DECLARE_REVS;
    OBJ_MEM_OVERLOAD(0x24);
    NEW_OBJ(OverdriveMeter)
    static void Init() { Register(); }
    REGISTER_OBJ_FACTORY_FUNC(OverdriveMeter)

    State mState; // 0x1dc
    ObjPtr<EventTrigger> mResetTrig;
    ObjPtr<EventTrigger> mSpotlightPhraseSuccessTrig;
    ObjPtr<EventTrigger> mBeDeployingTrig;
    ObjPtr<EventTrigger> mBeFillingTrig;
    ObjPtr<EventTrigger> mBeReadyTrig;
    ObjPtr<EventTrigger> mPulseMiloTrig;
    ObjPtr<EventTrigger> mNoOverdriveTrig;
    ObjPtr<RndGroup> mExtendAnimGroup;
    ObjPtr<RndGroup> mPulseAnimGroup;
    float mTestEnergy;
};