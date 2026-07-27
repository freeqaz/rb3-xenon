#pragma once
#include "BandProfile.h"
#include "game/Defines.h"
#include "meta_band/BandSongMetadata.h"
#include "meta_band/SavedSetlist.h"
#include "obj/Object.h"

class SongRecord : public Hmx::Object {
public:
    SongRecord(const BandSongMetadata *);
    virtual ~SongRecord() {}
    virtual DataNode Handle(DataArray *, bool);

    bool UpdateScoreType();
    bool UpdateSharedStatus();
    bool UpdatePerformanceData();
    bool UpdateReview();
    bool UpdateRestricted();
    int GetTier(Symbol) const;
    short GetInstrumentMask(ScoreType) const;
    Symbol GetShortDifficultySym(ScoreType) const;
    int GetScore() const { return mScores[mActiveScoreType]; }
    Symbol GetShortDifficultySym() const {
        return GetShortDifficultySym(mActiveScoreType);
    }
    int GetStars() const { return mStars[mActiveScoreType]; }
    int GetNotesPct() const { return mNotesPct[mActiveScoreType]; }
    short GetInstrumentMask() const { return GetInstrumentMask(mActiveScoreType); }
    int GetReview() const { return mReview; }
    short GetBandInstrumentMask() const { return mBandInstrumentMask; }
    Difficulty GetDifficulty(ScoreType s) const { return mDiffs[s]; }
    const BandSongMetadata *Data() const { return mData; }
    inline const BandSongMetadata *GetData() const { return mData; }
    bool GetRestricted() const { return mRestricted; }
    bool IsNotBand() const { return mActiveScoreType != kScoreBand; }
    bool IsDemo() const { return mDemo; }

    Symbol mShortName; // real 0x28
    // Order verified by MusicLibrary::Rebuild{Restricted,Shared}SongData, which
    // read both members and already match.  The apparent "swap" in
    // SongRecord::Update{SharedStatus,Restricted} was a MAP MISPAIR of those two
    // functions with each other, fixed in scripts/target_symbol_map.json
    // (fn_825BA970 touches 0x2c = mRestricted, fn_825BA9E0 touches 0x2e).
    bool mRestricted; // real 0x2c
    bool mDemo; // real 0x2d
    bool mIsShared; // real 0x2e
    std::map<Symbol, int> mTier; // 0x24
    ScoreType mActiveScoreType; // 0x3c
    int mScores[11]; // 0x40
    Difficulty mDiffs[11]; // 0x6c
    int mStars[11]; // 0x98
    int mNotesPct[11]; // 0xc4
    short mBandInstrumentMask; // 0xf0
    int mReview; // 0xf4
    int mPlays; // 0xf8
    const BandSongMetadata *mData; // 0xfc
};

class SetlistRecord : public Hmx::Object {
public:
    SetlistRecord(SavedSetlist *);
    virtual ~SetlistRecord() {}
    virtual DataNode Handle(DataArray *, bool);

    bool IsLocal() const;
    bool IsNetSetlist() const;
    bool IsProfileOwner(const BandProfile *) const;
    const char *GetOwner() const;
    Symbol GetSetlistTypeSym() const;
    bool HasViewableGamercard() const;
    void ViewGamercard(class LocalBandUser *);
    Symbol GetToken() const { return mToken; }
    SavedSetlist *GetSetlist() const { return mSetlist; }

    SavedSetlist *mSetlist; // 0x1c
    Symbol mToken; // 0x20
    bool unk24; // 0x24
    int mID; // 0x28
    int mBattleTimeLeft; // 0x2c
    int unk30; // 0x30
};