// DC3 Native Port - PlatformMgr Implementation
// Replaces PlatformMgr_Xbox.cpp - platform-specific methods only
// Non-platform methods are in PlatformMgr.cpp (shared)

#include "os/PlatformMgr.h"
#include "obj/Dir.h"
#include "os/Debug.h"
#include "utl/JobMgr.h"
#include "xdk/XAPILIB.h"
#include <cstring>

// Retail keeps these as PlatformMgr_Xbox.cpp file statics (lbl_82CCA8F0 and
// the four-XUID cache at lbl_82CCA8B8); UpdateSigninState is their only user.
namespace {
    int mSigninSameGuest;
    XUID mXuidCache[4];
}

PlatformMgr::PlatformMgr() {
    mSigninMask = 0;
    mScreenSaver = true;
    mSigninChangeMask = 0;
    mGuideShowing = false;
    mConfirmCancelSwapped = false;
    mConnected = false;
    mRegion = kRegionNone;
    mDiskError = kNoDiskError;
    unk3d = false;
    mRBNCheckInProgress = 0;
    mRBNCheckRerun = 0;
    mJobMgr = new JobMgr(this);
    // NOTE: the DC3-only XOVERLAPPED mOverlapped member was removed from
    // os/PlatformMgr.h on 2026-07-31 (lane NCCC f59 — the member block size is
    // load-bearing for the retail vbase offset). Nothing to zero here anymore.
}

PlatformMgr::~PlatformMgr() {
    delete mJobMgr;
}

// ThreadCallback (retail vtable 0x82088A4C). No Live privileges on the host.
int PlatformMgr::ThreadStart() { return -1; }
void PlatformMgr::ThreadDone(int) {}

// Platform-specific methods (Xbox stubs)
// Retail 0x8251D378 (PlatformMgr_Xbox.cpp) starts with
// SetName("platform_mgr", ObjectDir::Main()); that is the part kept here, so
// the UI scripts' `{platform_mgr ...}` calls find the object. The rest is
// XDK-only and skipped: the utility-drive probe (mHasHardDrive), WinSock and
// XOnlineStartup, the XNotify listener and the RBN member check (a Live job).
// UpdateSigninState runs, as in retail, so the sign-in mask reflects the
// console xdk_shims.cpp models (one local profile on user index 0). Only
// rb3-app calls Init (SystemInit, RB3_NATIVE_SYSTEM_NET_MGRS).
void PlatformMgr::Init() {
    SetName("platform_mgr", ObjectDir::Main());
    UpdateSigninState();
}
void PlatformMgr::PreInit() {}
void PlatformMgr::RegionInit() { SetRegion(kRegionNA); }
void PlatformMgr::Poll() {}
bool PlatformMgr::IsSignedIntoLive(int) const { return false; }
bool PlatformMgr::HasOnlinePrivilege(int) const { return false; }
bool PlatformMgr::IsPadAGuest(int) const { return false; }
void PlatformMgr::ShowFriendsUI(int) {}
void PlatformMgr::ShowOfferUI(int) {}
bool PlatformMgr::ShowPartyUI(int) { return false; }
void PlatformMgr::InviteParty(int) {}
int PlatformMgr::GetOwnerOfGuest(int pad) { return pad; }
bool PlatformMgr::IsEthernetCableConnected() { return true; }
const char *PlatformMgr::GetName(int) const { return "Player"; }
bool PlatformMgr::HasCreatedContentPrivilege() const { return true; }
bool PlatformMgr::HasKinectSharePrvilege() const { return true; }
void PlatformMgr::ShowControllerRequiredUI(Hmx::Object *) {}
bool PlatformMgr::IsInParty() { return false; }
bool PlatformMgr::IsInPartyWithOthers() { return false; }
bool PlatformMgr::ShowFitnessBodyProfileUI(int) { return false; }
void PlatformMgr::SetBackgroundDownloadPriority(bool) {}
void PlatformMgr::DisableXMP() {}
void PlatformMgr::EnableXMP() {}
void PlatformMgr::SetScreenSaver(bool b) { mScreenSaver = b; }
void PlatformMgr::CheckMailbox() {}
void PlatformMgr::RunNetStartUtility() {}
void PlatformMgr::SetNotifyUILocation(NotifyLocation) {}
bool PlatformMgr::PollXSocialCapabilities() { return true; }
bool PlatformMgr::QueryXSocialCapabilities() { return false; }
void PlatformMgr::SmartGlassSend(unsigned long, const DataArray *) {}
bool PlatformMgr::IsSmartGlassConnected() { return false; }
// Retail 0x8251C620 (PlatformMgr_Xbox.cpp), the same body over the XUser*
// shims: the sign-in mask, the per-pad XUID cache and the change masks. A
// signed-in index whose online XUID is zero (a local profile) takes its XUID
// from XUserGetXUID.
void PlatformMgr::UpdateSigninState() {
    XUID oldCache[4];
    memcpy(oldCache, mXuidCache, sizeof(oldCache));
    mSigninSameGuest = 0;
    mSigninChangeMask = 0;
    mSigninMask = 0;
    for (int i = 0; i < 4; i++) {
        if (XUserGetSigninState(i) != 0) {
            XUSER_SIGNIN_INFO info = {};
            mSigninMask |= (1 << i);
            if (XUserGetSigninInfo(i, 2, &info) != 0) {
                info.xuid = 0;
            }
            if (info.xuid == 0) {
                if (XUserGetXUID(i, &info.xuid) != 0) {
                    info.xuid = 0;
                }
            }
            mXuidCache[i] = info.xuid;
        } else {
            mXuidCache[i] = 0;
        }
        if (oldCache[i] != mXuidCache[i]) {
            mSigninChangeMask |= (1 << i);
            if (((oldCache[i] ^ mXuidCache[i]) & 0xff3fffffffffffffULL) == 0) {
                mSigninSameGuest |= (1 << i);
            }
        }
    }
}
void PlatformMgr::SetPadContext(int, int, int) const {}
void PlatformMgr::SetPadPresence(int, int) const {}
void PlatformMgr::SetPadProperty(int, int, unsigned short const *) const {}
void PlatformMgr::EnumerateFriends(int, std::vector<Friend *> &, Hmx::Object *) {}
DWORD PlatformMgr::ShowDeviceSelectorUI(DWORD, DWORD, DWORD, ULARGE_INTEGER, DWORD *, XOVERLAPPED *) { return 0; }
bool PlatformMgr::GetServiceID(const String &, unsigned int &) { return false; }
void PlatformMgr::SignInUsers(int, unsigned long) {}
ShowGamercardResult PlatformMgr::ShowGamercardForPadNum(int, const OnlineID *) { return kShowGamercardResult_Failed; }
