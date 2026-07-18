#pragma once
#include "gesture/Skeleton.h"
#include "obj/Object.h"

class HighFiveGestureFilter : public Hmx::Object {
public:
    // Hmx::Object
    virtual ~HighFiveGestureFilter();
    OBJ_CLASSNAME(HighFiveGestureFilter)
    OBJ_SET_TYPE(HighFiveGestureFilter)

    bool CheckHighFive();
    void Update(const Skeleton *, const Skeleton *);

    NEW_OBJ(HighFiveGestureFilter)

protected:
    HighFiveGestureFilter();

    // NewObject()'s `li r3, 0x30` (sizeof == 48, not 44) proves retail carries
    // 8 bytes of member data past Hmx::Object (0x28), not the 1 byte (bool,
    // padded to 4) this dc3-derived header assumed. mHighFived's true offset
    // is 0x28 (not 0x2c as previously commented -- that number was actually
    // just sizeof, mislabeled as the field offset). unk2c is an unidentified
    // reserved/unused field (source oracle for this Kinect-only 360 class is
    // unavailable -- no rb3-Wii equivalent, retail ctor target VA looks
    // misattributed in scripts/target_symbol_map.json) added purely to make
    // sizeof match; nothing in HighFiveGestureFilter.cpp reads or writes it.
    bool mHighFived; // 0x28
    int unk2c; // 0x2c
};
