#include "synth_xbox/Voice.h"
#include "synth_xbox/FxSend.h"
#include "synth_xbox/Synth.h"
#include "math/Utl.h"
#include "os/CritSec.h"
#include "os/Debug.h"
#include "os/Timer.h"
#include <deque>
#include <list>
#include <vector>
#include "xdk/win_types.h"
#include "xdk/xapilibi/processthreadsapi.h"
#include "xdk/xapilibi/synchapi.h"
#include "xdk/xapilibi/xbase.h"
#include "xdk/xapilibi/xbox.h"

HANDLE gEvent;
HANDLE gVoiceThread;
int Voice::sHeadsetTarget;
CriticalSection gLockPendingLists;
CriticalSection gVoiceGC;
std::list<Voice *> gPendingVoices;
std::list<Voice *> gPendingSyncVoices;
std::list<Voice *> gInProgressVoices;
std::list<Voice *> gInProgressSyncVoices;
std::deque<PoolVoice> s_voiceGC;
std::deque<PoolVoice> s_voiceGCInProgress;

bool gShutdownVoiceThread = false;
bool gCommitSyncVoices = false;
int gCommitTag = 0;
bool gHasPendingStopCommits = false;
bool gWasCommitSyncVoices = false;
int gWasCommitTag = 0;
int rolling = 0;
void StartSynchronizedVoices();

typedef void (*VoiceCallFunc)(int*, int*);
typedef void (*PoolVoiceCallFunc)(int*, int, int);
typedef HRESULT (*EndLoopFunc)(int *, int);

int Voice::GetVoice() { return mSourceVoice; }

Voice::Voice(bool b1, int i, bool b2)
    : mState(0), mBuffer(0), mAudioBytes(0), mNumSamples(0), mSampleRate(0), mStartSamp(0), mLoopStart(-1),
      mLoopEnd(-1), mVolume(1.0f), mPan(0), mSpeed(1.0f), mAttackRate(0.001f), mReleaseRate(0.001f),
      mXMA(b1), mFxSend(), mReverbEnabled(false), mReverbMixDb(-96.0f), unk48(false), mSynchronized(b2),
      mChannels(i), mTagState(0), unk54(false) {
    mEnvelopeEffect = 0;
    mEnvelopeParams = 0;
    mSourceVoice = 0;
    if (gEvent == INVALID_HANDLE_VALUE) {
        gEvent = CreateEventA(0, 0, 0, 0);
        MILO_ASSERT(gEvent, 0xfa);
        gVoiceThread = CreateThread(0, 0x10000, StartVoiceThreadEntry, 0, 4, 0);
        MILO_ASSERT(gVoiceThread, 0xff);
        SetThreadPriority(gVoiceThread, 0xf);
        DWORD ret = XSetThreadProcessor(gVoiceThread, 2);
        MILO_ASSERT(ret != -1, 0x107);
        ret = ResumeThread(gVoiceThread);
        MILO_ASSERT(ret != -1, 0x10c);
    }
}

Voice::~Voice() {
    for (;;) {
        int state = mState;
        if (state != 2)
            break;

        if (mSynchronized) {
            StartSynchronizedVoices();
        }
        Sleep(0);
    }

    if (mFxSend) {
        int *pVar1 = (int *)mFxSend;
        int *pVar2 = (int *)(*pVar1);
        VoiceCallFunc fn = (VoiceCallFunc)(*(int *)(*pVar2 + 0x10));
        fn(pVar1, (int*)this);
    }

    if (mSourceVoice) {
        int *pVar1 = (int *)mSourceVoice;
        int *pVar2 = (int *)(*pVar1);
        PoolVoiceCallFunc fn = (PoolVoiceCallFunc)(*(int *)(*pVar2 + 0x50));
        fn(pVar1, 0, 0);
        dispose(pVar1, mState);
    }
}

void Voice::dispose(int *, unsigned int) {}

void Voice::SetSampleRate(int i) {
    mSampleRate = i;
    MILO_ASSERT(0 < mSampleRate && mSampleRate <= 48000, 0x2c9);
}

