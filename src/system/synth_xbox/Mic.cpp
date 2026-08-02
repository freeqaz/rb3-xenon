#include "synth_xbox/Mic.h"
#include "macros.h"
#include "synth_xbox/ExternalMic.h"
#include "synth_xbox/Synth.h"
#include "utl/Std.h"
#include "math/Decibels.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "os/CritSec.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "os/System.h"
#include "math/Trig.h"
#include "math/Utl.h"
#include "utl/MemStream.h"
#include "utl/Symbol.h"
#include <cmath>
#include <cstring>

extern void *_xhv_voicechat_mode;

MicManagerXbox *MicManagerXbox::sInstance;

namespace GainEffect {
static float sGain;
}

// Separate file statics (not a struct): leaf global addresses schedule the
// addi r5 before the li r6 in the FindData arg setup, matching retail
// (same model DC3 verified vs its target asm). gNoiseInt provides the
// 4-byte gap between gNoiseThreshold (+0) and gLowCut (+8).
static float gNoiseThreshold = -10;
static int gNoiseInt = 5;
static float gLowCut = 800;
static float gLocalGain = -3;
static float gRemoteGain = 3;

#pragma region ChatReceiver

ChatReceiver::ChatReceiver(IXHV2Engine *engine, int i2)
    : mXHV(engine), unk4(i2), unk8(0), unk9(0), unkc(0), unk10(0), unk14(0), unk18(0),
      unk50(new MemStream(true)) {
    MILO_ASSERT(mXHV, 0x3F2);
}

ChatReceiver::~ChatReceiver() {
    ActivateProcessing(false);
    RELEASE(unk50);
}

void ChatReceiver::ActivateProcessing(bool b1) {
    if (b1 != unk9) {
        unk9 = b1;
        void *mode = _xhv_voicechat_mode;
        if (b1) {
            mXHV->RegisterLocalTalker(unk4);
            mXHV->StartLocalProcessingModes(unk4, &mode, 1);
        } else {
            mXHV->StopLocalProcessingModes(unk4, &mode, 1);
            mXHV->UnregisterLocalTalker(unk4);
        }
    }
}

void ChatReceiver::ProcessChatData(void *data, unsigned int size, int *flag) {
    float w = gLowCut * 0.000392699f;
    float b1 = Sine(w + 1.5707964f) * -2.0f;
    float a1 = -b1;
    float disc = b1 * b1 - (Sine(w + 1.5707964f) * 8.0f - 7.0f) * 4.0f;
    float coef = (a1 - sqrtf(disc)) * 0.5f;
    float gain = (coef + 1.0f) * 0.5f;

    float z1 = unkc;
    float z2 = unk10;
    unsigned int samps = size >> 1;
    if (samps != 0) {
        short *p = (short *)data - 1;
        for (unsigned int i = 0; i != samps; i++) {
            float in = (float)p[1];
            float out = (in - z1) * gain * 2.0f + z2 * coef;
            z1 = in;
            out = Clamp(-32767.0f, 32767.0f, out);
            p++;
            *p = (short)out;
            z2 = (float)(short)out;
        }
    }
    unk10 = z2;
    unkc = z1;

    short maxSamp = 0;
    short minSamp = 0;
    *flag = 1;
    float localRatio = DbToRatio(gLocalGain);
    if (samps != 0) {
        short *p = (short *)data - 1;
        for (unsigned int i = 0; i < samps; i++) {
            short s = p[1];
            if (s >= maxSamp) {
                maxSamp = s;
            }
            if (s < minSamp) {
                minSamp = s;
            }
            p++;
            p[0] = (short)((float)s * localRatio);
        }
    }

    int newCount;
    if ((float)maxSamp * (1.0f / 32767.0f) > DbToRatio(gNoiseThreshold)
        || (float)minSamp * (1.0f / 32767.0f) < -DbToRatio(gNoiseThreshold)) {
        newCount = gNoiseInt;
    } else if (unk14 > 0) {
        newCount = unk14 - 1;
    } else {
        *flag = 0;
        return;
    }
    unk14 = newCount;
    if (*flag != 0) {
        unk18 = 5;
    }
}

#pragma endregion

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

void MicXbox::SetGain(float gain) { mGain = Clamp(0.0f, 1.0f, gain); }

