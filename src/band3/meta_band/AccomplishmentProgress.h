#pragma once
#include "obj/Object.h"
#include "meta/FixedSizeSaveable.h"
#include "tour/TourGameRules.h"
#include "game/Defines.h"
#include "net_band/RockCentralMsgs.h"
#include "xdk/xapilibi/xbase.h"
#include "utl/Symbol.h"
#include <hash_map>

// The progress maps below were originally Harmonix `hash_map` (the save-format
// "hash_map" MILO_NOTIFY strings in FixedSizeSaveable prove it). The Wii decomp
// approximated them as std::map; retail X360 inlines STLport hashtable::find
// (out-of-line find returning iterator-by-value, NULL miss sentinel, value at
// slist node+0x8) — see ?GetToursPlayed etc. hash_map<Symbol,int> hashes the
// interned char* word identity (retail: lwz key; divwu), matching exactly.
#ifndef RB3_HASH_SYMBOL_DEFINED
#define RB3_HASH_SYMBOL_DEFINED
namespace stlpmtx_std {
_STLP_TEMPLATE_NULL struct hash<Symbol> {
    size_t operator()(const Symbol &s) const { return (size_t)s.Str(); }
};
}
#endif

class Accomplishment;
class Band;
class BandProfile;
class Performer;
struct Stats;

// 360-only enum for the gamer-award async write path. The Wii build had no
// equivalent (no XOVERLAPPED tail), so this type is reconstructed from the
// retail mangled name ??0GamerAwardStatus@@QAA@HW4GamerAwardType@@@Z.
enum GamerAwardType {
    kGamerAwardTypeNone = 0,
    // ⚠ (lane CG-4) 1 and 2 ARE constructed -- GiveGamerpic passes
    // (GamerAwardType)1 and GiveAvatarAsset passes (GamerAwardType)2
    // (AccomplishmentProgress.cpp:294,:308). With only enumerator 0 the enum's
    // [dcl.enum]/7 range is [0,0], so both casts are out of range and any test
    // against them is foldable. Named from their unambiguous call sites rather
    // than range-guarded, since the meaning here is not in doubt.
    // X360-neutral: enumerators emit no code (whole-binary A/B measured Δ0).
    kGamerAwardTypeGamerpic = 1,
    kGamerAwardTypeAvatarAsset = 2,
};

class GamerAwardStatus : public FixedSizeSaveable {
public:
    GamerAwardStatus();
    GamerAwardStatus(int, GamerAwardType);
    virtual ~GamerAwardStatus();
    virtual void SaveFixed(FixedSizeSaveableStream &) const;
    virtual void LoadFixed(FixedSizeSaveableStream &, int);

    static int SaveSize(int);

    int unk8; // 0x8
    int unkc; // 0xc
    bool unk10; // 0x10
    // 0x14/0x18: XUSER_AVATARASSET {dwUserIndex,dwAwardId} - never
    // ctor-initialized; filled by GiveAvatarAsset before XUserAwardAvatarAssets.
    XUSER_AVATARASSET mAsset; // 0x14
    XOVERLAPPED mOverlapped; // 0x1c - 360 async award write; memset-zeroed in ctors
};

class AccomplishmentProgress : public Hmx::Object, public FixedSizeSaveable {
public:
    AccomplishmentProgress(BandProfile *);
    virtual ~AccomplishmentProgress();
    virtual DataNode Handle(DataArray *, bool);
    virtual void SaveFixed(FixedSizeSaveableStream &) const;
    virtual void LoadFixed(FixedSizeSaveableStream &, int);

