#pragma once
// Xbox 360 stub for Wii-specific WiiProfile/WiiProfileMgr.
// OvershellSlot.h includes this for types used in Wii-only code paths.
// On X360, these paths are unreachable; forward declarations suffice.
#include "obj/Object.h"
#include "utl/BinStream.h"
class LocalUser;

class WiiProfile : public Hmx::Object {
public:
    WiiProfile() {}
    virtual ~WiiProfile() {}
    virtual DataNode Handle(DataArray *, bool) { return DataNode(kDataUnhandled, 0); }

    char mSlot;    // 0x28
    unsigned int mId;    // 0x2c
    unsigned int mFlags; // 0x30
    int mHasSeenFirstTimeInstrumentFlags; // 0x34
    char mProfileName[48]; // 0x38

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
    void SetIndexRegistered(int, bool);
    bool IsIndexLoaded(int) const;
    bool IsIndexLocked(int) const;
    bool IsIndexRegistered(int) const;
    void RemovePad(int);
    bool IsSlotAvailable() const;
    bool IsDeleteQueueFull() const;
    // ProfileMgr-specific Wii APIs (decl-only for X360 compilation)
    int Count(int) const { return 0; }
    int GetHasSeenFirstTimeInstrumentFlagsForUser(const LocalUser *) const { return 0; }
    void SetHasSeenFirstTimeInstrumentFlagsForUser(const LocalUser *, int, bool) {}
    // CharacterCreatorPanel uses this on X360 in an unreachable Wii sign-in path
    int GetIndexForUser(const LocalUser *) const { return -1; }
    // SaveLoadManager (band3/meta_band) references these Wii save-flow APIs on
    // unreachable Xbox paths. Decl-only; the retail 360 SaveLoadManager does not
    // route through WiiProfileMgr, so these bodies are never expected to match.
    bool NeedsLoading() const;
    void PreLoad();
    bool NeedsSave() const;
    void SetLocked(void *, bool);
    static int SaveSize(int);
};

extern WiiProfileMgr TheWiiProfileMgr;

#include "obj/Msg.h"
// Wii-only message types forward-declared for X360 compilation of RockCentral.h
DECLARE_MESSAGE(DeleteQueueUpdatedMsg, "delete_queue_update")
END_MESSAGE

DECLARE_MESSAGE(DeleteUserCompleteMsg, "delete_user_complete")
END_MESSAGE
