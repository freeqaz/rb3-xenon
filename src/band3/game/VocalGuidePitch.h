#pragma once
#include "obj/Dir.h"
#include "synth/MidiInstrument.h"

class VocalGuidePitch {
public:
    VocalGuidePitch();
    virtual ~VocalGuidePitch();

    void Load(bool);
    bool IsLoaded() const;
    void FinishLoad();
    void Unload();
    void Poll(float);
    void EnableGuideTrack(int);
    // Retail 0x826C9160 (76 B). ⚠ NAME PROVENANCE: the BODY, the CLASS and the
    // SIGNATURE (void, no arguments) are PROVEN on retail bytes; the SPELLING
    // `StopNote` is a lane-assigned descriptive label with NO oracle backing --
    // rb3-Wii has no such method (it inlines these three statements into
    // EnableGuideTrack) and DC3 has no VocalGuidePitch at all. Do not cite this
    // name as an identification. See docs/decomp/W16FE_*.md.
    void StopNote();
    void Init();
    void Terminate();
    void SetSong(const Symbol &);
    void UpdateTuning(float);
    int GetGuideTrack() const;
    void SetVolume(float);
    void Pause(bool);
    bool IsPaused() const;

    int mGuideTrack; // 0x4
    int unk8; // 0x8 - VocalNote* mCurrNote?
    int mGuidePitch; // 0xc
    MidiInstrument *mInstrument; // 0x10
    ObjDirPtr<ObjectDir> mBank; // 0x14
    float mTuningOffset; // 0x20
    bool mPaused; // 0x24
    int mPitchModifier; // 0x28
};