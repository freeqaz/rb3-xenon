#pragma once
#include "obj/Msg.h"

class GameMode : public MsgSource {
public:
    GameMode();
    virtual DataNode Handle(DataArray *, bool);
    virtual ~GameMode();

    void SetMode(Symbol);
    bool InMode(Symbol);
    DataNode OnSetMode(const DataArray *);
    Symbol GetMode() const { return mMode; }

    Symbol mMode; // 0x18
};

void GameModeInit();

extern GameMode *TheGameMode;

#include "obj/Msg.h"

DECLARE_MESSAGE(ModeChangedMsg, "mode_changed")
ModeChangedMsg() : Message(Type()) {}
END_MESSAGE