void Voice::SetLoopRegion(int loopStart, int loopEnd) {
    MILO_ASSERT_RANGE(loopStart, 0, mNumSamples, 0x2cf);
    MILO_ASSERT(loopEnd == -1 || loopEnd > loopStart, 0x2d0);
    mLoopStart = loopStart;
    mLoopEnd = loopEnd;
}

void Voice::SetReverbEnable(bool b) {
    if (mReverbEnabled == b)
        return;
    mReverbEnabled = b;
    UpdateSends();
}

void Voice::SetVolume(float f) {
    if (f != mVolume) {
        mVolume = f;
        if (4.0f < f) {
            MILO_NOTIFY("A gain of %f is rather loud", mVolume);
            mVolume = 4.0f;
        }
        UpdateMix();
    }
}

void Voice::SetPan(float f) {
    float mod = Mod(f - -4.0f, 8.0f);
    if (mod - 4.0f != mPan) {
        mPan = mod - 4.0f;
        UpdateMix();
    }
}

void Voice::SetStartSamp(int samp) {
    MILO_ASSERT(samp >= 0, 0x31e);
    MILO_ASSERT(samp < mNumSamples, 799);
    mStartSamp = samp;
}

void Voice::SetReverbMixDb(float f) {
    mReverbMixDb = f;
    UpdateMix();
}

void Voice::EndLoop() {
    // Call IXAudio2SourceVoice::ExitLoop(0) via vtable at offset 0x60
    int *pSourceVoice = (int *)mSourceVoice;
    HRESULT hr = ((EndLoopFunc)(*(int *)(*(int *)pSourceVoice + 0x60)))(pSourceVoice, 0);
    MILO_ASSERT(SUCCEEDED(hr), 0x2da);
}

void Voice::Start() { blockingStart(false); }

void Voice::SetData(const void *buffer, int bytes, int i) {
    MILO_ASSERT(buffer, 299);
    MILO_ASSERT(bytes >= 0, 300);
    mBuffer = buffer;
    mAudioBytes = bytes;
    if (i != 0) {
        mNumSamples = i;
    } else {
        MILO_ASSERT(!mXMA, 0x136);
        mNumSamples = bytes / 2;
        if (1 < mChannels) {
            MILO_ASSERT((mNumSamples & (mChannels)) == 0, 0x13a);
            mNumSamples = mNumSamples / mChannels;
        }
    }
}

void Voice::InitSourceBuffer(XAUDIO2_BUFFER &audio_buffer) {
    audio_buffer.pAudioData = (BYTE *)mBuffer;
    audio_buffer.AudioBytes = mAudioBytes;
    audio_buffer.pContext = 0;
    audio_buffer.PlayBegin = mStartSamp;
    audio_buffer.PlayLength = 0;
    if (mLoopStart >= 0) {
        if (mLoopEnd < 0) {
            mLoopEnd = mNumSamples;
        }
        if (mXMA) {
            mLoopStart = mLoopStart - (mLoopStart % 128);
            mLoopEnd = mLoopEnd - (mLoopEnd % 128);
        }
        audio_buffer.LoopCount = 0xff;
        audio_buffer.LoopBegin = mLoopStart;
        audio_buffer.LoopLength = mLoopEnd - mLoopStart;
    } else {
        audio_buffer.LoopBegin = 0;
        audio_buffer.LoopCount = 0;
        audio_buffer.LoopLength = 0;
    }
    audio_buffer.Flags = 0x40;
}

void StartSynchronizedVoices() {
    if (gShutdownVoiceThread)
        return;
    gLockPendingLists.Enter();
    gCommitSyncVoices = true;
    gCommitTag = 1;
    if (gEvent != (HANDLE)-1) {
        SetEvent(gEvent);
    }
    gLockPendingLists.Exit();
}

