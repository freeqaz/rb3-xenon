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

    // NOTE: name2, genderSymbol2, typeSymbol2, boutiqueSymbol2 and finish2 below
    // are unused copies that compile to nothing. They are here only because,
    // together, they reproduce retail's operand order for the inlined
    // DataArray::Node(i) add in the finishes loop (all five are needed; subsets
    // don't flip it). Same lever as W16-C (NEXTSONGPANEL_COMMUTE_AUDIT_2026-09-14).
    Symbol name = pConfig->Sym(0);
    Symbol name2 = name;
    mName = name;

    Symbol genderSymbol = gNullStr;
    pConfig->FindData(gender, genderSymbol, false);
    Symbol genderSymbol2 = genderSymbol;
    mGender = GetAssetGenderFromSymbol(genderSymbol);

    Symbol typeSymbol = gNullStr;
    pConfig->FindData(type, typeSymbol, true);
    Symbol typeSymbol2 = typeSymbol;
    AssetType assetType = GetAssetTypeFromSymbol(typeSymbol);
    mType = assetType;

    Symbol boutiqueSymbol = gNullStr;
    pConfig->FindData(boutique, boutiqueSymbol, false);
    Symbol boutiqueSymbol2 = boutiqueSymbol;
    mBoutique = GetAssetBoutiqueFromSymbol(boutiqueSymbol);

    pConfig->FindData(patchable, mPatchable, false);
    pConfig->FindData(hidden, mHidden, false);

    DataArray *finishesArray = pConfig->FindArray(finishes, false);
    if (finishesArray != NULL) {
        if (assetType == 10 || assetType == 2 || assetType == 3) {
            for (int i = 1; i < finishesArray->Size(); i++) {
                Symbol finish = finishesArray->Str(i);
                Symbol finish2 = finish;
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

bool Asset::HasFinishes() { return !mFinishes.empty(); }

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
