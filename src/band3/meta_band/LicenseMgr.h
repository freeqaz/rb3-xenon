#pragma once
#include "os/ContentMgr.h"
#include "obj/DataFile.h"
#include "utl/BinStream.h"
#include "utl/Symbol.h"
#include <set>
#include <map>
#include <vector>
#include <hash_map>

// The retail X360 LicenseMgr keeps a content cache the Wii decomp dropped:
// a hash_map<Symbol, vector<Symbol>> at 0x1c plus a dirty bool at 0x38. The
// cache's find() inlines the STLport hashtable::find COMDAT (out-of-line find
// returning the slist node by value, NULL miss, value at node+0x8 — see
// fn_82632150 / fn_82632730). The Wii std::set approximation can't reproduce
// that. hash<Symbol> hashes the interned char* word identity, matching retail.
// Guarded so other headers defining the same specialization can co-include.
#ifndef RB3_HASH_SYMBOL_DEFINED
#define RB3_HASH_SYMBOL_DEFINED
namespace stlpmtx_std {
_STLP_TEMPLATE_NULL struct hash<Symbol> {
    size_t operator()(const Symbol &s) const { return (size_t)s.Str(); }
};
}
#endif

class LicenseMgr : public ContentMgr::Callback {
public:
    LicenseMgr();
    virtual ~LicenseMgr() {}
    virtual void ContentStarted();
    virtual bool ContentDiscovered(Symbol);
    virtual void ContentMounted(const char *, const char *);
    virtual void ContentLoaded(class Loader *, ContentLocT, Symbol);
    virtual const char *ContentPattern();
    virtual const char *ContentDir();

    bool HasLicense(Symbol) const;
    void AddLicenses(DataArray *, DataLoader *, ContentLocT, Symbol);
    bool LicenseCacheNeedsWrite() const;
    bool WriteCachedMetadataToStream(BinStream &) const;
    bool ReadCachedMetadataFromStream(BinStream &, int);
    void ClearCachedContent();
    void ClearFromCache(Symbol);
    void GetLicensesInContent(Symbol, std::vector<Symbol> &) const;
    void MarkAvailable(Symbol, Symbol);

    std::set<Symbol> mLicenses; // 0x4
    std::hash_map<Symbol, std::vector<Symbol> > mCachedLicenses; // 0x1c
    bool mCacheNeedsWrite; // 0x38
};
