#include "meta_band/CurrentOutfitProvider.h"
#include "bandobj/BandCharDesc.h"
#include "meta_band/Asset.h"
#include "meta_band/AssetMgr.h"
#include "meta_band/ClosetMgr.h"
#include "os/Debug.h"
#include "ui/UILabel.h"
#include "ui/UIListLabel.h"
#include "ui/UIListMesh.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"

CurrentOutfitProvider::CurrentOutfitProvider() { unk20.push_back(none); }
CurrentOutfitProvider::~CurrentOutfitProvider() {}

void CurrentOutfitProvider::Update() {
    unk20.clear();
    ClosetMgr *pClosetMgr = ClosetMgr::GetClosetMgr();
    MILO_ASSERT(pClosetMgr, 0x23);
    BandCharDesc *pPreviewDesc = pClosetMgr->GetPreviewDesc();
    MILO_ASSERT(pPreviewDesc, 0x28);
    unk20.push_back(pPreviewDesc->mOutfit.mTorso.mName);
    unk20.push_back(pPreviewDesc->mOutfit.mLegs.mName);
    unk20.push_back(pPreviewDesc->mOutfit.mFeet.mName);
}

RndMat *CurrentOutfitProvider::Mat(int, int data, UIListMesh *slot) const {
    MILO_ASSERT(data < NumData(), 0x35);
    if (slot->Matches("new_bg"))
        return 0;
    else
        return slot->DefaultMat();
}

void CurrentOutfitProvider::Text(int, int data, UIListLabel *slot, UILabel *label) const {
    MILO_ASSERT(slot, 0x42);
    MILO_ASSERT(label, 0x43);
    // Retail materialises `none` as a FUNCTION-LOCAL static (guard word
    // 0x82E02078 / Symbol 0x82E02074 + an inline ??0Symbol@@QAA@PBD@Z),
    // where the rb3-Wii dev source references the utl/Symbols.h global.
    Symbol sym = DataSymbol(data);
    static Symbol none("none");
    if (sym != none) {
        AssetMgr *pAssetMgr = AssetMgr::GetAssetMgr();
        MILO_ASSERT(pAssetMgr, 0x4F);
        Asset *pAsset = pAssetMgr->GetAsset(sym);
        MILO_ASSERT(pAsset, 0x52);
        if (slot->Matches("name"))
            label->SetTextToken(pAsset->GetName());
        else
            label->SetTextToken(gNullStr);
    }
}

void CurrentOutfitProvider::UpdateExtendedText(int, int i_iData, UILabel *label) const {
    MILO_ASSERT(i_iData < NumData(), 0x61);
    Symbol sym = DataSymbol(i_iData);
    static Symbol none("none");
    if (sym != none) {
        AssetMgr *pAssetMgr = AssetMgr::GetAssetMgr();
        MILO_ASSERT(pAssetMgr, 0x6D);
        Asset *pAsset = pAssetMgr->GetAsset(sym);
        MILO_ASSERT(pAsset, 0x70);
        if (strcmp(label->Name(), "asset_desc_current.lbl") == 0) {
            // Named local: retail reloads the Symbol from its own stack slot
            // (lwz r4, 0x54(r31)) rather than re-deriving it from the sret ptr.
            Symbol desc = pAsset->GetDescription();
            label->SetTextToken(desc);
        } else
            label->SetTextToken(gNullStr);
    }
}

Symbol CurrentOutfitProvider::DataSymbol(int data) const {
    MILO_ASSERT_RANGE(data, 0, NumData(), 0x7F);
    return unk20[data];
}

int CurrentOutfitProvider::NumData() const { return unk20.size(); }
