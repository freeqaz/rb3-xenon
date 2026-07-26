#include "game/OverdriveTimeTracker.h"
#include "game/Game.h"
#include "game/TrackerDisplay.h"
#include "game/TrackerSource.h"
#include "math/Utl.h"
#include "os/Debug.h"
#include "ui/UILabel.h"
#include "utl/Locale.h"
#include "utl/Symbols.h"

OverdriveTimeTracker::OverdriveTimeTracker(
    TrackerSource *src, TrackerBandDisplay &banddisp, TrackerBroadcastDisplay &bcdisp
)
    : Tracker(src, banddisp, bcdisp), mPastDurationMs(0), mCurrentDurationMs(0), mLongestDurationMs(0), mDeployStartMs(-1.0f),
      mLastUpdateSeconds(1), mWasDeploying(0) {}

OverdriveTimeTracker::~OverdriveTimeTracker() {}

void OverdriveTimeTracker::FirstFrame_(float) {
    mPastDurationMs = 0;
    mCurrentDurationMs = 0;
    mLongestDurationMs = 0;
    mDeployStartMs = -1.0f;
    mLastUpdateSeconds = 1;
    mWasDeploying = false;
    mBandDisplay.Initialize(mDesc.mName);
    UpdateTimeRemainingDisplay();
}

void OverdriveTimeTracker::Poll_(float f) {
    MILO_ASSERT(TheGame, 0x3E);
    bool notIdle = TheGame->unkdc != -1;
    if (notIdle || mSource->IsFinished())
        return;
    else {
        bool o2 = false;
        for (TrackerPlayerID id = mSource->GetFirstPlayer(); id.NotNull();
             id = mSource->GetNextPlayer(id)) {
            Player *player = mSource->GetPlayer(id);
            MILO_ASSERT(player, 0x48);
            if (player->IsDeployingBandEnergy()) {
                o2 = true;
                break;
            }
        }
        if (o2) {
            float startMs = mDeployStartMs;
            if (-1.0f == startMs) {
                mDeployStartMs = f;
            } else {
                mCurrentDurationMs = f - startMs;
                // MaxEq, not a hand-written `if`: the template's `T &x` binding
                // loads mLongestDurationMs only after the mCurrentDurationMs
                // store is scheduled, matching the target's
                // `stfs 0x68` -> `lfs 0x6c` order. A plain `if` lets MSVC hoist
                // the non-aliasing 0x6c load above the store. Same shape as the
                // else-arm below, which already matches.
                MaxEq(mLongestDurationMs, mCurrentDurationMs);
            }
            UpdateTimeRemainingDisplay();
        } else if (mDeployStartMs != -1.0f) {
            mPastDurationMs = mPastDurationMs + mCurrentDurationMs;
            MaxEq(mLongestDurationMs, mCurrentDurationMs);
            mCurrentDurationMs = 0;
            mDeployStartMs = -1.0f;
        }
        mWasDeploying = o2;
    }
}

void OverdriveTimeTracker::UpdateGoalValueLabel(UILabel &label) const {
    int min, sec;
    TrackerDisplay::MsToMinutesSeconds(mTargets.front(), min, sec);
    static Symbol tour_goal_od_timer_goal_format("tour_goal_od_timer_goal_format");
    label.SetTokenFmt(tour_goal_od_timer_goal_format, min, sec);
}

void OverdriveTimeTracker::UpdateCurrentValueLabel(UILabel &label) const {
    int min, sec;
    TrackerDisplay::MsToMinutesSeconds(mLongestDurationMs, min, sec);
    static Symbol tour_goal_od_timer_result_format("tour_goal_od_timer_result_format");
    label.SetTokenFmt(tour_goal_od_timer_result_format, min, sec);
}

String OverdriveTimeTracker::GetPlayerContributionString(Symbol s) const {
    TrackerPlayerID pid = mSource->GetIDFromInstrument(s);
    float f1 = 0;
    if (pid.NotNull()) {
        Player *pPlayer = mSource->GetPlayer(pid);
        MILO_ASSERT(pPlayer, 0x8E);
        Stats &stats = pPlayer->mStats;
        f1 = stats.unk1c0;
    }
    int min, sec;
    TrackerDisplay::MsToMinutesSeconds(f1, min, sec);
    static Symbol tour_goal_od_timer_result_format("tour_goal_od_timer_result_format");
    return MakeString(Localize(tour_goal_od_timer_result_format, 0), min, sec);
}

void OverdriveTimeTracker::SavePlayerStats() const {
    for (TrackerPlayerID id = mSource->GetFirstPlayer(); id.NotNull();
         id = mSource->GetNextPlayer(id)) {
        Player *pPlayer = mSource->GetPlayer(id);
        MILO_ASSERT(pPlayer, 0xA6);
        pPlayer->mStats.unk1c0 = pPlayer->mStats.mTotalOverdriveDurationMs;
    }
}

void OverdriveTimeTracker::UpdateTimeRemainingDisplay() {
    float f60 = mLongestDurationMs;
    int floored = std::floor(f60 / 1000.0f);
    if (floored != mLastUpdateSeconds) {
        mLastUpdateSeconds = floored;
        mBandDisplay.SetTimeProgress(f60);
    }
}
