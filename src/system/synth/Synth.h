#pragma once
#include "ADSR.h"
#include "SynthSample.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "os/Platform.h"
#include "rndobj/Overlay.h"
#include "synth/ADSR.h"
#include "synth/ByteGrinder.h"
#include "synth/FxSendPitchShift.h"
#include "synth/Mic.h"
#include "synth/MicClientMapper.h"
#include "synth/MidiSynth.h"
#include "synth/Sfx.h"
#include "synth/Sound.h"
#include "synth/StandardStream.h"
#include "synth/Stream.h"
#include "utl/Str.h"

enum FXMode {
    kFXModeOff,
    kFXModeRoom,
    kFXModeSmallStudio,
    kFXModeMedStudio,
    kFXModeLargeStudio,
    kFXModeHall,
    kFXModeSpace,
    kFXModeEcho,
    kFXModeDelay,
    kFXModePipe,
    kFXModeChorus,
    kFXModeWah,
    kFXModeFlanger
};

class StreamReader;
class TranscodableMixerOutput;

class Synth : public Hmx::Object, public RndOverlay::Callback {
    friend void SynthTerminate();

public:
    Synth();
    // Hmx::Object
    virtual DataNode Handle(DataArray *, bool);
    virtual void PreInit() {}
    virtual void Init();
    virtual void InitSecurity();
    virtual void SetDolby(bool, bool) {} // 0x64
    virtual bool IsUsingDolby() const { return false; }
    virtual bool Fail() { return false; }
    virtual void Terminate();
    virtual void Poll();
    virtual bool HasPendingVoices() { return false; }
    virtual void SetFXMode(int, FXMode) {}
    virtual FXMode GetFXMode(int) const { return kFXModeOff; }
    virtual void SetFXVolume(int, float) {}
    virtual float GetFXVolume(int) const { return 0; }
    virtual void SetFXDelay(int, float) {}
    virtual float GetFXDelay(int) const { return 0; }
    virtual void SetFXFeedback(int, float) {}
    virtual float GetFXFeedback(int) const { return 0; }
    virtual void SetFXChain(bool) {}
    virtual bool GetFXChain() const { return false; }
    virtual void SetChatVoiceGain(int, float) {}
    virtual float GetChatVoiceGain(int) { return 1; }
    virtual Mic *GetMic(int idx) { return mMics[idx]; }
    virtual void SetMicFX(bool) {}
    virtual bool GetMicFX() const { return false; }
    virtual void SetMicVolume(float) {}
    virtual float GetMicVolume() const { return 0; }
    virtual void SuspendMics() {}
    virtual void ResumeMics() {}
    virtual int GetNumConnectedMics() { return 0; }
    virtual int GetNextAvailableMicID() const { return -1; }
    virtual bool IsMicConnected(int) const { return false; }
    virtual void CaptureMic(int) {}
    virtual void ReleaseMic(int) {}
    virtual void ReleaseAllMics() {}
    virtual TranscodableMixerOutput *GetSecureOutput() { return nullptr; }
    virtual bool DidMicsChange() const { return false; }
    virtual void ResetMicsChanged() {}
    virtual Stream *NewStream(const char *, float, float, bool);
    virtual Stream *NewBufStream(const void *, int, Symbol, float, bool);
    virtual StreamReader *NewStreamDecoder(File *, StandardStream *, Symbol);
    virtual void NewStreamFile(const char *, File *&, Symbol &);
    virtual void EnableLevels(bool) {}
    virtual void RequirePushToTalk(bool, int) {}
    virtual void SetIncomingVoiceChatVolume(float) {}
    virtual FxSendPitchShift *CreatePitchShift(int, SendChannels);
    virtual void DestroyPitchShift(FxSendPitchShift *);

    // RndOverlay::Callback
    virtual float UpdateOverlay(RndOverlay *, float);

