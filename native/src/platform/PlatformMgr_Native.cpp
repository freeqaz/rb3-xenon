// DC3 Native Port - PlatformMgr Implementation
// Replaces PlatformMgr_Xbox.cpp - platform-specific methods only
// Non-platform methods are in PlatformMgr.cpp (shared)

#include "os/PlatformMgr.h"
#include "os/Debug.h"
#include "utl/JobMgr.h"

PlatformMgr::PlatformMgr() {
    mSigninMask = 0;
    mScreenSaver = true;
    mSigninChangeMask = 0;
    mGuideShowing = false;
    mConfirmCancelSwapped = false;
    mConnected = false;
    mRegion = kRegionNone;
    mDiskError = kNoDiskError;
    unk69 = false;
    mJobMgr = new JobMgr(this);
    memset(&mOverlapped, 0, sizeof(mOverlapped));
}

PlatformMgr::~PlatformMgr() {
    delete mJobMgr;
}

// Platform-specific methods (Xbox stubs)
void PlatformMgr::Init() {}
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
void PlatformMgr::UpdateSigninState() {}
void PlatformMgr::SetPadContext(int, int, int) const {}
void PlatformMgr::SetPadPresence(int, int) const {}
void PlatformMgr::SetPadProperty(int, int, unsigned short const *) const {}
void PlatformMgr::EnumerateFriends(int, std::vector<Friend *> &, Hmx::Object *) {}
DWORD PlatformMgr::ShowDeviceSelectorUI(DWORD, DWORD, DWORD, ULARGE_INTEGER, DWORD *, XOVERLAPPED *) { return 0; }
bool PlatformMgr::GetServiceID(const String &, unsigned int &) { return false; }
void PlatformMgr::SignInUsers(int, unsigned long) {}
ShowGamercardResult PlatformMgr::ShowGamercardForPadNum(int, const OnlineID *) { return kShowGamercardResult_Failed; }
