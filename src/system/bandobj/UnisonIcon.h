#pragma once
#include "obj/ObjMacros.h"
// Ported from rb3-Wii src/system/bandobj/UnisonIcon.h.
#include "rndobj/Dir.h"
#include "rndobj/EventTrigger.h"
#include "bandobj/BandLabel.h"

class UnisonIcon : public RndDir {
public:
    UnisonIcon();
    OBJ_CLASSNAME(UnisonIcon)
    OBJ_SET_TYPE(UnisonIcon)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, CopyType);
    virtual ~UnisonIcon() {}
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    virtual void SyncObjects();

    void Reset();
    void SetProgress(float);
    void UnisonStart();
    void UnisonEnd();
    void Succeed();
    void Fail();
    void SetIcon(const char *);

    DECLARE_REVS;
    OBJ_MEM_OVERLOAD(0x1f);
    NEW_OBJ(UnisonIcon)
    static void Init() { Register(); }
    REGISTER_OBJ_FACTORY_FUNC(UnisonIcon)

    float mProgress; // 0x1dc
    EventTrigger *mStartTrig; // 0x1e0
    EventTrigger *mEndTrig; // 0x1e4
    EventTrigger *mSucceedTrig; // 0x1e8
    EventTrigger *mFailTrig; // 0x1ec
    EventTrigger *mResetTrig; // 0x1f0
    RndAnimatable *mMeterWipeAnim; // 0x1f4
    BandLabel *mIconLabel; // 0x1f8
};
