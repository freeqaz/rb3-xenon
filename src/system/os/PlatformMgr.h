#pragma once
#include "Friend.h"
#include "obj/Data.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "os/OnlineID.h"
#include "os/Timer.h"
#include "os/User.h"
#include "stl/_vector.h"
#include "utl/JobMgr.h"
#include "xdk/XSOCIAL.h"

// Wii-origin HomeMenu stub — referenced by ported RB3-Wii game code
// (meta_band/OvershellPanel). On Xbox 360 this object is never instantiated;
// mHomeMenuWii points to a zeroed placeholder so the ported code compiles.
struct HomeMenu {
    bool mHomeMenuActive; // 0x0
    bool mForcedHomeMenu; // 0x1
};

// Callback type used by OvershellPanel::Init() on Wii.
typedef bool SignInUserCallbackFunc(User *, unsigned long);

enum DiskError {
    kNoDiskError,
    kDiskError,
    kWrongDisk,
    kFailedChecksum
};

enum PlatformRegion {
    kRegionNone,
    kRegionNA,
    kRegionEurope,
    kRegionJapan,
    kNumRegions
};

enum NotifyLocation {
    kNotify0,
    kNotify1,
    kNotify2
};

enum QuitType {
    kQuitNone,
    kQuitShutdown,
    kQuitRestart,
    kQuitMenu,
    kQuitDataManager
};

enum ShowGamercardResult {
    kShowGamercardResult_Success = 0,
    kShowGamercardResult_Failed = -1,
    kShowGamercardResult_PrivilegeFailed = -2,
    kShowGamercardResult_NotSignedIn = -3,
    kShowGamercardResult_Offline = -1
};

typedef bool XCallbackFunc(unsigned long &);

class PlatformMgr : public Hmx::Object {
private:
    bool mHasXSocialPhotoPost; // 0x2c
    bool mHasXSocialLinkPost; // 0x2d
    XOVERLAPPED mOverlapped; // 0x30
    int unk4c; // 0x4c - ptr to something
    int mSigninMask; // 0x50
    int mSigninChangeMask; // 0x54
    bool mGuideShowing; // 0x58
    bool mConfirmCancelSwapped; // 0x59
    bool mConnected; // 0x5a
    bool mScreenSaver; // 0x5b
    PlatformRegion mRegion; // 0x5c
    DiskError mDiskError; // 0x60
    JobMgr *mJobMgr; // 0x64
    bool unk68; // 0x68
    bool unk69; // 0x69
    DataNode OnSignInUsers(DataArray *);

public:
    bool unkce6b; // TODO: needs correct X360 offset (Wii 0xce6b = content maturity flag)
    bool mNetworkPlay; // RB3 GameMode writes online_play_required here
    bool mIsRestarting; // RB3 RestartGameMsg::Dispatch sets this via SetIsRestarting
    void SetIsRestarting(bool b) { mIsRestarting = b; }
    // Hmx::Object
    virtual ~PlatformMgr();
    virtual DataNode Handle(DataArray *, bool);

    static XCallbackFunc *sXShowCallback;

    PlatformMgr();
    PlatformRegion GetRegion() const;
    bool IsAnyUserSignedIntoLive() const;
    bool IsSignedIntoLive(int) const;
    bool IsSignedIn(int) const;
    bool IsUserSignedIn(const LocalUser *) const;
    bool IsPadNumSignedIn(int) const;
    bool HasPadNumsSigninChanged(int) const;
    bool HasUserSigninChanged(const LocalUser *) const;
    bool IsUserSignedIntoLive(const LocalUser *) const;
    bool HasOnlinePrivilege(int) const;
    bool UserHasOnlinePrivilege(const LocalUser *) const;
    bool IsUserAGuest(const LocalUser *) const;
    bool IsPadAGuest(int) const;
    bool IsGuestOnlineID(const OnlineID *) const;
    void ShowUserFriendsUI(const LocalUser *);
    void ShowFriendsUI(int);
    void ShowOfferUI(const LocalUser *);
    void ShowOfferUI(int);
    bool ShowUserPartyUI(const LocalUser *);
    bool ShowPartyUI(int);
    void InviteUserParty(const LocalUser *);
    void InviteParty(int);
    LocalUser *GetOwnerUserOfGuestUser(LocalUser *);
    int GetOwnerOfGuest(int);
    void SetRegion(PlatformRegion);
    void SetDiskError(DiskError);
    void DebugFakeSigninChangeMsg(int);
    bool IsEthernetCableConnected();
    const char *GetName(int) const;
    void GetOnlineID(int, OnlineID *) const;
    bool HasCreatedContentPrivilege() const;
    bool HasKinectSharePrvilege() const;
    void ShowControllerRequiredUI(Hmx::Object *);
    bool IsInParty();
    bool IsInPartyWithOthers();
    bool ShowFitnessBodyProfileUI(int);
    void SetBackgroundDownloadPriority(bool);
    void DisableXMP();
    void EnableXMP();
    void SetScreenSaver(bool);
    void CheckMailbox();
    void RunNetStartUtility();
    void SetNotifyUILocation(NotifyLocation);
    bool PollXSocialCapabilities();
    bool QueryXSocialCapabilities();
    void SmartGlassSend(unsigned long, const DataArray *);
    bool IsSmartGlassConnected();
    void UpdateSigninState();
    void SetPadContext(int, int, int) const;
    void SetPadPresence(int, int) const;
    void SetPadProperty(int, int, unsigned short const *) const;
    void EnumerateFriends(int, std::vector<Friend *> &, Hmx::Object *);
    void Poll();

