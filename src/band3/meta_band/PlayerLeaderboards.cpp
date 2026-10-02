#include "meta_band/PlayerLeaderboards.h"
#include "decomp.h"
#include "meta_band/Leaderboard.h"
#include "meta_band/ProfileMgr.h"
#include "net/Net.h"
#include "net/Server.h"
#include "net_band/RockCentral.h"
#include "os/PlatformMgr.h"
#include "utl/Symbol.h"

PlayerLeaderboard::PlayerLeaderboard(Profile *p, Callback *cb)
    : Leaderboard(EntityID(), cb) {
    // Retail 0x826731E0: no guest-owner fallback; a null profile leaves the
    // default EntityID.
    if (p) {
        Server *s = TheNet.mServer;
        mEntityID = EntityID(s->GetPlayerID(p->GetPadNum()));
    }
}

// Retail guard word lbl_82E020C8 packs three local-static Symbols, claimed
// bit0..bit2 in declaration order: pad_error, privilege_error, gamertag_error
// (all constructed unconditionally, before the sign-in gate, right after the
// two early-return bounds checks).
//
// ShowGamercard's result maps as retail's branches do: PrivilegeFailed (-2)
// -> privilege_error; NotSignedIn (-3) -> pad_error, the same load the
// IsSignedInOnline() gate uses (its beq lands on that arm's lwz); any other
// negative result (-1) -> gamertag_error; success -> the shared gNullStr tail.
//
// Both early-return bounds checks share ONE physical destination in retail
// (a single far shared tail that constructs Symbol(gNullStr) once), which a
// single `&&` condition with a fall-through `return gNullStr` reproduces.
//
// The OnlineID copy is a raw ld/std pair in retail, so OnlineID's copy ctor is
// compiler-implicit there.
Symbol PlayerLeaderboard::OnSelectRow(int row, BandUser *user) {
    if (IsEnumComplete() && NumData() > row) {
        static Symbol pad_error("display_gamercard_pad_error");
        static Symbol privilege_error("display_gamercard_privilege_error");
        static Symbol gamertag_error("on_select_gamertag_error");

        LocalBandUser *localBandUser = user->GetLocalBandUser();
        if (!localBandUser->IsSignedInOnline())
            return pad_error;

        LeaderboardRow &lbRow = mLeaderboardRows[row];
        OnlineID oid = lbRow.mLBOnlineID;
        ShowGamercardResult result = ThePlatformMgr.ShowGamercard(localBandUser, &oid);
        if (result == kShowGamercardResult_PrivilegeFailed)
            return privilege_error;
        if (result == kShowGamercardResult_NotSignedIn)
            return pad_error;
        if (result < kShowGamercardResult_Success)
            return gamertag_error;
    }
    return gNullStr;
}

bool PlayerLeaderboard::CanRowsBeSelected() const { return false; }
bool PlayerLeaderboard::IsRowFriend(int idx) const {
    return mLeaderboardRows[idx].mIsFriend;
}
bool PlayerLeaderboard::IsRowSelf(int idx) const { return mLeaderboardRows[idx].mIsSelf; }

PlayerSongLeaderboard::PlayerSongLeaderboard(
    Profile *p, Leaderboard::Callback *cb, ScoreType s, int id
)
    : PlayerLeaderboard(p, cb), mScoreType(s), mSongID(id) {}

void PlayerSongLeaderboard::EnumerateFromID() {
    mDataResultList.Clear();
    std::vector<int> ids;
    GetPlayerIds(ids);
    TheRockCentral.GetLeaderboardByPlayer(
        ids,
        mSongID,
        mScoreType,
        kSong,
        ModeToLeaderboardMode(mMode),
        sPageSize,
        mDataResultList,
        this
    );
}

void PlayerSongLeaderboard::EnumerateRankRange(int i1, int i2) {
    mDataResultList.Clear();
    std::vector<int> ids;
    GetPlayerIds(ids);
    TheRockCentral.GetLeaderboardByRankRange(
        ids, mSongID, mScoreType, i1, i1 + i2, kSong, mDataResultList, this
    );
}

void PlayerSongLeaderboard::GetStats() {
    mDataResultList.Clear();
    std::vector<int> ids;
    GetPlayerIds(ids);
    TheRockCentral.GetMaxRank(ids, mSongID, mScoreType, kSong, mDataResultList, this);
}

PlayerBattleLeaderboard::PlayerBattleLeaderboard(
    Profile *p, Leaderboard::Callback *cb, int id
)
    : PlayerLeaderboard(p, cb), mSongID(id) {}

void PlayerBattleLeaderboard::EnumerateFromID() {
    mDataResultList.Clear();
    std::vector<int> ids;
    GetPlayerIds(ids);
    TheRockCentral.GetBattleLeaderboardByPlayer(
        ids, mSongID, ModeToLeaderboardMode(mMode), sPageSize, mDataResultList, this
    );
}

void PlayerBattleLeaderboard::EnumerateRankRange(int i1, int i2) {
    mDataResultList.Clear();
    std::vector<int> ids;
    GetPlayerIds(ids);
    TheRockCentral.GetBattleLeaderboardByRankRange(
        ids, mSongID, i1, i1 + i2, mDataResultList, this
    );
}

void PlayerBattleLeaderboard::GetStats() {
    mDataResultList.Clear();
    std::vector<int> ids;
    GetPlayerIds(ids);
    TheRockCentral.GetBattleMaxRank(ids, mSongID, mDataResultList, this);
}
