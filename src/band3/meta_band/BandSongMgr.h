#pragma once
#include "beatmatch/TrackType.h"
#include "game/BandUserMgr.h"
#include "system/meta/SongMgr.h"
#include "meta_band/SongUpgradeMgr.h"
#include "meta_band/LicenseMgr.h"
#include <hash_map>

// Retail RB3-360's BandSongMgr lookup members are STLport hash_map, not std::map
// (the Wii dev decomp's approximation). GetShortNameFromSongID (fn_8255F858)
// inlines the int-keyed hashtable::find COMDAT (FUN_82552CD0) against this+0xd4
// and this+0x10c, and the destructor (fn_825632E0) destroys the lookup members
// at +0xd4/+0xf0/+0x10c via the hash_map dtor (Function_82547CC8). sizeof(hash_map)
// is 0x1c naturally (the _M_max_load_factor float at +0x18) — exactly the offset
// stride between the three lookup members in retail, so NO 0x1c gate pad is
// needed. hash<Symbol> hashes the interned char* word identity (the guarded
// RB3_HASH_SYMBOL_DEFINED spec comes from SongMgr.h); hash<int> is the STLport
// built-in. See docs/decomp/research/2026-06-19-w8-hashmap-exhaustion.md.

enum SongID {
    kSongID_Invalid = -2,
    kSongID_Any = -1,
    kSongID_Random = 0
};

class BandSongMgr : public SongMgr {
public:
    class SongRanking {
    public:
        bool operator==(Symbol s) const { return mInstrument == s; }

        Symbol mInstrument; // 0x0
        std::vector<std::pair<float, float> > mTierRanges; // 0x4
    };

    BandSongMgr();
    virtual DataNode Handle(DataArray *, bool);
    virtual ~BandSongMgr() {}
    virtual void Init();
    virtual void Terminate();
    virtual SongMetadata *Data(int) const; // fix return type
    virtual SongInfo *SongAudioData(int) const;
    virtual void ContentDone();
    virtual void ContentMounted(const char *, const char *);
    virtual void GetContentNames(Symbol, std::vector<Symbol> &) const;
    virtual bool SongCacheNeedsWrite() const;
    virtual void ClearSongCacheNeedsWrite();
    virtual void AllowCacheWrite(bool);
    virtual void ClearCachedContent();
    virtual Symbol GetShortNameFromSongID(int, bool) const;
    virtual int GetSongIDFromShortName(Symbol, bool) const;
    virtual const char *SongName(int) const;
    virtual bool CanAddSong() const;
    virtual bool AllowContentToBeAdded(DataArray *, ContentLocT);
    virtual void AddSongData(DataArray *, DataLoader *, ContentLocT);
    virtual void
    AddSongData(DataArray *, std::hash_map<int, SongMetadata *> &, const char *, ContentLocT, std::vector<int> &);
    virtual void AddSongIDMapping(int, Symbol);
    virtual void ReadCachedMetadataFromStream(BinStream &, int);
    virtual void WriteCachedMetadataToStream(BinStream &) const;
    virtual const char *ContentPattern();
    virtual const char *ContentDir();
    virtual bool HasContentAltDirs() { return !mContentAltDirs.empty(); }
    virtual std::vector<String> *ContentAltDirs() { return &mContentAltDirs; }

    void AddSongs(DataArray *songs);
    const char *UpgradeMidiFile(int) const;
    const char *MidiFile(Symbol) const;
    const char *SongName(Symbol) const;
    SongUpgradeData *GetUpgradeData(int) const;
    const char *SongFilePath(Symbol, const char *) const;
    const char *GetAlbumArtPath(Symbol) const;
    const char *SongPath(Symbol) const;
    int NumRankTiers(Symbol) const;
    Symbol RankTierToken(int) const;
    void GetRankedSongs(std::vector<int> &, bool, bool) const;
    int GetValidSongCount(const std::hash_map<int, SongMetadata *> &) const;
    int GetValidSongs(
        const std::vector<int> &,
        BandUserMgr &,
        std::vector<int> &,
        float,
        float,
        bool,
        bool
    ) const;
    bool IsRestricted(int) const;
    int RankTier(float, Symbol) const;
    int GetNumVocalParts(Symbol) const;
    void AddRecentSong(int);
    bool IsDemo(int) const;
    bool HasLicense(Symbol) const;
    void SyncSharedSongs();
    int GetMaxSongCount() const;
    void CheatToggleMaxSongCount();
    bool InqAvailableSongSources(std::set<Symbol> &);
    int NumRankedSongs(TrackType, bool, Symbol) const;
    bool CreateSongCacheID(CacheID **);
    int GetCurSongCount() const;
    int GetPosInRecentList(int);
    bool IsInExclusionList(const char *, int) const;
    bool RemoveOldestCachedContent();
    int GetPartDifficulty(Symbol, Symbol) const;
    bool IsSongUnplayable(int, BandUserMgr &, bool) const;

    SongInfo *SongAudioData(Symbol s) const { return SongMgr::SongAudioData(s); }

    static bool GetFakeSongsAllowed();
    static void SetFakeSongsAllowed(bool);
    static bool sFakeSongsAllowed;

    mutable DataArraySongInfo *unkc0; // 0xd0
    std::hash_map<int, Symbol> mSongNameLookup; // 0xd4
    std::hash_map<Symbol, int> mSongIDLookup; // 0xf0
    std::hash_map<int, Symbol> mExtraSongIDMap; // 0x10c
    std::list<SongRanking> mSongRankings; // 0x128
    std::list<int> unk114; // 0x130
    std::vector<Symbol> unk11c; // 0x138
    bool unk124; // 0x144
    SongUpgradeMgr *mUpgradeMgr; // 0x148
    LicenseMgr *mLicenseMgr; // 0x14c
    std::vector<String> mContentAltDirs; // 0x150
    int mMaxSongCount; // 0x15c
    bool unk13c; // 0x160
    int unk140; // 0x164 - num valid songs
};

extern BandSongMgr &TheSongMgr;
