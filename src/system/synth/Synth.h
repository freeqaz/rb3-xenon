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
#include "synth/MidiInstrumentMgr.h"
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
    virtual StreamReader *NewStreamDecoder(File *, StandardStream *, Symbol, bool);
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
    MidiInstrumentMgr *GetMidiInstrumentMgr() const { return mMidiInstrumentMgr; }
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

    std::vector<LevelData> mLevelData; // 0x2c..0x38
    // Retail declares the grinder BEFORE mTrackLevels: InitSecurity ends with
    // `addi r3, r29, 0x38; bl ByteGrinder::Init`, i.e. mByteGrinder is at 0x38.
    ByteGrinder mByteGrinder; // 0x38
    // Retail RB3-360 has NO mTrackLevels instance slot: Synth::SetMic
    // (fn_826FC818) reads the mic count at +0x3c and Synth::Terminate RELEASEs
    // mMidiSynth from +0x40 / DeleteAll(mMics) at +0x44 — i.e. exactly ONE word
    // between mByteGrinder (0x38) and mMidiSynth (0x40). rb3-Wii's Synth (the
    // same game version) likewise has no mTrackLevels. Kept as a class static so
    // the two uses in Synth.cpp still compile.
    static bool mTrackLevels;
    int mNumMics; // 0x3c  (retail: Synth::SetMic reads +0x3c)
    MidiSynth *mMidiSynth; // 0x40 (retail: Synth::Terminate RELEASEs +0x40)
    std::vector<Mic *> mMics; // 0x44..0x50 (retail: DeleteAll(this+0x44))
    bool mMuted; // 0x50
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
    // Retail RB3-360 layout (default). VERIFIED from the retail binary
    // (2026-07-02): Synth ctor fn_826E2F10, Init fn_826E2668, Terminate
    // fn_826E3A30, Poll fn_826E0658, and DirectInstrument::Disable (0x826c4890,
    // `lwz r3, 0x78(TheSynth)`) — see the offset table in the branch commit.
    // Retail order after the faders is mMicClientMapper (0x74) then
    // mMidiInstrumentMgr (0x78) — NOT the DC3 interposition of two std::lists +
    // unk98/mDebugStream/mHud/mADSR/String that pushed mMidiInstrumentMgr to
    // 0xa4. The DC3-ish members are kept (the .cpp still uses them) but relocated
    // AFTER mMidiInstrumentMgr where they don't perturb the retail-critical
    // 0x74/0x78 slots. The +4 pad keeps the faders at retail 0x68/0x6c/0x70
    // (empirically our ObjDirPtr lands mMasterFader at 0x68 with this pad).
    // Retail ~Synth destroys, in order, a member at +0x5c (ObjDirPtr) then one
    // at +0x54 (out-of-line 8-byte dtor = the std::list) — reverse declaration
    // order, so the list is declared immediately BEFORE mCommonBank and the
    // ObjDirPtr's 0xc bytes land the faders on retail's 0x68/0x6c/0x70 with no
    // artificial padding (DC3 has a std::list in exactly this slot too).
    std::list<SampleInst *> mZombieInsts; // 0x54..0x5c
    ObjDirPtr<ObjectDir> mCommonBank; // 0x5c..0x68
    Fader *mMasterFader; // 0x68
    Fader *mSfxFader; // 0x6c
    Fader *mMidiInstrumentFader; // 0x70
    MicClientMapper *mMicClientMapper; // 0x74 (retail: fn_82664760 reads +0x74)
    MidiInstrumentMgr *mMidiInstrumentMgr; // 0x78 (retail: Terminate reads +0x78)
    // 8 unidentified, non-destructible bytes: rb3-Wii's Synth has `int unk60;`
    // (TranscodableMixer*?) and `int unk64;` (Stream* mDebugStream?) in exactly
    // these two slots, between mMidiInstrumentMgr and mHud.
    int unk7c; // 0x7c
    int unk80; // 0x80
#endif
    // mHud is retail-verified at 0x84 (Synth::ToggleHud reads +0x84 for mHud).
    RndOverlay *mHud; // 0x84
    // DC3-era members retail RB3-360 lacks entirely: retail sizeof(Synth) is
    // 0x88 (Synth360 ctor inits its CritSec at +0x88, mMics vector at +0xA8).
    // Kept as CLASS STATICS so synth/Synth.cpp's `this->`-style uses still
    // compile without inflating the instance and shifting every Synth360
    // member by +0x20 (which broke Synth360::EnableLevels et al).
    static std::list<Hmx::Object *> mPlayHandlers;
    static int unk98; // TranscodableMixer* mSecureMixer?
    static Stream *mDebugStream;
    static ADSRImpl *mADSR;
    static String unka8;
};

void SynthPreInit();
void SynthInit();
void SynthTerminate();

extern Synth *TheSynth;