void StopSynchronizedVoices() {
    if (gShutdownVoiceThread || !gHasPendingStopCommits)
        return;
    gLockPendingLists.Enter();
    gHasPendingStopCommits = false;
    gCommitSyncVoices = true;
    gCommitTag = 2;
    if (gEvent != INVALID_HANDLE_VALUE) {
        SetEvent(gEvent);
    }
    gLockPendingLists.Exit();
}

void TerminateVoiceThread() {
    gShutdownVoiceThread = true;
    if (gEvent != INVALID_HANDLE_VALUE) {
        SetEvent(gEvent);
    }
    if (gVoiceThread != INVALID_HANDLE_VALUE) {
        WaitForSingleObject(gVoiceThread, 500);
        CloseHandle(gVoiceThread);
    }
}

bool Voice::HasPendingVoices() {
    if (gShutdownVoiceThread)
        return false;
    gLockPendingLists.Enter();
    int count1 = 0;
    for (std::list<Voice *>::iterator it = gPendingVoices.begin();
         it != gPendingVoices.end(); ++it) {
        count1++;
    }
    int count2 = 0;
    for (std::list<Voice *>::iterator it = gPendingSyncVoices.begin();
         it != gPendingSyncVoices.end(); ++it) {
        count2++;
    }
    bool result = (count1 + count2) > 0;
    gLockPendingLists.Exit();
    return result;
}

void Voice::blockingStart(bool b) {
    if (gShutdownVoiceThread || (unsigned int)TheXboxSynth->unkf0 == 0)
        return;
    gLockPendingLists.Enter();
    Init(b);
    int *pVoice = (int *)mSourceVoice;
    HRESULT hr =
        ((HRESULT(*)(int *, int, int))(*(int *)(*(int *)pVoice + 0x4c)))(pVoice, 0, mSynchronized != 0);
    MILO_ASSERT(SUCCEEDED(hr), 0x29b);
    mState = 3;
    gLockPendingLists.Exit();
}

void Voice::Stop(bool immediate) {
    if (mSourceVoice) {
        if (immediate) {
            int *pVoice = (int *)mSourceVoice;
            ((void (*)(int *, int, int))(*(int *)(*(int *)pVoice + 0x50)))(pVoice, 0, 0);
        } else {
            MILO_ASSERT(mEnvelopeParams, 0x14d);
            *(float *)((int *)mEnvelopeParams + 2) = 1.0f;
            int *pVoice = (int *)mSourceVoice;
            HRESULT hr = ((HRESULT(*)(int *, int, int, int, int))(*(int *)(*(int *)pVoice + 0x18)))(
                pVoice, 0, (int)mEnvelopeParams, 0x10, 0
            );
            MILO_ASSERT(SUCCEEDED(hr), 0x150);
        }
    }
    mState = 1;
}

void Voice::Pause(bool b) {
    int isPaused = (mState == 4);
    if (!(b == isPaused) && IsPlaying()) {
        if (mSynchronized && mState == 2 && b) {
            StartSynchronizedVoices();
        }
        while (mState == 2) {
            Sleep(0);
        }
        MILO_ASSERT(GetVoice(), 0x2b4);
        if (b) {
            gHasPendingStopCommits = true;
            int *pVoice = (int *)mSourceVoice;
            bool sync = mSynchronized;
            HRESULT hr =
                ((HRESULT(*)(int *, int, int))(*(int *)(*(int *)pVoice + 0x50)))(pVoice, 0, sync ? 2 : 0);
            MILO_ASSERT(SUCCEEDED(hr), 700);
            mState = 4;
        } else {
            SafeRestart();
        }
    }
}

void Voice::SetSpeed(float speed) {
    float min_speed = 0.01f;
    float *pSpeed = &speed;
    if (speed <= min_speed)
        pSpeed = &min_speed;
    float clamped = *pSpeed;
    float max_speed = 2.0f;
    if (clamped > max_speed && mXMA) {
        MILO_NOTIFY_ONCE("can't pitch an XMA sound up more than one octave");
        clamped = max_speed;
    }
    mSpeed = clamped;
    if (mSourceVoice != 0) {
        int *pVoice = (int *)mSourceVoice;
        ((void (*)(int *, float, int))(*(int *)(*(int *)pVoice + 0x68)))(pVoice, mSpeed, 0);
    }
}

