#pragma once
#include "BandProfile.h"
#include "game/BandUser.h"
class Performer;
class SongStatusMgr;
#include "meta_band/AccomplishmentCategory.h"
#include "meta_band/AccomplishmentGroup.h"
#include "meta_band/SongSortMgr.h"
#include "obj/Object.h"
#include "os/ContentMgr.h"
#include "system/obj/Data.h"
#include "utl/Symbol.h"
#include <map>
#include <hash_map>
#include "AccomplishmentCategory.h"
#include "Accomplishment.h"
#include "meta_band/Award.h"

// The accomplishment maps below were originally Harmonix `hash_map` keyed on
// Symbol. The Wii decomp approximated them as std::map; retail X360 inlines the
// STLport hashtable::find COMDAT (out-of-line find returning iterator-by-value,
// NULL-miss sentinel, value at slist node+0x8) — see the Has*/Get* accessors.
// hash<Symbol> hashes the interned char* word identity, matching retail exactly.
// Guarded so AccomplishmentProgress.h (which defines the same specialization)
// and this header can both be included in one TU without an ODR clash.
#ifndef RB3_HASH_SYMBOL_DEFINED
#define RB3_HASH_SYMBOL_DEFINED
namespace stlpmtx_std {
_STLP_TEMPLATE_NULL struct hash<Symbol> {
    size_t operator()(const Symbol &s) const { return (size_t)s.Str(); }
};
}
#endif

struct GoalAlpaCmp {
    GoalAlpaCmp();
    bool operator()(Symbol, Symbol) const;
};

struct SongDifficultyCmp {
    SongDifficultyCmp(Symbol);
    bool operator()(Symbol, Symbol) const;

    Symbol mInst; // 0x0
};

struct GoalAcquisitionInfo {
    GoalAcquisitionInfo() {}
    Symbol unk0;
    String unk4;
    Symbol unk10;
};

struct GoalProgressionInfo {
    GoalProgressionInfo() {}
    Symbol unk0;
    String unk4;
    Symbol unk10;
    int unk14;
};

class AccomplishmentManager : public Hmx::Object, public ContentMgr::Callback {
public:
    AccomplishmentManager();
    virtual ~AccomplishmentManager();
    virtual DataNode Handle(DataArray *, bool);
    virtual const char *ContentDir() { return nullptr; }
    virtual void ContentDone();

    void InitializeDiscSongs();
    void InitializePrecachedFilters();

