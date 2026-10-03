#include "os/User.h"
#include "Joypad.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/PlatformMgr.h"
#include "xdk/xapilibi/xbox.h"

#pragma region User

User::User() : mOnlineID(new OnlineID()), mUserGuid(), mMachineID(-1) {
    mUserGuid.Generate();
}

BEGIN_HANDLERS(User)
    HANDLE_EXPR(is_local, IsLocal())
    HANDLE_EXPR(get_player_name, UserName())
    HANDLE_ACTION(reset, Reset())
    HANDLE_EXPR(comes_before, ComesBefore(_msg->Obj<User>(2)))
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

void User::Reset() { mMachineID = -1; }

// Retail 0x825236F0 (100 B): the assert's IsLocal() vcall survives because
// MILO_ASSERT keeps its operand.
void User::SetUserGuid(const UserGuid &id) {
    MILO_ASSERT(!IsLocal(), 0x2F);
    mUserGuid = id;
}

void User::SyncSave(BinStream &bs, unsigned int ui) const {
    bs << mMachineID;
    bs << UserName();
    bs << *mOnlineID;
}

#pragma endregion
#pragma region LocalUser

LocalUser::LocalUser() : mHasOnlineID(0) {}

BEGIN_HANDLERS(LocalUser)
    HANDLE_EXPR(get_pad_num, GetPadNum())
    HANDLE_EXPR(has_online_privilege, HasOnlinePrivilege())
    HANDLE_EXPR(is_signed_in_online, IsSignedInOnline())
END_HANDLERS

int LocalUser::GetPadNum() const { return JoypadGetUsersPadNum(this); }

// Retail 0x82523768 (112 B): reads the XUID for the user's pad.
void LocalUser::UpdateOnlineID() {
    mHasOnlineID = ThePlatformMgr.IsUserSignedIn(this);
    XUID xuid;
    XUserGetXUID(GetPadNum(), &xuid);
    mOnlineID->SetXUID(xuid);
}

// Retail has no fake_controllers override here; it asks the pad directly.
bool LocalUser::IsJoypadConnected() const {
    return JoypadIsConnectedPadNum(GetPadNum());
}

bool LocalUser::HasOnlinePrivilege() const {
    return ThePlatformMgr.UserHasOnlinePrivilege(this);
}
bool LocalUser::IsGuest() const { return ThePlatformMgr.IsUserAGuest(this); }
bool LocalUser::IsSignedIn() const { return ThePlatformMgr.IsUserSignedIn(this); }
bool LocalUser::IsSignedInOnline() const {
    return ThePlatformMgr.IsUserSignedIntoLive(this);
}
bool LocalUser::CanSaveData() const { return IsSignedIn() && !IsGuest(); }
const char *LocalUser::UserName() const { return ThePlatformMgr.GetName(GetPadNum()); }
bool LocalUser::IsLocal() const { return true; }
LocalUser *LocalUser::GetLocalUser() { return this; }
const LocalUser *LocalUser::GetLocalUser() const { return this; }

RemoteUser *LocalUser::GetRemoteUser() {
    MILO_FAIL("Bad Conversion");
    return nullptr;
}

const RemoteUser *LocalUser::GetRemoteUser() const {
    MILO_FAIL("Bad Conversion");
    return nullptr;
}
