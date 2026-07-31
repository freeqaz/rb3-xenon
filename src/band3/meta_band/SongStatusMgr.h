#pragma once
#include "game/Defines.h"
#include "meta/FixedSizeSaveable.h"
#include "meta_band/MetaPerformer.h"
#include "obj/Object.h"
#include "system/utl/BinStream.h"
#include "system/meta/FixedSizeSaveableStream.h"
#include "game/BandUser.h"
#include <hash_map>

// Retail RB3-360 SongStatusMgr replaced the Wii build's `SongStatusLookup
// mLookups[1000]` linear-scan cache (an embedded SongStatusCacheMgr sub-object,
// 0x1f48 bytes) with an STLport `hash_map<int, SongStatus*>` song-index cache
// living directly at the manager (this+0x38). Proven from the retail asm:
//  - 23 accessors decode `addi rX, r3, 0x38; bl <int-key hashtable::find>`
//    (find COMDAT lbl_82552CD0 = STLport hashtable<int,...>::find returning an
//    iterator-by-value, NULL-miss sentinel, value pointer at slist node+0x8).
//  - CreateOrAccessSongStatus (fn_825B9ED0) reads the slist count at this+0x4c
//    (= map+0x14), caps at 3000, news a 0x200-byte SongStatus, inserts.
//  - UpdateSong (fn_825BA440) / UploadDirtyScores (fn_825BA680) read
//    mUpdatingStatus@this+0xd8, mUpdatingScoreType@0xdc, mUpdatingDifficulty@0xe0,
//    mLocalUser@0x30, mSongMgr@0x34, the map slist head at +0x3c (map+0x4).
// hash<int> is provided by STLport by default (stl/_hash_fun.h) so no custom
// specialization is needed (unlike the Symbol-key maps elsewhere).
//
// Layout (Hmx::Object base 0x0..0x28, FixedSizeSaveable base 0x28..0x30):
//   mLocalUser            0x30
//   mSongMgr              0x34
//   mSongStatusCache      0x38  (hash_map<int,SongStatus*>, 0x1c)
//   mCachedTotalScores    0x54  (int[11], 0x2c)
//   mCachedTotalDiscScores 0x80 (int[11], 0x2c)
//   mCachedTotalStars     0xac  (int[11], 0x2c)
//   mUpdatingStatus       0xd8
//   mUpdatingScoreType    0xdc
//   mUpdatingDifficulty   0xe0   (sizeof = 0xe4)

class PlayerScore {
public:
    PlayerScore()
        : mScoreType(kScoreDrum), mScore(0), mStars(0), mPlayerID(0), unk18(-1),
          mDiff(kDifficultyEasy), mTotalScore(0), mTotalDiscScore(0), mAccuracy(0) {}
    virtual ~PlayerScore() {}

    ScoreType mScoreType; // 0x4
    int mScore; // 0x8
    int mStars; // 0xc
    int mPlayerID; // 0x10
    int unk14;
    int unk18; // 0x18 - slot
    Difficulty mDiff; // 0x1c
    int mTotalScore; // 0x20
    int mTotalDiscScore; // 0x24
    int mAccuracy; // 0x28
};

enum SongStatusFlagType {
    kSongStatusFlagNone = 0,
    kSongStatusFlag_HitBRE = 1,
    kSongStatusFlag_ChordbookComplete = 2,
    kSongStatusFlag_FullCombo = 4,
    kSongStatusFlag_LessonComplete = 8,
    kSongStatusFlag_PerfectDrumRolls = 0x10,
    kSongStatusFlag_AllDoubleAwesomes = 0x20,
    kSongStatusFlag_AllTripleAwesomes = 0x40,
    kSongStatusFlag_Dirty = 0x80
};

// i got this from bank 5 don't get mad at me about the union implementation
// personally i would've rather made this an anonymous union with the SongStatusData class
// declaration
union SongStatusInstrumentUnion {
    struct {
        unsigned char mHoposPercentage;
        unsigned char mSoloPercentage;
    } mGuitarDrums;

