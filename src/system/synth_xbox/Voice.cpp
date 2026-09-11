#include "synth_xbox/Voice.h"
#include "synth_xbox/EnvelopeGenerator.h"
#include "synth_xbox/FxSend.h"
#include "synth_xbox/Synth.h"
#include "math/Decibels.h"
#include "math/Utl.h"
#include "os/CritSec.h"
#include "os/Debug.h"
#include "utl/MemMgr.h"
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
static int gVoiceCounters[2]; // retail 0x82E120BC: createOrReuse does [0]++
int rolling = 0;
void StartSynchronizedVoices();

typedef void (*PoolVoiceCallFunc)(int*, int, int);
typedef HRESULT (*EndLoopFunc)(int *, int);

int Voice::GetVoice() { return mSourceVoice; }

Voice::Voice(bool b1, int i, bool b2)
    : mState(0), mBuffer(0), mAudioBytes(0), mNumSamples(0), mSampleRate(0), mStartSamp(0), mLoopStart(-1),
      mLoopEnd(-1), mVolume(1.0f), mPan(0), mSpeed(1.0f), mAttackRate(0.001f), mReleaseRate(0.001f),
      mXMA(b1), mFxSend(), mReverbEnabled(false), mReverbMixDb(-96.0f), unk48(false), mSynchronized(b2),
      mStereo(i > 1), unk4b(false), mTagState(0) {
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
        mFxSend->RemoveOwnerVoice(this);
    }

    if (mSourceVoice) {
        int *pVar1 = (int *)mSourceVoice;
        int *pVar2 = (int *)(*pVar1);
        PoolVoiceCallFunc fn = (PoolVoiceCallFunc)(*(int *)((int)pVar2 + 0x50));
        fn(pVar1, 0, 0);
        dispose((PoolVoice *)&mSourceVoice, unk0);
    }
}

// Written from retail 0x82B64DD8 (312 B); declared and called (Init) but never
// defined in this tree until now -- the match build only compiles, so nothing
// could flag it.  Four places it is NOT dc3's:
//   * no `if (!TheXboxSynth->OutputVoice()) return 0;` early-out;
//   * effectDesc.OutputChannels is the literal 1 (`li r25,1; stw r25,0x68(r31)`),
//     not a channel count;
//   * no hr-failure arm (no snprintf / MemPrintOverview / MILO_FAIL) -- the
//     return value is hr itself (`mr r28,r3` ... `mr r3,r28`);
//   * unk4b = (sends == 0 || sends->SendCount <= 0): `cmplwi r27,0; beq ->1;
//     lwz r11,0(r27); cmplwi r11,0; li r11,0; ble ->1; stb r11,0x4b(r24)`.
//     dc3 stores the OPPOSITE polarity (SendCount > 0).
// CreateSourceVoice is IXAudio2 slot 0x20 with (this, ppVoice, pFormat, 0,
// f1=maxFreqRatio, 0, sends, &chain); the critsec is TheXboxSynth+0x88
// (`addic. r30, r11, 0x88` / Enter ... Exit) and MemPushTemp/MemPopTemp
// bracket the whole thing (fn_827BC270 / fn_827BC2A0).
long Voice::createOrReuse(
    PoolVoice *pPoolVoice, unsigned int &, tWAVEFORMATEX &wfx, XAUDIO2_VOICE_SENDS *sends
) {
    MILO_ASSERT(pPoolVoice->eg == 0, 0x1c1);
    pPoolVoice->eg = (int)new EnvelopeGenerator();
    MILO_ASSERT(pPoolVoice->egParams == 0, 0x1c3);
    pPoolVoice->egParams = (int)new EnvelopeGeneratorParams;

    // chain before desc: retail keeps the chain at r31+0x58 and the descriptor
    // at r31+0x60 (declaration order is what places them).
    XAUDIO2_EFFECT_CHAIN effectChain;
    XAUDIO2_EFFECT_DESCRIPTOR effectDesc;
    effectDesc.InitialState = 0;
    effectChain.EffectCount = 1;
    effectDesc.pEffect = (IUnknown *)pPoolVoice->eg;
    effectDesc.OutputChannels = 1;
    effectChain.pEffectDescriptors = &effectDesc;

    MemPushTemp();

    HRESULT hr;
    {
        // CritSecTracker: ctor Enter / dtor Exit both inlined, the pointer kept
        // at r31+0x50 for the EH funclet (fn_82B64F60 -> ??1CritSecTracker@@).
        CritSecTracker tracker(&TheXboxSynth->unk88);
        int *pEngine = (int *)TheXboxSynth->unkc8;
        hr = ((HRESULT(*)(
            int *,
            PoolVoice *,
            tWAVEFORMATEX *,
            int,
            float,
            int,
            XAUDIO2_VOICE_SENDS *,
            XAUDIO2_EFFECT_CHAIN *
        ))(*(int *)(*(int *)pEngine + 0x20)))(
            pEngine, pPoolVoice, &wfx, 0, 4.0f, 0, sends, &effectChain
        );
    }
    gVoiceCounters[0]++;
    memcpy(&pPoolVoice->wfx, &wfx, 0x12);
    if (sends == 0 || sends->SendCount <= 0) {
        unk4b = true;
    } else {
        unk4b = false;
    }
    MemPopTemp();
    return hr;
}