    void Clear();
    void UpdateStats(ScoreType, Difficulty, int, const Stats &, Performer *, Band *);
    void UpdateScoreTypeSpecificStats(
        ScoreType, Difficulty, const Stats &, Performer *, Band *
    );
    int GetToursPlayed(Symbol) const;
    void UpdateTourPlayedForAllParticipants(Symbol);
    int GetToursGotAllStars(Symbol) const;
    void SetToursGotAllStars(Symbol, int);
    int GetQuestCompletedCount(TourGameType) const;
    void SetQuestCompletedCount(TourGameType, int);
    bool AddAccomplishment(Symbol);
    bool IsAccomplished(Symbol) const;
    void NotifyPlayerOfAccomplishment(Symbol, const char *);
    void NotifyPlayerOfCampaignLevel(Symbol);
    void NotifyPlayerOfCategoryComplete(Symbol);
    void NotifyPlayerOfGroupComplete(Symbol);
    bool AddAward(Symbol, Symbol);
    void Poll();
    void UpdateTourPlayed(Symbol);
    void UpdateMostStars(Symbol, int);
    int GetCurrentValue(Symbol);
    void ClearStepTrackingMap();
    void SetCurrentValue(Symbol, int);
    int GetNumCompleted() const;
    bool HasNewAwards() const;
    Symbol GetFirstNewAwardReason() const;
    Symbol GetFirstNewAward() const;
    void ClearFirstNewAward();
    bool HasNewRewardVignettes() const;
    Symbol GetFirstNewRewardVignette() const;
    void ClearFirstNewRewardVignette();
    bool HasNewRewardVignetteFestival() const;
    void ClearNewRewardVignetteFestival();
    int GetNumCompletedInCategory(Symbol) const;
    int GetNumCompletedInGroup(Symbol) const;
    void SetMetaScore(int);
    void AddNewRewardVignette(Symbol);
    bool IsUploadDirty() const;
    bool HasAward(Symbol) const;
    void SetHardCoreStatusUpdatePending(bool);
    bool IsHardCoreStatusUpdatePending();
    void SendHardCoreStatusUpdateToRockCentral();
    void HandlePendingGamerRewards();
    int GetMetaScore() const;
    int GetTotalGemsSmashed() const;
    int GetTotalGuitarHopos() const;
    int GetTotalBassHopos() const;
    int GetTotalUpstrums() const;
    int GetTotalTimesRevived() const;
    int GetTotalSaves() const;
    int GetTotalAwesomes() const;
    int GetTotalDoubleAwesomes() const;
    int GetTotalTripleAwesomes() const;
    int GetCareerFills() const;
    int GetBestStars(ScoreType, Difficulty) const;
    int GetBestStarsAtMinDifficulty(ScoreType, Difficulty) const;
    int GetBestSolo(ScoreType, Difficulty) const;
    int GetBestSoloAtMinDifficulty(ScoreType, Difficulty) const;
    int GetBestAccuracy(ScoreType, Difficulty) const;
    int GetBestAccuracyAtMinDifficulty(ScoreType, Difficulty) const;
    int GetBestHoposPercent(ScoreType, Difficulty) const;
    int GetBestHoposPercentAtMinDifficulty(ScoreType, Difficulty) const;
    int GetBestStreak(ScoreType) const;
    int GetBestScore(ScoreType) const;
    int GetBestBandScore() const;
    int GetTotalOverdriveDeploys(ScoreType) const;
    int GetTotalOverdriveTime(ScoreType) const;
    int GetTotalOverdrivePhrases(ScoreType) const;
    int GetTotalUnisonPhrases(ScoreType) const;
    int GetMostOverdriveDeploys(ScoreType) const;
    int GetMostOverdriveTime(ScoreType) const;
    int GetMostUnisonPhrases(ScoreType) const;
    int GetTotalBREsHit(ScoreType) const;
    int GetBestPercussionPercent(Difficulty) const;
    int GetBestPercussionPercentAtMinDifficulty(Difficulty) const;
    int GetTotalDrumRollCount(Difficulty) const;
    int GetTotalDrumRollCountAtMinDifficulty(Difficulty) const;
    int GetTotalProDrumRollCount(Difficulty) const;
    int GetTotalProDrumRollCountAtMinDifficulty(Difficulty) const;
    int GetBestKickPercent(Difficulty) const;
    int GetBestKickPercentAtMinDifficulty(Difficulty) const;
    int GetBestProKickPercent(Difficulty) const;
    int GetBestProKickPercentAtMinDifficulty(Difficulty) const;
    int GetBestDrumRollPercent(Difficulty) const;
    int GetBestDrumRollPercentAtMinDifficulty(Difficulty) const;
    int GetBestSoloButtonPercent(Difficulty) const;
    int GetBestSoloButtonPercentAtMinDifficulty(Difficulty) const;
    int GetTotalSongsPlayed() const;
    int GetTourTotalSongsPlayed() const;
    int GetToursPlayed() const;
    int GetTourMostStars(Symbol) const;
    int GetToursGotAllStars() const;
    int GetQuestCompletedCount() const;
    void SetTotalSongsPlayed(int);
    void SetTourTotalSongsPlayed(int);
    void SetToursPlayed(Symbol, int);
    void SetMostStars(Symbol, int);
    bool InqGoalLeaderboardData(std::hash_map<Symbol, int> &) const;
    void HandleUploadStarted();
    void HandleSuccessfulUpload();
    void FakeFill();

private:
    void GiveGamerpic(Accomplishment *);
    void GiveAvatarAsset(Accomplishment *);

public:
    const std::hash_map<Symbol, int> &GetToursMostStarsMap() const {
        return mTourMostStarsMap;
    }
    const std::hash_map<Symbol, int> &GetToursPlayedMap() const {
        return mToursPlayedMap;
    }
    const std::hash_map<Symbol, int> &GetToursGotAllStarsMap() const {
        return mToursGotAllStarsMap;
    }
    const std::hash_map<int, int> &GetGigTypeCompletedMap() const {
        return mGigTypeCompletedMap;
    }
    const std::set<Symbol> &GetNewGoalsSet() const { return mNewlyAcquiredAccomplishments; }
    // // int GetTotalGemsSmashed() const;
    // // int GetTotalGuitarHopos() const;
    // // int GetTotalBassHopos() const;
    // // int GetTotalUpstrums() const;
    // // int GetTotalTimesRevived() const;
    // // int GetTotalSaves() const;
    // // int GetTotalAwesomes() const;
    // // int GetTotalDoubleAwesomes() const;
    // // int GetTotalTripleAwesomes() const;
    // // int GetBestStarsAtMinDifficulty(ScoreType, Difficulty) const;
    // // int GetBestSoloAtMinDifficulty(ScoreType, Difficulty) const;
    // // int GetBestAccuracyAtMinDifficulty(ScoreType, Difficulty) const;
    // // int GetBestStreak(ScoreType) const;
    // // int GetTotalOverdriveDeploys(ScoreType) const;
    // // int GetTotalOverdriveTime(ScoreType) const;
    // // int GetTotalOverdrivePhrases(ScoreType) const;
    // // int GetTotalUnisonPhrases(ScoreType) const;
    // // int GetMostOverdriveDeploys(ScoreType) const;
    // // int GetMostOverdriveTime(ScoreType) const;
    // int GetMostUnisonPhrases(ScoreType) const;
    // int GetTotalBREsHit(ScoreType) const;
    // int GetTotalSongsPlayed() const;
    // int GetTourTotalSongsPlayed() const;
    // int GetBestPercussionPercentAtMinDifficulty(Difficulty) const;
    // int GetTotalDrumRollCountAtMinDifficulty(Difficulty) const;
    // int GetTotalProDrumRollCountAtMinDifficulty(Difficulty) const;
    // int GetBestKickPercentAtMinDifficulty(Difficulty) const;
    // int GetBestProKickPercentAtMinDifficulty(Difficulty) const;
    // int GetBestDrumRollPercentAtMinDifficulty(Difficulty) const;
    // int GetBestSoloButtonPercentAtMinDifficulty(Difficulty) const;
    // int GetBestHoposPercentAtMinDifficulty(ScoreType, Difficulty) const;
    // int GetCareerFills() const;
    // int GetBestScore(ScoreType) const;
    // int GetBestBandScore() const;

