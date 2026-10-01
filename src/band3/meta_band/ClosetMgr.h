#pragma once
#include "bandobj/BandCharDesc.h"
#include "bandobj/BandCharacter.h"
#include "bandobj/OutfitConfig.h"
#include "bandobj/PatchDir.h"
#include "game/BandUser.h"
#include "meta_band/AssetStore.h"
#include "meta_band/BandProfile.h"
#include "meta_band/CharData.h"
#include "obj/Msg.h"
#include "utl/Symbol.h"
#include "world/CameraShot.h"

class ClosetPanel;

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
    // TU5-only (retail 0x82566988, unnamed):
    // out-of-line `mAssetStore.unk34 != 0`, called by CustomizePanel's
    // ButtonDownMsg handler. The name is ours, not retail's.
    bool IsPurchaseUIActive() const;
    void RefreshAssetOffers();
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

    LocalBandUser *mUser; // 0x18
    int mSlot; // 0x1c
    bool mNoUserMode; // 0x20
    BandProfile *unk28; // 0x24
    CharData *mCurrentCharacter; // 0x28
    CharData *mPreviousCharacter; // 0x2c
    BandCharacter *mBandCharacter; // 0x30
    BandCharDesc *mBandCharDesc; // 0x34
    BandCharDesc *unk3c; // 0x38 - preview desc
    ClosetPanel *mCurrentClosetPanel; // 0x3c
    Symbol unk44;
    BandCharDesc::OutfitPiece *mCurrentOutfitPiece; // 0x44
    OutfitConfig *mCurrentOutfitConfig; // 0x48
    AssetStore mAssetStore; // 0x4c (360: base+0x4c) - sizeof 0x4c
    PatchDescriptor unk50;
    Symbol mReturnScreen; // 0xa0
    Symbol mGender; // 0xa4
    bool mCharacterLoading; // 0xa8
    bool unk61;
};

#include "obj/Msg.h"

DECLARE_MESSAGE(CharacterFinishedLoadingMsg, "character_finished_loading_msg")
CharacterFinishedLoadingMsg() : Message(Type()) {}
END_MESSAGE

DECLARE_MESSAGE(FinalizedColorsMsg, "finalized_colors_msg")
FinalizedColorsMsg() : Message(Type()) {}
END_MESSAGE