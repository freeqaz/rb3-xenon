#pragma once
#include "ui/UIListProvider.h"
#include "obj/Object.h"

class BandUserMgr;

enum WiiProfileActResult {
    kWiiProfileActResult_Done = 0,
    kWiiProfileActResult_NeedConfirm = 1,
    kWiiProfileActResult_NewProfile = 2,
    kWiiProfileActResult_EditLogo = 3,
    kWiiProfileActResult_Failed = 4,
    kWiiProfileActResult_SwapProfile = 5
};

class OvershellProfileProvider : public UIListProvider, public Hmx::Object {
public:
    enum WiiProfileListMode {
    };

    OvershellProfileProvider(BandUserMgr *);
    virtual ~OvershellProfileProvider();
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual RndMat *Mat(int, int, UIListMesh *) const;
    virtual int NumData() const;
    virtual DataNode Handle(DataArray *, bool);

    WiiProfileActResult ActOnProfile(int, LocalBandUser *, bool);
    WiiProfileActResult ActOnProfileConfirmed(LocalBandUser *);
    void SetWiiProfileListMode(WiiProfileListMode, bool);
    WiiProfileListMode GetWiiProfileListMode();
    int GetWiiProfileCount(LocalBandUser *) const;
    // Retail X360's call site (OvershellSlot::UpdateProfilesList) does a direct
    // `bl` with only `this` in r3 -- no GetUser()/IsLocal()/GetLocalBandUser()
    // chain feeding a second arg. Confirmed by full retail disassembly of both
    // the caller and callee (0x825deec8 / 0x826681a0): zero-arg on X360, unlike
    // rb3-Wii's `Reload(LocalBandUser*)`.
    void Reload();
    const char *GetWiiProfileSelectedName() const;

    int unk20;
    int unk24;
    int unk28;
    BandUserMgr *unk2c;
    // NOTE: the rb3-Wii DEV oracle carries two std::vector<int> Wii-profile
    // lists here (12 bytes each under STLport). RB3-360 retail does NOT: the
    // OvershellSlot ctor allocates sizeof(OvershellProfileProvider) == 0x3c
    // (60), which is exactly this class without those two vectors (0x54 - 24).
    // The Wii profile list is a Wii-only feature, so they are omitted here.
};