    bool GuideShowing() { return mGuideShowing; }
    // Wii-origin entry points referenced by ported RB3-Wii game code
    // (meta_band/MusicLibrary). Declaration-only: the Xbox 360 retail engine
    // has no Wii home-menu / Wii online-restriction concept, so there is no
    // body to link against on this platform — these exist purely so the ported
    // TU compiles. Append-only; does not alter existing PlatformMgr layout.
    void SetHomeMenuEnabled(bool);
    bool IsOnlineRestricted();
    // Profanity-check entry points referenced by ported RB3-Wii game code
    // (meta_band/EditSetlistPanel). Declaration-only, append-only — does not
    // alter the existing PlatformMgr layout. The retail Xbox 360 build compiled
    // these references into EditSetlistPanel; the link target lives in the
    // platform layer (not part of this TU's match).
    bool IsCheckingProfanity() const;
    void *StartProfanity(const unsigned short **, int, char *, Hmx::Object *);
    bool IsConnected() { return mConnected; }
    bool ScreenSaver() { return mScreenSaver; }
    DiskError GetDiskError() const { return mDiskError; }
    int SignInMask() const { return mSigninMask; }
    void QueueEnumJob(Job *);
    void CancelEnumJob(int);
    void Init();
    void RegionInit();
    void PreInit();
    DWORD
    ShowDeviceSelectorUI(DWORD, DWORD, DWORD, ULARGE_INTEGER, DWORD *, XOVERLAPPED *);
    bool GetServiceID(const String &, unsigned int &);
    void SignInUsers(int, unsigned long);
    ShowGamercardResult ShowGamercardForPadNum(int, const OnlineID *);
    ShowGamercardResult ShowGamercard(class LocalUser *, const OnlineID *);
    bool CanSeeUserCreatedContent(const OnlineID *) const;
    // Wii-origin entry points referenced by ported meta_band/OvershellPanel code.
    // Declaration-only; append-only — does not alter existing PlatformMgr layout.
    void RegisterSignInserCallback(SignInUserCallbackFunc *);

    // Wii-origin data members referenced by ported meta_band/OvershellPanel.
    // Added at the end to avoid disturbing the existing X360 layout; these are
    // never accessed in a matching TU.
    HomeMenu *mHomeMenuWii; // Wii: pointer to Wii home-menu object
    Timer mTimer;           // Wii: net-start-utility retry timer
};

extern PlatformMgr ThePlatformMgr;

Symbol PlatformRegionToSymbol(PlatformRegion);
PlatformRegion SymbolToPlatformRegion(Symbol);

// arg here is a bool
DECLARE_MESSAGE(DiskErrorMsg, "disk_error")
DiskErrorMsg() : Message(Type(), 0) {}
END_MESSAGE

DECLARE_MESSAGE(SigninChangedMsg, "signin_changed")
SigninChangedMsg(unsigned long u1, unsigned long u2) : Message(Type(), u1, u2) {}
int GetMask() const { return mData->Int(2); }
int GetChangedMask() const { return mData->Int(3); }
END_MESSAGE

DECLARE_MESSAGE(StorageChangedMsg, "storage_changed")
END_MESSAGE

DECLARE_MESSAGE(PartyMembersChangedMsg, "party_members_changed")
END_MESSAGE

DECLARE_MESSAGE(EnumerateMessagesCompleteMsg, "enumerate_messages_complete")
END_MESSAGE

// Wii DWC profanity-check result message — referenced by Jobs_Wii.h.
// On Xbox 360 this message is never sent; forward-declare to satisfy the type.
DECLARE_MESSAGE(DWCProfanityResultMsg, "dwc_profanity_result_msg")
DWCProfanityResultMsg() : Message(Type()) {}
bool IsProfane() const { return mData->Int(2); }
bool Success() const { return mData->Int(2); }
END_MESSAGE

// Wii platform-mgr op-complete message — referenced by RockCentralJobs.h / the
// friend-list jobs. On Xbox 360 this is never sent; declared to satisfy the type.
DECLARE_MESSAGE(PlatformMgrOpCompleteMsg, "platform_mgr_op_complete")
PlatformMgrOpCompleteMsg(int i) : Message(Type(), i) {}
bool Success() const { return mData->Int(2); }
END_MESSAGE
