#pragma once
#include "obj/Object.h"
#include "ui/UIListProvider.h"
#include <vector>

// Retail X360-only class (absent from the Wii dev tree). Reconstructed from
// the retail XEX: default ctor fn_826493A0 (allocated with operator new(0x40)
// in OvershellSlot's ctor fn_825C7188), RTTI ".?AVFriendsProvider@@",
// NumData (UIListProvider vtable slot 10) = fn_82657CB8 returning
// (m0x30 - m0x2c) >> 2.
//
// (CORRECTED, lane W16-HEADERTRUTH, tools/vtable_claim_audit.py: this comment
// used to cite "vtable @ 0x820D4424" and "TypeDescriptor @ 0x82C44328". BOTH
// were wrong. 0x820D4424 IS a vtable -- but its ??_R4 Complete Object Locator
// names .?AVArtistCmp@@, a different class; and 0x82C44328 is not a descriptor
// at all, it lies in .text. The real values, decoded from retail RTTI:
//     TypeDescriptor  0x82c74a88   (in .data, NOT .rdata)
//     vtable 0x820d73bc  COL 0x821e581c  subobject offset 0x0  (primary)
//     vtable 0x820d7364  COL 0x821e5870  subobject offset 0x4
// Two vtables is what the UIListProvider + Hmx::Object multiple inheritance
// predicts, so the class shape below is unaffected -- only the citations were
// bogus.)
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

    // Declared but not yet decompiled (called from OvershellSlot::UpdateFriendsList
    // -- retail's target asm loads mFriendsProvider and does a direct `bl` to a
    // no-arg method whose ICF-folded symbol name in the target map is a
    // same-shaped UIList method, not a real name). No definition needed here:
    // objdiff compares .obj files, not a linked binary, so an undefined external
    // reference is fine, matching the OvershellProfileProvider::Reload precedent
    // (HX_NATIVE-only body) elsewhere in this header family.
    void Reload();

    std::vector<int> unk2c; // 0x2c — friend entries; NumData() = size()
    int unk38; // 0x38
    int unk3c; // 0x3c
};
