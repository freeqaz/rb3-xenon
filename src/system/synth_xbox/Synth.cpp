#include "synth_xbox/Synth.h"
#include "synth_xbox/HeadsetXferEffect.h"
#include "FxSendChorus.h"
#include "FxSendCompress.h"
#include "FxSendDelay.h"
#include "FxSendDistortion.h"
#include "FxSendEQ.h"
#include "FxSendFlanger.h"
#include "FxSendMeterEffect.h"
#include "synth_xbox/FxSendPitchShift360.h"
#include "FxSendReverb.h"
#include "synth_xbox/FxSendSynapse360.h"
#include "FxSendWah.h"
#include "Synth.h"
#include "MeterEffect.h"
#include "dsp/StandardEffect.h"
#include "synth/CompressionEffect.h"
#include "macros.h"
#include "math/Decibels.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/BufFile.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/System.h"
#include "synth/StandardStream.h"
#include "synth/StreamNull.h"
#include "synth/VorbisReader.h"
#include "utl/MakeString.h"
#include "utl/Str.h"
#include "stl/_algobase.h"
#include "synth/Synth.h"
#include "synth_xbox/ExternalMic.h"
#include "synth_xbox/FxSend.h"
#include "synth_xbox/Mic.h"
#include "synth_xbox/Voice.h"
#include "synth_xbox/StreamReceiver360.h"
#include "synth_xbox/XMAReader.h"
#include "synth_xbox/SynthSample.h"
#include "utl/Std.h"
#include "xdk/xapilibi/xbox.h"
#include "xdk/xaudio2/xaudio2.h"

// Harmonix XAudio2 wrappers (retail: CreateXAudio2Object @82B908A8,
// CreateAudioReverb @82B8C300).
extern "C" HRESULT CreateXAudio2Object(void **ppXAudio2, UINT32 flags, UINT32 processor);
extern "C" HRESULT CreateAudioReverb(IUnknown **ppApo);

Synth360 *TheXboxSynth;

void StartSynchronizedVoices();

static unsigned char sHeadsetSilence[0x100];

Synth *Synth::New() { return new Synth360(); }

Synth360::Synth360()
    : unkc0(0), unkc8(0), unkcc(0), mDolbyEnabled(true), mDolbyPending(false),
      unk110(false), mLevelValues(0) {}

BEGIN_HANDLERS(Synth360)
    HANDLE_ACTION(set_headset_target, Voice::sHeadsetTarget = _msg->Int(2))
    HANDLE_SUPERCLASS(Synth)
END_HANDLERS

