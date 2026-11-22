#pragma once
#include "ui/UIListProvider.h"
#include "obj/Object.h"
#include <vector>

class BandUserMgr;
class LocalBandUser;

class OvershellProfileProvider : public UIListProvider, public Hmx::Object {
public:
    OvershellProfileProvider(BandUserMgr *);
    virtual ~OvershellProfileProvider();
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual RndMat *Mat(int, int, UIListMesh *) const;
    virtual int NumData() const;
    virtual DataNode Handle(DataArray *, bool);

    // Retail X360 has no Wii profile-list API here (ActOnProfile,
    // ActOnProfileConfirmed, Set/GetWiiProfileListMode, GetWiiProfileCount,
    // GetWiiProfileSelectedName): the TU at 0x82667FC8-0x8266849C holds only
    // GetUser, Handle, Text, Reload, the ctor and the dtors, and none of the
    // OvershellSlot handler names that reached them is in band.exe.
    // Retail X360's call site (OvershellSlot::UpdateProfilesList) does a direct
    // `bl` with only `this` in r3 -- no GetUser()/IsLocal()/GetLocalBandUser()
    // chain feeding a second arg. Confirmed by full retail disassembly of both
    // the caller and callee (0x825deec8 / 0x826681a0): zero-arg on X360, unlike
    // a `Reload(LocalBandUser*)` form.
    void Reload();

    // Out of line in OvershellProfileProvider.cpp (retail 0x82667FC8, the
    // TU's first function); OvershellSlot calls it with a `bl`.
    LocalBandUser *GetUser(int i) const;

    // Retail X360 layout (ctor 0x82668278 / Reload 0x826681A0): the
    // BandUserMgr* at 0x2c and the swappable-profile user list at 0x30,
    // sizeof 0x3c.

    BandUserMgr *mBandUserMgr; // 0x2c
    std::vector<LocalBandUser *> mUsers; // 0x30
    // NOTE: no two std::vector<int> Wii-profile
    // lists here (12 bytes each under STLport): RB3-360 retail does NOT have them; the
    // OvershellSlot ctor allocates sizeof(OvershellProfileProvider) == 0x3c
    // (60), which is exactly this class without those two vectors (0x54 - 24).
    // The Wii profile list is a Wii-only feature, so they are omitted here.
};