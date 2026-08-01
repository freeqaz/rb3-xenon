#pragma once
#include <vector>
#include <set>
#include <map>
#include <hash_map>
#include "system/utl/Symbol.h"
#include "system/obj/Object.h"
#include "AssetTypes.h"
#include "Asset.h"
#include "band3/game/BandUser.h"

// Retail X360 stores the asset table in an STLport hash_map, not a std::map:
// AssetMgr::AddAssets calls
// hashtable<pair<const Symbol,...>, Symbol, hash<Symbol>, ...>::_M_find and
// then writes the mapped value at node+0x8, and both AssetMgr::GetEyebrows and
// FaceHairProvider::FaceHairProvider iterate it as a plain NULL-terminated
// slist walk (STLport 5 keeps every hashtable element in one _M_elems slist,
// so begin()/end() are slist iterators and end() is the NULL node). The three
// empty functor members (_M_hash/_M_equals/_M_get_key) occupy the container's
// first 4 bytes, which is why retail loads the chain head from this+0x2c.
#ifndef RB3_HASH_SYMBOL_DEFINED
#define RB3_HASH_SYMBOL_DEFINED
namespace stlpmtx_std {
_STLP_TEMPLATE_NULL struct hash<Symbol> {
    size_t operator()(const Symbol &s) const { return (size_t)s.Str(); }
};
}
#endif

class BandCharDesc;

class AssetMgr : public Hmx::Object {
public:
    AssetMgr();
    virtual ~AssetMgr();
    static void Init();
    static AssetMgr *GetAssetMgr();
    Asset *GetAsset(Symbol) const;
    bool HasAsset(Symbol) const;
    AssetType GetTypeFromName(Symbol) const;
    void GetEyebrows(std::vector<Symbol> &, Symbol) const;
    int GetEyebrowsCount(Symbol) const;
    Symbol StripFinish(Symbol);
    void ConfigureAssetTypeToIconPathMap();
    void AddAssets();
    void VerifyAssets(const char *);
    void VerifyAssets(const char *, const char *);
    bool EquipAsset(BandCharDesc *, Symbol);
    void EquipAssets(LocalBandUser *, const std::vector<Symbol> &);

    const std::hash_map<Symbol, Asset *> &GetAssets() const { return mAssets; }
    const std::map<int, String> &GetIconPaths() const { return mIconPaths; }

    std::hash_map<Symbol, Asset *> mAssets; // 0x28
    std::map<int, String> mIconPaths; // 0x34
};

static AssetMgr *TheAssetMgr;
