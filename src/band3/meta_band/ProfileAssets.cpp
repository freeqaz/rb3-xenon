#include "ProfileAssets.h"

#include "meta_band/AccomplishmentManager.h"
#include "meta_band/AssetMgr.h"
#include "meta_band/BandProfile.h"
#include "os/Debug.h"

// Retail-vs-Wii-dev notes (laneBO1):
//  * `AddAsset` in the rb3-Wii DEV oracle opens with an AssetMgr lookup plus a
//    MILO_WARN("Could not find asset %s") arm and a kMaxSymbols_Assets bound
//    check.  Retail has NEITHER -- 0x82655328 goes straight to
//    `if (!HasAsset(...))`.
//  * `HasAsset` in the dev oracle opens with `if (MetaPanel::sUnlockAll) return
//    true;`.  Retail (0x82655148) drops it and starts at DoesAssetHaveSource.
//  * `SaveSize` in the dev oracle prints through sPrintoutsEnabled.  Retail
//    (0x82655090) is eight bytes: `li r3,0x5dc8 ; blr`.
//  * `FakeFill` is not emitted by retail at all.

ProfileAssets::ProfileAssets(BandProfile *profile) : mParentProfile(profile) {
    Clear();
    mSaveSizeMethod = SaveSize;
}

ProfileAssets::~ProfileAssets() {}

void ProfileAssets::Clear() { mAssets.clear(); }

void ProfileAssets::AddAsset(Symbol asset) {
    if (!HasAsset(asset)) {
        mAssets.insert(asset);
        mNewAssets.insert(asset);
        mParentProfile->MakeDirty();
    }
}

bool ProfileAssets::HasAsset(Symbol asset) const {
    if (!TheAccomplishmentMgr->DoesAssetHaveSource(asset))
        return true;
    return mAssets.find(asset) != mAssets.end();
}

bool ProfileAssets::IsNew(Symbol name) const {
    MILO_ASSERT(HasAsset(name), 0x53);
    return mNewAssets.find(name) != mNewAssets.end();
}

void ProfileAssets::SetOld(Symbol name) {
    std::set<Symbol>::iterator it = mNewAssets.find(name);
    if (it != mNewAssets.end()) {
        mNewAssets.erase(it);
        mParentProfile->MakeDirty();
    }
}

void ProfileAssets::GetNewAssets(std::vector<Symbol> &assets, AssetGender gender) const {
    AssetMgr *pAssetMgr = AssetMgr::GetAssetMgr();
    for (std::set<Symbol>::const_iterator it = mNewAssets.begin(); it != mNewAssets.end();
         ++it) {
        Symbol assetName = *it;
        Asset *pAsset = pAssetMgr->GetAsset(assetName);
        AssetGender assetGender = (AssetGender)pAsset->mGender;
        if (assetGender == gender || assetGender == kAssetGender_None) {
            assets.push_back(assetName);
        }
    }
}

int ProfileAssets::GetNumNewAssets(AssetGender gender) const {
    std::vector<Symbol> assets;
    GetNewAssets(assets, gender);
    return assets.size();
}

int ProfileAssets::SaveSize(int) { return 0x5dc8; }

void ProfileAssets::SaveFixed(FixedSizeSaveableStream &stream) const {
    FixedSizeSaveable::SaveStd(stream, mAssets, 0xbb8);
    FixedSizeSaveable::SaveStd(stream, mNewAssets, 0xbb8);
}

void ProfileAssets::LoadFixed(FixedSizeSaveableStream &stream, int) {
    FixedSizeSaveable::LoadStd(stream, mAssets, 0xbb8);
    FixedSizeSaveable::LoadStd(stream, mNewAssets, 0xbb8);
}