    void InitializeTourSafeDiscSongs();
    void Init(DataArray *);
    void SanityCheckAwards();
    void Poll();
    Accomplishment *FactoryCreateAccomplishment(DataArray *, int);
    void ConfigureFanValueData(DataArray *);
    void ConfigureFanScalingData(DataArray *);
    void ConfigureAccomplishmentCategoryData(DataArray *);
    void ConfigureAccomplishmentGroupData(DataArray *);
    void ConfigureAwardData(DataArray *);
    void ConfigureAccomplishmentData(DataArray *);
    void ConfigureAccomplishmentCategoryGroupingData();
    void ConfigureAccomplishmentGroupToCategoriesData();
    void ConfigurePrecachedFilterData(DataArray *);
    void ConfigureAccomplishmentRewardData(DataArray *);
    std::list<Symbol> *GetCategoryListForGroup(Symbol) const;
    std::set<Symbol> *GetAccomplishmentSetForCategory(Symbol) const;
    int GetNumAccomplishmentsInCategory(Symbol) const;
    int GetNumAccomplishmentsInGroup(Symbol) const;
    bool HasFanValue(Symbol);
    int GetMetaScoreValue(Symbol);
    bool HasAccomplishmentCategory(Symbol) const;
    bool HasAccomplishmentGroup(Symbol) const;
    int GetPrecachedFilterCount(Symbol) const;
    void SetPrecachedFilterCount(Symbol, int);
    SongSortMgr::SongFilter *GetPrecachedFilter(Symbol) const;
    bool HasAward(Symbol) const;
    std::vector<Symbol> *GetAwardSourceList(Symbol) const;
    void AddAwardSource(Symbol, Symbol);
    void UpdateMostStarsForAllParticipants(Symbol, int);
    bool DoesAssetHaveSource(Symbol) const;
    void UpdateTourPlayedForAllParticipants(Symbol);
    Accomplishment *GetAccomplishment(Symbol) const;
    void AddGoalAcquisitionInfo(Symbol, const char *, Symbol);
    int GetLeaderboardHardcoreStatus(int) const;
    int GetIconHardCoreStatus(int) const;
    AccomplishmentCategory *GetAccomplishmentCategory(Symbol) const;
    bool IsCategoryComplete(BandProfile *, Symbol) const;
    bool IsGroupComplete(BandProfile *, Symbol) const;
    AccomplishmentGroup *GetAccomplishmentGroup(Symbol) const;
    Symbol GetTourSafeDiscSongAtDifficultyIndex(int index);
    Award *GetAward(Symbol) const;
    void AddAssetAward(Symbol, Symbol);
    void CheckForFinishedTrainerAccomplishmentsForUser(LocalBandUser *);
    void Cleanup();
    Symbol GetAwardSource(Symbol) const;
    bool HasAccomplishment(Symbol) const;
    bool IsAvailableToView(Symbol) const;
    bool InqAssetSourceList(Symbol, std::vector<Symbol> &) const;
    Symbol GetAssetAward(Symbol) const;
    String GetHintStringForSource(Symbol) const;
    void UpdateAssetHintLabel(Symbol, UILabel *);
    void EarnAccomplishment(LocalBandUser *, Symbol);
    void EarnAccomplishment(BandProfile *, Symbol);
    bool IsAvailableToEarn(Symbol) const;
    void EarnAccomplishmentForProfile(BandProfile *, Symbol);
    void UpdatePlayedTourForAllRemoteParticipants(Symbol);
    void UpdateMostStarsForAllRemoteParticipants(Symbol, int);
    void EarnAccomplishmentForAllParticipants(Symbol);
    void EarnAccomplishmentForAllRemoteParticipants(Symbol);
    void CheckForIncrementalProgressForUserGoal(Symbol, Symbol, LocalBandUser *);
    void AddGoalProgressionInfo(Symbol, const char *, Symbol, int);
    void CheckForFinishedTourAccomplishments();
    void CheckForFinishedTourAccomplishmentsForProfile(BandProfile *);
    void CheckForFinishedTourAccomplishmentsForUser(LocalBandUser *);
    void CheckForFinishedAccomplishmentsForUser(Symbol, LocalBandUser *);
    void HandlePreSongCompleted(Symbol);
    void HandlePreSongCompletedForUser(Symbol, LocalBandUser *);
    void HandleSetlistCompleted(Symbol, bool, Difficulty, int);
    void HandleSetlistCompletedForUser(Symbol, bool, LocalBandUser *, Difficulty, int);
    void HandleSongCompleted(Symbol, Difficulty);
    void HandleSongCompletedForUser(Symbol, LocalBandUser *, Difficulty);
    void InitializeSongIncrementalDataForUserGoal(Symbol, LocalBandUser *);
    void UpdateSongStatusFlagsForUser(Symbol, LocalBandUser *, Difficulty);
    void UpdateSongStatusFlagsForPerformer(Performer *, SongStatusMgr *, Symbol, ScoreType, Difficulty);
    void UpdateMiscellaneousSongDataForUser(Symbol, LocalBandUser *);
    void CheckForOneShotAccomplishments(Symbol, LocalBandUser *, Difficulty);
    int GetNumAccomplishments() const;
    bool HasCompletedAccomplishment(LocalBandUser *, Symbol) const;
    int GetNumCompletedAccomplishments(LocalBandUser *) const;
    bool HasNewAwards() const;
    LocalBandUser *GetUserForFirstNewAward();
    Symbol GetReasonForFirstNewAward(LocalBandUser *) const;
    Symbol GetNameForFirstNewAward(LocalBandUser *) const;
    Symbol GetAwardDescription(Symbol) const;
    Symbol GetAwardNameDisplay(Symbol) const;
    void UpdateReasonLabelForAward(Symbol, UILabel *);
    bool CanEquipAward(LocalBandUser *, Symbol) const;
    void EquipAward(LocalBandUser *, Symbol);
    bool HasAwardIcon(Symbol) const;
    String GetAwardIcon(Symbol) const;
    void ClearFirstNewAward(LocalBandUser *);
    Symbol GetNameForFirstNewRewardVignette() const;
    void ClearFirstNewRewardVignette();
    bool HasNewRewardVignetteFestival() const;
    void ClearNewRewardVignetteFestival();
    Symbol GetFirstUnfinishedAccomplishmentEntry(BandProfile *, Symbol);
    bool IsAvailable(Symbol, bool) const;
    void HandleRemoteAccomplishmentEarned(Symbol, const char *, Symbol);
    int GetNumOtherGoalsAcquired(const char *, Symbol);
    bool InqGoalsAcquiredForSong(BandUser *, Symbol, std::vector<Symbol> &);
    bool DidUserMakeProgressOnGoal(LocalBandUser *, Symbol);
    void CheatReloadData(DataArray *);
    bool HasNewRewardVignettes() const;
    void ClearGoalProgressionAcquisitionInfo();
    int GetScaledFanValue(int);
    const std::hash_map<Symbol, Accomplishment *> &GetAccomplishments() const {
        return mAccomplishments;
    }
    const std::hash_map<Symbol, AccomplishmentCategory *> &GetCategories() const {
        return mAccomplishmentCategory;
    }
    const std::hash_map<Symbol, AccomplishmentGroup *> &GetGroups() const {
        return mAccomplishmentGroups;
    }
    const std::vector<Symbol> &GetDiscSongs() const { return mDiscSongs; }

