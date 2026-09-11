#pragma once
#include "types.h"
#include "xdk/win_types.h"
#include "xdk/XAPILIB.h"
#include "xdk/XAUDIO2.h"
#include "utl/PoolAlloc.h"

class FxSend360;

struct PoolVoice {
    int sourceVoice;        // 0x00 - IXAudio2SourceVoice*
    int eg;                 // 0x04 - XAPO envelope generator
    int egParams;           // 0x08 - envelope effect parameters
    tWAVEFORMATEX wfx;     // 0x0c - cached wave format (0x12 bytes)
    short pad1e;            // 0x1e - padding
    unsigned int disposeTick; // 0x20 - GetTickCount() timestamp for GC (DWORD: the thread entry loads it with `lwz`, no `extsw`)
};

class Voice {
public:
    POOL_OVERLOAD(Voice, 0x28);
    Voice(bool xma, bool synchronized, bool stereo); // retail (bool,bool,bool), see the ctor
    ~Voice();
    void InitSourceBuffer(XAUDIO2_BUFFER &);
    int GetAddr();
    void SetData(void const *, int, int);
    void Stop(); // retail: NO parameter (see Voice.cpp)
    void InitVoiceParameters(XMA2WAVEFORMATEX &, XAUDIO2_BUFFER);
    void SetSampleRate(int);
    void SetLoopRegion(int, int);
    void EndLoop();
    bool IsPlaying();
    void SetStartSamp(int);
    void SetReverbMixDb(float);
    void Pause(bool);
    void SetVolume(float);
    void SetPan(float);
    void SetReverbEnable(bool);
    void SetSend(FxSend360 *);
    static bool HasPendingVoices();
    void SetSpeed(float);

    IXAudio2SourceVoice *GetVoice() { return (IXAudio2SourceVoice *)mSourceVoice; }
    /** Retail has no stored channel count -- InitVoiceParameters computes
     *  `mStereo ? 2 : 1` inline (0x82B652E0).  Only 1 and 2 are reachable. */
    int NumChannels() const { return mStereo ? 2 : 1; }

    static int sHeadsetTarget;
    void Init(bool);
    void blockingStart(bool);
    void Start();

    unsigned int unk0; // 0x0
    int mState; // 0x4 - voice play state (2 = pending)
    const void *mBuffer; // 0x8 - audio buffer pointer (pAudioData)
    int mAudioBytes; // 0xc - audio buffer size in bytes
    int mNumSamples; // 0x10
    int mSampleRate; // 0x14
    int mStartSamp; // 0x18 - start sample position (PlayBegin)
    int mLoopStart; // 0x1c
    int mLoopEnd; // 0x20
    float mVolume; // 0x24
    float mPan; // 0x28
    float mSpeed; // 0x2c
    float mAttackRate; // 0x30 - ADSR attack rate
    float mReleaseRate; // 0x34 - ADSR release rate
    bool mXMA; // 0x38
    FxSend360 *mFxSend; // 0x3c
    bool mReverbEnabled; // 0x40
    float mReverbMixDb; // 0x44 - reverb mix in dB
    bool unk48; // 0x48
    bool mSynchronized; // 0x49 - requires synchronized voice start
    // 0x4a/0x4b were one `short mChannels` until now.  They are TWO BOOLS.
    // Retail never touches either as a halfword: across the whole Voice.cpp
    // target listing every access to 0x4a and 0x4b is an `lbz`/`stb`, and
    // InitVoiceParameters (retail 0x82B652E0) derives the wave format's channel
    // count from 0x4a as `lbz` / `cntlzw` / `extrwi` / `xori` / `addi r11,r11,1`
    // -- i.e. literally `mStereo ? 2 : 1`.  A `short` at 0x4a would make
    // `lbz 0x4a` the big-endian HIGH byte, which is 0 for every channel count
    // retail supports, so the three `if (lbz 0x4a)` branches would be dead code.
    // The offsets are unchanged, so the mTagState/mSourceVoice evidence the old
    // comment cited (Ghidra ~Voice at 0x82b662e8, StreamReceiver360::Tag) still
    // holds exactly as before.
    bool mStereo; // 0x4a
    bool unk4b; // 0x4b - gates the no-output-voice SetOutputMatrix in UpdateMix
    int mTagState; // 0x4c - stream tag state
    int mSourceVoice; // 0x50 - IXAudio2SourceVoice* (as int for vtable dispatch)
    int mEnvelopeEffect; // 0x54 - XAPO envelope generator (PoolVoice.eg)
    void *mEnvelopeParams; // 0x58 - envelope effect parameters (PoolVoice.egParams)
    tWAVEFORMATEX mWaveFormat; // 0x5c - cached wave format (0x12 bytes, copied in createOrReuse)
    short mPadding76; // 0x6e - alignment padding
    unsigned int mDisposeTick; // 0x70 - GetTickCount() timestamp for voice GC (mirrors PoolVoice::disposeTick)

private:
    void UpdateMix();
    void UpdateSends();
    void SafeRestart();
    void SetSendImpl(FxSend360 *);
    __declspec(noinline) void dispose(PoolVoice *, unsigned int);
    long createOrReuse(PoolVoice *, unsigned int &, tWAVEFORMATEX &, XAUDIO2_VOICE_SENDS *);
};

unsigned long StartVoiceThreadEntry(void *);
void StopSynchronizedVoices();
// No TerminateVoiceThread / gShutdownVoiceThread in RB3 retail -- see Voice.cpp.

extern bool gHasPendingStopCommits;