    struct {
        unsigned char mAwesomes;
        unsigned char mDoubleAwesomes;
        unsigned char mTripleAwesomes;
    } mVocals;
};

class SongStatusData {
public:
    void Clear(ScoreType);
    void SaveToStream(BinStream &, ScoreType) const;
    void LoadFromStream(BinStream &, ScoreType);

    static int SaveSize(int);

    unsigned char mStars; // 0x0
    unsigned char mAccuracy; // 0x1
    unsigned short mStreak; // 0x2
    unsigned char mFlags; // 0x4
    SongStatusInstrumentUnion mShared; // 0x5-0x7
};

class SongStatus : public FixedSizeSaveable {
public:
    SongStatus();
    virtual ~SongStatus();
    virtual void SaveFixed(FixedSizeSaveableStream &) const;
    virtual void LoadFixed(FixedSizeSaveableStream &, int);

    void Clear();
    void SetLastPlayed(int);
    int GetLastPlayed() const;
    void SetPlayCount(int);
    void SetBitmapLessonComplete(unsigned int &, int, bool);
    void SetProGuitarLessonSectionComplete(Difficulty, int, bool);
    void SetProBassLessonSectionComplete(Difficulty, int, bool);
    void SetProKeyboardLessonSectionComplete(Difficulty, int, bool);
    void SetID(int);
    int GetID() const;
    void SetReview(unsigned char);
    void SetInstrumentMask(unsigned short);
    void SetDirty(ScoreType, Difficulty, bool);
    void SetFlag(SongStatusFlagType, ScoreType, Difficulty);
    void ClearFlag(SongStatusFlagType, ScoreType, Difficulty);
    unsigned char GetSoloPercent(ScoreType, Difficulty) const;
    unsigned char GetHOPOPercent(ScoreType, Difficulty) const;
    unsigned char GetAwesomes(ScoreType, Difficulty) const;
    unsigned char GetDoubleAwesomes(ScoreType, Difficulty) const;
    unsigned char GetTripleAwesomes(ScoreType, Difficulty) const;
    bool UpdateScore(ScoreType, Difficulty, int);
    bool UpdateStars(ScoreType, Difficulty, unsigned char);
    bool UpdateAccuracy(ScoreType, Difficulty, unsigned char);
    bool UpdateStreak(ScoreType, Difficulty, unsigned short);
    bool UpdateSoloPercent(ScoreType, Difficulty, unsigned char);
    bool UpdateHOPOPercent(ScoreType, Difficulty, unsigned char);
    bool UpdateAwesomes(ScoreType, Difficulty, unsigned char);
    bool UpdateDoubleAwesomes(ScoreType, Difficulty, unsigned char);
    bool UpdateTripleAwesomes(ScoreType, Difficulty, unsigned char);

    int GetPlayCount() const { return mPlayCount; }
    int GetScore(ScoreType ty) const { return mHighScores[ty]; }
    int GetHighScore(ScoreType ty) const { return mHighScores[ty]; }
    Difficulty GetHighScoreDifficulty(ScoreType ty) const { return mHighScoreDiffs[ty]; }
    unsigned char GetReview() const { return mReview; }
    unsigned short GetInstrumentMask() const { return mBandScoreInstrumentMask; }
    unsigned char GetStars(ScoreType ty, Difficulty diff) const {
        return mSongData[ty][diff].mStars;
    }
    unsigned char GetAccuracy(ScoreType ty, Difficulty diff) const {
        return mSongData[ty][diff].mAccuracy;
    }
    unsigned short GetStreak(ScoreType ty, Difficulty diff) const {
        return mSongData[ty][diff].mStreak;
    }

    bool CheckFlag(SongStatusFlagType flag, ScoreType ty, Difficulty diff) const {
        return mSongData[ty][diff].mFlags & flag;
    }

