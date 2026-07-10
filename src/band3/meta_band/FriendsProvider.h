#pragma once
#include "obj/Object.h"
#include "ui/UIListProvider.h"
#include <vector>

// Retail X360-only class (absent from the Wii dev tree). Reconstructed from
// the retail XEX: default ctor fn_826493A0 (allocated with operator new(0x40)
// in OvershellSlot's ctor fn_825C7188), vtable @ 0x820D4424 with RTTI
// ".?AVFriendsProvider@@" (TypeDescriptor @ 0x82C44328), NumData (UIListProvider
// vtable slot 10) = fn_82657CB8 returning (m0x30 - m0x2c) >> 2.
// It fills OvershellSlot's setup_providers slot 2 (the invite_friends.lst
// provider — the slot the Wii build passes 0 for) and backs the
// kState_InviteFriends -> kState_InviteFriendsDenial check in
// OvershellSlot::UpdateState.
class FriendsProvider : public UIListProvider, public Hmx::Object {
public:
    FriendsProvider();
    virtual ~FriendsProvider();
    // UIListProvider overrides (bodies not yet decompiled — declarations only
    // so no COMDATs leak into including TUs)
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual Symbol DataSymbol(int) const;
    virtual int NumData() const;

    std::vector<int> unk2c; // 0x2c — friend entries; NumData() = size()
    int unk38; // 0x38
    int unk3c; // 0x3c
};
