#pragma once
#include "Friend.h"
#include "obj/Data.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "os/ContentMgr.h"
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

// LAYOUT NOTE (2026-07-21, supersedes 2026-07-18 "do NOT re-anchor"): retail
// RB3-360 PlatformMgr IS MsgSource-lineage and the re-anchor PAYS. The 07-18
// note was right that ThePlatformMgr.<field> cross-unit accesses are global
// relocs whose addends objdiff normalizes away — but it missed the receiver
// this-adjust for base-subobject method calls, which is a REAL instruction:
// retail passes ThePlatformMgr+4 to MsgSource::AddSink (`addi r3, rX, 0x4`,
// e.g. StorePanel::Load, fn_82767BA0 = MsgSource::AddSink doing a vbtable
// walk *(vbptr)+4). Our old flat `: public Hmx::Object` emitted `mr r3, rX`
// (+0, Hmx::Object::AddSink) — an un-normalized mismatch at EVERY
// ThePlatformMgr.AddSink/RemoveSink call site in the binary.
//
// Ground truth (Ghidra default_tu5.xex, ThePlatformMgr @ 0x82cc9d1c):
//   class PlatformMgr : public MsgSource, public ContentMgr::Callback
//   (rb3-Wii lineage). MSVC hoists Callback (the vftable base) to primary:
//   Callback vfptr@0x0 | MsgSource@0x4 (vbptr@0x4, mSinks@0x8,
//   mEventSinks@0x10, mExporting@0x18) | members@0x1c | virtual Hmx::Object
//   at tail. Hence IsSignedIn (0x82514988) reads this+0x1c (mSigninMask),
//   CheckForLostConnection reads +0x26 (mConnected), SetScreenSaver
//   (0x8251c180) writes this+0x2c (mScreenSaver).
//
// Whole-binary A/B of this re-base (2026-07-21, twice-reproduced): +4 strict
// (BandUI::Init, BandUI::Terminate, ~Campaign,
// ConnectionStatusPanel::CheckForLostConnection), 0 strict regressions; only
// fuzzy slips are 4 unmapped anonymous fn_ (3 SessionMgr EH funclets + one
// 12-byte address-of-global accessor) — heuristic-pairing noise, not real
// losses. The DC3-only XSocial members are parked at the tail of the member
// block; do not move them back ahead of the retail members.
class PlatformMgr : public MsgSource, public ContentMgr::Callback {
private:
    // Retail RB3-360 layout (Ghidra default_tu5.xex): Callback vfptr@0x0
    // (MSVC hoists the vftable-carrying base to primary), MsgSource@0x4
    // (vbptr@0x4, mSinks@0x8, mEventSinks@0x10, mExporting@0x18), then
    // PlatformMgr members from 0x1c; virtual Hmx::Object base at the tail.
    int mSigninMask;            // 0x1c (retail IsSignedIn 0x82514988 reads this+0x1c)
    int mSigninChangeMask;      // 0x20
    bool mGuideShowing;         // 0x24
    bool mConfirmCancelSwapped; // 0x25
    bool mConnected;            // 0x26 (retail CheckForLostConnection reads +0x26)
    bool mHasHardDrive;         // 0x27 (retail Handle's "has_hard_drive" wire reads this+0x27 as a
                                //       byte — ground truth from Ghidra decompile of Handle
                                //       (0x825152e0), verified 2026-07-31)
    int mRBNMemberPadNum;       // 0x28 (retail Handle's "get_rbn_member_pad_num" wire reads this+0x28
                                //       as a uint — ground truth from Ghidra decompile of Handle,
                                //       verified 2026-07-31. This offset was PREVIOUSLY misattributed
                                //       to mDiskError below, which was only ever an unverified guess —
                                //       no function in the whole-binary report reads/writes mDiskError,
                                //       so relocating it here carries no regression risk.)
    bool mScreenSaver;          // 0x2c (retail SetScreenSaver 0x8251c180 writes this+0x2c)
    PlatformRegion mRegion;     // 0x30 (retail SetRegion writes/reads this+0x30 — ground truth from
                                //       the objdiff TARGET obj for SetRegion itself, verified 2026-07-30;
                                //       NOT from Ghidra like the other offsets on this page)
    JobMgr *mJobMgr;            // 0x34
    bool unk68;                 // 0x38
    bool unk69;                 // 0x39
    // ★ SIZE IS LOAD-BEARING (2026-07-31, lane NCCC f59/opus). The retail member
    // block runs 0x1c..0x47 inclusive — 44 bytes — putting the vtordisp at 0x48
    // and the virtual Hmx::Object base's vfptr at 0x4c. Ground truth: retail
    // PlatformMgr::Handle (0x825152e0) is compiled against that vbase subobject
    // and opens `mr r25,r4` / `subi r3,r25,0x4c` to recover the PlatformMgr*.
    // Our block used to run to 0xa0 (vbase at 0xa4), which biased EVERY
    // this-relative access in Handle by exactly 0x58 = 88 bytes.
    // The DC3-only XSocial block (mHasXSocialPhotoPost, mHasXSocialLinkPost,
    // XOVERLAPPED mOverlapped, int unk4c = 36 bytes) and the Wii-only
    // `Timer mTimer` (48 bytes + 4 pad = 52) were removed here to give back
    // exactly those 88 bytes. Neither is referenced by any COMPILED TU —
    // PlatformMgr_Xbox.cpp (their only user) is not in objects.json.
    // ⚠ Do not add members above the vbase without re-checking this budget.
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
    bool HasHardDrive() { return mHasHardDrive; }
    bool ScreenSaver() { return mScreenSaver; }
    DiskError GetDiskError() const { return mDiskError; }
    int SignInMask() const { return mSigninMask; }
    int RBNMemberPadNum() const { return mRBNMemberPadNum; }
    void QueueEnumJob(Job *);
    void CancelEnumJob(int);
    void Init();
    // Rehomed from 0x28 (see mRBNMemberPadNum above): retail's 0x28 is the RBN pad num.
    // 0x64 is existing tail padding between mHomeMenuWii (0x60) and the 8-aligned mTimer
    // (0x68), so this costs no sizeof change. The true retail offset is still UNKNOWN —
    // SetDiskError is not identified in the retail binary, so this is a placement of
    // convenience, not ground truth.
    DiskError mDiskError;   // 0x40 (placeholder home; NOT verified against retail)
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
    HomeMenu *mHomeMenuWii; // 0x44 — last member; block ends at 0x48 (see the
                            // "SIZE IS LOAD-BEARING" note above).
    // `Timer mTimer` was an INSTANCE member (48B) until 2026-07-31; it pushed the
    // virtual Hmx::Object base from retail's 0x4c out to 0xa4. Retail-360's member
    // block is only 44 bytes (0x1c..0x47), which physically cannot hold a 48-byte
    // Timer, so retail has no such instance member. Demoted to `static` so it costs
    // zero object storage while keeping OvershellPanel's Wii-lineage
    // `ThePlatformMgr.mTimer.Running()` call site compiling unchanged (that function,
    // OvershellPanel::ResolveAutoSignInStates, is unmapped — not a measured symbol).
    static Timer mTimer;
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
