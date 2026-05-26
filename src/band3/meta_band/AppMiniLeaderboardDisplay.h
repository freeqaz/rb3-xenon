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

    ScoreType mScoreType; // 0x84
    int mSongID; // 0x88
    int mNotesPct; // 0x8c
};

class AppMiniLeaderboardDisplay : public MiniLeaderboardDisplay,
                                  public Leaderboard::Callback {
public:
    AppMiniLeaderboardDisplay();
    virtual ~AppMiniLeaderboardDisplay();
    OBJ_CLASSNAME(AppMiniLeaderboardDisplay)
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
    int UpdateLeaderboard(int songID, ScoreType scoreType);
    void UpdateLeaderboardOnline(int songID);
    void CancelOldServerRequest();

    LeaderboardStatus mStatus; // 0x114
    AppLabel *mTitleLabel; // 0x118
    AppLabel *mIconsLabel; // 0x11c
    UIList *mLeaderboardList; // 0x120
    RndGroup *mPendingGroup; // 0x124
    EventTrigger *mResetTrigger; // 0x128
    EventTrigger *mFadeInTrigger; // 0x12c
    EventTrigger *mFadeOutTrigger; // 0x130
    PlayerMiniLeaderboard *mLeaderboard; // 0x134
    int mSongID; // 0x138
    ScoreType mScoreType; // 0x13c
    float mUpdateTime; // 0x140
};