    Fader *MasterFader() const { return mMasterFader; }
    Fader *SfxFader() const { return mSfxFader; }
    Fader *InstFader() const { return mMidiInstrumentFader; }
    void SetDir(ObjectDir *dir) { mCommonBank = dir; }
    ByteGrinder &Grinder() { return mByteGrinder; }
    MicClientMapper *GetMicClientMapper() { return mMicClientMapper; }
    std::vector<LevelData> &GetLevelData() { return mLevelData; }
    bool CheckCommonBank(bool);
    void SetMasterVolume(float);
    float GetMasterVolume();
    // RB3-era fire-and-forget cue playback (GamePanel::PlayBandDiedCue); dc3's
    // newer Synth dropped it. Declared decl-only.
    void Play(const char *, float, float, float);
    void ToggleHud();
    const ADSRImpl *DefaultADSR();
    void DrawMeterScale(float &);
    void SetFX(const DataArray *);
    void SetMic(const DataArray *);
    int GetFXOverhead();
    int GetSPUOverhead();
    void RunFlow(const char *);
    void StopPlaybackAllMics();
    void StopAllSfx(bool);
    void PauseAllSfx(bool);
    int GetSampleMem(ObjectDir *, Platform);
    void StopAllSounds();
    void AddPlayHandler(Hmx::Object *);
    void RemovePlayHandler(Hmx::Object *);
    void SendToPlayHandlers(Sound *);
    void PlaySound(const char *, float, float, float);
    void AddZombie(SampleInst *);
    int GetNumMics() const;
    void DrawMeter(float &, float, float, const char *);

    template <class T>
    T *Find(const char *name, bool fail = true) {
        if (!CheckCommonBank(false))
            return nullptr;
        else {
            T *obj = mCommonBank->Find<T>(name, false);
            if (!obj && fail) {
                MILO_FAIL(
                    "Synth::Find() - %s %s not found in %s",
                    T::StaticClassName(),
                    name,
                    mCommonBank->GetPathName()
                );
            }
            return obj;
        }
    }

private:
    void CullZombies();

    DataNode OnPassthrough(DataArray *);
    DataNode OnStartMic(const DataArray *);
    DataNode OnStopMic(const DataArray *);
    DataNode OnNumConnectedMics(const DataArray *);
    DataNode OnSetMicVolume(const DataArray *);
    DataNode OnSetFX(const DataArray *);
    DataNode OnSetFXVol(const DataArray *);

    static Synth *New();

protected:
    virtual ~Synth() {}

    std::vector<LevelData> mLevelData; // 0x30
    bool mTrackLevels; // 0x3c
    ByteGrinder mByteGrinder; // 0x40
    int mNumMics; // 0x44
    MidiSynth *mMidiSynth; // 0x48
    std::vector<Mic *> mMics; // 0x4c
    bool mMuted; // 0x58
#ifdef RB3_SYNTH_DC3_LISTS
    // DC3-era layout (gate ON). Retail RB3-360 differs: no leading ObjectDir*
    // list (unk5c has ZERO uses in our tree), mZombieInsts placed AFTER the
    // faders, and mCommonBank's ObjDirPtr is a full 0xc bytes — so mMasterFader
    // is at 0x68, not the 0x74 this layout produces.
    std::list<ObjectDir *> unk5c; // 0x5c (DC3-only)
    ObjDirPtr<ObjectDir> mCommonBank; // 0x64
    std::list<SampleInst *> mZombieInsts; // 0x78
    Fader *mMasterFader; // 0x80
    Fader *mSfxFader; // 0x84
    Fader *mMidiInstrumentFader; // 0x88
#else
    // Retail RB3-360 layout (default). Verified: rb3-Wii oracle Synth.h
    // (mMuted@0x3c, ObjDirPtr unk40, mMasterFader@0x4c == retail offsets +0x1c,
    // no lists in between) + retail Stream ctor `lwz r4, 0x68(TheSynth)` =
    // mMasterFader. The +4 pad compensates our 8-byte ObjDirPtr vs retail's 0xc
    // (a separate engine-wide ObjPtr-layout deficit; see obj/Object.h note).
    ObjDirPtr<ObjectDir> mCommonBank; // 0x5c (8 bytes here; 0xc in retail)
    int mCommonBankPad_Dc3Deficit; // +4 to push mMasterFader to retail 0x68
    Fader *mMasterFader; // 0x68
    Fader *mSfxFader; // 0x6c
    Fader *mMidiInstrumentFader; // 0x70
    std::list<SampleInst *> mZombieInsts; // 0x74
#endif
    std::list<Hmx::Object *> mPlayHandlers; // 0x8c
    MicClientMapper *mMicClientMapper; // 0x94
    int unk98; // TranscodableMixer* mSecureMixer?
    Stream *mDebugStream; // 0x9c
    RndOverlay *mHud; // 0xa0
    ADSRImpl *mADSR; // 0xa4
    String unka8; // 0xa8
};

void SynthPreInit();
void SynthInit();
void SynthTerminate();

extern Synth *TheSynth;
