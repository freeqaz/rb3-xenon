#pragma once
#include "game/Metronome.h"
#include "game/VocalGuidePitch.h"
#include "synth/Faders.h"
#include "ui/UIPanel.h"
#include "utl/Symbol.h"

class PracticePanel : public UIPanel {
public:
    PracticePanel();
    OBJ_CLASSNAME(PracticePanel);
    OBJ_SET_TYPE(PracticePanel);
    static Hmx::Object *NewObject();
    virtual DataNode Handle(DataArray *, bool);
    virtual ~PracticePanel();
    virtual void Enter();
    virtual void Exit();
    virtual void Poll();
    virtual void Load();
    virtual void Unload();
    virtual bool IsLoaded() const;
    virtual void FinishLoad();

    bool PlayAllTracks() const;
    void OnFadeSongIn(float);
    void OnFadeSongOut(float);
    bool IsDrums() const;
    void MarkGemsAsProcessed();
    bool HasPlayer() const;
    bool IsVocals() const;
    void GetSectionBounds(float &, float &) const;
    void SetPlayAllTracks(bool);
    void SetPitchShiftRatio(float);
    bool InVocalMode() const;
    void PracticeMetronome(Symbol);
    void StopMics();
    void SetUsesHarmony(bool);
    bool GetUsesHarmony() const;
    void EnableGuideTrack(int);
    void IncScorePart();
    void IncPart(int, int &, int &);
    void SyncGuidePart();
    int GetScorePart() const;
    void IncGuidePart();
    int GetGuidePart() const;
    void ToggleGuidePart();
    int GetNumVocalParts() const;
    void SetGuidePitchPaused(bool);
    void UpdateGuideTrack(const Symbol &);
    void TrackIn();
    void TrackOut();
    void PauseGuideTrack();
    void UnpauseGuideTrack();
    void SetInVocalMode();

    // RB3-360 layout (retail ctor Function_82693E60 init map; byte offsets):
    // unk54 sits BETWEEN unk4c and mScorePart. NewObject()'s `li r3, 0x94`
    // (sizeof == 148, not 144) proved retail keeps one more byte-sized field
    // than previously assumed here.
    //
    // mMetronome/unk64 SWAP (2026-08-03, lane NCCC-0803-b2bb/f15): the
    // constructor's off:+4 diff on the `new Metronome()` store in Enter()
    // (target `stw r3, 0x60, r30` vs our `stw r3, 0x64, r30`, a CLEAN 1:1
    // pairing, not LCS misalignment) proves mMetronome is at 0x60, not 0x64.
    // Offset 0x64 is a DIFFERENT field: the ctor loads a value from a fixed
    // global (`lbl_82C71838`) and stores it there instead of a literal 0;
    // that global is `gNullStr` (confirmed via Enter()'s own tail code,
    // which loads the same global immediately before `bl ??0Symbol@@...`
    // as the `const char*` ctor arg). `Symbol()`'s default ctor is exactly
    // `mStr(gNullStr)` (see utl/Symbol.h) -- a single 4-byte pointer field,
    // so 0x64 is a default-constructed `Symbol`, not a scratch int.
    bool mInVocalMode; // 0x3c
    Fader *mFader; // 0x40
    float unk40; // 0x44
    bool mPlayAllTracks; // 0x48
    VocalGuidePitch *mGuidePitch; // 0x4c
    int unk4c; // 0x50
    bool unk54; // 0x54 (moved before mScorePart to match retail)
    int mScorePart; // 0x58
    bool unk55; // 0x5c
    bool unk56; // 0x5d
    bool unk57; // 0x5e
    bool unk58; // 0x5f
    Metronome *mMetronome; // 0x60 (was mistakenly placed at 0x64; see note above)
    Symbol unk64; // 0x64 (default-constructed; mStr==gNullStr)
};

extern PracticePanel *ThePracticePanel;