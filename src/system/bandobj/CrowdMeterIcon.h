#pragma once
#include "rndobj/Dir.h"
#include "rndobj/EventTrigger.h"
#include "bandobj/BandLabel.h"

enum CrowdMeterState {
    kCrowdMeterNormal = 0,
    kCrowdMeterWarning = 1,
    kCrowdMeterFailed = 2,
    kCrowdMeterInvalidState = 3
};

class TrackPanelDirBase;

class CrowdMeterIcon : public RndDir {
public:
    CrowdMeterIcon();
    OBJ_CLASSNAME(CrowdMeterIcon)
    OBJ_SET_TYPE(CrowdMeterIcon)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual ~CrowdMeterIcon() {}
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &bs);
    virtual void SyncObjects();

    void Deploy();
    void StopDeploy();
    void Reset();
    void SetQuarantined(bool);
    void DropIn();
    void DropOut();
    void SetState(CrowdMeterState, bool);
    void ArrowShow(bool);
    void SetGlowing(bool);
    void SetIcon(const char *);
    bool HasIcon() const;

    DECLARE_REVS;
    NEW_OVERLOAD;
    DELETE_OVERLOAD;
    NEW_OBJ(CrowdMeterIcon)
    static void Init() { Register(); }
    REGISTER_OBJ_FACTORY_FUNC(CrowdMeterIcon)

    ObjPtr<EventTrigger> mResetTrig; // 0x18c
    ObjPtr<EventTrigger> mArrowHideTrig; // 0x198
    ObjPtr<EventTrigger> mArrowShowTrig; // 0x1a4
    ObjPtr<EventTrigger> mDeployTrig; // 0x1b0
    ObjPtr<EventTrigger> mStopDeployTrig; // 0x1bc
    ObjPtr<EventTrigger> mStateFailedTrig; // 0x1c8
    ObjPtr<EventTrigger> mStateFailingTrig; // 0x1d4
    ObjPtr<EventTrigger> mStateNormalTrig; // 0x1e0
    ObjPtr<EventTrigger> mGlowTrig; // 0x1ec
    ObjPtr<EventTrigger> mGlowStopTrig; // 0x1f8
    ObjPtr<EventTrigger> mStateQuarantinedTrig; // 0x204
    ObjPtr<EventTrigger> mDropInTrig; // 0x210
    ObjPtr<EventTrigger> mDropOutTrig; // 0x21c
    ObjPtr<BandLabel> mIconLabel; // 0x228
    ObjPtr<RndAnimatable> mIconStateAnim; // 0x234
    TrackPanelDirBase *unk240; // 0x240
    CrowdMeterState mState; // 0x244
    bool mQuarantined; // 0x248
};