// Retail 0x82B661F0 (248 B; it was pinned under Lit.cpp until lane W3-D).  Two
// things the old body here did not do: when unk4b is set (the voice was
// created with no output voice) retail detaches it with an EMPTY
// XAUDIO2_VOICE_SENDS {0,0} via SetOutputVoices (slot 0x4) and clears unk4b
// before pooling; and the two voice counters at 0x82E120BC move inside the GC
// lock ([0]--, [1]++).
void Voice::dispose(PoolVoice *voice, unsigned int) {
    ((IXAudio2SourceVoice *)voice->sourceVoice)->FlushSourceBuffers();
    if (unk4b) {
        XAUDIO2_VOICE_SENDS emptySends;
        emptySends.SendCount = 0;
        emptySends.pSends = 0;
        ((IXAudio2SourceVoice *)voice->sourceVoice)->SetOutputVoices(&emptySends);
        unk4b = false;
    }
    voice->disposeTick = GetTickCount() - 500000;
    gVoiceGC.Enter();
    s_voiceGC.push_back(*voice);
    gVoiceCounters[0]--;
    gVoiceCounters[1]++;
    gVoiceGC.Exit();
    voice->eg = 0;
    voice->egParams = 0;
    voice->sourceVoice = 0;
}

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

// Retail 0x82B65C58 is eight bytes: `stb r4, 0x40(r3); b UpdateSends`.  There
// is no early-out compare -- the compare this function used to carry was
// StreakMeter::CombineMultipliers' shape (0x822D8610, bool at this+0x1e8),
// which the map had mislabelled as this function (lane W3-D, 2026-09-11).
void Voice::SetReverbEnable(bool b) {
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
        if (1 < NumChannels()) {
            MILO_ASSERT((mNumSamples & (NumChannels())) == 0, 0x13a);
            mNumSamples = mNumSamples / NumChannels();
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
    gLockPendingLists.Enter();
    gCommitSyncVoices = true;
    gCommitTag = 1;
    if (gEvent != (HANDLE)-1) {
        SetEvent(gEvent);
    }
    gLockPendingLists.Exit();
}

void StopSynchronizedVoices() {
    if (!gHasPendingStopCommits)
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
    if (gShutdownVoiceThread || (unsigned int)TheXboxSynth->unkcc == 0)
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

// Retail 0x82B64D60 never reads its bool: there is no immediate-stop arm here.
// The only immediate IXAudio2SourceVoice::Stop (vtable +0x50) in Voice.cpp is
// the one ~Voice issues inline before dispose().  The parameter is kept
// because retail's callers still pass it (SampleInst360::StopImpl(bool) is
// `lwz r3,0x54(r3); b fn_82B64D60`, r4 untouched) and DC3's map spells this
// ?Stop@Voice@@QAAX_N@Z; dc3's two-arm body is DC3's, not RB3's.
void Voice::Stop(bool /* immediate -- ignored by retail */) {
    if (mSourceVoice) {
        MILO_ASSERT(mEnvelopeParams, 0x14d);
        *(float *)((int *)mEnvelopeParams + 2) = 1.0f;
        int *pVoice = (int *)mSourceVoice;
        HRESULT hr = ((HRESULT(*)(int *, int, int, int, int))(*(int *)(*(int *)pVoice + 0x18)))(
            pVoice, 0, (int)mEnvelopeParams, 0x10, 0
        );
        MILO_ASSERT(SUCCEEDED(hr), 0x150);
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

// Reconstructed from retail 0x82B65510 (936 B).  Not ported from dc3: dc3's
// Voice::UpdateMix is materially different (its mChannels>1 arm builds a
// 12-entry matrix, and both of its output-voice lookups fall back to
// TheXboxSynth->OutputVoice(); retail here has neither).  What the two DO
// share is the 5.1 pan ring -- identical constants, identical arc order --
// and the cos/cos shipping bug in the 2-channel reverb send.
void Voice::UpdateMix() {
    // Retail compares the STORED int, not a pointer: `cmpwi`, signed.
    if (mSourceVoice == 0)
        return;

    if (mStereo) {
        // Stereo source: no panning law at all, just the gain.
        // IXAudio2Voice::SetVolume is vtable+0x30.
        int *pVoice = (int *)mSourceVoice;
        ((void (*)(int *, float, int))(*(int *)(*(int *)pVoice + 0x30)))(pVoice, mVolume, 0);
        return;
    }

    int destChannels = 6;
    // ...whereas the output voice IS compared as a pointer: `cmplwi`.  Both
    // output-voice tests below re-evaluate the ternary rather than caching it,
    // which is what retail does (it loads mFxSend and its unk4 twice per site).
    if ((int *)(mFxSend ? mFxSend->unk4 : 0) != 0) {
        XAUDIO2_VOICE_DETAILS details;
        int *pOut = (int *)(mFxSend ? mFxSend->unk4 : 0);
        // IXAudio2Voice::GetVoiceDetails is vtable+0x0.
        ((void (*)(int *, XAUDIO2_VOICE_DETAILS *))(*(int *)(*(int *)pOut + 0x0)))(
            pOut, &details
        );
        destChannels = details.InputChannels;
    }

    float levels[6];
    for (int i = 0; i < 6; i++) {
        levels[i] = 0.0f;
    }

    // Constant-power pan around the 5.1 ring.  mPan runs -4..4; the ring is
    // six arcs, each interpolating between two speakers.  Deliberately left
    // uninitialised on the paths retail leaves them uninitialised on -- the
    // reverb block below reads them back unconditionally, which is how retail
    // behaves and is why they are function-scope.
    int loChannel, hiChannel;
    float loPan, hiPan;
    if (destChannels == 6 || destChannels == 2) {
        if (mPan < -3.0f) {
            loPan = -3.0f;
            loChannel = 4;
            hiChannel = 5;
            hiPan = -5.0f;
        } else if (mPan < -1.0f) {
            loPan = -1.0f;
            loChannel = 0;
            hiChannel = 4;
            hiPan = -3.0f;
        } else if (mPan < 0.0f) {
            loPan = -1.0f;
            loChannel = 0;
            hiChannel = 2;
            hiPan = 0.0f;
        } else if (mPan < 1.0f) {
            loChannel = 2;
            loPan = 0.0f;
            hiChannel = 1;
            hiPan = 1.0f;
        } else if (mPan < 3.0f) {
            loChannel = 1;
            loPan = 1.0f;
            hiChannel = 5;
            hiPan = 3.0f;
        } else {
            loPan = 3.0f;
            loChannel = 5;
            hiChannel = 4;
            hiPan = 5.0f;
        }
        float angle = (mPan - loPan) / (hiPan - loPan) * 1.5707964f;
        if (destChannels == 6) {
            levels[loChannel] = (float)cos(angle) * mVolume;
            levels[hiChannel] = (float)sin(angle) * mVolume;
        } else {
            levels[0] = (float)cos(angle) * mVolume;
            // Shipping-game bug, reproduced verbatim: the right channel gets
            // cos() a second time instead of sin(), so a 2-channel send is
            // correlated rather than panned.  Both call sites in retail
            // resolve to the same `cos` (0x8282B570); `sin` is 0x8282B490 and
            // is only reached from the 6-channel arm above.
            levels[1] = (float)cos(angle) * mVolume;
        }
    } else if (destChannels == 1) {
        levels[0] = mVolume;
    }

    if ((int *)(mFxSend ? mFxSend->unk4 : 0) == 0) {
        if (unk4b) {
            // IXAudio2Voice::SetOutputMatrix is vtable+0x40.
            int *pVoice = (int *)mSourceVoice;
            ((void (*)(int *, int, int, int, float *, int))(
                *(int *)(*(int *)pVoice + 0x40)
            ))(pVoice, 0, 1, 6, levels, 0);
        }
    } else {
        int *pVoice = (int *)mSourceVoice;
        ((void (*)(int *, int, int, int, float *, int))(
            *(int *)(*(int *)pVoice + 0x40)
        ))(pVoice, mFxSend ? mFxSend->unk4 : 0, 1, destChannels, levels, 0);
    }

    if (mReverbEnabled && unk48) {
        float ratio = DbToRatio(mReverbMixDb);
        for (int i = 0; i < 6; i++) {
            levels[i] = 0.0f;
        }
        float angle = (mPan - loPan) / (hiPan - loPan) * 1.5707964f;
        if (destChannels == 6) {
            levels[loChannel] = (float)cos(angle) * ratio;
            levels[hiChannel] = (float)sin(angle) * ratio;
        } else {
            levels[0] = (float)cos(angle) * ratio;
            levels[1] = (float)cos(angle) * ratio;
        }
        ((void (*)(int *, int, int, int, float *, int))(
            *(int *)(*(int *)(int *)mSourceVoice + 0x40)
        ))((int *)mSourceVoice, TheXboxSynth->unkd4, 1, destChannels, levels, 0);
    }
}

// Reconstructed from retail 0x82B65948 (272 B).  Declared in Voice.h and called
// from SetReverbEnable and SetSendImpl, but defined in NO translation unit until
// now -- the X360 match build only compiles, so a missing definition is invisible
// to it.  Every line below is read off retail's own listing.
void Voice::UpdateSends() {
    // Retail compares the STORED int, signed (`cmpwi`), exactly as UpdateMix does.
    if (mSourceVoice == 0)
        return;

    XAUDIO2_VOICE_SENDS voiceSends;
    XAUDIO2_SEND_DESCRIPTOR sendDesc;

    // Flags is cleared BEFORE the output-voice lookup -- retail issues
    // `li r30,0` / `stw r30, 0x58(r1)` ahead of the mFxSend test, so the
    // assignment cannot be folded into a struct initialiser after it.
    sendDesc.Flags = 0;
    IXAudio2Voice *outputVoice = (IXAudio2Voice *)(mFxSend ? mFxSend->unk4 : 0);
    sendDesc.pOutputVoice = outputVoice;
    voiceSends.SendCount = 1;
    voiceSends.pSends = &sendDesc;

    if (mReverbEnabled) {
        // reverbDesc is scoped, not sendDesc[1].  Retail overlays it on the
        // same 8-byte slot (r1+0x60) as the empty XAUDIO2_VOICE_SENDS in the
        // other arm below, which only happens if both are inner-scope locals;
        // a function-scope `XAUDIO2_SEND_DESCRIPTOR sends[2]` reserves its slot
        // for the whole frame and pushes the empty one to its own (measured:
        // that spelling scores 94.1%, this one 100.0%).
        XAUDIO2_SEND_DESCRIPTOR reverbDesc;
        reverbDesc.Flags = 0;
        reverbDesc.pOutputVoice = (IXAudio2Voice *)TheXboxSynth->unkd4;
        voiceSends.SendCount = 1;
        voiceSends.pSends = &reverbDesc;
        // SHIPPING BUG, reproduced verbatim.  pSends still points at reverbDesc
        // when the count goes to 2, so XAudio2 reads a second descriptor from
        // the 8 uninitialised bytes past it (r1+0x68).  The layout shows the
        // intent: sendDesc is at r1+0x58 and reverbDesc at r1+0x60, i.e. already
        // adjacent and in the right order, so `pSends = &sendDesc` with count 2
        // would have been correct.  Retail stores to voiceSends.pSends at
        // exactly two sites (0x822D93A8 and 0x822D93D0) and neither is on this
        // path -- it is the re-point above that was never undone, not a missing
        // store objdiff could be hiding.  Effect: with reverb ON and an FxSend
        // output voice present, the dry send is dropped and a garbage send takes
        // its place.
        // The ternary is spelled out again rather than reusing outputVoice
        // because retail re-loads mFxSend->unk4 here (`lwz r11, 0x4(r10)`) while
        // keeping the first result live in r9 for the test further down; the cast
        // is load-bearing -- unk4 is an int, and an uncast test compiles to a
        // signed `cmpwi` where retail has `cmplwi`.
        if ((IXAudio2Voice *)(mFxSend ? mFxSend->unk4 : 0))
            voiceSends.SendCount = 2;
    }

    int *pVoice = (int *)mSourceVoice;
    // IXAudio2Voice::SetOutputVoices is vtable+0x4.
    if (outputVoice == 0 && !mReverbEnabled) {
        XAUDIO2_VOICE_SENDS emptySends;
        emptySends.pSends = 0;
        emptySends.SendCount = 0;
        ((void (*)(int *, XAUDIO2_VOICE_SENDS *))(*(int *)(*(int *)pVoice + 0x4)))(
            pVoice, &emptySends
        );
        unk4b = false;
        unk48 = false;
    } else {
        ((void (*)(int *, XAUDIO2_VOICE_SENDS *))(*(int *)(*(int *)pVoice + 0x4)))(
            pVoice, &voiceSends
        );
        unk4b = true;
        if (mReverbEnabled)
            unk48 = true;
    }
    UpdateMix();
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
    ((void (*)(int *, XAUDIO2_VOICE_STATE *))(*(int *)(*(int *)pVoice + 0x64)))(pVoice, &state);

    int addr = mStartSamp + (unsigned int)state.SamplesPlayed;
    const void *buf = mBuffer;
    if (buf != 0) {
        int bytesPerSample = NumChannels() * 2;
        int samplesInBuffer = mAudioBytes / bytesPerSample;
        unsigned int uaddr = (unsigned int)addr;
        addr = (int)(uaddr - (uaddr / (unsigned int)samplesInBuffer) * (unsigned int)samplesInBuffer) * NumChannels();
    } else {
        addr = NumChannels() * addr;
    }
    return addr << 1;
}

bool Voice::IsPlaying() {
    if (mState == 2)
        return true;
    int voice = mSourceVoice; // retail tests this as a signed int (cmpwi), not a pointer
    if (voice == 0)
        return false;
    int *pVoice = (int *)voice;
    if (mState == 1)
        return false;
    if (mState == 4)
        return true;

    // Retail 0x82B65058: GetState (vtable +0x64), and "not playing" only when
    // BuffersQueued (+4) and the 64-bit SamplesPlayed (+8) are BOTH zero.
    // Otherwise the answer comes from the envelope generator: read its
    // 0x10-byte parameter block back (GetEffectParameters, +0x1c) under the
    // synth's critical section (this+0x88, TryEnter/Exit) and report playing
    // while the last word is still 0.0f -- the release-done flag.  The old
    // body here tested three words of the state struct and never consulted the
    // envelope at all; dc3's body has retail's shape exactly.
    XAUDIO2_VOICE_STATE state;
    ((void (*)(int *, XAUDIO2_VOICE_STATE *))(*(int *)(*(int *)pVoice + 0x64)))(pVoice, &state);
    if (state.BuffersQueued == 0 && state.SamplesPlayed == 0)
        return false;

    EnvelopeGeneratorParams params;
    params.unkc = 0.0f;
    if (TheXboxSynth->unk88.TryEnter()) {
        pVoice = (int *)mSourceVoice; // retail reloads this+0x50 here (GetVoice())
        HRESULT hr = ((HRESULT(*)(int *, int, void *, int))(*(int *)(*(int *)pVoice + 0x1c)))(
            pVoice, 0, &params, 0x10
        );
        TheXboxSynth->unk88.Exit();
        MILO_ASSERT(SUCCEEDED(hr), 0x2ff);
    }
    return params.unkc == 0.0f;
}

void Voice::Init(bool b) {
    if ((unsigned int)TheXboxSynth->unkcc == 0)
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
        outputVoice = (IXAudio2Voice *)TheXboxSynth->unkcc;
    }
    sendDesc.pOutputVoice = outputVoice;

    std::vector<XAUDIO2_SEND_DESCRIPTOR> sends;
    if (outputVoice) {
        sends.push_back(sendDesc);
    }

    // Add reverb send if enabled
    if (mReverbEnabled) {
        sendDesc.Flags = 0;
        sendDesc.pOutputVoice = (IXAudio2Voice *)TheXboxSynth->unkd4;
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
        // Retail (0x82B652E0) HARDCODES a mono XMA2 stream: nChannels, nBlockAlign
        // and ChannelMask are all literals here (`li r10,1` / `li r29,2` /
        // `li r11,4` -> sth 0x2 / sth 0xc / stw 0x14), with no branch on the
        // channel count at all.  We used to write NumChannels(), the derived
        // (nChannels * wBitsPerSample) / 8, and a three-way ChannelMask arm
        // including a 5-channel 0x60f case; RB3 retail has no such arm, and the
        // difference was worth 67 percentage points on this function (32.7 ->
        // 100.0) and 80 bytes of extra code.  So an XMA2 voice is always
        // announced to XAudio2 as 1 channel / SPEAKER_FRONT_CENTER even when
        // mStereo is set -- the stereo case is only honoured on the PCM path
        // below.  Left exactly as retail has it.  No HX_NATIVE arm is needed:
        // native/CMakeLists.txt excludes all of synth_xbox from the native build
        // (platform-only guest), so nothing natively depends on the 5.1 spelling.
        fmt.wfx.wFormatTag = 0x166;
        fmt.wfx.nChannels = 1;
        fmt.wfx.nSamplesPerSec = mSampleRate;
        fmt.wfx.wBitsPerSample = 0x10;
        fmt.wfx.nBlockAlign = 2;
        fmt.wfx.cbSize = 0x22;
        fmt.NumStreams = 1;
        fmt.ChannelMask = 4;
        fmt.SamplesEncoded = mNumSamples;
        fmt.BytesPerBlock = 0x10000;
        fmt.PlayBegin = buf.PlayBegin;
        fmt.PlayLength = buf.PlayLength;
        fmt.LoopBegin = buf.LoopBegin;
        fmt.LoopLength = buf.LoopLength;
        fmt.LoopCount = buf.LoopCount;
        fmt.EncoderVersion = 4;
        float duration = (float)(long long)mAudioBytes * 1.5258789e-05f;
        fmt.BlockCount = (unsigned short)ceil(duration);
    } else {
        // The PCM path DOES honour the channel count -- retail derives it from
        // mStereo at 0x4a inline (`lbz` / `cntlzw` / `extrwi` / `xori` /
        // `addi r11,r11,1`), which is what NumChannels() spells.
        fmt.wfx.wFormatTag = 1;
        fmt.wfx.nChannels = NumChannels();
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
            int *pMasterVoice = (int *)TheXboxSynth->unkc8;
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
            CriticalSection *cs = &TheXboxSynth->unk88;
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
