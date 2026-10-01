#pragma once
#include "obj/Object.h"
#include "stl/_vector.h"
#include "utl/Symbol.h"

class LocalBandUser;

/** TU5-only Rock Band Network audition support. Neither class has a body in
    our tree and no surviving source has them; only the surface AuditionSessionPanel
    reaches is declared, read off the retail callers. Method names are ours;
    the bodies live in an unpinned retail block after AccomplishmentGroup
    (0x825EAC38 onward) and are unnamed in our map.

    AuditionSessionBuilder: retail RTTI `.?AVAuditionSessionBuilder@@`, vtable
    0x820B954C (21 slots, i.e. a plain Hmx::Object subclass). */
class AuditionSessionBuilder : public Hmx::Object {
public:
    /** One audition slot. AuditionSessionPanel reads the user at +0 and a
        has-part flag at +0x10. */
    struct Slot {
        LocalBandUser *mUser; // 0x0
        int unk4; // 0x4
        int unk8; // 0x8
        int unkc; // 0xc
        bool mHasPart; // 0x10
    };

    /** retail 0x825EADC8: 0 = empty, 2 = autoplay. */
    int GetSlotState(int slot) const;
    /** retail 0x825EADF8: 0-7 (guitar, bass, keys, pro keys, drums, pro drums,
        vocals, harmony vocals; see AuditionSessionPanel::GetSlotTrackSym). */
    int GetSlotTrack(int slot) const;
    /** retail 0x825EAE10: 0-3 (easy..expert). */
    int GetSlotDifficulty(int slot) const;
    /** retail 0x825EACA8: no `this`; maps a GetSlotTrack value to the
        ControllerType GetFontCharFromControllerType takes. */
    static int SlotTrackToControllerType(int track);
    /** retail 0x825EBE10. */
    void SetSlotAutoplay(int slot, bool autoplay);
    /** retail 0x825EB1A0. */
    void NextSlotDifficulty(int slot);
    /** retail 0x825EB0D0. */
    void NextSlotTrack(int slot);
    /** retail 0x825EBB08, called from AuditionSessionPanel::Enter. */
    void Reset();

    int unk28; // 0x28
    int unk2c; // 0x2c
    std::vector<Slot *> mSlots; // 0x30
    int unk3c; // 0x3c
    int unk40; // 0x40
    int unk44; // 0x44
    /** The panel to notify; AuditionSessionPanel sets it on Enter and clears it
        on Exit. */
    Hmx::Object *mPanel; // 0x48
};

/** Retail global pointer @0x82C7292C. App's init names the object
    "audition_mgr" (SetName, Object vtable slot 16) and stores Symbols at
    +0x48/+0x4c. */
class AuditionMgr : public Hmx::Object {
public:
    int unk28; // 0x28
    int unk2c; // 0x2c
    int unk30; // 0x30
    AuditionSessionBuilder *mBuilder; // 0x34
};

extern AuditionMgr *TheAuditionMgr;
