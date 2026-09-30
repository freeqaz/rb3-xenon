#pragma once
#include "meta_band/AccomplishmentManager.h"
#include "meta_band/CampaignLeaderboards.h"
#include "obj/ObjMacros.h"
#include "ui/UIPanel.h"

class CampaignGoalsLeaderboardPanel : public Leaderboard::Callback, public UIPanel {
public:
    CampaignGoalsLeaderboardPanel();
    // W16-HR: implicit dtor -- retail's ??1 resets no derived vptrs (fixable-declarations.md)
    virtual void EnumerationStarted();
    virtual void ResultSuccess(bool, bool, bool);
    virtual void ResultFailure();
    OBJ_CLASSNAME(CampaignGoalsLeaderboardPanel);
    OBJ_SET_TYPE(CampaignGoalsLeaderboardPanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual void Poll();
    virtual void Enter();
    virtual void Load();
    virtual void Unload();

    void UpdateProvider();
    void SetGoal(Symbol);
    Symbol GetGoalDescription() const;
    Symbol GetGoalUnits() const;
    const char *GetGoalIcon() const;
    void CycleMode();
    Symbol GetModeSymbol() const {
        return mCampaignGoalsLeaderboardProvider->GetModeSymbol();
    }
    Symbol GetGoal() const { return mGoal; }
    bool HasGoalIcon() const { return TheAccomplishmentMgr->HasAccomplishment(mGoal); }

    NEW_OBJ(CampaignGoalsLeaderboardPanel);
    static void Init() { REGISTER_OBJ_FACTORY(CampaignGoalsLeaderboardPanel); }

    PlayerCampaignGoalLeaderboard *mCampaignGoalsLeaderboardProvider; // 0x40
    Symbol mGoal; // 0x44
};