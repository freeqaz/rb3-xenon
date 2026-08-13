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

// Retail 0x825245E8: ReadEndian(&oid, 8) -- XUID is u64 at offset 0 -- then a
// `stb 1` into offset 0x8 (mValid), returning bs. Matches the rb3-Wii oracle
// modulo the platform ID member (Wii reads mPrincipalID, X360 reads mXUID).
BinStream &operator>>(BinStream &bs, OnlineID &oid) {
    bs >> oid.mXUID;
    oid.mValid = true;
    return bs;
}

// sw2 scatter-include (default/OnlineID <- os/Keyboard.cpp)  [lane CY-3]
// Retail 0x82524630/0x82524660 sit inside the OnlineID .text pin
// [0x82524510, 0x82524A28) and are Keyboard{,Un}Subscribe, NOT the Joypad
// pair: JoypadInitCommon stores gJoypadMsgSource to 0x82CCB2BC (read by the
// REAL pair at 0x82524A08/0x82524A38), whereas 0x82524630/0x82524660 read
// 0x82CCB29C, which is never stored anywhere in .text -- Keyboard.cpp's
// gSource, never initialised on X360.  Their caller set (Rnd.cpp
// FailRestartConsole, UIManager::Init/Terminate, CheatsInit/Terminate) is
// exactly the oracle's three KeyboardSubscribe sites.  The two bodies are
// masked-reloc SHAPE TWINS (both `if (g) g->AddSink(o)`), which is why the
// old mis-assignment scored 100.0.  Without this include the corrected rows
// have no base counterpart here and the repair costs -1 matched.
#if !HX_NATIVE
#include "os/Keyboard.cpp"
#endif

// sw2 scatter-include (default/OnlineID <- os/Joypad.cpp)
#define gRev gRev_Joypad
#define gAltRev gAltRev_Joypad
#if !HX_NATIVE  // native: skip X360 scatter/COMDAT-pairing include
#include "os/Joypad.cpp"
#endif
#undef gRev
#undef gAltRev
