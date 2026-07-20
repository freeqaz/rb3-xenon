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

    // TU5 inserts a 4-byte word here that our TU0 model lacked: retail TU5 reads
    // mMode at 0x1c (objdiff InMode: lwz r,0x1c vs base 0x18). rb3-Wii/DC3 show no
    // named GameMode member at this slot, so this is a named placeholder pinning
    // the +4 shift. TODO(tu5): identify the real field (likely a base/layout word).
    int mUnkTU5_0x18; // 0x18
    Symbol mMode; // 0x1c
};

void GameModeInit();

extern GameMode *TheGameMode;

#include "obj/Msg.h"

DECLARE_MESSAGE(ModeChangedMsg, "mode_changed")
ModeChangedMsg() : Message(Type()) {}
END_MESSAGE