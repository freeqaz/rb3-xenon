#pragma once
#include "game/Metronome.h"
#include "game/VocalGuidePitch.h"
#include "synth/Faders.h"
#include "ui/UIPanel.h"

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
    // unk54 sits BETWEEN unk4c and mScorePart, and retail has NO unk59/unk5c/
    // unk60 (Wii-dev-only) — members end at mMetronome 0x60, vtordisp 0x64,
    // vbase Hmx::Object at 0x68 (RTTI COL vbase offset 0x68).
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
    Metronome *mMetronome; // 0x60
};

extern PracticePanel *ThePracticePanel;