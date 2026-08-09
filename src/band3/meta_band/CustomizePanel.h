#pragma once
#include "bandobj/BandCharDesc.h"
#include "meta_band/AssetProvider.h"
#include "meta_band/AssetTypes.h"
#include "meta_band/BandProfile.h"
#include "meta_band/CharData.h"
#include "meta_band/ClosetMgr.h"
#include "meta_band/CurrentOutfitProvider.h"
#include "meta_band/InstrumentFinishProvider.h"
#include "meta_band/MakeupProvider.h"
#include "meta_band/NewAssetProvider.h"
#include "os/ContentMgr.h"
#include "os/JoypadMsgs.h"
#include "ui/UIComponent.h"
#include "ui/UIPanel.h"
#include <hash_map>

class CustomizePanel : public UIPanel, public ContentMgr::Callback {
public:
    enum CustomizeState {
        kCustomizeState_Invalid = 0,
        // 2 is a preview state, 1, 3 and 4 are not
        kCustomizeState_BrowseTorso = 5,
        kCustomizeState_BrowseLegs = 6,
        kCustomizeState_BrowseFeet = 7,
        kCustomizeState_BrowseHats = 9,
        kCustomizeState_BrowseEarrings = 10,
        kCustomizeState_BrowsePiercings = 11,
        kCustomizeState_BrowseGlassesAndMasks = 12,
        kCustomizeState_BrowseBandanas = 13,
        kCustomizeState_BrowseWrists = 14,
        kCustomizeState_BrowseRings = 15,
        kCustomizeState_BrowseGloves = 16,
        kCustomizeState_HairAndMakeup = 17,
        kCustomizeState_BrowseHair = 18,
        kCustomizeState_BrowseEyebrows = 19,
        kCustomizeState_BrowseFaceHair = 20,
        kCustomizeState_BrowseEyeMakeup = 21,
        kCustomizeState_BrowseLipMakeup = 22,
        kCustomizeState_Instruments = 23,
        kCustomizeState_BrowseGuitars = 24,
        kCustomizeState_BrowseBasses = 25,
        kCustomizeState_BrowseDrums = 26,
        kCustomizeState_BrowseMicrophones = 27,
        kCustomizeState_BrowseKeyboards = 28,
        // ⚠ RANGE GUARD (lane CG-4). Enumerators topping out at 28 give this
        // enum a [dcl.enum]/7 range of [0,31], but states 0x20..0x24 are both
        // constructed ((CustomizeState)0x20 at CustomizePanel.cpp:46,:905) and
        // TESTED (`mCustomizeState == 0x21/0x22/0x23` at :796,:813,:824 and
        // `case 0x20..0x24` at :193,:897-:902) -- all outside [0,31] and hence
        // foldable by a compiler that trusts the declared range. Same defect
        // class as the missing Character::DrawMode 4 that killed char shadows.
        // A RANGE GUARD, not a recovered name: the rb3-Wii oracle stops at 28
        // too, so it cannot supply the real enumerators for 0x20..0x24.
        // X360-neutral: an enumerator emits no code (A/B measured Δ0).
        kCustomizeState_MaxUsed = 0x24
    };
    CustomizePanel();
    OBJ_CLASSNAME(CustomizePanel);
    OBJ_SET_TYPE(CustomizePanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~CustomizePanel();
    virtual void Enter();
    virtual void Exit();
    virtual bool Unloading() const;
    virtual void Poll();
    virtual void Load();
    virtual void Unload();
    virtual bool IsLoaded() const;
    virtual void FinishLoad();
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void ContentStarted();
    virtual void ContentDone();
    virtual const char *ContentDir() { return nullptr; }

    void EnableFaceHair();
    void DisableFaceHair();
    void SetCustomizeState(CustomizeState);
    void SetPendingState(CustomizeState);
    void SetPatchMenuReturnState(CustomizeState);
    bool InPreviewState() const;
    bool InClothingState() const;
    void UpdateNewAssetProvider();
    void UpdateCurrentOutfitProvider();
    void RefreshNewAssetsList();
    void UpdateAssetProvider();
    AssetType GetAssetTypeFromCurrentState();
    void RefreshAssetsList();
    void UpdateMakeupProvider(Symbol);
    void SetCurrentBoutique(Symbol);
    Symbol GetCurrentBoutique();
    void ClearCurrentBoutique();
    Symbol GetWearing();
    Symbol StripFinish(Symbol);
    void RefreshCurrentOutfitList();
    void PreviewAsset(Symbol);
    void PreviewFinish(Symbol);
    void SelectAsset(Symbol);
    void ShowLockedDialog();
    void ChooseFinish();
    void ChooseColors();
    void GotoCustomizeClothingScreen();
    Symbol GetCurrentMakeup(Symbol);
    void ClearCurrentMakeupIndex();
    void SetCurrentMakeupIndex(int);
    void PreviewMakeup(Symbol);
    bool HasNewAssets();
    bool AssetProviderHasAsset(Symbol);
    void SetupCurrentOutfit(Symbol);
    bool HasPatch();
    void RotatePatch(int);
    int HasLicense(Symbol);
    Symbol GetAssetShot(Symbol);
    void SetFocusComponent(CustomizeState, Symbol);
    void StoreFocusComponent();
    UIComponent *GetFocusComponent();
    void MovePatch(float, float);
    void ScalePatch(float, float);
    void RefreshPatchEdit();
    void SetIsWaitingToLeave(bool);
    void ClearAssetPatchData();
    bool IsCurrentAssetPatchable();
    const char *GetPlacementMeshFromCurrentCamShot();
    void PreparePatchEdit(BandCharDesc::Patch::Category);
    void PrepareAssetPatchEdit();
    void SetCurrentCharacterPatch();
    void FinishPatchEdit();
    DataNode SavePrefab(const char *);
    void SetupAssetPatchData(Symbol);
    bool IsAssetPatchable();
    CharData *GetCharData() const { return mCharData; }
    CustomizeState GetCustomizeState() const { return mCustomizeState; }
    CustomizeState GetPatchMenuReturnState() const { return mPatchMenuReturnState; }

    DataNode LeaveState(bool);
    DataNode LeaveCustomizePanel();
    DataNode OnMsg(const SigninChangedMsg &);
    DataNode OnMsg(const ButtonDownMsg &);
    DataNode OnMsg(const UIComponentScrollMsg &);

    static CustomizeState sBackStates[];
    NEW_OBJ(CustomizePanel);
    static void Init() { REGISTER_OBJ_FACTORY(CustomizePanel); }

    CustomizeState mCustomizeState; // 0x40
    CustomizeState mPendingState; // 0x44
    CustomizeState mPatchMenuReturnState; // 0x48
    // Retail keys this with an STLport hash_map, not the Wii build's std::map.
    // ??0CustomizePanel@@QAA@XZ does `addi r3, r30, 0x4c` and then
    // `bl ??0?$hash_map@...@stlpmtx_std@@QAA@XZ` (retail 0x8255D480, whose body
    // is `li r4, 0x64` -> _M_initialize_buckets(100) -- a hashtable ctor, not an
    // _Rb_tree one).  A std::map ctor has no call at all here: STLport inlines
    // the rb-tree header init, which is why our side emitted the
    // `std r29,0x0(r11) / std r29,0x8(r11)` pair the target lacks.
    //
    // hash_map is 0x1c, map is 0x18, so this reaches mClosetMgr@0x68 on its own.
    // The `int unk64; // 0x64` that used to sit here was a fabricated pad -- an
    // earlier pass measured retail's +4 correctly but attributed it to the
    // members rather than to the container, and bought it with a spare word
    // (before that, with a /DRB3_MAP_0x1C flag that fattened every map in the
    // TU).  Nothing in the tree ever referenced unk64.  It MUST be deleted in
    // the same edit as this swap: keeping it would push mClosetMgr to 0x6c and
    // the +4 fix would read as a regression on its own.
    std::hash_map<int, UIComponent *> mFocusComponents; // 0x4c
    ClosetMgr *mClosetMgr; // 0x68
    LocalBandUser *mUser; // 0x6c
    BandProfile *mProfile; // 0x70
    CharData *mCharData; // 0x74
    BandCharDesc *mPreviewDesc; // 0x78
    NewAssetProvider *mNewAssetProvider; // 0x7c
    CurrentOutfitProvider *mCurrentOutfitProvider; // 0x80
    AssetProvider *mAssetProvider; // 0x84
    AssetProvider *mPremiumAssetProvider; // 0x88
    MakeupProvider *mMakeupProvider; // 0x8c
    InstrumentFinishProvider *mInstrumentFinishProvider; // 0x90
    AssetBoutique mCurrentBoutique; // 0x94
    Symbol unk90; // 0x98
    int mCurrentMakeupIndex; // 0x9c
    bool mUnlockedFacePaint; // 0xa0
    bool mUnlockedTattoos; // 0xa1
    bool mRefreshingContent; // 0xa2
    bool mWaitingToLeave; // 0xa3
    BandCharDesc::Patch::Category mPatchCategory; // 0xa4
    String mPatchName; // 0xa8
    // RB3-360: no trailing mShowAssetTokens — retail members end after
    // mPatchName (RTTI: vtordisp 0xB4, vbase Hmx::Object at 0xB8). The
    // Wii-dev-only bool pushed the vbase to 0xBC and biased every
    // r26-relative displacement in Handle. Its only users were the two
    // RB3_STRIP_CHEAT_HANDLERS-stripped cheat arms + CheatToggleAssetTokens.
};