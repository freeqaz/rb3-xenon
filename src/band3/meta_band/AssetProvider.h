#pragma once
#include "AssetTypes.h"
#include "BandProfile.h"
#include "system/ui/UILabel.h"
#include "system/ui/UIListLabel.h"
#include "system/ui/UIListMesh.h"

class AssetProvider : public UIListProvider, public Hmx::Object {
public:
    AssetProvider(BandProfile *, AssetGender);
    virtual ~AssetProvider();
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual RndMat *Mat(int, int, UIListMesh *) const;
    virtual void UpdateExtendedText(int, int, UILabel *) const;
    virtual Symbol DataSymbol(int) const;
    virtual int NumData() const;
    virtual UIComponent::State ComponentStateOverride(int, int, UIComponent::State) const;

    bool HasAsset(Symbol);
    void Update(AssetType, AssetBoutique);
    static bool SortAssetsByIndex(Symbol, Symbol);

    BandProfile *mProfile; // 0x2c
    std::vector<Symbol> mAssets; // 0x30
    AssetGender mGender; // 0x3c
};

/** Retail-only list provider for premium (licensed) assets. RTTI
    `.?AVPremiumAssetProvider@@`, vtable 0x820DA20C (Hmx::Object vtable
    0x820DA1B4 at +4). Its members sit at the front of AssetProvider.cpp's retail
    range (Mat 0x82670698 .. ??_G 0x82670C68). The dtor at 0x82670BA0 frees one
    4-byte-element vector at +0x2c, so sizeof is 0x38. Retail slots 2 (Mat),
    8 (DataSymbol) and 10 (NumData) are ICF-folded with MakeupProvider::Mat,
    OutfitProvider::DataSymbol and Band::NumActivePlayers; the bodies below are
    those shapes. CustomizePanel::Load allocates it. */
class PremiumAssetProvider : public UIListProvider, public Hmx::Object {
public:
    PremiumAssetProvider(AssetGender);
    virtual ~PremiumAssetProvider();
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual RndMat *Mat(int, int, UIListMesh *) const;
    virtual void UpdateExtendedText(int, int, UILabel *) const;
    virtual Symbol DataSymbol(int) const;
    virtual int NumData() const;

    std::vector<Symbol> mAssets; // 0x2c
};
