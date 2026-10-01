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
// two early-return bounds checks). The NotSignedIn (-3) and the fallback
// (ShowGamercard result == -1) branches share identical retail code -- both
// return gamertag_error -- which reads as an explicit NotSignedIn check
// followed by a catch-all default, not a coding oddity.
// Note: kShowGamercardResult_Offline and kShowGamercardResult_Failed are both
// -1 in PlatformMgr.h; retail bytes only prove the numeric value -1 reaches
// this path, not which enumerator name the original source spelled.
//
// Both early-return bounds checks share ONE physical destination in retail
// (a single far shared tail that constructs Symbol(gNullStr) once) rather
// than each getting its own inline reconstruction -- combining them into one
// `&&` condition (instead of two sequential `if (...) return gNullStr;`
// statements) is what reproduces that: 82.0% -> 87.0% fuzzy. The same
// mechanism applies to the NotSignedIn/fallback pair below: writing them as
// two sequential returns gives each its own local reconstruction, while
// folding NotSignedIn into the "not-Success" condition (so both paths funnel
// through the single trailing `return gamertag_error;`) reproduces retail's
// shared destination there too: 87.0% -> 92.7% fuzzy, diff_op: none.
//
// Residual (92.7%, not chased further): retail's `OnlineID oid = ...` copy
// compiles to a raw 5-instruction ld/std struct copy with NO constructor
// call, which is what a compiler-IMPLICIT (trivial) OnlineID copy ctor would
// produce. Our shared os/OnlineID.h explicitly declares
// `OnlineID(const OnlineID &);` (out-of-line, in OnlineID.cpp), which forces
// a real `bl ??0OnlineID@@QAA@ABV0@@Z` at every call site in the whole
// binary, not just here. Making OnlineID's copy ctor implicit/trivial would
// fix this row but is a shared-header change with unknown blast radius
// across every other OnlineID user -- out of scope for this lane (one named
// row); flagged for whoever next touches OnlineID.h.
Symbol PlayerLeaderboard::OnSelectRow(int row, BandUser *user) {
    if (IsEnumComplete() && NumData() > row) {
        static Symbol pad_error("display_gamercard_pad_error");
        static Symbol privilege_error("display_gamercard_privilege_error");
        static Symbol gamertag_error("on_select_gamertag_error");

        LocalBandUser *localBandUser = user->GetLocalBandUser();
        if (!localBandUser->IsSignedInOnline())
            return pad_error;

        OnlineID oid = mLeaderboardRows[row].mLBOnlineID;
        ShowGamercardResult result = ThePlatformMgr.ShowGamercard(localBandUser, &oid);
        if (result == kShowGamercardResult_PrivilegeFailed)
            return privilege_error;
        if (result != kShowGamercardResult_NotSignedIn && result >= kShowGamercardResult_Success)
            return gNullStr;
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
