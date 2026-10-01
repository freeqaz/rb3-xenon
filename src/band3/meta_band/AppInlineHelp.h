#pragma once
#include "bandobj/InlineHelp.h"
#include "game/BandUser.h"
#include "meta_band/InputMgr.h"
#include "meta_band/SessionMgr.h"

class AppInlineHelp : public InlineHelp {
public:
    AppInlineHelp();
    // Base-name registration: retail band.exe has no "AppInlineHelp" C string;
    // 0x8256e688 -- called by ClassName/SetType@AppInlineHelp -- builds
    // "InlineHelp", which is what the InlineHelp base declares.  (Writing
    // AppInlineHelp here is not what 360 retail links.)
    OBJ_CLASSNAME(InlineHelp)
    OBJ_SET_TYPE(AppInlineHelp)
    NEW_OBJ(AppInlineHelp)
    virtual DataNode Handle(DataArray *, bool);
    // W16-HR: implicit dtor -- retail ??_D calls ??1InlineHelp directly
    virtual void Enter();
    virtual void Exit();
    virtual void UpdateIconTypes(bool);

    void SetOverrideUser(LocalBandUser *);

    DataNode OnMsg(const InputStatusChangedMsg &);
    DataNode OnMsg(const LocalUserLeftMsg &);
    DataNode OnMsg(const AddLocalUserResultMsg &);

    LocalBandUser *mOverrideUser; // 0x17c
};