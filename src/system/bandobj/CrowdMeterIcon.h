#pragma once
#include "obj/ObjMacros.h"
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

    ObjPtr<EventTrigger> mResetTrig; // 0x1dc
    ObjPtr<EventTrigger> mArrowHideTrig; // 0x1e8
    ObjPtr<EventTrigger> mArrowShowTrig; // 0x1f4
    ObjPtr<EventTrigger> mDeployTrig; // 0x200
    ObjPtr<EventTrigger> mStopDeployTrig; // 0x20c
    ObjPtr<EventTrigger> mStateFailedTrig; // 0x218
    ObjPtr<EventTrigger> mStateFailingTrig; // 0x224
    ObjPtr<EventTrigger> mStateNormalTrig; // 0x230
    ObjPtr<EventTrigger> mGlowTrig; // 0x23c
    ObjPtr<EventTrigger> mGlowStopTrig; // 0x248
    ObjPtr<EventTrigger> mStateQuarantinedTrig; // 0x254
    ObjPtr<EventTrigger> mDropInTrig; // 0x260
    ObjPtr<EventTrigger> mDropOutTrig; // 0x26c
    ObjPtr<BandLabel> mIconLabel; // 0x278
    ObjPtr<RndAnimatable> mIconStateAnim; // 0x284
    TrackPanelDirBase *unk240; // 0x290
    CrowdMeterState mState; // 0x294
    bool mQuarantined; // 0x298
};
