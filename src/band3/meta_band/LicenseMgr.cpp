#include "meta_band/LicenseMgr.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "os/ContentMgr.h"
#include "os/Debug.h"
#include "utl/BinStream.h"

template <class K, class V>
BinStream &operator<<(BinStream &bs, const std::hash_map<K, V> &m) {
    bs << m.size();
    for (typename std::hash_map<K, V>::const_iterator it = m.begin(); it != m.end();
         ++it) {
        bs << it->first << it->second;
    }
    return bs;
}

template <class K, class V>
BinStream &operator>>(BinStream &bs, std::hash_map<K, V> &m) {
    // Retail counts an unsigned size down in memory (cmplwi / subic. / stw),
    // the same loop SongUpgradeMgr.cpp's reader uses.
    unsigned int size;
    bs >> size;
    for (; size != 0; size--) {
        K key;
        bs >> key;
        bs >> m[key];
    }
    return bs;
}

LicenseMgr::LicenseMgr() : mCacheNeedsWrite(false) {
    TheContentMgr.RegisterCallback(this, false);
}

bool LicenseMgr::HasLicense(Symbol s) const {
    return mLicenses.find(s) != mLicenses.end();
}

void LicenseMgr::ContentStarted() { mLicenses.clear(); }

bool LicenseMgr::ContentDiscovered(Symbol s) {
    Symbol key = s;
    if (mCachedLicenses.find(key) != mCachedLicenses.end()) {
        std::vector<Symbol> licenses;
        GetLicensesInContent(key, licenses);
        for (std::vector<Symbol>::iterator it = licenses.begin(); it != licenses.end();
             ++it) {
            Symbol lic = *it;
            if (mLicenses.find(lic) == mLicenses.end()) {
                MarkAvailable(lic, key);
            }
        }
        return true;
    } else {
        return false;
    }
}

const char *LicenseMgr::ContentPattern() { return "licenses.dta"; }
const char *LicenseMgr::ContentDir() { return "licenses"; }

// Retail 0x8264e920 (li r11,0; stb r11,0x38(r3); blr), placed right after
// ContentDir; its only caller is BandSongMgr::ClearSongCacheNeedsWrite via
// mLicenseMgr (+0x15c). The name is ours: no symbol survives for this method.
void LicenseMgr::ClearLicenseCacheNeedsWrite() { mCacheNeedsWrite = false; }
// Retail (128 B) is not empty: it seeds an empty license list for a newly
// mounted package that has no cached entry yet. Both keys are built from the
// FIRST argument (r30 = r4 at both Symbol ctors); the second is unused.
// The lookup Symbol is constructed at 0x54 and then copied to 0x50 before
// _M_find -- the shape of an inlined callee taking Symbol BY VALUE, hence
// HasCachedContent (our name; it is inline and leaves no symbol). The empty
// vector is zeroed BEFORE the key Symbol is built, so it is a local declared
// ahead of the operator[] call, not a temporary on the right-hand side.
void LicenseMgr::ContentMounted(const char *contentName, const char *) {
    if (!HasCachedContent(contentName)) {
        std::vector<Symbol> none;
        mCachedLicenses[contentName] = none;
    }
}

void LicenseMgr::ContentLoaded(Loader *loader, ContentLocT ct, Symbol s) {
    DataLoader *d = dynamic_cast<DataLoader *>(loader);
    MILO_ASSERT(d, 0x87);
    DataArray *data = d->Data();
    if (data) {
        AddLicenses(data, d, ct, s);
    } else {
        ClearFromCache(s);
    }
}

bool LicenseMgr::LicenseCacheNeedsWrite() const { return mCacheNeedsWrite; }

bool LicenseMgr::WriteCachedMetadataToStream(BinStream &bs) const {
    bs << mCachedLicenses;
    return true;
}

bool LicenseMgr::ReadCachedMetadataFromStream(BinStream &bs, int) {
    ClearCachedContent();
    bs >> mCachedLicenses;
    return true;
}

void LicenseMgr::ClearCachedContent() { mCachedLicenses.clear(); }

void LicenseMgr::ClearFromCache(Symbol s) {
    mCachedLicenses.erase(mCachedLicenses.find(s));
}

void LicenseMgr::GetLicensesInContent(Symbol s, std::vector<Symbol> &licenses) const {
    std::hash_map<Symbol, std::vector<Symbol> >::const_iterator it =
        mCachedLicenses.find(s);
    if (it != mCachedLicenses.end())
        licenses = it->second;
}

void LicenseMgr::AddLicenses(
    DataArray *data, DataLoader *loader, ContentLocT ct, Symbol s
) {
    std::vector<Symbol> existing;
    GetLicensesInContent(s, existing);
    if (!existing.empty())
        return;
    std::vector<Symbol> new_licenses;
    for (int i = 0; i < data->Size(); i++) {
        Symbol new_license = data->Sym(i);
        MarkAvailable(new_license, s);
        new_licenses.push_back(new_license);
    }
    mCachedLicenses[s] = new_licenses;
    mCacheNeedsWrite = true;
}

void LicenseMgr::MarkAvailable(Symbol s, Symbol) { mLicenses.insert(s); }