Mic::Type MicXbox::GetType() const {
    return ExternalMicClientMgr::ConnectedForClient(this) ? kMicNull : kDisconnected;
}

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
    CritSecTracker t(&MicManagerXbox::GetInstance()->unk68);
    mFxSend = fx;
    if (mPlaybackVoice) {
        StopPlayback();
        StartPlayback();
    }
}

short *MicXbox::GetRecentBuf(int &iref) {
    CritSecTracker t(&MicManagerXbox::GetInstance()->unk68);
    unk302c.Peek(unk3054, 0xC00);
    iref = 0x600;
    return (short *)unk3054;
}

short *MicXbox::GetContinuousBuf(int &iref) {
    CritSecTracker t(&MicManagerXbox::GetInstance()->unk68);
    iref = unk3040.Read(unk3054, 0x6000) / sizeof(short);
    return (short *)unk3054;
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

void MicXbox::Poll() {
    if (mPlaybackVoice && mPlaybackVoice->IsPlaying()) {
        int written = (char *)unk301c - (char *)unk1c;
        int delta = written - mPlaybackVoice->GetAddr();
        unk905c = ModRange(unk905c - 6144.0f, unk905c + 6144.0f, (float)delta);
        unk9058 = unk9058 * 0.9f + unk905c * 0.1f;
        if (unk905c > 12288.0f && unk9058 > 12288.0f) {
            unk905c -= 12288.0f;
            unk9058 -= 12288.0f;
        }
        if (unk905c < -12288.0f && unk9058 < -12288.0f) {
            unk905c += 12288.0f;
            unk9058 += 12288.0f;
        }
        float pos = ModRange(
            (unkc ? 2700.0f : 1800.0f) - 600.0f,
            (unkc ? 2700.0f : 1800.0f) - 600.0f + 12288.0f,
            unk9058
        );
        float vol = mMute ? 0.0f : mVolume;
        if (pos > (unkc ? 2700.0f : 1800.0f) + 600.0f) {
            unk9054 = unk9054 > 1.0f ? 1.08f : 0.92f;
            vol = 0.0f;
        } else if (pos > (unkc ? 2700.0f : 1800.0f) + 150.0f) {
            unk9054 = 1.0002f;
        } else if (pos < (unkc ? 2700.0f : 1800.0f) - 150.0f) {
            unk9054 = 0.9998f;
        } else if (pos > (unkc ? 2700.0f : 1800.0f) + 300.0f) {
            unk9054 = 1.0006f;
        } else if (pos < (unkc ? 2700.0f : 1800.0f) - 300.0f) {
            unk9054 = 0.9994f;
        } else if ((unk9054 > 1.0f && pos < (unkc ? 2700.0f : 1800.0f) * 0.5f) || (unk9054 < 1.0f && pos > (unkc ? 2700.0f : 1800.0f) * 0.5f)) {
            unk9054 = 1.0f;
        }
        mPlaybackVoice->SetVolume(vol);
        mPlaybackVoice->SetSpeed(unk9054);
    }
    if (mChangeNotify && GetType() != unk10) {
        MicrophonesChangedMsg msg(unk10 != 0);
        ThePlatformMgr.Handle(msg, false);
        unk10 = GetType();
    }
}

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
    : unk18(-1), unk1c(0), unk20(0), mMicsChanged(false), mPushToTalkPad(-1) {
    for (int i = 0; i < 4; i++) {
        unkc.push_back(0);
    }
    unk28.reserve(4);

    // Register data functions for headset configuration
    DataRegisterFunc(Symbol("set_noise_gate"), SetNoiseGate);
    DataRegisterFunc(Symbol("set_low_cut"), SetLowCut);
    DataRegisterFunc(Symbol("set_local_gain"), SetLocalGain);
    DataRegisterFunc(Symbol("set_remote_gain"), SetRemoteGain);

    // Load headset configuration from system config
    DataArray *arr = SystemConfig(Symbol("synth"), Symbol("xbox_headset"));
    arr->FindData(Symbol("noise_threshold"), gNoiseThreshold, true);
    arr->FindData(Symbol("low_cut"), gLowCut, true);
    arr->FindData(Symbol("local_gain"), gLocalGain, true);
    arr->FindData(Symbol("remote_gain"), gRemoteGain, true);

    // Convert remote gain from dB to linear ratio
    GainEffect::sGain = DbToRatio(gRemoteGain);
}

MicManagerXbox::~MicManagerXbox() {}

void MicManagerXbox::RequirePushToTalk(bool b, int pad) {
    CritSecTracker t(&unk68);
    if (b) {
        MILO_ASSERT(pad >= 0, 0x2c7);
        mPushToTalkPad = pad;
    } else {
        mPushToTalkPad = -1;
    }
}

void MicManagerXbox::AddMic(MicXbox *mic) {
    FOREACH (it, unk0) {
        if (*it == mic) {
            return;
        }
    }
    unk0.push_back(mic);
    mic->SetChangeNotify(true);
}

void MicManagerXbox::RemoveMic(MicXbox *mic) {
    FOREACH (it, unk0) {
        if (*it == mic) {
            unk0.erase(it);
            mic->SetChangeNotify(false);
            return;
        }
    }
}

void MicManagerXbox::Poll() {
    FOREACH (it, unk0) {
        (*it)->Poll();
    }
    FOREACH (it, unkc) {
        ChatReceiver *receiver = *it;
        int count = receiver->unk18;
        if (count < 2) {
            count = 0;
        } else {
            count--;
        }
        receiver->unk18 = count;
    }
    FOREACH (it, unk28) {
        ChatBuffer &cb = *it;
        if (cb.unk8[250] != 0) {
            UINT32 count = cb.unk8[250];
            unk1c->SubmitIncomingChatData(
                *(UINT64 *)&cb, (unsigned char *)cb.unk8, &count
            );
            cb.unk8[250] -= count;
            memcpy(cb.unk8, (char *)cb.unk8 + count, cb.unk8[250]);
        } else if (!TheXboxSynth->mHeadsetSubmixes.empty() && *(UINT64 *)&cb == 0x00DEADBEEFFACEF0ULL) {
            unk38.Split();
            if (!unk38.Running() || unk38.Ms() > 2000.0f) {
                unsigned char buf[0x14] = { 0 };
                UINT32 count = sizeof(buf);
                unk1c->SubmitIncomingChatData(*(UINT64 *)&cb, buf, &count);
                unk38.Restart();
            }
        }
    }
}

MicManagerXbox *MicManagerXbox::GetInstance() {
    if (!sInstance) {
        sInstance = new MicManagerXbox();
    }
    return sInstance;
}

#pragma endregion MicManagerXbox

// laneAE scatter force-emit (default/Mic).
// Retail was built with function-level COMDATs (/Gy); the linker placed these
// four STL specializations' COMDATs inside the .text span pinned to default/Mic
// even though their natural owners are elsewhere (vector<unsigned short> /
// vector<int> helpers are emitted all over our tree, and
// vector<const MoveParent*>::_M_allocate_and_copy nowhere at all).  objdiff pairs
// target<->base by mangled name *within a unit*, so with Mic.obj not defining
// them the four targets were structurally pinned at 0%.  ODR-use them here so
// MSVC emits the COMDATs into this obj.
#ifndef HX_NATIVE
#include <vector>
class MoveParent;
void ForceEmit_LaneAE_Mic(
    std::vector<unsigned short> &us,
    std::vector<int> &si,
    std::vector<const MoveParent *> &dst,
    std::vector<const MoveParent *> &src
) {
    // -> ?_M_insert_overflow@?$vector@G...@ABU__true_type@2@I_N@Z
    us.push_back(0);
    // -> ?_M_fill_insert@?$vector@G...@AAAXPAGIABG@Z
    us.insert(us.begin(), (std::vector<unsigned short>::size_type)2, (unsigned short)0);
    // -> ?_M_insert_overflow@?$vector@H...@ABU__true_type@2@I_N@Z
    si.push_back(0);
    dst.assign(src.begin(), src.end());
}
// _M_allocate_and_copy is defined inline in the class body, so the assign() call
// above inlines it away -- only an explicit instantiation forces the standalone
// COMDAT that retail's linker parked in this unit's .text span.
// -> ??$_M_allocate_and_copy@PAPBVMoveParent@@@?$vector@PBVMoveParent@@...
template const MoveParent **std::vector<const MoveParent *>::_M_allocate_and_copy<
    const MoveParent **>(unsigned int, const MoveParent **, const MoveParent **);
#endif
