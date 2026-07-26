#pragma once
#include "os/Debug.h"
#include "system/obj/Data.h"
#include "utl/MemMgr.h"
#include <set>
#include "BandProfile.h"
#include "band3/game/BandUser.h"
// game/Tracker.h was removed: it pulls in TrackPanel.h -> TrackPanelDirBase.h ->
// GemTrackDir.h -> many missing bandobj engine headers (not yet ported).
// TrackerDesc is only needed as a reference parameter in InitializeTrackerDesc —
// forward declaration suffices.
class TrackerDesc;
#include "band3/meta_band/MusicLibrary.h"
#include "system/ui/UILabel.h"

enum AccomplishmentType {
    kAccomplishmentTypeUnique = 0,
    kAccomplishmentTypeSongListConditional = 1,
    kAccomplishmentTypeSongFilterConditional = 2,
    kAccomplishmentTypeLessonSongListConditional = 3,
    kAccomplishmentTypeLessonDiscSongConditional = 4,
    kAccomplishmentTypePlayerConditional = 5,
    kAccomplishmentTypeTourConditional = 6,
    kAccomplishmentTypeTrainerListConditional = 7,
    kAccomplishmentTypeTrainerCategoryConditional = 8,
    kAccomplishmentTypeOneShot = 9,
    kAccomplishmentTypeSetlist = 10,
    kAccomplishmentTypeDiscSongConditional = 11
};

class Accomplishment {
public:
    Accomplishment(DataArray *, int);
    virtual ~Accomplishment();
    virtual AccomplishmentType GetType() const;
    virtual bool ShowBestAfterEarn() const;
    virtual void UpdateIncrementalEntryName(UILabel *, Symbol) { MILO_ASSERT(false, 109); }
    virtual bool IsFulfilled(BandProfile *) const;
    virtual bool IsRelevantForSong(Symbol) const;
    virtual Difficulty GetRequiredDifficulty() const;
    virtual bool InqRequiredScoreTypes(std::set<ScoreType> &) const;
    virtual bool InqProgressValues(BandProfile *, int &, int &);
    virtual bool InqIncrementalSymbols(BandProfile *, std::vector<Symbol> &) const;
    virtual bool IsSymbolEntryFulfilled(BandProfile *, Symbol) const;
    virtual Symbol GetFirstUnfinishedAccomplishmentEntry(BandProfile *) const;
    virtual bool CanBeLaunched() const;
    virtual bool HasSpecificSongsToLaunch() const;
    virtual void
    InitializeMusicLibraryTask(MusicLibrary::MusicLibraryTask &, BandProfile *) const;
    virtual void InitializeTrackerDesc(TrackerDesc &) const;

    NEW_OVERLOAD;
    DELETE_OVERLOAD;

    void Configure(DataArray *);
    Symbol GetName() const;
    Symbol GetDescription() const;
    Symbol GetSecretDescription() const;
    Symbol GetFlavorText() const;
    bool GetShouldShowDenominator() const;
    bool HideProgress() const;
    Symbol GetSecretCampaignLevelPrereq() const;
    const std::vector<Symbol> &GetSecretPrereqs() const;
    bool IsDynamic() const;
    bool GetDynamicAlwaysVisible() const;
    const std::vector<Symbol> &GetDynamicPrereqsSongs() const;
    int GetDynamicPrereqsNumSongs() const;
    Symbol GetDynamicPrereqsFilter() const;
    Symbol GetCategory() const;
    int GetContextID() const;
    const char *GetIconArt() const;
    Symbol GetAward() const;
    bool HasAward() const;
    Symbol GetMetaScoreValue() const;
    bool IsUserOnValidScoreType(LocalBandUser *) const;
    bool IsUserOnValidController(LocalBandUser *) const;
    ScoreType GetRequiredScoreType() const;
    int GetRequiredMinPlayers() const;
    int GetRequiredMaxPlayers() const;
    bool GetRequiresUnisonAbility() const;
    bool GetRequiresBREAbility() const;
    bool CanBeEarnedWithNoFail() const;
    bool IsTrackedInLeaderboard() const;
    Symbol GetUnitsToken(int) const;
    Symbol GetPassiveMsgChannel() const;
    int GetPassiveMsgPriority() const;
    int GetGamerpicReward() const;
    int GetAvatarAssetReward() const;
    static const char *GetIconPath();

    // ------------------------------------------------------------------
    // Retail-360 member layout, reverse-engineered from the retail
    // Accomplishment::Accomplishment(DataArray*, int) at 0x82595D18 (which
    // initialises every member in declaration order) plus the +8 offset delta
    // observed on mDynamicPrereqsSongs in ~Accomplishment (0x82594D98).
    //
    // Two differences from the rb3-Wii DEV header:
    //   * retail INTERLEAVES the bools with the ints instead of grouping all
    //     eight at the tail (the init values still form the same F,F,F,T,T,F,T,F
    //     sequence in declaration order, which is what pins the assignment);
    //   * retail has two extra ints at 0x74/0x78 that the DEV header lacks --
    //     both ctor-initialised to -1 and written by Configure from the
    //     "gamerpic_reward" / "avatarasset_reward" DataArray keys (string
    //     constants 0x820A3DF8 / 0x820A3DE4).  These are the members
    //     AccomplishmentProgress::GiveGamerpic / ::GiveAvatarAsset read.
    // sizeof(Accomplishment) == 0x90, so AccomplishmentConditional::m_lConditions
    // still lands at 0x90 (previously faked with an mUnkTail[4] pad).
    // ------------------------------------------------------------------
    Symbol mName; // 0x04
    std::vector<Symbol> mSecretPrereqs; // 0x08
    int mAccomplishmentType; // 0x14
    Symbol mCategory; // 0x18
    Symbol mAward; // 0x1c
    Symbol mUnitsToken; // 0x20
    Symbol mUnitsTokenSingular; // 0x24
    Symbol mIconOverride; // 0x28
    Symbol mSecretCampaignLevelPrereq; // 0x2c
    std::vector<ControllerType> mControllerTypes; // 0x30
    ScoreType mScoreType; // 0x3c
    Difficulty mLaunchableDifficulty; // 0x40
    Symbol mPassiveMsgChannel; // 0x44
    int mPassiveMsgPriority; // 0x48
    bool mRequiresUnison; // 0x4c
    bool mRequiresBre; // 0x4d
    int mPlayerCountMin; // 0x50
    int mPlayerCountMax; // 0x54
    bool mDynamicAlwaysVisible; // 0x58
    int mDynamicPrereqsNumSongs; // 0x5c
    std::vector<Symbol> mDynamicPrereqsSongs; // 0x60
    Symbol mDynamicPrereqsFilter; // 0x6c
    int mProgressStep; // 0x70
    int mGamerpicReward; // 0x74
    int mAvatarAssetReward; // 0x78
    bool mShouldShowDenominator; // 0x7c
    bool mShowBestAfterEarn; // 0x7d
    bool mHideProgress; // 0x7e
    int mIndex; // 0x80
    int mContextId; // 0x84
    Symbol mMetaScoreValue; // 0x88
    bool mCanBeEarnedWithNoFail; // 0x8c
    bool mIsTrackedInLeaderboard; // 0x8d
};
