#pragma once
#include "obj/DataFile.h"
#include "os/ContentMgr.h"
#include "utl/BinStream.h"
#include <set>
#include <map>
#include <hash_map>
#include "utl/Str.h"
#include "obj/Data.h"
#include "utl/Symbol.h"

// Retail RB3-360 SongUpgradeMgr's "map" members are actually STLport hash_map:
// the accessors (UpgradeData/ContentName/GetUpgradeSongsInContent/MarkAvailable)
// inline the out-of-line hashtable::find COMDAT (int-key find lbl_82552CD0,
// Symbol-key find fn_82543F88), returning an iterator-by-value with a NULL-miss
// sentinel and the value at slist node+0x8 — not a red-black tree (which puts
// the value at node+0x14 and finds via 82552c.. _Rb_tree::_M_find). sizeof
// (hash_map)=0x1c naturally (the _M_max_load_factor float at +0x18), so the
// retail manager lays out as set<int>@0x4 (0x18), then three 0x1c hash_maps at
// 0x1c / 0x38 / 0x54, then the bool at 0x70. hash<Symbol> hashes the interned
// char* word identity (matches retail: lwz key; divwu). Guarded so a TU that
// also includes another hash<Symbol> header (e.g. via BandSongMgr.h) doesn't
// ODR-clash.
#ifndef RB3_HASH_SYMBOL_DEFINED
#define RB3_HASH_SYMBOL_DEFINED
namespace stlpmtx_std {
_STLP_TEMPLATE_NULL struct hash<Symbol> {
    size_t operator()(const Symbol &s) const { return (size_t)s.Str(); }
};
}
#endif

class SongUpgradeData {
public:
    SongUpgradeData();
    SongUpgradeData(DataArray *);

    void InitSongUpgradeData();
    int RealGuitarTuning(int) const;
    int RealBassTuning(int) const;
    float Rank(Symbol) const;
    const char *MidiFile() const;
    void Save(BinStream &);
    void Load(BinStream &);
    bool HasPart(Symbol) const;
    bool CorrectMidiFile(ContentLocT, Symbol);

    static int sSaveVer;

    int mUpgradeVersion; // 0x0
    String mMidiFile; // 0x4
    float mRealGuitarRank; // 0x10
    float mRealBassRank; // 0x14
    int mRealGuitarTuning[6]; // 0x18
    int mRealBassTuning[4]; // 0x30
};

class SongUpgradeMgr : public ContentMgr::Callback {
public:
    SongUpgradeMgr();
    virtual ~SongUpgradeMgr() {}
    virtual void ContentStarted();
    virtual bool ContentDiscovered(Symbol);
    virtual void ContentMounted(const char *, const char *);
    virtual void ContentLoaded(class Loader *, ContentLocT, Symbol);
    virtual const char *ContentPattern();
    virtual const char *ContentDir();

    bool HasUpgrade(int) const;
    const char *ContentName(int) const;
    SongUpgradeData *UpgradeData(int) const;
    bool SongCacheNeedsWrite() const;
    void ClearSongCacheNeedsWrite();
    bool WriteCachedMetadataToStream(BinStream &) const;
    bool ReadCachedMetadataFromStream(BinStream &, int);
    void ClearCachedContent();
    void ClearFromCache(Symbol);
    void GetUpgradeSongsInContent(Symbol, std::vector<int> &) const;
    void AddUpgradeData(DataArray *, DataLoader *, ContentLocT, Symbol);
    void MarkAvailable(int, Symbol);

    std::set<int> mAvailableUpgrades; // 0x4
    std::hash_map<int, SongUpgradeData *> mUpgradeData; // 0x1c
    std::hash_map<Symbol, std::vector<int> > unk34; // 0x38 - cached
    std::hash_map<int, Symbol> unk4c; // 0x54
    bool mSongCacheNeedsWrite; // 0x70
};
