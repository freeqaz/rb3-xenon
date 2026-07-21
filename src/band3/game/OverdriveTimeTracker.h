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

    float mPastDurationMs; // 0x58
    float mCurrentDurationMs; // 0x5c
    float mLongestDurationMs; // 0x60
    float mDeployStartMs; // 0x64
    int mLastUpdateSeconds; // 0x68
    bool mWasDeploying; // 0x6c
};