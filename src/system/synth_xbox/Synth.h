#pragma once
#include "FxSend.h"
#include "obj/Data.h"
#include "os/CritSec.h"
#include "os/Timer.h"
#include "stl/_vector.h"
#include "synth/FxSend.h"
#include "synth/Mic.h"
#include "synth/Synth.h"
#include "xdk/xaudio2/xaudio2.h"
#include "xdk/xvh2/xvh2.h"

class FxSend360;

// RB3-360 layout (derived from retail ctor @82B2E438 / Terminate @82B2DF00 /
// PreInit @82B2E8E8): CritSec @0x88, mMics @0xA8, voices C0/C8-D8,
// dolby bools @0xDC/0xDD, Timer @0xE0, levels ptr @0x114, mFxSends @0x118.
class Synth360 : public Synth {
public:
    virtual void PreInit();
    virtual DataNode Handle(DataArray *, bool);
    virtual bool IsUsingDolby() const;
    virtual bool HasPendingVoices();
    virtual void EnableLevels(bool);
    virtual int GetNumConnectedMics();
    virtual void SetDolby(bool, bool);
    virtual bool DidMicsChange() const;
    virtual void ResetMicsChanged();
    virtual Stream *NewStream(char const *, float, float, bool);
    virtual Stream *NewBufStream(void const *, int, Symbol, float, bool);
    virtual StreamReader *NewStreamDecoder(File *, StandardStream *, Symbol);
    virtual void NewStreamFile(char const *, File *&, Symbol &);
    virtual void CaptureMic(int);
    virtual void ReleaseAllMics();
    virtual void RequirePushToTalk(bool, int);
    virtual void Poll();
    virtual int GetNextAvailableMicID() const;
    virtual bool IsMicConnected(int) const;
    virtual void Terminate();
    virtual Mic *GetMic(int);
    virtual void ReleaseMic(int);
    virtual void Init();

    CriticalSection unk88; // 0x88
    std::vector<Mic *> mMics; // 0xa8
    std::vector<IXAudio2SubmixVoice *> mHeadsetSubmixes; // 0xb4
    int unkc0; // 0xc0 headset silence source voice (IXAudio2SourceVoice*)
    bool unkc4; // 0xc4 use-hxma-streams (never initialized in retail ctor)
    int unkc8; // 0xc8 IXAudio2* engine
    int unkcc; // 0xcc IXAudio2MasteringVoice*
    int unkd0; // 0xd0 fx submix (2ch, reverb chain)
    int unkd4; // 0xd4 fx submix (6ch, routes to unkd0)
    int unkd8; // 0xd8 reverb XAPO (IUnknown*)
    bool mDolbyEnabled; // 0xdc
    bool mDolbyPending; // 0xdd
    Timer mDolbyTimer; // 0xe0
    bool unk110; // 0x110
    int *mLevelValues; // 0x114 -> mLevelData.begin(), fed to master MeterEffect
    std::vector<FxSend360 *> mFxSends; // 0x118
    bool unk124; // 0x124

    Synth360();
    IXAudio2SubmixVoice *GetHeadsetSubmix(int);
    void RemoveFxSend(FxSend360 *);
    void AddFxSend(FxSend360 *);

private:
    void UpdateDolby();
    void SetupHeadsetSubmixes();
};

extern Synth360 *TheXboxSynth;
