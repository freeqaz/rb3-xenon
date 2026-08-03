#pragma once
#include "beatmatch/TrackType.h"
#include "game/BandUserMgr.h"
#include "system/meta/SongMgr.h"
#include "meta/Jukebox.h"
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
    // +0x148 is a `Jukebox` (lane DP-1). This slot was previously TWO invented
    // members -- a `vector<pair<float,float>>` placeholder plus an `int unk140`
    // "num valid songs" at +0x154 -- and the header itself recorded that neither
    // could be pinned. Both are now resolved by one real type, and every piece
    // of evidence the old comment listed as unexplained is explained by it:
    //   * the destructor (fn_8257A7E0) tears down a 3-pointer STLport vector at
    //     +0x148/+0x150 whose element type is 8 bytes and TRIVIALLY DESTRUCTIBLE
    //       -> `std::vector<JukeboxItem>`, and JukeboxItem is exactly {int,int}.
    //   * the "4 bytes at +0x154 never referenced in-TU but must exist"
    //       -> `Jukebox::mPlayCounter`, which needs no teardown.
    //   * `sizeof(Jukebox) == 0x10` puts mUpgradeMgr at +0x158, where
    //     ClearCachedContent (a 100% match) already proved it is.
    //   * the constructor (retail 0x8257AA00) does `li r4,0x7d0 / addi r3,r30,0x148
    //     / bl ??0Jukebox@@QAA@H@Z`, and 0x827B1838 has EXACTLY ONE caller in the
    //     whole binary -- this one.
    //   * BandSongMgr::Handle's `add_recent_song` arm calls ?Play@Jukebox@@QAAXH@Z
    //     with `this + 0x148`, likewise the only Jukebox::Play call in the binary.
    // ADJUDICATED ON RETAIL BYTES, and note the refutation: `0x154` occurs ZERO
    // times in the entire pinned retail TU, so the old "AddSongData stores
    // GetValidSongCount() at +0x154" claim had no byte support at all --
    // GetCurSongCount now recomputes instead of reading an invented cache.
    // Jukebox lives in DC3 (src/system/meta/Jukebox.h, already 4/4 at 100%) and
    // NOT in the rb3-Wii oracle, which is why a source diff never showed it.
    Jukebox mJukebox; // 0x148 (mJukeboxItems 0x148..0x154, mPlayCounter 0x154)
    //
    // The RTTI argument below still stands and still fixes the tail; only the
    // identity of +0x148..+0x158 changed.
    //
    // Ground truth is retail's own RTTI: ??_R4BandSongMgr (COL at 0x821E0074,
    // type descriptor 0x82C72C00 -> ".?AVBandSongMgr@@") records the Object
    // vbase vfptr at offset 0x178, so the non-virtual prefix ends at 0x174 and
    // there is no room for a member there. Corroborated three ways: the sibling
    // ??_R4SongMgr COL says 0xd4 (which our SongMgr layout already reproduces
    // exactly); retail's BandSongMgr vtordisp thunk family at 0x8257AD18 uses
    // {0x178 dtor, 0x158 x4 MsgSource virtuals} against our {0x17c, 0x15c x4};
    // and the pinned retail TU makes ZERO references to +0x174 or +0x154 while
    // unk13c is provably a byte at +0x170 (lbz/stb 0x170). Folding the former
    // `unk154` placeholder and `unk140` into one member is the only arrangement
    // consistent with all four instruments.
    SongUpgradeMgr *mUpgradeMgr; // 0x158
    LicenseMgr *mLicenseMgr; // 0x15c
    // mContentAltDirs FOLLOWS the two manager pointers in retail: Terminate,
    // Init and the destructor all address it as `this + 0x160`. mMaxSongCount
    // then lands at 0x16c -- retail Init stores cfg->FindInt(max_song_count)
    // to +0x16c.
    std::vector<String> mContentAltDirs; // 0x160
    int mMaxSongCount; // 0x16c
    bool unk13c; // 0x170 (prefix ends at 0x174: vtordisp 0x174, Object vbase 0x178)
};

// Retail 360 exposes the song manager through a MUTABLE POINTER global (the
// GamePanel::Load target reloads both the object pointer AND its vptr around
// every intervening call: lwz obj -> lwz vptr -> vcall, twice from the same
// @ha base). A C++ reference (Wii-dev shape) lets MSVC CSE both loads across
// calls -- reference-immutability makes the object address (and thus vptr)
// invariant -- which produces a hoisted preload retail provably lacks. Keep
// the `TheSongMgr.` spelling for all call sites via the macro.
extern BandSongMgr *TheSongMgrPtr;
#define TheSongMgr (*TheSongMgrPtr)
