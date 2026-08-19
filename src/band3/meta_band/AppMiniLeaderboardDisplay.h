#pragma once
#include "bandobj/MiniLeaderboardDisplay.h"
#include "meta/Profile.h"
#include "meta_band/AppLabel.h"
#include "meta_band/Leaderboard.h"
#include "meta_band/PlayerLeaderboards.h"
#include "obj/ObjMacros.h"
#include "rndobj/EventTrigger.h"
#include "rndobj/Group.h"
#include "ui/UIList.h"

enum LeaderboardStatus {
    kLeaderboardUnloaded = 0,
    kLeaderboardReady = 1,
    kLeaderboardLoading = 2,
    kLeaderboardError = 3
};

class PlayerMiniLeaderboard : public PlayerLeaderboard {
public:
    PlayerMiniLeaderboard(Profile *p, Callback *cb, ScoreType s, int id, int notesPct)
        : PlayerLeaderboard(p, cb), mScoreType(s), mSongID(id), mNotesPct(notesPct) {}
    virtual ~PlayerMiniLeaderboard() {}
    virtual void EnumerateFromID();
    virtual void EnumerateRankRange(int, int) {}
    virtual void GetStats() {}

    ScoreType mScoreType; // 0xa0
    int mSongID; // 0xa4
    int mNotesPct; // 0xa8
};

class AppMiniLeaderboardDisplay : public MiniLeaderboardDisplay,
                                  public Leaderboard::Callback {
public:
    AppMiniLeaderboardDisplay();
    virtual ~AppMiniLeaderboardDisplay();
    // Base-name registration: retail band.exe has no "AppMiniLeaderboardDisplay"
    // C string; 0x8264bce8 -- called by ClassName/SetType@AppMiniLeaderboardDisplay
    // -- builds "MiniLeaderboardDisplay".  DC3's AppMiniLeaderboardDisplay.h agrees.
    OBJ_CLASSNAME(MiniLeaderboardDisplay)
    OBJ_SET_TYPE(AppMiniLeaderboardDisplay)
    virtual DataNode Handle(DataArray *, bool);
    virtual void DrawShowing();
    virtual void Exit();
    virtual void Poll();
    virtual void Update();
    virtual void ResultSuccess(bool, bool, bool);
    virtual void ResultFailure();

    NEW_OBJ(AppMiniLeaderboardDisplay)
    static void Init();
    static void Register() { REGISTER_OBJ_FACTORY(AppMiniLeaderboardDisplay) }

    void SetLeaderboardStatus(LeaderboardStatus);
    bool IsReady();
    bool HasRows();
    bool UpdateLeaderboard(int songID, ScoreType scoreType);
    void UpdateLeaderboardOnline(int songID);
    void CancelOldServerRequest();

    LeaderboardStatus mStatus; // 0x148
    AppLabel *mTitleLabel; // 0x14c
    AppLabel *mIconsLabel; // 0x150
    UIList *mLeaderboardList; // 0x154
    RndGroup *mPendingGroup; // 0x158
    EventTrigger *mResetTrigger; // 0x15c
    EventTrigger *mFadeInTrigger; // 0x160
    EventTrigger *mFadeOutTrigger; // 0x164
    PlayerMiniLeaderboard *mLeaderboard; // 0x168
    int mSongID; // 0x16c
    ScoreType mScoreType; // 0x170
    float mUpdateTime; // 0x174
};