    DataNode OnMsg(const RockCentralOpCompleteMsg &);

    static int SaveSize(int);

    std::hash_map<Symbol, int> mStepTrackingMap; // 0x30 (hashtable, 0x1c)
    BandProfile *mParentProfile; // 0x4c
    bool mHardCoreStatusUpdatePending; // 0x50
    std::list<GamerAwardStatus *> mGamerAwardStatusList; // 0x54
    std::set<Symbol> mAccomplishments; // 0x5c
    std::set<Symbol> mNewlyAcquiredAccomplishments; // 0x74
    std::vector<Symbol> unk7c; // 0x8c
    int mMetaScore; // 0x98
    std::set<Symbol> mAwards; // 0x9c
    std::list<std::pair<Symbol, Symbol> > mNewAwards; // 0xb4
    std::list<Symbol> mNewRewardVignettes; // 0xbc
    std::set<Symbol> unkb0; // 0xc4
    int mTotalGemsSmashed; // 0xdc
    int mTotalBassHopos; // 0xe0
    int mTotalGuitarHopos; // 0xe4
    int mTotalUpstrums; // 0xe8
    int mTotalTimesRevived; // 0xec
    int mTotalSaves; // 0xf0
    int mTotalAwesomes; // 0xf4
    int mTotalDoubleAwesomes; // 0xf8
    int mTotalTripleAwesomes; // 0xfc
    int mCareerFills; // 0x100
    int mBestStars[kNumScoreTypes][kNumDifficulties]; // 0xf0
    int mBestSolo[kNumScoreTypes][kNumDifficulties]; // 0x1a0
    int mBestAccuracy[kNumScoreTypes][kNumDifficulties]; // 0x250
    int mBestHoposPercent[kNumScoreTypes][kNumDifficulties]; // 0x300
    int mBestScore[kNumScoreTypes]; // 0x3c4
    int mBestBandScore; // 0x3f0
    int mBestStreak[kNumScoreTypes]; // 0x3f4
    int mTotalOverdriveDeploys[kNumScoreTypes]; // 0x420
    int mTotalOverdriveTime[kNumScoreTypes]; // 0x44c
    int mTotalOverdrivePhrases[kNumScoreTypes]; // 0x478
    int mTotalUnisonPhrases[kNumScoreTypes]; // 0x4a4
    int mMostOverdriveDeploys[kNumScoreTypes]; // 0x4d0
    int mMostOverdriveTime[kNumScoreTypes]; // 0x4fc
    int mMostUnisonPhrases[kNumScoreTypes]; // 0x528
    int mTotalBREsHit[kNumScoreTypes]; // 0x554
    int mBestPercussionPercent[kNumDifficulties]; // 0x580
    int mBestKickPercent[kNumDifficulties]; // 0x590
    int mBestProKickPercent[kNumDifficulties]; // 0x5a0
    int mTotalDrumRollCount[kNumDifficulties]; // 0x5b0
    int mTotalProDrumRollCount[kNumDifficulties]; // 0x5c0
    int mBestSoloButtonPercent[kNumDifficulties]; // 0x5d0
    int mBestDrumRollPercent[kNumDifficulties]; // 0x5e0
    int mTotalSongsPlayed; // 0x5f0
    int mTourTotalSongsPlayed; // 0x5f4
    std::hash_map<Symbol, int> mToursPlayedMap; // 0x5f8 (hashtable, 0x1c)
    std::hash_map<Symbol, int> mTourMostStarsMap; // 0x614
    std::hash_map<Symbol, int> mToursGotAllStarsMap; // 0x630
    std::hash_map<int, int> mGigTypeCompletedMap; // 0x64c
    bool mUploadDirty; // 0x668
    bool unk645; // 0x669
    int unk648; // 0x66c
};