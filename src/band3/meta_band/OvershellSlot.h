#pragma once
#include "meta/WiiProfileMgr.h"
#include "obj/Object.h"
#include "net_band/DataResults.h"
#include "game/BandUserMgr.h"
#include "game/BandUser.h"
#include "bandobj/OvershellDir.h"
#include "meta_band/SessionMgr.h"
#include "bandobj/BandLabel.h"
#include "meta_band/OvershellProfileProvider.h"
#include "os/JoypadMsgs.h"
#include "os/VirtualKeyboard.h"
#include "tour/TourCharLocal.h"

class OvershellPanel;
class OvershellSlotState;
class OvershellSlotStateMgr;
class PassiveMessageQueue;
class CharProvider;
class SessionUsersProvider;
class OvershellPartSelectProvider;
class CymbalSelectionProvider;
class FriendsProvider;

enum JoinState {
    kMetaJoinNeedsOnline = 0,
    kMetaJoinOK = 1,
    kMetaJoinNeedsMic = 2
};

class PotentialUserEntry {
public:
    LocalBandUser *mUser; // 0x0
    JoinState mJoinState; // 0x4
};

enum OvershellOverrideFlow {
    kOverrideFlow_None = 0,
    kOverrideFlow_SongSettings = 1,
    kOverrideFlow_RegisterOnline = 2
};

class OvershellSlot : public Hmx::Object {
public:
    OvershellSlot(int, OvershellPanel *, OvershellDir *, BandUserMgr *, SessionMgr *);
    virtual ~OvershellSlot();
    virtual DataNode Handle(DataArray *, bool);
    virtual void SetTypeDef(DataArray *);
    virtual class ObjectDir *DataDir();

    void Enter();
    void Poll();
    int GetSlotNum();
    PanelDir *GetPanelDir();
    bool IsHidden() const;
    bool IsLeavingOptions() const;
    Symbol GetCurrentView() const;
    void ClearPotentialUsers();
    void AddPotentialUser(PotentialUserEntry);
    int NumPotentialUsers() const;
    bool LookupUserInJoinList(const LocalBandUser *, JoinState *);
    bool IsValidControllerType(ControllerType);
    BandUser *GetUser() const;
    OvershellSlotState *GetState();
    void ShowState(OvershellSlotStateID);
    void LeaveOptions();
    void SelectPart(TrackType);
    void SelectPartImpl(TrackType, bool, bool);
    void SelectVocalPart(bool);
    void SelectDrumPart(bool);
    void ToggleCymbal(Symbol);
    bool IsCymbalSelected(Symbol);
    void FinishCymbalSelect(bool);
    void DismissCymbalMessage();
    void LeaveDifficultyConfirmation();
    void LeaveChoosePart();
    void CancelSongSettings();
    void LeaveChoosePartWait();
    void LeaveChooseDifficulty();
    void SetOverrideFlowReturnState(OvershellSlotStateID);
    void EndOverrideFlow(OvershellOverrideFlow, bool);
    void AttemptRemoveUser();
    void LeaveReadyToPlay();
    void LeaveSignInWait();
    void ShowEnterWiiSpeakOptions();
    void ShowEnterWiiProfile();
    void AttemptDisconnect();
    void ShowChordBook();
    void PracticeNewSection();
    void ToggleMetronome();
    void KickUser(int);
    void ConfirmKick();
    void LeaveKickConfirmation();
    OvershellSlotState *GenerateCurrentState();
    void RemoveUser();
    void ToggleMuteUser(int);
    void SelectDifficulty(Difficulty);
    void ShowSongOptions();
    void ActOnUserProfile(int);
    void HandleWiiProfileActResult(WiiProfileActResult);
    void ActOnUserProfileConfirm();
    void ShowWiiProfileConfirm();
    void ShowWiiProfilePreconfirm();
    void ShowWiiProfileSwitchConfirm();
    void ShowWiiProfileFail();
    void ShowWiiProfileFailBusy();
    void ShowWiiProfileFailCreate();
    void ShowWiiProfilePostAction();
    void AttemptSwapUserProfile(int);
    void SelectGuestProfile();
    void ShowWiiProfileSwapFail();
    void SetWiiProfileListMode(int, bool);
    OvershellProfileProvider::WiiProfileListMode GetWiiProfileListMode();
    int GetWiiProfileLastIndex();
    void ShowWiiProfileList(int);
    void ShowWiiProfileOptions();
    void ShowWaitWii();
    void LeaveWaitWii();
    void ShowWiiProfileSelector(bool);
    void CancelWiiProfileSelector();
    void FetchLinkingCode();
    void ToggleVocalStyle();
    void Update();
    void ToggleLeftyFlip();
    void ResetSlotCamera();
    void EnableAutohide(bool);
    void SetBlockAllInput(bool);
    void SetInGame(bool);
    void SetInTrackMode(bool);
    void SetView(Symbol);
    void RevertToOverrideFlowReturnState();
    bool IsQuitToken(Symbol) const;
    void BeginOverrideFlow(OvershellOverrideFlow, bool);
    void SetOverrideType(OvershellOverrideFlow, bool);
    void UpdateState();
    void UpdateView();
    void CheckViewOverride(Symbol, bool, Symbol &);
    void UpdateMuteUsersList();
    void UpdateKickUsersList();
    void UpdateGamercardUsersList(); // retail X360-only
    void UpdateProfilesList();
    void UpdateFriendsList(); // retail X360-only
    void UpdatePartSelectList();
    void ViewUserGamercard(int);  // retail X360-only
    void InviteFriend(int);       // retail X360-only
    bool CanChangeSynapseOption(); // retail X360-only
    void ShowProfiles();
    void ShowOnlineOptions();
    void AttemptRegisterOnline();
    void ShowOptionsDrum();
    void CancelLinkingCode();
    void ShowCharEdit(int);
    void AttemptShowCharDelete();
    void ShowChoosePartWait();
    void ToggleHiHatPedal();
    void RefreshHighlightedChar(int);
    void UpdateCharacterList();
    int GetDefaultCharIndex() const;
    void SelectChar(int);
    void ShowEnterFlowPrompt(OvershellSlotStateID);
    bool ConfirmSwapUserProfile();
    void AttemptToggleAutoVocals();
    void ConfirmChooseDiff();
    void RenameCharacter(const char *);
    void DeleteCharacter();
    bool CanEditCharacter(int) const;
    bool IsWiiProfileFull() const;
    bool IsWiiProfileDeleteQueueFull() const;
    const char *GetWiiProfileListSelectedName() const;
    bool SwapUserProfile(LocalBandUser *);
    void AddValidController(ControllerType);
    void AddAutoVocalsValidController(ControllerType);
    void ToggleWiiSpeak();
    void AddUser(LocalBandUser *);
    bool IsValidUser(BandUser *) const;
    WiiProfile *GetUserWiiProfile();
    bool InOverrideFlow(OvershellOverrideFlow) const;

