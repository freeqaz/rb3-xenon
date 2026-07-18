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
    int disposeTick;        // 0x20 - GetTickCount() timestamp for GC
};

class Voice {
public:
    POOL_OVERLOAD(Voice, 0x28);
    Voice(bool, int, bool);
    ~Voice();
    void InitSourceBuffer(XAUDIO2_BUFFER &);
    int GetAddr();
    void SetData(void const *, int, int);
    void Stop(bool);
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

    int GetVoice();

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
    short mChannels; // 0x4a - channel count. 2 bytes (not int): keeps mTagState and
                     // mSourceVoice at retail's true (tighter) offsets. Verified via
                     // Ghidra decompile of ~Voice (target 0x82b662e8): mSourceVoice
                     // reads at this+0x50; and StreamReceiver360::Tag()'s objdiff
                     // shows target/base off:+4 on the mTagState store (real 0x4c).
                     // No unk54 filler either -- mTagState(int) ends exactly at 0x50.
    int mTagState; // 0x4c - stream tag state
    int mSourceVoice; // 0x50 - IXAudio2SourceVoice* (as int for vtable dispatch)
    int mEnvelopeEffect; // 0x54 - XAPO envelope generator (PoolVoice.eg)
    void *mEnvelopeParams; // 0x58 - envelope effect parameters (PoolVoice.egParams)
    tWAVEFORMATEX mWaveFormat; // 0x5c - cached wave format (0x12 bytes, copied in createOrReuse)
    short mPadding76; // 0x6e - alignment padding
    int mDisposeTick; // 0x70 - GetTickCount() timestamp for voice GC

private:
    void UpdateMix();
    void UpdateSends();
    void SafeRestart();
    void SetSendImpl(FxSend360 *);
    void dispose(int *, unsigned int);
    long createOrReuse(PoolVoice *, unsigned int &, tWAVEFORMATEX &, XAUDIO2_VOICE_SENDS *);
};

unsigned long StartVoiceThreadEntry(void *);
void StopSynchronizedVoices();
void TerminateVoiceThread();

extern bool gHasPendingStopCommits;
