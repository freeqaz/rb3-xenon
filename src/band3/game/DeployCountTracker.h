#pragma once
#include "Tracker.h"
#include "game/TrackerDisplay.h"
#include "game/TrackerSource.h"

class DeployCountTracker : public Tracker {
public:
    class PlayerDeployData {
    public:
        bool unk0;
        bool unk1;
        bool unk2;
        bool unk3;
        int unk4;
    };

    DeployCountTracker(TrackerSource *, TrackerBandDisplay &, TrackerBroadcastDisplay &);
    virtual ~DeployCountTracker();
    virtual void TranslateRelativeTargets();
    virtual void UpdateGoalValueLabel(UILabel &) const;
    virtual void UpdateCurrentValueLabel(UILabel &) const;
    virtual String GetPlayerContributionString(Symbol) const;
    virtual void ConfigureTrackerSpecificData(const DataArray *);
    virtual void FirstFrame_(float);
    virtual void Poll_(float);
    virtual DataArrayPtr GetTargetDescription(int) const;
    virtual TrackerChallengeType GetChallengeType() const {
        return (TrackerChallengeType)2;
    }
    virtual float GetCurrentValue() const { return mDeployCount; }
    virtual void SavePlayerStats() const;

    void RemoteDeploy(Player *);
    void LocalDeploy(const TrackerPlayerID &);

    // TU5/Xbox layout (verified in Ghidra: ctor inits rb_tree at 0x64,
    // mDeployCount at 0x7c, flags at 0x80/0x81). The in-tree base Tracker
    // already ends at 0x64, so no padding is needed vs the rb3-Wii oracle's
    // 0x58 comment.
    std::map<TrackerPlayerID, PlayerDeployData> mDeployDataMap; // 0x64
    int mDeployCount; // 0x7c
    bool mRequireFullEnergy; // 0x80
    bool mRequireMaxMultiplier; // 0x81
};