    DataNode OnMsg(const AddLocalUserResultMsg &);
    DataNode OnMsg(const LocalUserLeftMsg &);
    DataNode OnMsg(const RockCentralOpCompleteMsg &);
    DataNode OnMsg(const UIComponentScrollMsg &);
    DataNode OnMsg(const UIComponentSelectMsg &);
    DataNode OnMsg(const VirtualKeyboardResultMsg &);
    DataNode OnMsg(const ButtonDownMsg &);
    DataNode OnMsg(const ButtonUpMsg &);
    DataNode OnMsg(const UserLoginMsg &);

    bool SongOptionsRequired() const { return mSongOptionsRequired; }
    bool InGame() const { return mInGame; }
    bool BlockAllInput() const { return mBlockAllInput; }
    bool AutoHideEnabled() const { return mAutohideEnabled; }
    PassiveMessageQueue *GetMessageQueue() const { return mMessageQueue; }

    // Retail X360 layout: Hmx::Object base is 0x28. mUserNameLabel is a ctor
    // local in retail (never read outside the ctor), and unk28 is declared at
    // the class tail (both confirmed via objdiff member-offset evidence: retail
    // has mOvershell at 0x34, i.e. exactly ONE enum slot between mState and
    // mOvershell). The // offsets below are the retail X360 offsets.
    OvershellSlotStateMgr *mStateMgr; // 0x28
    OvershellSlotState *mState; // 0x2c
    OvershellSlotStateID mOverrideFlowReturnState; // 0x30
    OvershellPanel *mOvershell; // 0x34
    BandUserMgr *mBandUserMgr; // 0x38
    SessionMgr *mSessionMgr; // 0x3c
    int mSlotNum; // 0x40
    std::vector<ControllerType> unk40;
    std::vector<ControllerType> unk48;
    OvershellDir *mOvershellDir; // 0x5c
    bool mAutohideEnabled; // 0x60
    bool mIsLeavingOptions; // 0x61
    bool unk80; // 0x56 (retail packs these two into the 2-byte pad before mCurrentView)
    bool unk81; // 0x57
    Symbol mCurrentView; // 0x64
    bool mBlockAllInput; // 0x68
    bool mInGame; // 0x69
    bool mSongOptionsRequired; // 0x6a
    std::vector<PotentialUserEntry> mPotentialUsers; // 0x6c
    DataResultList mLinkingCodeResultList; // 0x68 (ends 0x80)
    TourCharLocal *mCharForEdit; // 0x90
    unsigned int mCymbalConfiguration; // 0x94
    PassiveMessageQueue *mMessageQueue; // 0x98
    OvershellOverrideFlow mSlotOverrideFlow; // 0x9c
    // Retail provider block (offsets verified from the retail ctor fn_825C7188
    // / dtor fn_825C7A90): mCharProvider 0xa0, mKickUsers 0xa4, mMuteUsers 0xa8,
    // mGamercardUsers 0xac (X360-only), mSwappableProfiles 0xb0, mFriendsProvider
    // 0xb4 (X360-only), mPartSelect 0xb8, mCymbal 0xbc; sizeof == 0xC0
    // (operator new(0xC0) in OvershellPanel::CreateSlots).
    CharProvider *mCharProvider; // 0xa0
    SessionUsersProvider *mKickUsersProvider; // 0xa4
    SessionUsersProvider *mMuteUsersProvider; // 0xa8
    SessionUsersProvider *mGamercardUsersProvider; // 0xac (retail X360-only)
    OvershellProfileProvider *mSwappableProfilesProvider; // 0xb0
    FriendsProvider *mFriendsProvider; // 0xb4 (retail X360-only)
    OvershellPartSelectProvider *mPartSelectProvider; // 0xb8
    CymbalSelectionProvider *mCymbalProvider; // 0xbc
    // NOTE: the Wii-only `OvershellSlotStateID unk28` member that used to live
    // here has been removed — retail X360 sizeof(OvershellSlot) is 0xC0 and the
    // trailing member made ours 0xC4 (objdiff: OvershellPanel::FinishLoad
    // emitted `li r3, 0xc4` for the operator new size where retail has 0xc0).
    // The three Wii-profile-selector methods that referenced it (all absent from
    // the retail X360 .text) now use a file-scope static in OvershellSlot.cpp.
};