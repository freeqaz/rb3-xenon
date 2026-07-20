#pragma once
#include "obj/Data.h"
#include "obj/Object.h"
#include "game/BandUser.h"

class PresenceMgr : public Hmx::Object {
public:
    PresenceMgr();
    virtual ~PresenceMgr() {}
    virtual DataNode Handle(DataArray *, bool);

    void UpdatePresence();
    void SetSongID(int); // retail-360-only: GamePanel::Enter pushes the current songID into rich presence (target fn at 0x82662E70)
    Symbol GetPresenceMode();
    int GetPresenceContextFromMode(Symbol, bool);
    int GetPlayModeContextFromUser(const LocalBandUser *, bool);

protected:
    DataNode OnPresenceChange(DataArray *);

public:
    DataArray *unk1c;
    DataArray *unk20;
    DataArray *unk24;
    Symbol unk28;
    std::vector<Symbol> unk2c;
    int unk34;
    bool unk38;
    bool unk39;
    int unk3c;
};

extern PresenceMgr ThePresenceMgr;