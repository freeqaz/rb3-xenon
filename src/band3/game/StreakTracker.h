#pragma once
#include "Tracker.h"
#include "game/TrackerDisplay.h"
#include "game/TrackerSource.h"
#include "game/TrackerUtils.h"

class StreakTracker : public Tracker {
public:
    class PlayerStreakData {
    public:
        int unk0;
        int unk4;
        int unk8;
        int unkc;
        int unk10;
        int unk14;
        int unk18;
        int unk1c;
    };

    StreakTracker(TrackerSource *, TrackerBandDisplay &, TrackerBroadcastDisplay &);
    virtual ~StreakTracker();
    virtual void TranslateRelativeTargets();
    virtual void UpdateGoalValueLabel(UILabel &) const;
    virtual void UpdateCurrentValueLabel(UILabel &) const;
    virtual String GetPlayerContributionString(Symbol) const;
    virtual void RemoteEndStreak_(Player *, float, int);
    virtual void ConfigureTrackerSpecificData(const DataArray *);
    virtual void HandlePlayerSaved_(const TrackerPlayerID &);
    virtual void FirstFrame_(float);
    virtual void Poll_(float);
    virtual void TargetSuccess(int) const;
    virtual DataArrayPtr GetBroadcastDescription() const;
    virtual DataArrayPtr GetTargetDescription(int) const;
    virtual TrackerChallengeType GetChallengeType() const {
        return (TrackerChallengeType)3;
    }
    virtual float GetCurrentValue() const { return unk64; }
    virtual void SavePlayerStats() const;

    void LocalEndStreak(const TrackerPlayerID &, float);

    int unk58; // 0x58 (unknown, set outside carved range)
    int unk5c; // 0x5c
    int unk60; // 0x60
    float unk64; // 0x64 streak total (GetCurrentValue)
    int unk68; // 0x68 streak count
    float unk6c; // 0x6c streak_length_multiplier
    std::map<TrackerPlayerID, PlayerStreakData> mStreakDataMap; // 0x70
    TrackerMultiplierMap unk88; // 0x88
};