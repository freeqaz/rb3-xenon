#pragma once
#include "meta/SongMetadata.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "os/ContentMgr.h"
#include "utl/BufStream.h"
#include "utl/Cache.h"
#include "utl/MemStream.h"
#include "utl/SongInfoCopy.h"
#include <set>
#include <vector>
#include <map>
#include <hash_map>

// Retail RB3-360 SongMgr's "map" members are actually STLport hash_map (the
// find paths inline the out-of-line hashtable::find COMDAT — int-key find
// lbl_82552CD0, Symbol-key find fn_82543F88 — returning iterator-by-value with
// a NULL-miss sentinel and value at slist node+0x8, and the cache serialization
// operator<< walks an slist (size at +0x14, key@+0x4, value@+0x8, next@+0x0),
// not a red-black tree). sizeof(hash_map)=0x1c naturally (the _M_max_load_factor
// float at +0x18) — which is what the former RB3_MAP_0x1C gate was compensating
// for with a padded std::map. Converting all five maps to hash_map and dropping
// the gate is the genuine layout. hash<Symbol> hashes the interned char* word
// identity (matches retail: lwz key; divwu).
#ifndef RB3_HASH_SYMBOL_DEFINED
#define RB3_HASH_SYMBOL_DEFINED
namespace stlpmtx_std {
_STLP_TEMPLATE_NULL struct hash<Symbol> {
    size_t operator()(const Symbol &s) const { return (size_t)s.Str(); }
};
}
#endif

// Retail RB3-360 SongMgr derives from MsgSource (virtual Hmx::Object base) +
// ContentMgr::Callback, NOT plain Hmx::Object. Proven from the retail RTTI
// Complete Object Locator @0x821d89ec (attr=0x3 = MI+virtual, 5 base classes)
// and the retail primary vtable @0x8209cd1c (own-virtuals begin at slot 14/+0x38,
// vs our former 21-slot Hmx::Object prefix). The MsgSource base shrinks the
// vtable prefix by 0x1c and the object head by 0x10; combined with dropping the
// DC3-era AlternateSongDir virtual slot (retail has no such slot), every member
// offset and own-virtual slot then matches retail. The retail SongMgr.cpp TU
// also uses sizeof(map)=0x1c with sizeof(set)=0x18 -> gate RB3_MAP_0x1C per-TU.
// See docs/decomp/research/2026-06-11-bp4-songmgr.md.
#ifdef HX_NATIVE
#define SONGMGR_DC3_VIRTUAL virtual
#else
#define SONGMGR_DC3_VIRTUAL
#endif

// from RB2 taken from RB3 decomp
enum SongMgrState {
    kSongMgr_SaveMount = 0,
    kSongMgr_SaveWrite = 1,
    kSongMgr_SaveUnmount = 2,
    kSongMgr_Ready = 3,
    kSongMgr_Failure = 4,
    kSongMgr_Max = 5,
    kSongMgr_Nil = -1,
};

// SongID enum removed: DC3 had it here but RB3 only defines it in BandSongMgr.h
// (with different values: Invalid=-2, Any=-1, Random=0). BandSongMgr.h includes
// this header, causing a redefinition error. Removing from engine base.
class SongMgr : public MsgSource, public ContentMgr::Callback {
public:
    SongMgr() {}
    // MsgSource / Hmx::Object
    virtual DataNode Handle(DataArray *, bool);
    virtual ~SongMgr();

    // ContentMgr::Callback
    virtual void ContentStarted();
    virtual bool ContentDiscovered(Symbol);
    virtual void ContentMounted(char const *, char const *);
    virtual void ContentUnmounted(char const *);
    virtual void ContentLoaded(Loader *, ContentLocT, Symbol);
    virtual void ContentDone();

    // SongMgr
    virtual void Init();
    virtual void Terminate() {}
    /** Get the song metadata associated with the supplied song ID.
     * @param [in] songID The song ID.
     * @returns The corresponding song metadata.
     */
    virtual const SongMetadata *Data(int songID) const; // 0x60
    /** Get the song audio data associated with the supplied song ID. */
    virtual SongInfo *SongAudioData(int songID) const = 0;
    // retail RB3-360 has NO AlternateSongDir vtable slot (DC3-only virtual); kept
    // non-virtual so SongPath() can still call it, gated like DRAW_DC3_VIRTUAL.
    SONGMGR_DC3_VIRTUAL char const *AlternateSongDir() const { return "songs/updates/"; }
    /** Add a song's content name to the given vector of names.
     * @param [in] shortname The song's shortname.
     * @param [out] names The collection of song content names.
     */
    virtual void GetContentNames(Symbol shortname, std::vector<Symbol> &names) const;
    virtual bool SongCacheNeedsWrite() const { return mSongCacheNeedsWrite; }
    virtual void ClearSongCacheNeedsWrite() { mSongCacheNeedsWrite = false; }
    virtual void ClearCachedContent();
    /** Get the song shortname associated with the supplied song ID.
     * @param [in] songID The song ID.
     * @param [in] fail If true, and the song can't be found, fail the system.
     * @returns The corresponding song shortname.
     */
    virtual Symbol GetShortNameFromSongID(int songID, bool fail = true) const = 0;
    /** Get the song ID associated with the supplied song shortname.
     * @param [in] shortname The song shortname.
     * @param [in] fail If true, and the song can't be found, fail the system.
     * @returns The corresponding song ID.
     */
    virtual int GetSongIDFromShortName(Symbol shortname, bool fail = true) const = 0;

