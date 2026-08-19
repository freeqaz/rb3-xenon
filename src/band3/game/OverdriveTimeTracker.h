#pragma once
#include "Tracker.h"
#include "game/TrackerDisplay.h"

class OverdriveTimeTracker : public Tracker {
public:
    OverdriveTimeTracker(TrackerSource *, TrackerBandDisplay &, TrackerBroadcastDisplay &);
    virtual ~OverdriveTimeTracker();
    virtual void UpdateGoalValueLabel(UILabel &) const;
    virtual void UpdateCurrentValueLabel(UILabel &) const;
    virtual String GetPlayerContributionString(Symbol) const;
    virtual void FirstFrame_(float);
    virtual void Poll_(float);
    virtual void TargetSuccess(int) const {}
    virtual DataArrayPtr GetTargetDescription(int idx) const {
        return TrackerDisplay::MakeTimeTargetDescription(mTargets[idx]);
    }
    virtual float GetCurrentValue() const { return mLongestDurationMs; }
    virtual void SavePlayerStats() const;

    void UpdateTimeRemainingDisplay();

    float mPastDurationMs; // 0x64
    float mCurrentDurationMs; // 0x68
    float mLongestDurationMs; // 0x6c
    float mDeployStartMs; // 0x70
    int mLastUpdateSeconds; // 0x74
    bool mWasDeploying; // 0x78
};