void Voice::SetSend(FxSend360 *send) {
    if (mFxSend == send)
        return;
    SetSendImpl(send);
}

void Voice::SetSendImpl(FxSend360 *send) {
    if (mFxSend) {
        int *pSend = (int *)mFxSend;
        ((void (*)(int *, Voice *))(*(int *)(*(int *)pSend + 0x10)))(pSend, this);
    }
    if (send) {
        ((void (*)(FxSend360 *, Voice *))(*(int *)(*(int *)send + 0x0c)))(send, this);
    }
    mFxSend = send;
    UpdateSends();
}

void Voice::SafeRestart() {
    MILO_ASSERT(mSourceVoice, 0x471);
    int *pVoice = (int *)mSourceVoice;
    bool sync = mSynchronized != 0;
    ((void (*)(int *, int, bool))(*(int *)(*(int *)pVoice + 0x4c)))(pVoice, 0, sync);
    mState = 3;
}

int Voice::GetAddr() {
    if (mSourceVoice == 0 || mXMA)
        return 0;

    int *pVoice = (int *)mSourceVoice;
    XAUDIO2_VOICE_STATE state;
    ((void (*)(int *, XAUDIO2_VOICE_STATE *, int))(*(int *)(*(int *)pVoice + 0x64)))(pVoice, &state, 0);

    int addr = mStartSamp + (unsigned int)state.SamplesPlayed;
    const void *buf = mBuffer;
    if (buf != 0) {
        int bytesPerSample = mChannels * 2;
        int samplesInBuffer = mAudioBytes / bytesPerSample;
        unsigned int uaddr = (unsigned int)addr;
        addr = (int)(uaddr - (uaddr / (unsigned int)samplesInBuffer) * (unsigned int)samplesInBuffer) * mChannels;
    } else {
        addr = mChannels * addr;
    }
    return addr << 1;
}

bool Voice::IsPlaying() {
    if (mState == 2)
        return true;
    if (!mSourceVoice || mState == 1)
        return false;
    if (mState == 4)
        return true;

    int state[4] = {0, 0, 0, 0};
    int *pVoice = (int *)mSourceVoice;
    ((void (*)(int *, int *, int))(*(int *)(*(int *)pVoice + 100)))(pVoice, state, 0);

    if (state[0] == 0 && state[1] == 0 && state[2] == 0)
        return false;

    return true;
}