    bool CheckLessonBit(unsigned int part, int bit) const { return part & (1 << bit); }

    bool CheckProGuitarLessonBit(Difficulty diff, int bit) const {
        return CheckLessonBit(mProGuitarLessonParts[diff], bit);
    }

    bool CheckProBassLessonBit(Difficulty diff, int bit) const {
        return CheckLessonBit(mProBassLessonParts[diff], bit);
    }

    bool CheckProKeyboardLessonBit(Difficulty diff, int bit) const {
        return CheckLessonBit(mProKeyboardLessonParts[diff], bit);
    }

    static int SaveSize(int);

    int mSongID; // 0x8
    unsigned short mBandScoreInstrumentMask; // 0xc
    unsigned char mReview; // 0xe
    int mLastPlayed; // 0x10
    int mPlayCount; // 0x14
    unsigned int mProGuitarLessonParts[4]; // 0x18
    unsigned int mProBassLessonParts[4]; // 0x28
    unsigned int mProKeyboardLessonParts[4]; // 0x38
    int mHighScores[11]; // 0x48
    Difficulty mHighScoreDiffs[11]; // 0x74
    SongStatusData mSongData[11][4]; // 0xa0
};

class BandSongMgr;

class SongStatusMgr : public Hmx::Object, public FixedSizeSaveable {
public:
    SongStatusMgr(LocalBandUser *, BandSongMgr *);
    virtual ~SongStatusMgr();
    virtual DataNode Handle(DataArray *, bool);
    virtual void SaveFixed(FixedSizeSaveableStream &) const;
    virtual void LoadFixed(FixedSizeSaveableStream &, int);

    void Clear();
    int GetScore(int, ScoreType) const;
    int GetStars(int, ScoreType, Difficulty) const;
    int GetBestStars(int, ScoreType, Difficulty) const;
    int GetBestAccuracy(int, ScoreType, Difficulty) const;
    int GetBestStreak(int, ScoreType, Difficulty) const;
    int GetBestHOPOPercent(int, ScoreType, Difficulty) const;
    int GetBestSoloPercent(int, ScoreType, Difficulty) const;
    int GetBestAwesomes(int, ScoreType, Difficulty) const;
    int GetBestDoubleAwesomes(int, ScoreType, Difficulty) const;
    int GetBestTripleAwesomes(int, ScoreType, Difficulty) const;
    int GetBestSongStatusFlag(Symbol, SongStatusFlagType, ScoreType, Difficulty) const;
    bool IsProGuitarSongLessonComplete(int, Difficulty) const;
    bool IsProBassSongLessonComplete(int, Difficulty) const;
    bool IsProKeyboardSongLessonComplete(int, Difficulty) const;
    int GetCachedTotalScore(ScoreType) const;
    int GetCachedTotalDiscScore(ScoreType) const;
    int CalculateTotalScore(ScoreType, Symbol) const;
    int GetTotalSongs(ScoreType, Symbol) const;
    int GetCompletedSongs(ScoreType, Difficulty, Symbol) const;
    int GetPossibleStars(ScoreType, Symbol) const;
    int GetTotalBestStars(ScoreType, Difficulty, Symbol) const;
    bool UpdateSongStats(ScoreType, Difficulty, const PerformerStatsInfo &, SongStatus *);
    void UpdateCachedTotalStars(ScoreType);
    bool UpdateSong(int, const PerformerStatsInfo &, bool);
    int UpdateCachedTotalDiscScore(ScoreType);
    void UpdateCachedTotalScore(ScoreType);
    unsigned short GetBandInstrumentMask(int) const;
    Difficulty GetHighScoreDifficulty(int, ScoreType) const;
    int GetHighScore(int, ScoreType) const;
    bool IsSongPlayed(int, ScoreType, Difficulty) const;
    bool IsSongPlayedAtMinDifficulty(int, ScoreType, Difficulty) const;
    int GetAccuracy(int, ScoreType, Difficulty) const;
    int GetStreak(int, ScoreType, Difficulty) const;
    int GetSoloPercent(int, ScoreType, Difficulty) const;
    int GetHOPOPercent(int, ScoreType, Difficulty) const;
    int GetAwesomes(int, ScoreType, Difficulty) const;
    int GetDoubleAwesomes(int, ScoreType, Difficulty) const;
    int GetTripleAwesomes(int, ScoreType, Difficulty) const;
    int GetCachedTotalStars(ScoreType) const;
    int CalculateTotalStars(ScoreType) const;
    int GetSongPlayCount(int) const;
    void SetSongPlayCount(int, int);
    void SetProGuitarSongLessonComplete(int, Difficulty);
    void SetProBassSongLessonComplete(int, Difficulty);
    void SetProKeyboardSongLessonComplete(int, Difficulty);
    void SetProGuitarSongLessonSectionComplete(int, Difficulty, int);
    void SetProBassSongLessonSectionComplete(int, Difficulty, int);
    void SetProKeyboardSongLessonSectionComplete(int, Difficulty, int);
    bool IsProGuitarSongLessonSectionComplete(int, Difficulty, int) const;
    bool IsProBassSongLessonSectionComplete(int, Difficulty, int) const;
    bool IsProKeyboardSongLessonSectionComplete(int, Difficulty, int) const;
    unsigned char GetSongReview(int) const;
    bool SetSongReview(int, unsigned char);
    void SetSongStatusFlag(Symbol, SongStatusFlagType, ScoreType, Difficulty);
    bool GetSongStatusFlag(Symbol, SongStatusFlagType, ScoreType, Difficulty) const;
    void PopulatePlayerScore(SongStatus *, ScoreType, Difficulty, PlayerScore &);
    void UploadDirtyScores();
    void FakeFill();

