#include "meta_band/CampaignLeaderboards.h"
#include "meta_band/Leaderboard.h"
#include "meta_band/PlayerLeaderboards.h"
#include "net_band/RockCentral.h"

// Retail TU5 0x8266B7D0-0x8266BDC8, in retail emission order. Each request
// clears the result list, gathers the profile's player ids, and asks
// RockCentral with this as the completion target. The career board reads
// the uncapped (kCareer) table when DLC scores count, else kCappedCareer.

PlayerCampaignCareerLeaderboard::PlayerCampaignCareerLeaderboard(
    Profile *p, Callback *cb, ScoreType s, bool dlc
)
    : PlayerLeaderboard(p, cb), mScoreType(s), mUseDLC(dlc) {}

void PlayerCampaignCareerLeaderboard::SetScoreType(ScoreType ty) {
    if (ty != mScoreType) {
        mScoreType = ty;
        CancelEnumerate();
        StartEnumerate();
    }
}

PlayerCampaignGoalLeaderboard::PlayerCampaignGoalLeaderboard(
    Profile *p, Callback *cb, Symbol goal
)
    : PlayerLeaderboard(p, cb), mGoal(goal) {}

void PlayerCampaignGoalLeaderboard::EnumerateFromID() {
    mDataResultList.Clear();
    std::vector<int> ids;
    GetPlayerIds(ids);
    TheRockCentral.GetAccLeaderboardByPlayer(
        ids, mGoal, ModeToLeaderboardMode(mMode), sPageSize, mDataResultList, this
    );
}

void PlayerCampaignGoalLeaderboard::EnumerateRankRange(int start, int count) {
    mDataResultList.Clear();
    std::vector<int> ids;
    GetPlayerIds(ids);
    TheRockCentral.GetAccLeaderboardByRankRange(
        ids, mGoal, start, start + count, mDataResultList, this
    );
}

void PlayerCampaignGoalLeaderboard::GetStats() {
    mDataResultList.Clear();
    std::vector<int> ids;
    GetPlayerIds(ids);
    TheRockCentral.GetAccMaxRank(ids, mGoal, mDataResultList, this);
}

void PlayerCampaignCareerLeaderboard::EnumerateFromID() {
    mDataResultList.Clear();
    LeaderboardType ltype = mUseDLC ? kCareer : kCappedCareer;
    std::vector<int> ids;
    GetPlayerIds(ids);
    TheRockCentral.GetLeaderboardByPlayer(
        ids,
        0,
        mScoreType,
        ltype,
        ModeToLeaderboardMode(mMode),
        sPageSize,
        mDataResultList,
        this
    );
}

void PlayerCampaignCareerLeaderboard::EnumerateRankRange(int start, int count) {
    mDataResultList.Clear();
    LeaderboardType ltype = mUseDLC ? kCareer : kCappedCareer;
    std::vector<int> ids;
    GetPlayerIds(ids);
    TheRockCentral.GetLeaderboardByRankRange(
        ids, 0, mScoreType, start, start + count, ltype, mDataResultList, this
    );
}

void PlayerCampaignCareerLeaderboard::GetStats() {
    mDataResultList.Clear();
    LeaderboardType ltype = mUseDLC ? kCareer : kCappedCareer;
    std::vector<int> ids;
    GetPlayerIds(ids);
    TheRockCentral.GetMaxRank(ids, 0, mScoreType, ltype, mDataResultList, this);
}
