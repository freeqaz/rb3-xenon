#pragma once
#include "bandobj/BandCharDesc.h"
#include "bandobj/BandCharacter.h"
#include "bandobj/OutfitConfig.h"
#include "bandobj/PatchDir.h"
#include "game/BandUser.h"
#include "meta_band/BandProfile.h"
#include "meta_band/CharData.h"
#include "obj/Msg.h"
#include "utl/Symbol.h"
#include "world/CameraShot.h"

class ClosetPanel;

// Xbox-360-only DLC asset-offer store embedded in ClosetMgr (retail RTTI
// ".?AVAssetStore@@", sizeof 0x4c, ctor @0x825D1A38, methods @0x825D15A8/
// 0x825D1718/0x825D1748 in an unpinned TU). Declaration-only: absent from the
// rb3-Wii oracle; only ClosetMgr::Handle's call sites are matched here.
class AssetStore : public Hmx::Object {
public:
    AssetStore();
    bool HasAssetOffer(Symbol);
    bool HasAnyAssetOffers() const;
    void ShowPurchaseUI(Symbol);
    bool IsDownloading() const { return unk3c != 0; }

    int unk28; // 0x28 - retail ctor inits to 4
    int unk2c; // 0x2c
    int unk30; // 0x30
    void *unk34; // 0x34 - set by ShowPurchaseUI (download object)
    void *unk38; // 0x38 - current offer
    void *unk3c; // 0x3c - checked by is_downloading handler
    int unk40; // 0x40 - offers begin (HasAnyAssetOffers compares 0x40 vs 0x44)
    int unk44; // 0x44 - offers end
    int unk48; // 0x48
};

class ClosetMgr : public MsgSource {
public:
    ClosetMgr();
    virtual DataNode Handle(DataArray *, bool);
    virtual ~ClosetMgr();

    void Poll();
    void PreviewCharacter(bool, bool);
    bool IsCurrentCharacterFinalized();
    void UpdateCurrentCharacter();
    void SetCurrentOutfitPiece(Symbol);
    void UpdateBandCharDesc(BandCharDesc *);
    void FinalizeCharCreatorChanges();
    void FinalizeChanges(bool, bool);
    void ResetCharacterPreview();
    void ForceClosetPoll();
    int GetUserSlot() const;
    void CharacterFinishedLoading();
    bool InNoUserMode() const;
    void SetNoUserMode(bool);
    void SetUser(LocalBandUser *);
    void UpdatePreviousCharacter();
    void ClearUser();
    Symbol GetAssetFromAssetType(AssetType);
    void SetCurrentClosetPanel(ClosetPanel *);
    void ClearCurrentClosetPanel();
    void ResetNewCharacterPreview(Symbol);
    void FinalizeBodyChanges(Symbol);
    void PlayFinalizedSound(bool);
    void MakeProfileDirty();
    void TakePortrait();
    void UpdateCurrentOutfitConfig();
    void FinalizedColors();
    void SetCurrentCharacterPatch(BandCharDesc::Patch::Category, const char *);
    void UpdateCharacterPatch(BandCharDesc::Patch::Category, const char *);
    void RecomposePatches(int);
    void SetPatches();
    void ResetPatches();
    bool IsAlreadyLoaded();
    void SetDefaultColors();
    void HideClothes();
    void ShowClothes();
    CamShot *GetCurrentShot();
    void CycleCamera();
    void GotoArtMakerShot();
    void LeaveArtMakerShot();
    void SetInstrumentType(Symbol);
    void ClearInstrument();
    void SetReturnScreen(Symbol);
    bool HasAssetOffer(Symbol);
    void ShowPurchaseUI(Symbol);
    bool IsCharacterLoading() { return mCharacterLoading; }
    Symbol GetReturnScreen() const { return mReturnScreen; }
    LocalBandUser *GetUser() const { return mUser; }
    ClosetPanel *CurrentClosetPanel() const { return mCurrentClosetPanel; }
    OutfitConfig *GetCurrentOutfitConfig() const { return mCurrentOutfitConfig; }
    BandCharDesc::OutfitPiece *GetCurrentOutfitPiece() const {
        return mCurrentOutfitPiece;
    }
    BandCharDesc *GetPreviewDesc() const { return unk3c; }
    BandProfile *GetProfile() const { return unk28; }
    Symbol GetGender() const { return mGender; }

    DataNode OnMsg(const ProfileSwappedMsg &);

    static void Init();
    static ClosetMgr *GetClosetMgr();

    LocalBandUser *mUser; // 0x1c
    int mSlot; // 0x20
    bool mNoUserMode; // 0x24
    BandProfile *unk28; // 0x28
    CharData *mCurrentCharacter; // 0x2c
    CharData *mPreviousCharacter; // 0x30
    BandCharacter *mBandCharacter; // 0x34
    BandCharDesc *mBandCharDesc; // 0x38
    BandCharDesc *unk3c; // 0x3c - preview desc
    ClosetPanel *mCurrentClosetPanel; // 0x40
    Symbol unk44;
    BandCharDesc::OutfitPiece *mCurrentOutfitPiece; // 0x48
    OutfitConfig *mCurrentOutfitConfig; // 0x4c
    AssetStore mAssetStore; // 0x50 (360: base+0x4c) - sizeof 0x4c
    PatchDescriptor unk50;
    Symbol mReturnScreen; // 0x58
    Symbol mGender; // 0x5c
    bool mCharacterLoading; // 0x60
    bool unk61;
};

#include "obj/Msg.h"

DECLARE_MESSAGE(CharacterFinishedLoadingMsg, "character_finished_loading_msg")
CharacterFinishedLoadingMsg() : Message(Type()) {}
END_MESSAGE

DECLARE_MESSAGE(FinalizedColorsMsg, "finalized_colors_msg")
FinalizedColorsMsg() : Message(Type()) {}
END_MESSAGE