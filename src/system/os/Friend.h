#pragma once
#include "obj/Msg.h"
#include "utl/MemMgr.h"
#include "utl/Str.h"
#include "xdk/xapilibi/xbase.h"

// size 0x20
class Friend {
public:
    Friend();
    void SetName(String name) { mName = name; }
    const char *GetName() const { return mName.c_str(); }

    MEM_OVERLOAD(Friend, 0x1b)

    String mName; // 0x0
    String unkc; // 0x8
    String unk10; // 0x10
    XUID mXUID; // 0x18
};

DECLARE_MESSAGE(FriendsListChangedMsg, "friends_list_changed")
FriendsListChangedMsg(int i) : Message(Type(), i) {}
END_MESSAGE
