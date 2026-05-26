#include "synth_xbox/Mic.h"
#include "macros.h"
#include "math/Decibels.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "os/CritSec.h"
#include "os/Debug.h"
#include "os/System.h"
#include "utl/Symbol.h"
#include <cstring>

MicManagerXbox *sInstance;

namespace GainEffect {
static float sGain;
}

struct HeadsetConfig {
    float noiseThreshold;
    float pad;
    float lowCut;
    float localGain;
    float remoteGain;
};

extern "C" {
extern HeadsetConfig lbl_82F474C8;
}

#pragma region MicXbox

MicXbox::MicXbox(int, float volume)
    : mRunning(false), unk10(0), mChangeNotify(false), mPlaybackVoice(0), unk301c(unk1c),
      unk9054(1.0f), unk9058(0), unk905c(0), mFxSend(0), mVolume(volume), mMute(false),
      unk906c(0), mGain(1.0f), mOutputGain(1.0f), mSensitivity(1.0f), unk907c(0),
      mDroppedSamples(0), mDeviceName("generic_usb"), mClipping(false) {
    unk302c.Init(0xc00);
    unk3040.Init(0x6000);
    unk3020.reserve(0x1800);
    memset(unk1c, 0, 0x3000);
}

MicXbox::~MicXbox() {
    if (mRunning)
        Stop();
    delete mPlaybackVoice;
    mPlaybackVoice = 0;
}

bool MicXbox::GetClipping() const { return mClipping; }

float MicXbox::GetGain() const { return mGain; }

int MicXbox::GetDroppedSamples() { return mDroppedSamples; }

float MicXbox::GetOutputGain() const { return mOutputGain; }

float MicXbox::GetSensitivity() const { return mSensitivity; }

Symbol &MicXbox::GetName() const { return (Symbol &)mDeviceName; }

void MicXbox::ClearBuffers() {
    unk302c.Reset();
    unk3040.Reset();
}

void MicXbox::SetOutputGain(float f) {
    mOutputGain = f;
    MILO_ASSERT(mOutputGain >= 0.0f, 0x32c);
}

void MicXbox::SetSensitivity(float f) {
    mSensitivity = f;
    MILO_ASSERT(mOutputGain >= 0.0f, 0x337);
}

void MicXbox::SetVolume(float f) { mVolume = DbToRatio(f); }

void MicXbox::SetChangeNotify(bool b) { mChangeNotify = b; }

void MicXbox::SetMute(bool b) { mMute = b; }

bool MicXbox::IsPlaying() { return mPlaybackVoice; }

void MicXbox::Start() {
    if (!mRunning) {
        unk301c = unk1c;
        MicManagerXbox *x = MicManagerXbox::GetInstance();
        x->AddMic(this);
        mRunning = true;
    }
}

void MicXbox::Stop() {
    if (mRunning) {
        MicManagerXbox *x = MicManagerXbox::GetInstance();
        x->RemoveMic(this);
        mRunning = false;
        if (mPlaybackVoice) {
            StopPlayback();
        }
    }
}

void MicXbox::SetFxSend(FxSend *fx) {
    CriticalSection *cs = &MicManagerXbox::GetInstance()->unk68;
    if (cs)
        cs->Enter();
    mFxSend = fx;
    if (mPlaybackVoice) {
        StopPlayback();
        StartPlayback();
    }
    if (cs)
        cs->Exit();
}

bool MicXbox::IsRunning() const { return mRunning; }
void MicXbox::SetDMA(bool b) {}
bool MicXbox::GetDMA() const { return false; }
void MicXbox::SetEarpieceVolume(float f) {}
float MicXbox::GetEarpieceVolume() const { return 0.0f; }
void MicXbox::SetCompressor(bool b) {}
bool MicXbox::GetCompressor() const { return false; }
void MicXbox::SetCompressorParam(float f) {}
float MicXbox::GetCompressorParam() const { return 0.0f; }
int MicXbox::GetSampleRate() const { return 16000; }

void MicXbox::OnMicConnected(unsigned long ul, bool b, Symbol const &s) {
    unkc = b;
    mDeviceName = s;
    MicManagerXbox *x = MicManagerXbox::GetInstance();
    x->mMicsChanged = true;
}

void MicXbox::OnMicDisconnected() {
    MicManagerXbox *x = MicManagerXbox::GetInstance();
    x->mMicsChanged = true;
}

#pragma endregion MicXbox
#pragma region MicManagerXbox

MicManagerXbox::MicManagerXbox()
    : unk18(-1), unk1c(0), unk2c(0), mMicsChanged(false), mPushToTalkPad(-1) {
    for (int i = 0; i < 4; i++) {
        unkc.push_back(0);
    }
    unk20.reserve(4);

    // Register data functions for headset configuration
    DataRegisterFunc(Symbol("set_noise_gate"), SetNoiseGate);
    DataRegisterFunc(Symbol("set_low_cut"), SetLowCut);
    DataRegisterFunc(Symbol("set_local_gain"), SetLocalGain);
    DataRegisterFunc(Symbol("set_remote_gain"), SetRemoteGain);

    // Load headset configuration from system config
    DataArray *arr = SystemConfig(Symbol("synth"), Symbol("xbox_headset"));
    arr->FindData(Symbol("noise_threshold"), lbl_82F474C8.noiseThreshold, true);
    arr->FindData(Symbol("low_cut"), lbl_82F474C8.lowCut, true);
    arr->FindData(Symbol("local_gain"), lbl_82F474C8.localGain, true);
    arr->FindData(Symbol("remote_gain"), lbl_82F474C8.remoteGain, true);

    // Convert remote gain from dB to linear ratio
    GainEffect::sGain = DbToRatio(lbl_82F474C8.remoteGain);
}

MicManagerXbox::~MicManagerXbox() {}

void MicManagerXbox::RequirePushToTalk(bool b, int pad) {
    unk68.Enter();
    if (b) {
        MILO_ASSERT(pad >= 0, 0x2c7);
        mPushToTalkPad = pad;
    } else {
        mPushToTalkPad = -1;
    }

    unk68.Exit();
}

#pragma endregion MicManagerXbox
