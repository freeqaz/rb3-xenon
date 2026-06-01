#pragma once
// Xbox 360 stub for Wii-specific WiiProfile/WiiProfileMgr.
// OvershellSlot.h includes this for types used in Wii-only code paths.
// On X360, these paths are unreachable; forward declarations suffice.
#include "obj/Object.h"
#include "utl/BinStream.h"

class WiiProfile : public Hmx::Object {
public:
    WiiProfile() {}
    virtual ~WiiProfile() {}
    virtual DataNode Handle(DataArray *, bool) { return DataNode(kDataUnhandled, 0); }

    char mSlot;    // 0x1C
    unsigned int mId;    // 0x20
    unsigned int mFlags; // 0x24
    int mHasSeenFirstTimeInstrumentFlags; // 0x28
    char mProfileName[48]; // 0x2c

    static int SaveSize() { return 0; }
};

class WiiProfileMgr {
public:
    WiiProfileMgr() {}
    // Declaration-only (X360 stub): referenced by ported RB3-Wii Wii-friends
    // code paths (MusicLibrary::FilterSetlist) that are unreachable on Xbox 360.
    const char *GetNameForIndex(int) const;

    // Wii-only API used by game code (e.g. OvershellSlot). Declared to mirror the
    // rb3-Wii WiiProfileMgr signatures so 360 game TUs that reference these
    // (unreachable) Wii paths compile. Decl-only; no storage added.
    bool IsIndexValid(int) const;
    int GetIndexForPad(int) const;
    int GetPadForIndex(int) const;
    void SetPadToGuest(int);
    WiiProfile *GetProfileForPad(int);
    bool IsPadAGuest(int) const;
    bool IsPadRegistered(int) const;
    void RemovePad(int);
    bool IsSlotAvailable() const;
    bool IsDeleteQueueFull() const;
};

extern WiiProfileMgr TheWiiProfileMgr;

#include "obj/Msg.h"
// Wii-only message types forward-declared for X360 compilation of RockCentral.h
DECLARE_MESSAGE(DeleteQueueUpdatedMsg, "delete_queue_update")
END_MESSAGE

DECLARE_MESSAGE(DeleteUserCompleteMsg, "delete_user_complete")
END_MESSAGE
