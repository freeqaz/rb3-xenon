#include "meta_band/AssetStore.h"
#include "obj/Data.h"
#include "utl/Symbol.h"

// Retail TU5 0x8266B4C0-0x8266B7D0: the implicit ~StorePurchaseable (a tail
// call to ~Object, referenced from the ctor's base-class unwind), HasAsset,
// the ctor, and the implicit ~AssetOffer / scalar-deleting dtor (vtable
// 0x820D8644 slot 0).

bool AssetOffer::HasAsset(Symbol asset) const {
    for (unsigned int i = 0; i < mAssets.size(); i++) {
        if (mAssets[i] == asset)
            return true;
    }
    return false;
}

// One store index entry: (id "<16 hex digits>") (assets sym sym ...).
AssetOffer::AssetOffer(DataArray *data) {
    static Symbol id("id");
    static Symbol assets("assets");
    const char *idStr;
    data->FindData(id, idStr, true);
    songID = OfferStringToID(idStr);
    DataArray *assetArr = data->FindArray(assets, true);
    for (int i = 1; i < assetArr->Size(); i++) {
        mAssets.push_back(assetArr->Sym(i));
    }
}