void Voice::Init(bool b) {
    if ((unsigned int)TheXboxSynth->unkf0 == 0)
        return;
    if (!b) {
        mState = 1;
    }
    MILO_ASSERT(0 < mSampleRate && mSampleRate <= 48000, 0x160);
    MILO_ASSERT(mBuffer, 0x161);

    // If FxSend360 has no submix voices, rebuild the FxSend chain
    if (mFxSend) {
        if (mFxSend->unk8.empty()) {
            FxSend *fs = dynamic_cast<FxSend *>(mFxSend);
            fs->RebuildChain();
        }
    }

    // Build send descriptors
    XAUDIO2_SEND_DESCRIPTOR sendDesc;
    sendDesc.Flags = 0;
    IXAudio2Voice *outputVoice;
    if (mFxSend) {
        outputVoice = (IXAudio2Voice *)(*(int *)((char *)mFxSend + 4));
    } else {
        outputVoice = (IXAudio2Voice *)TheXboxSynth->unkf0;
    }
    sendDesc.pOutputVoice = outputVoice;

    std::vector<XAUDIO2_SEND_DESCRIPTOR> sends;
    if (outputVoice) {
        sends.push_back(sendDesc);
    }

    // Add reverb send if enabled
    if (mReverbEnabled) {
        sendDesc.Flags = 0;
        sendDesc.pOutputVoice = (IXAudio2Voice *)TheXboxSynth->unkf8;
        sends.push_back(sendDesc);
        unk48 = true;
    }

    // Add headset send if available
    XAUDIO2_SEND_DESCRIPTOR *pOldData = sends.data();
    sendDesc.Flags = 0;
    sendDesc.pOutputVoice = (IXAudio2Voice *)TheXboxSynth->GetHeadsetSubmix(sHeadsetTarget);
    if (sendDesc.pOutputVoice) {
        sends.push_back(sendDesc);
        pOldData = sends.data();
    }

    // Build voice sends structure
    int sendCount = ((char *)sends.end() - (char *)pOldData) >> 3;
    XAUDIO2_VOICE_SENDS voiceSends;
    voiceSends.SendCount = sendCount;
    voiceSends.pSends = (sendCount != 0) ? pOldData : 0;

    // Initialize source buffer and voice parameters
    XAUDIO2_BUFFER audioBuffer;
    InitSourceBuffer(audioBuffer);
    XMA2WAVEFORMATEX fmt;
    InitVoiceParameters(fmt, audioBuffer);

    // Create or reuse source voice
    MILO_ASSERT(!GetVoice(), 0x194);
    XAUDIO2_VOICE_SENDS *pSends = 0;
    if (voiceSends.SendCount != 0) {
        pSends = &voiceSends;
    }
    HRESULT hr = createOrReuse((PoolVoice *)&mSourceVoice, (unsigned int &)unk0, fmt.wfx, pSends);
    MILO_ASSERT(SUCCEEDED(hr), 0x19d);
    MILO_ASSERT(GetVoice(), 0x19e);

    // Submit source buffer
    int *pVoice = (int *)mSourceVoice;
    hr = ((HRESULT(*)(int *, XAUDIO2_BUFFER *, int))(*(int *)(*(int *)pVoice + 0x54)))(
        pVoice, &audioBuffer, 0
    );
    MILO_ASSERT(SUCCEEDED(hr), 0x1a3);

    // Update mix and frequency
    UpdateMix();
    if (mSourceVoice) {
        pVoice = (int *)mSourceVoice;
        ((void (*)(int *, float, int))(*(int *)(*(int *)pVoice + 0x68)))(pVoice, mSpeed, 0);
    }

    // Set envelope parameters
    ((float *)mEnvelopeParams)[0] = mAttackRate;
    ((float *)mEnvelopeParams)[1] = mReleaseRate;
    ((float *)mEnvelopeParams)[2] = 0.0f;
    ((float *)mEnvelopeParams)[3] = 0.0f;
    pVoice = (int *)mSourceVoice;
    hr = ((HRESULT(*)(int *, int, void *, int, int))(*(int *)(*(int *)pVoice + 0x18)))(
        pVoice, 0, mEnvelopeParams, 0x10, 0
    );
    MILO_ASSERT(SUCCEEDED(hr), 0x1b0);
}

void Voice::InitVoiceParameters(XMA2WAVEFORMATEX &fmt, XAUDIO2_BUFFER buf) {
    if (mXMA) {
        fmt.wfx.wFormatTag = 0x166;
        fmt.wfx.nChannels = mChannels;
        fmt.wfx.nSamplesPerSec = mSampleRate;
        fmt.wfx.wBitsPerSample = 0x10;
        fmt.wfx.cbSize = 0x22;
        fmt.NumStreams = 1;
        fmt.wfx.nBlockAlign = (fmt.wfx.nChannels * fmt.wfx.wBitsPerSample) / 8;
        if (mChannels == 1) {
            fmt.ChannelMask = 4;
        } else if (mChannels == 2) {
            fmt.ChannelMask = 3;
        } else if (mChannels == 5) {
            fmt.ChannelMask = 0x60f;
        }
        fmt.SamplesEncoded = mNumSamples;
        fmt.PlayBegin = buf.PlayBegin;
        fmt.BytesPerBlock = 0x10000;
        fmt.PlayLength = buf.PlayLength;
        fmt.LoopBegin = buf.LoopBegin;
        fmt.LoopLength = buf.LoopLength;
        fmt.LoopCount = buf.LoopCount;
        fmt.EncoderVersion = 4;
        float duration = (float)(long long)mAudioBytes * 1.5258789e-05f;
        fmt.BlockCount = (unsigned short)ceil(duration);
    } else {
        fmt.wfx.wFormatTag = 1;
        fmt.wfx.nChannels = mChannels;
        fmt.wfx.nSamplesPerSec = mSampleRate;
        fmt.wfx.wBitsPerSample = 16;
        fmt.wfx.nBlockAlign = (fmt.wfx.nChannels * fmt.wfx.wBitsPerSample) / 8;
        fmt.wfx.nAvgBytesPerSec = (unsigned int)fmt.wfx.nBlockAlign * fmt.wfx.nSamplesPerSec;
        fmt.wfx.cbSize = 0;
    }
}

