#pragma once
#include "game/BandUser.h"
#include "game/Defines.h"
#include "meta_band/AppMiniLeaderboardDisplay.h"
#include "meta/HeldButtonPanel.h"
#include "meta_band/Leaderboard.h"

class SongSelectPanel : public HeldButtonPanel, public Leaderboard::Callback {
public:
    SongSelectPanel();
    OBJ_CLASSNAME(SongSelectPanel);
    OBJ_SET_TYPE(SongSelectPanel);
    NEW_OBJ(SongSelectPanel);
    virtual DataNode Handle(DataArray *, bool);
    // No user-declared dtor: HeldButtonPanel's is already virtual. A
    // user-declared `virtual ~SongSelectPanel() {}` makes MSVC emit a separate
    // ??1SongSelectPanel COMDAT and CALL it from ??_G -- retail has no such
    // function row and inlines the body (vptr store @0x44 + base dtor @+0x48)
    // straight into the scalar deleting destructor.
    virtual bool Exiting() const;
    virtual void Poll();
    virtual void Load();
    virtual void Unload();
    virtual bool IsLoaded() const;
    virtual void FinishLoad();
    virtual void ResultSuccess(bool, bool, bool);
    virtual void ResultFailure();

    DataNode OnMsg(const ButtonDownMsg &);
    Leaderboard *GetLeaderboard(LocalBandUser *, ScoreType, int, Leaderboard::Mode);
    void RestartLeaderboardTimer();
    void CancelLeaderboardTimer();

    Leaderboard *mLeaderboard; // 0x44
    AppMiniLeaderboardDisplay *unk48; // 0x48
    float unk4c;
    float unk50;
    bool unk54;
    float unk58;
};