// Retail @82B2E8E8 (0x518). Builds the master output chain:
// [StandardEffect<CompressionEffect> ("limiter"), MeterEffect] on the
// mastering voice, plus the global reverb submix pair (2ch reverb @0xD0,
// 6ch feeder @0xD4).
void Synth360::PreInit() {
    TheXboxSynth = this;

    {
        const char *busNames[9] = { "front_left", "front_right", "center",
                                    "lfe",        "surr_left",   "surr_right",
                                    "",           "out_left",    "out_right" };
        for (int i = 0; i < 9; i++) {
            mLevelData.push_back(LevelData(busNames[i]));
        }
    }

    CreateXAudio2Object((void **)&unkc8, 0, 0x10);
    (((HRESULT(*)(int *, int *, int, int, int, int, int))(*(int *)(*(int *)unkc8 + 0x28)))(
        (int *)unkc8, &unkcc, 0, 0, 0, 0, 0
    ));

    {
        XAUDIO2_EFFECT_DESCRIPTOR effectDescs[2];
        XAUDIO2_EFFECT_CHAIN chain;
        effectDescs[0].pEffect =
            static_cast<CXAPOBase *>(new StandardEffect<CompressionEffect>());
        effectDescs[0].InitialState = 1;
        effectDescs[0].OutputChannels = 6;
        effectDescs[1].pEffect = static_cast<CXAPOBase *>(new MeterEffect());
        effectDescs[1].InitialState = 1;
        effectDescs[1].OutputChannels = 6;

        mLevelValues = new int;
        *mLevelValues = *(int *)&mLevelData;

        chain.EffectCount = 2;
        chain.pEffectDescriptors = effectDescs;
        ((IXAudio2Voice *)unkcc)->SetEffectChain(&chain);

    DataArray *limiterCfg = SystemConfig(Symbol("synth"), Symbol("limiter"));
    float threshold = limiterCfg->FindArray(Symbol("threshold"), true)->Float(1);
    float ratio = limiterCfg->FindArray(Symbol("ratio"), true)->Float(1);
    float attack = limiterCfg->FindArray(Symbol("attack_ms"), true)->Float(1) * 0.001f;
    float release = limiterCfg->FindArray(Symbol("release_ms"), true)->Float(1) * 0.001f;
    float outputDb = limiterCfg->FindArray(Symbol("output_db"), true)->Float(1);

    CompressionEffect::Params params;
    ((IXAudio2Voice *)unkcc)->GetEffectParameters(0, &params, sizeof(params));
    params.mThresholdDb = threshold;
    params.mRatio = ratio;
    params.mAttackTime = attack;
    params.mReleaseTime = release;
    params.mGateThreshDb = -140.0f;
    params.mOutputGainDb = (1.0f - 1.0f / ratio) * threshold + outputDb;
    ((IXAudio2Voice *)unkcc)->SetEffectParameters(0, &params, sizeof(params), 0);

    CreateAudioReverb((IUnknown **)&unkd8);
    effectDescs[0].pEffect = (IUnknown *)unkd8;
    effectDescs[0].InitialState = 1;
    effectDescs[0].OutputChannels = 2;
    chain.EffectCount = 1;
    chain.pEffectDescriptors = effectDescs;
    (((HRESULT(*)(int *, int *, int, int, int, int, int, XAUDIO2_EFFECT_CHAIN *)
    )(*(int *)(*(int *)unkc8 + 0x24)))(
        (int *)unkc8, &unkd0, 2, 48000, 0, 0x8000, 0, &chain
    ));
    }

    {
        XAUDIO2_SEND_DESCRIPTOR sendDesc;
        sendDesc.Flags = 0;
        sendDesc.pOutputVoice = (IXAudio2Voice *)unkd0;
        XAUDIO2_VOICE_SENDS sends;
        sends.SendCount = 1;
        sends.pSends = &sendDesc;
        (((HRESULT(*)(int *, int *, int, int, int, int, XAUDIO2_VOICE_SENDS *, int)
        )(*(int *)(*(int *)unkc8 + 0x24)))(
            (int *)unkc8, &unkd4, 6, 48000, 0, 0x7fff, &sends, 0
        ));
    }

    ((IXAudio2Voice *)unkd0)->SetVolume(4.0f, 0);

    char reverbParams[0x34];
    ((IXAudio2Voice *)unkd0)->GetEffectParameters(0, reverbParams, sizeof(reverbParams));
    *(float *)(reverbParams + 0x28) = 1.6f;
    ((IXAudio2Voice *)unkd0)
        ->SetEffectParameters(0, reverbParams, sizeof(reverbParams), 0);

    EnableLevels(false);
}

// Retail @82B2D3D0 (0x27C).
void Synth360::Poll() {
    ((IXAudio2Voice *)unkcc)->SetEffectParameters(1, (const void *)mLevelValues, 4, 0);

    static float gainCenter = DbToRatio(-3.0f);
    static float gainSide = DbToRatio(-1.2f);
    static float gainRear = DbToRatio(-6.2f);

    mLevelData[7].mRMS = mLevelData[2].mRMS * gainCenter +
        (mLevelData[5].mRMS * gainRear + mLevelData[4].mRMS * gainSide) + mLevelData[0].mRMS;
    mLevelData[7].mPeak = mLevelData[2].mPeak * gainCenter +
        (mLevelData[5].mPeak * gainRear + mLevelData[4].mPeak * gainSide) + mLevelData[0].mPeak;
    mLevelData[8].mRMS = mLevelData[2].mRMS * gainCenter +
        (mLevelData[5].mRMS * gainSide + mLevelData[4].mRMS * gainRear) + mLevelData[1].mRMS;
    mLevelData[8].mPeak = mLevelData[2].mPeak * gainCenter +
        (mLevelData[5].mPeak * gainSide + mLevelData[4].mPeak * gainRear) + mLevelData[1].mPeak;

    Synth::Poll();

    if (!mMics.empty()) {
        MicManagerXbox::GetInstance()->Poll();
    }

    if (mDolbyTimer.Running()) {
        float ms = mDolbyTimer.SplitMs();
        float volume;
        if (ms < 300.0f) {
            volume = ms * -0.32f;
        } else if (ms < 600.0f) {
            volume = -96.0f;
        } else if (mDolbyPending) {
            UpdateDolby();
            mDolbyPending = false;
            volume = -96.0f;
        } else if (ms < 900.0f) {
            volume = -96.0f;
        } else if (ms < 1800.0f) {
            volume = (1800.0f - ms) * -0.10666667f;
        } else {
            mDolbyTimer.Reset();
            volume = 0.0f;
        }
        SetMasterVolume(volume);
    }

    StartSynchronizedVoices();
    StopSynchronizedVoices();
    VorbisReader::SignalDecodeThread();
}