    // Cache index. Retail inlines STLport hash_map<int,SongStatus*>::find at
    // this+0x38 (the find COMDAT returns an iterator-by-value with a NULL-miss
    // sentinel; the value pointer is at slist node+0x8). The Wii build's
    // GetSongStatusIndex/HasSongStatus/AccessSongStatus linear scan over
    // mLookups[1000] is replaced by these map lookups.
    bool HasSongStatus(int songID) const {
        return mSongStatusCache.find(songID) != mSongStatusCache.end();
    }
    SongStatus *GetSongStatus(int songID) const {
        // Retail inlines this after a dominating HasSongStatus(songID) (same
        // inlined find), so MSVC value-numbers away the miss-branch: the
        // accessor reads the find node's value (node+0x8) unconditionally on
        // the hit path. Returning it->second directly (no end() re-check)
        // reproduces that inlined codegen byte-for-byte.
        return mSongStatusCache.find(songID)->second;
    }
    SongStatus *AccessSongStatus(int songID) const { return GetSongStatus(songID); }
    SongStatus *CreateOrAccessSongStatus(int songID) const;
    void ClearLeastImportantSongStatusEntry();

    void SetLocalUser(LocalBandUser *u) { mLocalUser = u; }

    DataNode OnMsg(const RockCentralOpCompleteMsg &);

    static int SaveSize(int);
    static bool sFakeLeaderboardUploadFailure;

    LocalBandUser *mLocalUser; // 0x30
    BandSongMgr *mSongMgr; // 0x34
    mutable std::hash_map<int, SongStatus *> mSongStatusCache; // 0x38
    int mCachedTotalScores[11]; // 0x54
    int mCachedTotalDiscScores[11]; // 0x80
    int mCachedTotalStars[11]; // 0xac
    SongStatus *mUpdatingStatus; // 0xd8
    ScoreType mUpdatingScoreType; // 0xdc
    Difficulty mUpdatingDifficulty; // 0xe0
};
