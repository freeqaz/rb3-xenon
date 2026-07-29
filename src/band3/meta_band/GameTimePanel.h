#pragma once
#include "os/Timer.h"
#include "ui/UIPanel.h"

// RB3-360 retail layout, read off the target bodies in the 0x8261EA40 block:
//   mTimer  @0x40 (ctor `addi r3,r30,0x40` -> Timer::Timer)
//   mTempo  @0x70 (Enter: `stfs f0,0x70` = DeltaBeat()/DeltaSeconds())
//   mPeriod @0x74 (Load: old TheLoadMgr period)
//   mStartSeconds @0x78 (Enter: Seconds(kRealTime)+DeltaSeconds(); Poll reads it)
// The rb3-Wii dev header lists 0x38/0x68/0x6c (Wii base size) and lacks the
// 0x78 member entirely.  It also redeclares `virtual ~GameTimePanel() {}`;
// retail's scalar-deleting dtor (0x8261EF60) has no own-vptr store, so the
// redeclaration is dropped here (laneBL §7).
class GameTimePanel : public UIPanel {
public:
    GameTimePanel();
    OBJ_CLASSNAME(GameTimePanel);
    OBJ_SET_TYPE(GameTimePanel);
    virtual void Enter();
    virtual void Exit();
    virtual void Poll();
    virtual void Load();
    virtual void Unload();
    NEW_OBJ(GameTimePanel);
    static void Init() { REGISTER_OBJ_FACTORY(GameTimePanel); }

    Timer mTimer; // 0x40
    float mTempo; // 0x70
    float mPeriod; // 0x74
    float mStartSeconds; // 0x78
};