// Retail @82B2DF00 (0x1C4).
void Synth360::Terminate() {
    for (unsigned int i = 0; i < mFxSends.size(); i++) {
        mFxSends[i]->CleanChain();
    }
    TheXboxSynth = nullptr;
    Synth::Terminate();
    ExternalMic::Terminate();

    std::for_each(mMics.begin(), mMics.end(), Delete());
    if (!mMics.empty()) {
        MicManagerXbox::GetInstance()->Shutdown();
    }

    if (!mHeadsetSubmixes.empty()) {
        ((IXAudio2SourceVoice *)unkc0)->Stop(0, 0);
        ((IXAudio2SourceVoice *)unkc0)->DestroyVoice();
        unkc0 = 0;
        for (unsigned int i = 0; i < mHeadsetSubmixes.size(); i++) {
            mHeadsetSubmixes[i]->DestroyVoice();
        }
        mHeadsetSubmixes.erase(mHeadsetSubmixes.begin(), mHeadsetSubmixes.end());
    }

    ((IXAudio2Voice *)unkd4)->DestroyVoice();
    unkd4 = 0;
    ((IXAudio2Voice *)unkd0)->DestroyVoice();
    unkd0 = 0;
    ((IXAudio2Voice *)unkcc)->DestroyVoice();
    unkcc = 0;
    ((IUnknown *)unkc8)->Release();
    delete mLevelValues;
    mLevelValues = 0;
}

void Synth360::Init() {
    Synth::Init();
    SynthSample360::Init();
    StreamReceiver360::Init();
    REGISTER_OBJ_FACTORY(FxSendReverb360)
    REGISTER_OBJ_FACTORY(FxSendDistortion360)
    REGISTER_OBJ_FACTORY(FxSendDelay360)
    REGISTER_OBJ_FACTORY(FxSendChorus360)
    REGISTER_OBJ_FACTORY(FxSendCompress360)
    REGISTER_OBJ_FACTORY(FxSendEQ360)
    REGISTER_OBJ_FACTORY(FxSendFlanger360)
    REGISTER_OBJ_FACTORY(FxSendMeterEffect360)
    REGISTER_OBJ_FACTORY(FxSendPitchShift360)
    REGISTER_OBJ_FACTORY(FxSendSynapse360)
    REGISTER_OBJ_FACTORY(FxSendWah360)

    unkc4 = SystemConfig(Symbol("synth"))->FindArray(Symbol("use_xma"), true)->Int(1) != 0;

    if (SystemConfig(Symbol("synth"))->FindArray(Symbol("enable_headset_output"), true)->Int(1)) {
        SetupHeadsetSubmixes();
    }

    float micVolume = 0.0f;
    SystemConfig(Symbol("synth"), Symbol("mic"))->FindData(Symbol("volume"), micVolume, false);

    if (GetNumMics() > 0) {
        MicManagerXbox::GetInstance()->Init();
        mMics.resize(GetNumMics(), nullptr);
        ExternalMic::Init();
        for (unsigned int i = 0; i < mMics.size(); i++) {
            mMics[i] = new MicXbox(-1, DbToRatio(micVolume));
            ExternalMicClientMgr::Associate(i, dynamic_cast<MicXbox *>(mMics[i]));
        }
    }
}

Mic *Synth360::GetMic(int index) { return mMics[index]; }

bool Synth360::HasPendingVoices() { return Voice::HasPendingVoices(); }

bool Synth360::DidMicsChange() const {
    if (mMics.empty())
        return false;
    else {
        MicManagerXbox *x = MicManagerXbox::GetInstance();
        return x->mMicsChanged;
    }
}

void Synth360::ResetMicsChanged() {
    if (!mMics.empty()) {
        MicManagerXbox *x = MicManagerXbox::GetInstance();
        x->mMicsChanged = false;
    }
}

