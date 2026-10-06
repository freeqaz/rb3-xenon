#pragma once
#include "obj/Object.h"
#include "os/ThreadCall.h"
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
        /** Autoplay flag: retail ctor `stb 0,4`, GetSlotState `lbz 4` (nonzero
            => state 2), SetSlotAutoplay `stb r5,4`. A byte, not an int. */
        bool mAutoplay; // 0x4
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
    /** retail ctor 0x825EC118: `stb 0,0x4c`, then an 8-iteration loop storing
        1 into each byte of 0x4d..0x54. Retail allocates this class with
        `li r3,0x58` (site 0x82560D34) -- these 9 bytes are what carry the object
        from our former 0x4c to retail's 0x58. */
    bool unk4c; // 0x4c
    bool unk4d[8]; // 0x4d
};

/** Retail global pointer @0x82C7292C. App's init names the object
    "audition_mgr" (SetName, Object vtable slot 16) and stores Symbols at
    +0x48/+0x4c.

    Layout from retail bytes (lane W16-OP):
      * RTTI: `.?AVAuditionMgr@@` has bases Hmx::Object (offset 0) and
        ThreadCallback (PMD m=40, i.e. offset 0x28) -- two vtables, the primary
        0x8209803C (22 slots = Object's 21 + one new virtual) and the
        ThreadCallback one 0x8209802C (3 slots: dtor thunk, ThreadStart,
        ThreadDone). The old `int unk28` at 0x28 was that second vfptr.
      * dtor 0x825618D8: stores both vtables, releases the DataArray at 0x44,
        restores ThreadCallback's vtable at 0x28, then ~Hmx::Object.
      * static-init 0x82C3FD60 constructs a retail object with the ctor fully
        inlined and initialises every member listed below (0x54 is the only
        word it leaves alone).
      * ThreadStart (0x82561168, this = the 0x28 subobject) reads the state at
        +4 (= 0x2c) and the Symbol at +0x20 (= 0x48); ThreadDone (0x825637A8)
        adjusts `this - 0x28` and calls back into AuditionMgr.
    Retail does not override ClassName/SetType (slots 4/5 are Object's) but
    does override Handle (slot 6, 0x825638B0). */
class AuditionMgr : public Hmx::Object, public ThreadCallback {
public:
    virtual ~AuditionMgr();
    virtual DataNode Handle(DataArray *, bool);
    /** Primary slot 21, 0x82563708: the one virtual AuditionMgr introduces.
        A void(void) state-machine step over mState/0x30/0x3c, so named Poll;
        the name is not attested by any string or symbol. */
    virtual void Poll();
    virtual int ThreadStart();
    virtual void ThreadDone(int);

    /** 0x825610B8, called once from App::App right after TheUI's Init.
        Names the object "audition_mgr" (Object slot 16) and fills the two
        Symbols at 0x48/0x4c. */
    void Init();

    int mState; // 0x2c -- switched on by slot 21, ThreadStart and ThreadDone
    int unk30; // 0x30
    AuditionSessionBuilder *mBuilder; // 0x34
    int unk38; // 0x38
    /** slot 21 calls 0x825614F8 on it when non-null. */
    void *unk3c; // 0x3c
    bool unk40; // 0x40
    DataArray *unk44; // 0x44 -- Release()d and nulled by the dtor
    Symbol unk48; // 0x48
    Symbol unk4c; // 0x4c
    bool unk50; // 0x50
    bool unk51; // 0x51
    int unk54; // 0x54
    int unk58; // 0x58
    int unk5c; // 0x5c
    bool unk60; // 0x60
};

extern AuditionMgr *TheAuditionMgr;