unsigned long StartVoiceThreadEntry(void *) {
    rolling++;
    WaitForSingleObject(gEvent, INFINITE);
    while (!gShutdownVoiceThread) {
        gLockPendingLists.Enter();
        gInProgressVoices = gPendingVoices;
        gPendingVoices.clear();

        gWasCommitSyncVoices = false;
        if (gCommitSyncVoices) {
            gCommitSyncVoices = false;
            gWasCommitSyncVoices = true;
            gWasCommitTag = gCommitTag;
            gInProgressSyncVoices = gPendingSyncVoices;
            gPendingSyncVoices.clear();
        }
        gLockPendingLists.Exit();

        if (gInProgressVoices.size() > 0) {
            for (std::list<Voice *>::iterator it = gInProgressVoices.begin();
                 it != gInProgressVoices.end(); ++it) {
                (*it)->blockingStart(true);
            }
            gInProgressVoices.clear();
        }

        if (gInProgressSyncVoices.size() > 0) {
            for (std::list<Voice *>::iterator it = gInProgressSyncVoices.begin();
                 it != gInProgressSyncVoices.end(); ++it) {
                (*it)->blockingStart(true);
            }
            gInProgressSyncVoices.clear();
        }

        if (gWasCommitSyncVoices && TheXboxSynth) {
            int *pMasterVoice = (int *)TheXboxSynth->unkec;
            HRESULT hr =
                ((HRESULT(*)(int *, int))(*(int *)(*(int *)pMasterVoice + 0x34)))(pMasterVoice, 0);
            MILO_ASSERT(SUCCEEDED(hr), 0x76);
        }

        // Process voice garbage collection
        gVoiceGC.Enter();
        int gcCount = 0;
        while (!s_voiceGC.empty() && gcCount < 4) {
            s_voiceGCInProgress.push_back(s_voiceGC.front());
            s_voiceGC.pop_front();
            gcCount++;
        }
        gVoiceGC.Exit();

        if (TheXboxSynth) {
            CriticalSection *cs = &TheXboxSynth->unkb0;
            cs->Enter();
            for (std::deque<PoolVoice>::iterator it = s_voiceGCInProgress.begin();
                 it != s_voiceGCInProgress.end(); ++it) {
                PoolVoice &pv = *it;
                if (pv.sourceVoice) {
                    int *pSv = (int *)pv.sourceVoice;
                    ((void (*)(int *, int))(*(int *)(*(int *)pSv + 0x48)))(pSv, 0);
                }
                if (pv.eg) {
                    int *pEg = (int *)pv.eg;
                    ((void (*)(int *, int))(*(int *)(*(int *)pEg + 0x38)))(pEg, 1);
                    pv.eg = 0;
                    PoolFree(0x10, (void *)pv.egParams, __FILE__, 0x1e, "EnvelopeGeneratorParams");
                    pv.egParams = 0;
                }
            }
            cs->Exit();
        }
        s_voiceGCInProgress.clear();

        rolling++;
        WaitForSingleObject(gEvent, INFINITE);
    }
    return 0;
}
