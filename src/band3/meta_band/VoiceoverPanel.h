#pragma once
#include "os/ContentMgr.h"
#include "synth/MoggClip.h"
#include "synth/Faders.h"
#include "ui/UIPanel.h"

class VoiceoverPanel : public UIPanel, public ContentMgr::Callback {
public:
    VoiceoverPanel();
    OBJ_CLASSNAME(VoiceoverPanel);
    OBJ_SET_TYPE_ENGINE(VoiceoverPanel);
    NEW_OBJ(VoiceoverPanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~VoiceoverPanel();
    virtual void Exit();
    virtual void Poll();
    virtual void Unload();
    virtual void PollForLoading();
    virtual const char *ContentDir() { return nullptr; }
    virtual void ContentMounted(const char *, const char *);
    virtual void ContentFailed(const char *);
    virtual bool ShouldHandleFadeOutOnExit() const { return true; }
    virtual bool ShouldFade() { return true; }

    bool LoadingFailed() const { return mLoadingFailed; }
    bool DoneLoading() const {
        return !LoadingFailed() && (mWaitingForLoad || mWaitingForMount);
    }
    MoggClip *GetVoiceover() const { return mVoiceOver; }

    void FadeOutVoiceover();
    void UpdateVoiceoverState();
    void SetVoiceoverFile(const char *, Symbol);
    void SetVoiceoverSymbol(Symbol);
    void PlayVoiceover();
    void SetVolumeOffsetSymbol(Symbol);
    void UpdateVolumeOffset();

    // Offsets below are read off the RETAIL bodies (PlayVoiceover 0x8262F6B0,
    // SetVoiceoverFile 0x8262F5E8, UpdateVolumeOffset 0x8262FC20), not inherited
    // from the rb3-Wii header -- those comments said 0x3c..0x50 and were uniformly
    // 4 bytes stale.
    MoggClip *mVoiceOver; // 0x40
    Fader *mFader; // 0x44
    Symbol mVolumeOffsetSymbol; // 0x48
    bool mWaitingForLoad; // 0x4c
    bool mWaitingForMount; // 0x4d
    bool mLoadingFailed; // 0x4e
    Symbol mDLCName; // 0x50
    String mDLCVoiceoverPath; // 0x54
};