#pragma once
#include "obj/Object.h"
#include "types.h"

class Instarank : public virtual Hmx::Object {
public:
    const char *Str() const;

private:
    bool mFriendsOnly; // 0x4
    char mToken; // 0x5
    char mStr[255]; // 0x6
    int unk_0x108; // 0x108
    int unk_0x10C; // 0x10c
};
