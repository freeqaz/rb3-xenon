#pragma once
// Stub header: declares only the BandCharacter surface area used by
// BandDirector / BandWardrobe. Full Wii->360 port deferred (multi-base
// inheritance + Wii-only engine deps; see docs/plans/bandobj-port.md Stage 4).
#include "char/Character.h"
#include "char/CharLipSyncDriver.h"
#include "char/CharLipSync.h"

class BandCharacter : public Character {
public:
    BandCharacter();
    virtual ~BandCharacter();

    // Used by BandDirector.cpp
    Symbol InstrumentType() const { return mInstrumentType; }
    void SetLipSync(CharLipSync *);
    void SetSingalong(float);
    CharLipSyncDriver *GetLipSyncDriver();
    void SetSongOwner(CharLipSyncDriver *);
    bool IsLoading();

    // Storage for InstrumentType() inline.
    Symbol mInstrumentType;
};