void Synth360::CaptureMic(int micID) {
    MILO_ASSERT_RANGE(micID, 0, mMics.size(), 0x350);
    MILO_ASSERT(!mMics[micID]->IsInUse(), 0x351);
    mMics[micID]->MarkAsInUse(true);
}

void Synth360::ReleaseAllMics() {
    for (int i = 0; i < mMics.size(); i++) {
        mMics[i]->MarkAsInUse(false);
    }
}

void Synth360::AddFxSend(FxSend360 *fx) { mFxSends.push_back(fx); }

bool Synth360::IsMicConnected(int i) const {
    if (i < 0 || i >= mMics.size())
        return false;
    else {
        return mMics[i]->GetType() != 0;
    }
}

void Synth360::RequirePushToTalk(bool b, int i) {
    if (!mMics.empty()) {
        MicManagerXbox::GetInstance()->RequirePushToTalk(b, i);
    }
}

void Synth360::ReleaseMic(int micID) {
    MILO_ASSERT_RANGE(micID, 0, mMics.size(), 0x35b);
    if (!mMics[micID]->IsInUse()) {
        MILO_NOTIFY_ONCE("Releasing a microphone [%d]that was not in use\n", micID);
    }
    mMics[micID]->MarkAsInUse(false);
}

void Synth360::RemoveFxSend(FxSend360 *fx) {
    auto *findFx = std::find(mFxSends.begin(), mFxSends.end(), fx);
    if (findFx != mFxSends.end()) {
        mFxSends.erase(findFx);
    }
}

IXAudio2SubmixVoice *Synth360::GetHeadsetSubmix(int i) {
    if (!mHeadsetSubmixes.empty() && i != -1) {
        return mHeadsetSubmixes[i];
    }
    return nullptr;
}

int Synth360::GetNextAvailableMicID() const {
    for (int i = 0; i < mMics.size(); i++) {
        if (!mMics[i]->IsInUse() && mMics[i]->GetType() != 0)
            return i;
    }
    return -1;
}

// Retail @82B2EE80 (0x24C).
void Synth360::SetupHeadsetSubmixes() {
    std::vector<IXAudio2SubmixVoice *> &submixes = mHeadsetSubmixes;
    submixes.resize(4, 0);

    for (int i = 0; i < 4; i++) {
        HeadsetXferEffect *effect = new HeadsetXferEffect();

        XAUDIO2_EFFECT_DESCRIPTOR effectDesc;
        XAUDIO2_EFFECT_CHAIN effectChain;
        effectChain.pEffectDescriptors = &effectDesc;
        effectDesc.pEffect = static_cast<CXAPOBase *>(effect);
        effectDesc.InitialState = 0;
        effectChain.EffectCount = 1;
        effectDesc.OutputChannels = 1;

        ((HRESULT(*)(int *, IXAudio2SubmixVoice **, int, int, int, int, int, XAUDIO2_EFFECT_CHAIN *)
        )(*(int *)(*(int *)(int *)unkc8 + 0x24)))(
            (int *)unkc8, &submixes[i], 1, 48000, 0, 0, 0, &effectChain
        );
    }

    std::vector<XAUDIO2_SEND_DESCRIPTOR> sendDescs;

    for (int i = 0; i < 4; i++) {
        XAUDIO2_SEND_DESCRIPTOR desc;
        desc.Flags = 0;
        desc.pOutputVoice = submixes[i];
        sendDescs.push_back(desc);
    }

    WAVEFORMATEX format;
    format.wFormatTag = 1;
    format.wBitsPerSample = 16;
    format.nChannels = 1;
    format.nBlockAlign = 2;
    format.nAvgBytesPerSec = 96000;
    format.nSamplesPerSec = 48000;
    format.cbSize = 0;

    XAUDIO2_VOICE_SENDS voiceSends;
    voiceSends.pSends = &sendDescs[0];
    voiceSends.SendCount = sendDescs.size();

    IXAudio2SourceVoice *headsetVoice;
    int *pEngine = (int *)unkc8;
    HRESULT hr = ((HRESULT(*)(
        int *, IXAudio2SourceVoice **, WAVEFORMATEX *, int, float, int, XAUDIO2_VOICE_SENDS *, int
    ))(*(int *)(*(int *)pEngine + 0x20)))(
        pEngine, &headsetVoice, &format, 2, 2.0f, 0, &voiceSends, 0
    );
    MILO_ASSERT(SUCCEEDED(hr), 0x30a);

    XAUDIO2_BUFFER buffer;
    buffer.Flags = 0;
    memset(&buffer.AudioBytes, 0, sizeof(buffer) - 4);
    buffer.LoopBegin = 0;
    buffer.LoopLength = 0;
    buffer.AudioBytes = 0x100;
    buffer.pAudioData = (const BYTE *)sHeadsetSilence;
    buffer.LoopCount = 0xff;
    buffer.PlayBegin = 0;
    buffer.PlayLength = 0;
    buffer.pContext = 0;
    int *pSourceVoice = (int *)unkc0;
    hr = ((HRESULT(*)(int *, XAUDIO2_BUFFER *, int))(*(int *)(*(int *)pSourceVoice + 0x54)))(
        pSourceVoice, &buffer, 0
    );
    MILO_ASSERT(SUCCEEDED(hr), 0x319);

    pSourceVoice = (int *)unkc0;
    hr = ((HRESULT(*)(int *, int, int))(*(int *)(*(int *)pSourceVoice + 0x4c)))(pSourceVoice, 0, 0);
    MILO_ASSERT(SUCCEEDED(hr), 0x31c);
}