    DataNode OnEarnAccomplishment(const DataArray *);

    // Hmx::Object (0x28) + ContentMgr::Callback vptr (0x28) -> own data at 0x2c.
    // hash_map sizeof is 0x1c (STLport _ht: _M_max_load_factor at +0x18).
    std::hash_map<Symbol, Accomplishment *> mAccomplishments; // 0x2c
    std::hash_map<Symbol, AccomplishmentCategory *> mAccomplishmentCategory; // 0x48
    std::hash_map<Symbol, AccomplishmentGroup *> mAccomplishmentGroups; // 0x64
    std::hash_map<Symbol, Award *> mAwards; // 0x80
    std::hash_map<Symbol, Symbol> mAssetToAward; // 0x9c
    std::hash_map<Symbol, Symbol> mAwardToSource; // 0xb8
    std::hash_map<Symbol, std::vector<Symbol> *> unkb0; // 0xd4
    std::hash_map<Symbol, int> mFanValues; // 0xf0
    std::vector<std::pair<int, int> > m_vFanScalingData; // 0x10c
    std::hash_map<Symbol, std::list<Symbol> *> m_mapGroupToCategories; // 0x118
    std::hash_map<Symbol, std::set<Symbol> *> m_mapCategoryToAccomplishmentSet; // 0x134
    int mAccomplishmentRewardLeaderboardThresholds[4]; // 0x150
    int mAccomplishmentRewardIconThresholds[4]; // 0x160
    std::vector<GoalAcquisitionInfo> mGoalAcquisitionInfos; // 0x170
    std::vector<GoalProgressionInfo> mGoalProgressionInfos; // 0x17c
    std::vector<Symbol> mDiscSongs; // 0x188
    std::vector<Symbol> mTourSafeDiscSongs; // 0x194
    std::hash_map<Symbol, SongSortMgr::SongFilter *> mPrecachedFilters; // 0x1a0
    std::hash_map<Symbol, int> mPrecachedFilterCounts; // 0x1bc
};

extern AccomplishmentManager *TheAccomplishmentMgr;
