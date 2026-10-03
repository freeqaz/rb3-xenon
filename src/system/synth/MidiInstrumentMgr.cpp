#include "synth/MidiInstrumentMgr.h"

// Retail keeps these in their own TU (0x82716240..0x827163f8, after
// MidiInstrument's last function): UnloadInstrument at 0x82716248 calls
// MidiInstrument::KillAllVoices (0x827145f0) out of line, which a definition in
// MidiInstrument.cpp would have inlined.

MidiInstrumentMgr::MidiInstrumentMgr() : mObjectDir(), mInstrument(0) {}

// Retail 0x82716358 (called from the Synth dtor at 0x82701a10): UnloadInstrument,
// then the ObjPtr (+0xc) and ObjDirPtr (+0x0) member dtors.
MidiInstrumentMgr::~MidiInstrumentMgr() { UnloadInstrument(); }

void MidiInstrumentMgr::SetInstrument(MidiInstrument *inst) { mInstrument = inst; }

// Retail 0x82716248 open-codes the null assignment:
// `if (mObject) { mObject->Release(this); mObject = 0; }`.
void MidiInstrumentMgr::UnloadInstrument() {
    if (mInstrument)
        mInstrument->KillAllVoices();
    mInstrument.ReleaseObjConcrete();
}

void MidiInstrumentMgr::Poll() {
    if (!mInstrument)
        return;
    mInstrument->Poll();
}

// Retail Synth::Init (0x82700270) calls this as an empty body (its call site
// branches to a shared blr).
void MidiInstrumentMgr::Init() {}
