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
    int size;
    bs >> size;
    for (int i = 0; i < size; i++) {
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
void LicenseMgr::ContentMounted(const char *, const char *) {}

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
