#include "meta_band/Asset.h"

#include "os/Debug.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols3.h"
#include "meta_band/AssetTypes.h"

Asset::Asset(DataArray *pConfig, int index)
    : mName(gNullStr), mType(kAssetType_None), mGender((AssetGender)0),
      mBoutique((AssetBoutique)0), mPatchable(false), mHidden(false), mIndex(index) {
    MILO_ASSERT(pConfig, 21);
    // Retail emits all six static-Symbol guard blocks consecutively at the top of
    // the function (guard bits 0x01..0x20 in this order) and keeps the six Symbol
    // addresses live in r17-r26 across the whole body -- so the declarations sit
    // here, not at their point of use.
    static Symbol gender("gender");
    static Symbol type("type");
    static Symbol boutique("boutique");
    static Symbol patchable("patchable");
    static Symbol hidden("hidden");
    static Symbol finishes("finishes");

    Symbol name = pConfig->Sym(0);
    mName = name;

    Symbol genderSymbol = gNullStr;
    pConfig->FindData(gender, genderSymbol, false);
    mGender = GetAssetGenderFromSymbol(genderSymbol);

    Symbol typeSymbol = gNullStr;
    pConfig->FindData(type, typeSymbol, true);
    AssetType assetType = GetAssetTypeFromSymbol(typeSymbol);
    mType = assetType;

    Symbol boutiqueSymbol = gNullStr;
    pConfig->FindData(boutique, boutiqueSymbol, false);
    mBoutique = GetAssetBoutiqueFromSymbol(boutiqueSymbol);

    pConfig->FindData(patchable, mPatchable, false);
    pConfig->FindData(hidden, mHidden, false);

    DataArray *finishesArray = pConfig->FindArray(finishes, false);
    if (finishesArray != NULL) {
        if (assetType == 10 || assetType == 2 || assetType == 3) {
            for (int i = 1; i < finishesArray->Size(); i++) {
                Symbol finish = finishesArray->Str(i);
                mFinishes.push_back(finish);
            }
        } else {
            MILO_WARN(
                "(%s) should not have \"finishes\" in ui/customize/assets.dta", name.Str()
            );
        }
    }
}

Asset::~Asset() {}

Symbol Asset::GetDescription() const { return MakeString("%s_desc", mName); }

bool Asset::HasFinishes() { return mFinishes.size() != 0; }

void Asset::GetFinishes(std::vector<Symbol> &v) const {
    for (int i = 0; i < mFinishes.size(); i++) {
        Symbol s = mFinishes[i];
        v.push_back(s);
    }
}

Symbol Asset::GetFinish(int index) const {
    MILO_ASSERT_RANGE(index, 0, mFinishes.size(), 100);
    return mFinishes[index];
}

Symbol Asset::GetHint() const { return MakeString("%s_hint", mName); }
