#include "os/OnlineID.h"
#include "os/Debug.h"
#include "utl/BinStream.h"

OnlineID::OnlineID() : mValid(false) {}
void OnlineID::Clear() { mValid = false; }
OnlineID::OnlineID(const XUID &id) : mXUID(id), mValid(true) {}

void OnlineID::SetXUID(const XUID &id) {
    mValid = true;
    mXUID = id;
}

void OnlineID::SetPlayerName(const char *player_name) {
    MILO_ASSERT(player_name, 0x34);
    MILO_ASSERT(strlen(player_name), 0x35);
    // Retail OnlineID has no inline player-name String (size 0x10); name is not
    // stored here.
}

XUID OnlineID::GetXUID() const {
    MILO_ASSERT(mValid, 0x6C);
    return mXUID;
}

const char *OnlineID::ToString() const {
    if (mValid) {
        return MakeString("%0x16llx", mXUID);
    } else
        return "";
}

bool OnlineID::operator==(const OnlineID &oid) const {
    if (!mValid || !oid.mValid)
        return mValid == oid.mValid;
    else
        return mXUID == oid.mXUID;
}

BinStream &operator<<(BinStream &bs, const OnlineID &ssm) {
    MILO_ASSERT(ssm.mValid, 0xE6);
    bs << ssm.mXUID;
    return bs;
}

// sw2 scatter-include (default/OnlineID <- os/Joypad.cpp)
#define gRev gRev_Joypad
#define gAltRev gAltRev_Joypad
#include "os/Joypad.cpp"
#undef gRev
#undef gAltRev
