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
    mPlayerName = player_name;
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

BinStream &operator<<(BinStream &bs, const OnlineID &ssm) {
    MILO_ASSERT(ssm.mValid, 0xE6);
    bs << ssm.mXUID;
    return bs;
}
