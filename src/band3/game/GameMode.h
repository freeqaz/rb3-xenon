#pragma once
#include "obj/Msg.h"

class GameMode : public MsgSource {
public:
    GameMode();
    virtual DataNode Handle(DataArray *, bool);
    virtual ~GameMode();
    virtual bool InMode(Symbol);
    virtual void SetMode(Symbol);

    DataNode OnSetMode(const DataArray *);
    Symbol GetMode() const { return mMode; }

    // Retail X360 layout (proven from the GameMode.cpp target span):
    //  * InMode/SetMode read+write mMode at 0x1c  (lwz r11,0x1c(r3) / stw r29,0x1c(r23))
    //  * the deleting-dtor thunk adjusts by 0x24, i.e. the Hmx::Object virtual base
    //    sits at 0x24, and GameModeInit allocates sizeof(GameMode) == 0x4c.
    // MsgSource's non-virtual part ends at 0x1c (GameMode adds its own vfptr at 0,
    // which pushes MsgSource's vbptr to 4), so mMode is GameMode's ONLY member and
    // it lands at 0x1c. The 4 bytes at 0x20 are not a member: they are the vtordisp
    // slot MSVC places immediately before the virtual base (GameMode's dtor thunk is
    // mangled `??_E...$4PPPPPPPM@A@...`, i.e. vtordisp(-4,0)) -- hence vbase 0x24 and
    // sizeof == 0x4c. rb3-Wii's DEV build carries ~20 extra cached-mode-flag ints
    // here (mOverdriveEnabled, mIsPractice, ...); retail X360 has NONE of them, and
    // its SetMode correspondingly does no Property() caching.
    // The earlier `mUnkTU5_0x18` placeholder sat *before* mMode and pushed it to
    // 0x20 (vbase 0x28, size 0x50) -- wrong on both counts.
    Symbol mMode; // 0x1c
};

void GameModeInit();

extern GameMode *TheGameMode;

#include "obj/Msg.h"

DECLARE_MESSAGE(ModeChangedMsg, "mode_changed")
ModeChangedMsg() : Message(Type()) {}
END_MESSAGE