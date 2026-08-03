#pragma once
#include "game/BandUser.h"
#include "game/GameMode.h"
#include "meta_band/SavedSetlist.h"
#include "net/Synchronize.h"
#include "obj/Msg.h"
#include "meta_band/BandSongMgr.h"
#include "game/Defines.h"
#include "game/Performer.h"
#include "meta_band/BandProfile.h"
#include "meta_band/Instarank.h"
#include "net_band/DataResults.h"
#include "ui/UILabel.h"

class PlayerScore;

class PerformerStatsInfo {
public:
    PerformerStatsInfo();
    virtual ~PerformerStatsInfo();

    void Clear();
    void Update(int, int, ScoreType, Difficulty, short, Performer *);

    short mInstrumentMask; // 0x4
    ScoreType mScoreType; // 0x8
    int unkc;
    int unk10;
    Difficulty mDifficulty; // 0x14
    int mScore; // 0x18
    int mStars; // 0x1c
    int mAccuracy; // 0x20
    int mStreak; // 0x24
    int mAwesomes; // 0x28
    int mDoubleAwesomes; // 0x2c
    int mTripleAwesomes; // 0x30
    int mSoloPercent; // 0x34
    int mHOPOPercent; // 0x38
    int mUnisonPhrasesHit; // 0x3c
};

class BandStatsInfo {
public:
    BandStatsInfo();
    virtual ~BandStatsInfo();

    void Clear();
    void UpdateBandStats(Difficulty, short, Performer *);
    const PerformerStatsInfo &GetBandStats() const;
    BandProfile *GetSoloProfile(int) const;
    const PerformerStatsInfo &GetSoloStats(int) const;
    void AddSoloStats(int, int, ScoreType, Difficulty, BandProfile *, Performer *);
    int NumSoloStats() const { return mSoloStats.size(); }

    PerformerStatsInfo mBandStats; // 0x4
    std::vector<std::pair<BandProfile *, PerformerStatsInfo> > mSoloStats; // 0x44
};

class MetaPerformerImpl : public Hmx::Object {
public:
    MetaPerformerImpl() {}
    virtual void CompleteSong(std::vector<BandUser *> &, const BandStatsInfo *, bool) {}
    virtual bool IsRandomSetList() const = 0;
    virtual bool IsWinning() const { return false; }
    virtual void RestartLastSong() {}
    virtual bool HasSyncPermission() const = 0;
    virtual void OnSynchronized(unsigned int) {}
    virtual void SyncSave(BinStream &, unsigned int) const {}
    virtual void SyncLoad(BinStream &, unsigned int) {}
};

class QuickplayPerformerImpl : public MetaPerformerImpl {
public:
    QuickplayPerformerImpl();
    virtual ~QuickplayPerformerImpl() {}
    virtual bool IsRandomSetList() const;
    virtual bool HasSyncPermission() const;
};

class MetaPerformer : public Synchronizable, public MsgSource {
public:
    class PendingDataInfo {
    public:
        PendingDataInfo() {}
        void Clear() {
            ir_result.Clear();
            friendMode = true;
            stats.Clear();
            song = "";
        }

        DataResultList ir_result; // 0x0
        bool friendMode; // 0x18
        BandStatsInfo stats; // 0x1c
        Symbol song; // 0x68
    };
    enum WiiPendingFlags {
    };

    MetaPerformer(const BandSongMgr &, const char *);
    virtual ~MetaPerformer();
    virtual void SyncSave(BinStream &, unsigned int) const;
    virtual void SyncLoad(BinStream &, unsigned int);
    virtual bool HasSyncPermission() const;
    virtual void OnSynchronized(unsigned int);
    virtual DataNode Handle(DataArray *, bool);

