#pragma once
#include "obj/Msg.h"
#include "utl/MemMgr.h"
#include "utl/Str.h"
#include "xdk/xapilibi/xbase.h"

// Retail RB3 Xbox 360 layout (verified against RockCentral::UpdateFriendList,
// fn_824EA128): mName.c_str() is read at mStr (String+0x8) and the 64-bit XUID
// is read at object+0x20 (formatted "%lld"). The dc3-derived layout placed the
// XUID at 0x18, which is the rb3-WII friend-key slot. RB3-360 widens it to a
// u64 XUID at 0x20 (8-byte aligned after the 0xc-byte String members), matching
// rb3-Wii's {mName, mOnline, mGame, key} shape. size 0x28.
class Friend {
public:
    Friend();
    void SetName(String name) { mName = name; }
    const char *GetName() const { return mName.c_str(); }

    MEM_OVERLOAD(Friend, 0x1b)

    String mName; // 0x0
    bool mOnline; // 0xc
    String mGame; // 0x10
    XUID mXUID; // 0x20 (8-byte aligned)
};

DECLARE_MESSAGE(FriendsListChangedMsg, "friends_list_changed")
FriendsListChangedMsg(int i) : Message(Type(), i) {}
END_MESSAGE