    /** Get the song audio data associated with the supplied shortname. */
    SongInfo *SongAudioData(Symbol shortname) const;
    bool IsSongCacheWriteDone() const;
    char const *GetCachedSongInfoName() const;
    char const *SongPath(Symbol shortname, int version) const;
    char const *SongFilePath(Symbol, char const *, int) const;
    /** Dump the contents of the SongMgr to the console.
     * @param [in] all If true, print all the songs we have. Else, skip non-DLC songs.
     */
    void DumpSongMgrContents(bool all);
    /** Do we have the supplied songID in our list of available songs? */
    bool HasSong(int songID) const;
    /** Do we have the supplied shortname in our list of available songs? */
    bool HasSong(Symbol shortname, bool fail = true) const;
    int GetCachedSongInfoSize() const;
    bool IsSongMounted(Symbol shortname) const;
    bool SaveCachedSongInfo(BufStream &);
    /** Does the supplied content file name house the supplied song ID? */
    bool IsContentUsedForSong(Symbol contentName, int songID) const;
    void StartSongCacheWrite();
    /** Remove the supplied content file name from the cache. */
    void ClearFromCache(Symbol contentName);
    /** Given a songID, get the name of the content file it comes from. */
    const char *ContentName(int songID) const;
    /** Given a shortname, get the name of the content file it comes from. */
    const char *ContentName(Symbol shortname, bool fail = true) const;
    bool LoadCachedSongInfo(BufStream &);

    /** Do we have this content file name in our records? */
    bool HasContent(Symbol contentName) {
        return mSongIDsInContent.find(contentName) != mSongIDsInContent.end();
    }
    const std::set<int> &GetAvailableSongSet() const;

protected:
    virtual bool AllowContentToBeAdded(DataArray *, ContentLocT) { return true; }
    virtual void AddSongData(DataArray *, DataLoader *, ContentLocT) = 0;
    virtual void AddSongData(
        DataArray *,
        std::hash_map<int, SongMetadata *> &,
        const char *,
        ContentLocT,
        std::vector<int> &
    ) = 0;
    virtual void AddSongIDMapping(int, Symbol) = 0;
    virtual void ReadCachedMetadataFromStream(BinStream &, int) = 0;
    virtual void WriteCachedMetadataToStream(BinStream &) const = 0;

    char const *CachedPath(Symbol shortname, const char *, int version) const;
    void SaveMount();
    void SaveUnmount();
    void SaveWrite();
    /** Given a content file name, get the file's song IDs. */
    void GetSongsInContent(Symbol contentName, std::vector<int> &songIDs) const;
    char const *ContentNameRoot(Symbol contentName) const;
    int NumSongsInContent(Symbol contentName) const;
    void SetState(SongMgrState state);
    void OnCacheMountResult(int result);
    void OnCacheWriteResult(int result);
    void OnCacheUnmountResult(int result);
    void CacheSongData(
        DataArray *, DataLoader *loader, ContentLocT location, Symbol contentName
    );

    // Offsets below are the retail RB3-360 layout (MsgSource head + per-TU
    // RB3_MAP_0x1C map size 0x1c, set size 0x18); proven against the retail asm.
    /** The available songs we can select in-game. Key = song ID */
    std::set<int> mAvailableSongs; // 0x1c (genuine std::set, size 0x18)
    std::hash_map<int, SongMetadata *> mUncachedSongMetadata; // 0x34
    /** The current state of the SongMgr. */
    SongMgrState mState; // 0x50
    std::hash_map<int, SongMetadata *> mCachedSongMetadata; // 0x54
    /** A collection of content files (CON/LIVES), and the song IDs inside each file.
        Key = content file name (i.e. RBMEGAPACK01OF10); Value = the song IDs.
    */
    std::hash_map<Symbol, std::vector<int> > mSongIDsInContent; // 0x70
    /** A collection of song IDs, and the contents they came from.
        Key = song ID;
        Value = the content file name (i.e. RBMEGAPACK01OF10) that houses this song
    */
    std::hash_map<int, Symbol> mContentUsedForSong; // 0x8c
    // key = content file name. value = root name???
    std::hash_map<Symbol, String> unkmap5; // 0xa8 - mounted content?
    CacheID *mSongCacheID; // 0xc4
    Cache *mSongCache; // 0xc8
    bool mHasNewContent; // 0xcc
    bool mSongCacheNeedsWrite; // 0xcd
    bool mSongCacheWriteAllowed; // 0xce
};

// TheSongMgr removed from engine header: RB3's BandSongMgr.h redeclares it as
// BandSongMgr& (the actual subclass), causing a type redefinition error.
// BandSongMgr.h includes this header, so both decls would be visible.