    MetaPerformerImpl *CurrentImpl() const;
    Symbol GetVenue() const;
    Symbol GetVenueClass() const;
    Symbol GetLastVenueClass() const;
    bool SongEndsWithEndgameSequence() const;
    bool IsWinning() const;
    bool IsLastSong() const;
    int NumSongs() const;
    int NumCompleted() const;
    Symbol Song() const;
    int SongsID() const;
    bool HasSong() const;
    const char *GetSetlistName() const;
    bool HasSetlist() const;
    void SetSongs(const std::vector<Symbol> &);
    void SetSongs(const std::vector<int> &);
    void SetSong(Symbol);
    void SetSongs(DataArray *);
    void ResetSongs();
    Symbol GetCompletedSong() const;
    const std::vector<Symbol> &GetSongs() const;
    bool IsFirstSong() const;
    bool IsSetComplete() const;
    bool PartPlaysInSet(Symbol) const;
    bool PartPlaysInSong(Symbol) const;
    bool VocalHarmonyInSong() const;
    int GetSetlistMaxVocalParts() const;
    bool SetlistHasVocalHarmony() const;
    bool SetHasMissingPart(Symbol) const;
    bool SetHasMissingVocalHarmony() const;
    bool SongAllowsVocalHarmony() const;
    ScoreType GetScoreTypeForUser(BandUser *);
    bool IsUsingRealDrums() const;
    bool IsNowUsingVocalHarmony() const;
    bool IsPlayingDemo() const;
    bool IsNoFailActive() const;
    bool IsBandNoFailSet() const;
    bool CanUpdateScoreLeaderboards();
    void SetSetlist(Symbol);
    void SelectRandomVenue();
    void SetVenue(Symbol);
    bool HasBattle() const;
    ScoreType GetBattleInstrument() const;
    void UnlockBandOrSolo();
    void SetCreditsPending();
    void SetBattle(const BattleSavedSetlist *);
    void SetSetlist(const SavedSetlist *);
    void SetSetlistImpl(const SavedSetlist *, bool);
    int GetBattleID() const;
    bool HasValidBattleInstarank() const;
    const char *GetBattleName();
    int GetBattleInstrumentMask();
    int GetBattleScore();
    void UpdateBattleTypeLabel(UILabel *);
    void LockBandOrSolo();
    int GetHighestDifficultyForPart(Symbol) const;
    void PopulatePlayerBandScores(const BandStatsInfo &, std::vector<PlayerScore> &);
    void
    PopulateSoloPlayerScore(const PerformerStatsInfo &, BandProfile *, PlayerScore &);
    void PopulatePlayerScores(const BandStatsInfo &, std::vector<PlayerScore> &);
    void UpdateScores(Symbol, const BandStatsInfo &, bool);
    void UpdateLastOfflineScores(Symbol, const BandStatsInfo &);
    void
    SaveAndUploadScores(std::vector<LocalBandUser *> &, Symbol, const BandStatsInfo &);
    void RecordBattleScore(const BandStatsInfo &, bool);
    ScoreType GetInstarankScoreTypeForSlot(int, const BandStatsInfo &);
    Instarank &GetInstarankForPlayerID(int);
    void UpdateInstarankData(DataResultList &, const BandStatsInfo &);
    void UpdateBattleInstarankData(DataResultList &);
    void ClearInstarankData();
    void ClearBattleInstarankData();
#ifndef RB3_NO_WII_META_MEMBERS
    Symbol GetVenueOverride(); // Wii/dev-only: absent from retail RB3-360
#endif
    void SetBandNoFail(bool);
    void ExportUpdateMetaPerformer();
    void LoadFestival();
    void ClearVenues();
    void ResetCompletion();
    void HostRestartLastSong();
    void Restart();
    void TriggerSongCompletion();
    void CompleteSong(std::vector<BandUser *> &, const BandStatsInfo *, bool);
    void SetCheating(bool);
    void
    PotentiallyUpdateLeaderboards(std::vector<BandUser *> &, bool, Symbol, const BandStatsInfo &);
    void IncrementSongPlayCount(std::vector<BandUser *> &, Symbol);
    int TotalStars(bool) const;
    bool IsRandomSetList() const;
    void SkipSong();
    void AdvanceSong(int);
    bool HasBattleHighscore();
    bool HasHighscore();
    int GetLastOfflineScore();
    int GetLastOfflineSoloScore(BandUser *);
    bool HasSoloHighscore(BandUser *);
    bool HasValidBandScore();
    bool HasValidUserScore(BandUser *);
    unsigned char HasValidInstarankData() const;
    void UpdateInstarankRankLabel(UILabel *);
    void UpdateInstarankHighscore1Label(UILabel *);
    void UpdateInstarankHighscore2Label(UILabel *);
    void UpdateBattleInstarankHighscore1Label(UILabel *);
    void UpdateBattleInstarankHighscore2Label(UILabel *);
    const char *GetSoloScoreTypeIcon(BandUser *);
    void UpdateSoloInstarankRankLabel(BandUser *, UILabel *);
    void UpdateSoloInstarankHighscore1Label(BandUser *, UILabel *);
    void UpdateSoloInstarankHighscore2Label(BandUser *, UILabel *);
    void UploadDebugStats();
    void ClearCreditsPending();
    bool AreCreditsPending() const;
    void SetWiiPending(WiiPendingFlags);
    void ClearWiiPending(WiiPendingFlags);
    bool IsWiiPending(WiiPendingFlags) const;
    short GetRecentInstrumentMask() const;
    bool CheatToggleFinale();
    Symbol GetSongSymbol(int idx) const { return mSongs[idx]; }
    int GetWinMetric() const { return 0; }
    int GetPersistentGameData() { return 0; }
    bool InFinale() const { return mCheatInFinale && IsLastSong(); }
    bool GetCheating() const { return mCheating; }