int Synth360::GetNumConnectedMics() { return ExternalMic::NumConnectedMics(); }

// Retail @82B2BE98 (0x30): no null check, effect slot 0 on the mastering voice.
void Synth360::EnableLevels(bool enable) {
    if (enable) {
        ((IXAudio2Voice *)unkcc)->EnableEffect(0, 0);
    } else {
        ((IXAudio2Voice *)unkcc)->DisableEffect(0, 0);
    }
}

bool Synth360::IsUsingDolby() const {
    DWORD speakerConfig;
    XAudioGetSpeakerConfig(&speakerConfig);
    return (speakerConfig >> 16) & 1;
}

// Retail @82B2BE38 (0x54).
void Synth360::UpdateDolby() {
    DWORD speakerConfig;
    XAudioGetSpeakerConfig(&speakerConfig);
    DWORD mask = mDolbyEnabled ? 0x10000 : 0x80000000;
    if (mask != speakerConfig) {
        XAudioOverrideSpeakerConfig(mask);
    }
}

void Synth360::SetDolby(bool b1, bool b2) {
    if (b2) {
        mDolbyEnabled = b1;
        UpdateDolby();
    } else if (mDolbyEnabled != b1) {
        mDolbyTimer.Restart();
        mDolbyEnabled = b1;
        mDolbyPending = true;
    }
}

// Retail @82B2CDA8 (0x14C): hxma attempt gated on unkc4, then mogg.
void Synth360::NewStreamFile(const char *name, File *&file, Symbol &sym) {
    String str(name);
    static Symbol hxma("hxma");
    sym = hxma;
    String path(MakeString("%s.hxma", str));
    if (unkc4) {
        file = NewFile(path.c_str(), 2);
        if (file)
            return;
    }
    path = MakeString("%s.mogg", str);
    file = NewFile(path.c_str(), 2);
    if (file) {
        static Symbol mogg("mogg");
        sym = mogg;
    }
}

// Retail @82B2C9F8 (0xDC): no debug notify on miss, straight to StreamNull.
Stream *Synth360::NewStream(const char *name, float volume, float pan, bool b) {
    File *file;
    Symbol sym;
    NewStreamFile(name, file, sym);
    if (file) {
        return new StandardStream(file, volume, pan, sym, b, true);
    }
    return new StreamNull(volume);
}

Stream *Synth360::NewBufStream(const void *buf, int size, Symbol ext, float startMs, bool b) {
    return new StandardStream(new BufFile(buf, size), startMs, 0.0f, ext, false, b);
}

// Retail @82B2CC10 (0xFC): hxma -> XMAReader, mogg -> VorbisReader, else 0.
StreamReader *Synth360::NewStreamDecoder(File *file, StandardStream *stream, Symbol ext) {
    static Symbol hxma("hxma");
    static Symbol mogg("mogg");
    if (ext == hxma) {
        return new XMAReader(file, stream);
    } else if (ext == mogg) {
        return new VorbisReader(file, true, stream, true);
    } else {
        return nullptr;
    }
}
