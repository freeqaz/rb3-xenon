#pragma once
#include "BandProfile.h"
#include "CampaignLevel.h"
#include "bandobj/MeterDisplay.h"
#include "beatmatch/TrackType.h"
#include "game/Defines.h"
#include "meta_band/AccomplishmentManager.h"
#include "meta_band/CampaignKey.h"
#include "meta_band/ProfileMessages.h"
#include "obj/Data.h"
#include <hash_map>
#include "os/User.h"
#include "game/BandUser.h"
#include "ui/UIPicture.h"

// Retail X360 stores the campaign level/key tables in STLport hash_maps, not
// std::maps: Campaign::GetCampaignLevel calls
// hashtable<pair<const Symbol,...>, Symbol, hash<Symbol>, ...>::_M_find with an
// sret _Slist_iterator and then tests the returned node pointer for NULL.
#ifndef RB3_HASH_SYMBOL_DEFINED
#define RB3_HASH_SYMBOL_DEFINED
namespace stlpmtx_std {
_STLP_TEMPLATE_NULL struct hash<Symbol> {
    size_t operator()(const Symbol &s) const { return (size_t)s.Str(); }
};
}
#endif

class BandProfile;

class Campaign : public Hmx::Object {
public:
    Campaign(DataArray *);
    virtual ~Campaign();
    virtual DataNode Handle(DataArray *, bool);

    LocalBandUser *GetUser() const;
    LocalBandUser *GetLaunchUser() const;
    bool HasCampaignKey(Symbol) const;
    bool HasCampaignLevel(Symbol) const;
    CampaignKey *GetCampaignKey(Symbol) const;
    CampaignLevel *GetCampaignLevel(Symbol) const;
    Symbol GetCurrentGoal() const;
    Symbol GetCampaignLevelForMetaScore(int) const;
    int GetCampaignMetaScoreForProfile(BandProfile *) const;
    void SetupLaunchedAccomplishmentInfo(Symbol);
    BandProfile *GetProfile() const;
    bool HasReachedCampaignLevel(LocalBandUser *, Symbol) const;
    void SetWasLaunchedIntoMusicLibrary(bool);
    TrackType GetRequiredTrackTypeForGoal(Symbol) const;
    TrackType GetRequiredTrackTypeForCurrentAccomplishment() const;
    ScoreType GetRequiredScoreTypeForCurrentAccomplishment() const;
    void Init(DataArray *);
    void Cleanup();
    void ConfigureCampaignLevelData(DataArray *);
    void ConfigureCampaignKeyData(DataArray *);
    String GetCampaignLevelIconForUser(LocalBandUser *);
    Symbol GetCampaignLevelForUser(LocalBandUser *) const;
    bool HasScoreReachedCampaignLevel(int, Symbol) const;
    int GetCampaignMetaScoreForUser(LocalBandUser *) const;
    bool HasReachedCampaignLevel(Symbol) const;
    int GetCampaignFanCountForUser(LocalBandUser *) const;
    int GetPrimaryCampaignFanCount() const;
    Symbol GetPrimaryCampaignLevel() const;
    bool IsUserOnLastCampaignLevel(LocalBandUser *);
    bool IsPrimaryUserOnLastCampaignLevel();
    bool IsLastCampaignLevel(Symbol) const;
    Symbol GetNextCampaignLevel(Symbol) const;
    String GetCurrentMajorLevelIcon(LocalBandUser *);
    Symbol GetMajorLevelForMetaScore(int);
    Symbol GetNextMajorLevelForMetaScore(int);
    String GetIconArt() const;
    String GetNextMajorLevelIcon(LocalBandUser *);
    String GetPrimaryCurrentMajorLevelIcon();
    String GetPrimaryNextMajorLevelIcon();
    void UpdatePrimaryCurrentMajorLevelIcon(UIPicture *);
    void UpdatePrimaryNextMajorLevelIcon(UIPicture *);
    void UpdateCurrentMajorLevelIcon(LocalBandUser *, UIPicture *);
    void UpdateNextMajorLevelIcon(LocalBandUser *, UIPicture *);
    int GetTotalPointsForNextCampaignLevelForUser(LocalBandUser *);
    int GetCurrentPointsForNextCampaignLevelForUser(LocalBandUser *);
    int GetTotalPointsForNextMajorCampaignLevelForMetaScore(int);
    int GetTotalPointsForNextMajorCampaignLevelForPrimary();
    int GetTotalPointsForNextMajorCampaignLevelForUser(LocalBandUser *);
    int GetCurrentPointsForNextMajorCampaignLevelForMetaScore(int);
    int GetCurrentPointsForNextMajorCampaignLevelForPrimary();
    int GetCurrentPointsForNextMajorCampaignLevelForUser(LocalBandUser *);
    void ClearCurrentGoal();
    bool HasCurrentGoal() const;
    Symbol GetCurrentGoalDescription() const;
    String GetCurrentGoalIcon() const;
    Difficulty GetMinimumDifficultyForCurrentAccomplishment() const;
    ScoreType GetRequiredScoreTypeForGoal(Symbol) const;
    bool HasValidUser() const;
    LocalBandUser *HasUser() const;
    void UpdateProgressMeter(MeterDisplay *, LocalBandUser *);
    void UpdatePrimaryProgressMeter(MeterDisplay *);
    bool CanSkipSongs();
    bool CanResumeSongs();
    bool CanSaveSetlists();
    Symbol GetNextHintToShow() const;
    bool HasHintsToShow() const;
    Symbol GetCampaignLevelAdvertisement(Symbol) const;
    bool GetWasLaunchedIntoMusicLibrary() const;
    bool DidUserMakeProgressOnGoal(LocalBandUser *, const Symbol &);
    bool HasDisplayGoal();
    Symbol GetCategoryGroup(Symbol);
    Symbol GetGoalCategory(Symbol);
    Symbol GetDisplayGoal();
    bool ShouldReturnToCategoryScreen();
    void HandleLaunchedGoalComplete();
    RndTex *GetPrimaryBandLogoTex();
    void CheatNextMetaLevel();
    const char *GetCheatMetaLevel();
    void CheatReloadCampaignData();
    void UpdateEndGameInfoForCurrentCampaignGoal(UILabel *, UILabel *, UIPicture *);
    void UpdateEndGameInfo(UILabel *, UILabel *, UIPicture *);

    DataNode OnMsg(const ProfileSwappedMsg &);
    DataNode OnMsg(const PrimaryProfileChangedMsg &);
    const std::hash_map<Symbol, CampaignKey *> &CampaignKeys() const {
        return m_mapCampaignKeys;
    }

    AccomplishmentManager *m_pAccomplishmentMgr; // 0x28
    Symbol m_symCurrentAccomplishment; // 0x2c
    bool m_bWasLaunchedIntoMusicLibrary; // 0x30
    bool unk25; // 0x31
    Symbol unk28; // 0x34
    std::vector<Symbol> m_vCampaignLevels; // 0x38
    std::hash_map<Symbol, CampaignLevel *> m_mapCampaignLevels; // 0x44
    std::hash_map<Symbol, Symbol> unk4c; // 0x60 (verified via class_layout_report; was mis-typed as std::map)
    std::vector<Symbol> unk64; // 0x7c
    std::hash_map<Symbol, CampaignKey *> m_mapCampaignKeys; // 0x88
    BandProfile *unk84; // 0xa4
    int unk88; // 0xa8
};

extern Campaign *TheCampaign;