    DataNode OnMsg(const RockCentralOpCompleteMsg &);
    DataNode OnMsg(const ModeChangedMsg &);

    static void Init();
    static MetaPerformer *Current();
    static MetaPerformer *sMetaPerformer;

    // Retail 360 layout (verified from ctor fn_8256A970 + SetBattle fn_825691D0):
    // base (Synchronizable@0 + MsgSource@0x20) ends at 0x38; the first own member
    // is mQpPerformer@0x38. Retail does NOT carry the Wii-only mWiiPending byte or
    // the mLastVenue Symbol in this early cluster (both are Wii-isms rb3-Wii kept);
    // they live at the tail here so the battle cluster lands at 0x58 like retail.
    QuickplayPerformerImpl *mQpPerformer; // 0x38
    bool mCreditsPending; // 0x3c
    Symbol mVenue; // 0x40
    Symbol mSetlist; // 0x44
    String mSetlistTitle; // 0x48
    bool mSetlistIsLocal; // 0x54
    bool mSetlistIsHmx; // 0x55
    int mSetlistBattleID; // 0x58
    bool mIsBattle; // 0x5c
    int mBattleScore; // 0x60
    ScoreType mBattleScoreType; // 0x64
    std::vector<Symbol> mSongs; // 0x68
    std::vector<int> mStars; // 0x74
    BandSongMgr *mSongMgr; // 0x80
    Instarank mBattleInstarank; // 0x84
    Instarank mBandInstarank; // 0xdc
    Instarank mInstaranks[4]; // 0x134
    int mSongID; // 0x294
    int mSongHighscore; // 0x298
    int mInstarankScores[4]; // 0x29c
    ScoreType mInstarankScoreTypes[4]; // 0x2ac
    bool mHasOnlineScoring; // 0x2bc
    bool mSkippedSong; // 0x2bd
    int unk2c0; // some sort of instrument mask?
    bool mFestivalReward; // 0x2c4
    bool mCheatInFinale; // 0x2c5
    PendingDataInfo mPendingData; // 0x2c8
    bool mCheating; // 0x334
    int unk338;
    int unk33c;
    int mRecordBattleContextID; // 0x340
    DataResultList mDataResults; // 0x344
    bool mHarmonyOverride; // 0x35c
    bool mRealDrumsOverride; // 0x35d
    int unk360; // 0x37c -- LAST own member in retail; vtordisp follows at 0x380.
#ifndef RB3_NO_WII_META_MEMBERS
    // Wii/dev-build-only members. Retail Xbox drops all three: the ctor packs
    // 0x38..0x380 with non-Wii members, then a vtordisp word at 0x380 and the
    // Hmx::Object virtual base at 0x384.
    //
    // mVenueOverride (lane CO-1/METAPERF, 2026-08-02): retail RB3-360 has NO
    // venue-override feature at all. Evidence, each with a live control:
    //   1. retail ctor stores 0x354/0x358/0x35c/0x360/0x378/0x379/0x37c and
    //      NOTHING at 0x380 (0x8258212C); grep of the whole target
    //      MetaPerformer.s finds 3 accesses to 0x37c and ZERO to 0x380.
    //   2. band.exe contains 0 occurrences of "no_venue_override",
    //      "set_venue_override", "get_venue_override" -- while the positive
    //      controls "is_now_using_vocal_harmony" and "select_random_venue" are
    //      present (1 each) and "venue" appears 128 times, so this is a real
    //      absence and not a blind scan.
    //   3. the get/set_venue_override handlers were already #ifdef HX_NATIVE.
    // Keeping it added 4 surplus bytes at 0x380, which a prior lane absorbed
    // with a TU-wide /vd0 that stripped MsgSource's vtordisp -- that made
    // HANDLE_SUPERCLASS(MsgSource) emit `subi r4,r24,0x34c` instead of retail's
    // 0x348, because the adjustor is vbase - MsgSource - (Object vfptr offset
    // inside a standalone MsgSource): 0x384-0x20-0x18 vs 0x384-0x20-0x1c.
    // Dropping the member lets /vd0 go, so MsgSource keeps its vtordisp.
    unsigned char mWiiPending; // tail (Wii-only)
    Symbol mLastVenue; // tail (Wii-only)
    Symbol mVenueOverride; // tail (Wii/dev-only)
#endif
};