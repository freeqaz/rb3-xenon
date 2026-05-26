#include "synth/StandardStream.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/System.h"
#include "synth/ADSR.h"
#include "synth/Synth.h"
#include "utl/MemMgr.h"
#include "utl/Std.h"
#include "synth/StreamReceiver.h"
#include "synth/StreamReceiverFile.h"
#ifdef HX_NATIVE
#include "platform/StreamReceiver_Native.h"
#endif
#include "utl/Symbol.h"
#include <cmath>
#include <functional>
#ifdef HX_NATIVE
// std::mem_fun removed in C++17; use std::mem_fn (no underscore)
#define mem_fun mem_fn
#endif
#include "math/Decibels.h"
#include "math/Utl.h"

bool StandardStream::sReportLargeTimerErrors = true;
#ifdef HX_NATIVE
float StandardStream::sAudioOffsetMs = 0.0f;
#endif

StandardStream::ChannelParams::ChannelParams()
    : mPan(0.0f), mSlipSpeed(1.0f), mSlipEnabled(0), mADSR(), mFaders(nullptr),
      mFxSend(nullptr) {
    static Symbol _parent("_parent");
    static Symbol _default("_default");
    mFaders.AddLocal(_parent)->SetVolume(0);
    mFaders.AddLocal(_default)->SetVolume(0);
}

StandardStream::StandardStream(
    File *f, float f1, float f2, Symbol ext, bool b1, bool b2, bool b3
)
    : mPollingEnabled(b2), unk158(b3) {
    MILO_ASSERT(f, 0x4A);
    mExt = ext;
    mFile = f;
    mInfoChannels = -1;
    unkec = -1;
    Init(f1, f2, ext, false);
}

StandardStream::~StandardStream() {
    RELEASE(mFile);
    for (int i = 0; i < unk10c.size(); i++) {
        delete unk10c[i];
    }
    Destroy();
    DeleteAll(mChanParams);
    while (!mVirtBufs.empty()) {
        MemFree(mVirtBufs.back());
        mVirtBufs.pop_back();
    }
}

#ifdef HX_WEB
void StandardStream::UpdateWebDebugLabels() {
    String baseLabel = mDebugTag;
    if (baseLabel.empty()) {
        String fileNameStr = mFile ? mFile->Filename() : String("--no-file--");
        baseLabel = FileGetName(fileNameStr.c_str());
    }
    for (int i = 0; i < mChannels.size(); i++) {
        StreamReceiverNative *webRcvr = dynamic_cast<StreamReceiverNative *>(mChannels[i]);
        if (webRcvr) {
            String label = MakeString("%s ch%d", baseLabel.c_str(), i);
            webRcvr->SetDebugLabel(label.c_str());
        }
    }
}

void StandardStream::SetDebugTag(const char *tag) {
    if (tag) {
        mDebugTag = tag;
    } else {
        mDebugTag = "";
    }
    UpdateWebDebugLabels();
}
#endif

bool StandardStream::Fail() { return mRdr && mRdr->Fail(); }
bool StandardStream::IsReady() const {
    return mState == kReady || mState == kPlaying || mState == kStopped;
}
bool StandardStream::IsFinished() const { return mState == kFinished; }
int StandardStream::GetNumChannels() const { return mChannels.size(); }
int StandardStream::GetNumChanParams() const { return mChanParams.size(); }

void StandardStream::Play() {
#ifdef HX_NATIVE
    // On Xbox, a background decode thread processes Vorbis headers between
    // stream creation and Play(). On native there's no decode thread, so we
    // must pump the reader until the stream transitions from kInit → kReady.
    if (mState == kInit && mRdr) {
        for (int i = 0; i < 500 && !IsReady(); i++) {
            PollStream();
        }
    }
#endif
    if (!IsReady() && mState != kSuspended) {
        MILO_FAIL(
            "StandardStream::Play() failed. IsReady=%d mState=%d", IsReady(), mState
        );
    }
    UpdateVolumes();
#ifdef HX_NATIVE
    // Pre-fill ring buffers before registering with AudioDevice.
    // The IsReady pump above only runs until header parsing completes (kReady).
    // Without pre-filling, the audio callback fires on empty buffers for the
    // first frame(s), causing an audible gap and delayed timing.
    for (int i = 0; i < 20; i++) {
        PollStream();
    }
#endif
    std::for_each(mChannels.begin(), mChannels.end(), std::mem_fun(&StreamReceiver::Play));
    mState = kPlaying;
    mTimer.Start();
}

void StandardStream::Stop() {
    if (mState == kPlaying) {
        std::for_each(
            mChannels.begin(), mChannels.end(), std::mem_fun(&StreamReceiver::Stop)
        );
        mState = kStopped;
        mTimer.Stop();
    }
}

bool StandardStream::IsPlaying() const { return mState == kPlaying; }
bool StandardStream::IsPaused() const { return mState == kStopped; }

void StandardStream::Resync(float f) {
    int bytes;
    while (!mFile->ReadDone(bytes))
        ;
    Destroy();
    mFile->Seek(0, 0);
    float f88 = mJumpFromMs;
    float f8c = mJumpToMs;
    String str94 = mJumpFile;
    Init(f, mBufSecs, mExt, true);
    if (f88 != 0) {
        SetJump(f88, f8c, str94.c_str());
    }
}

void StandardStream::EnableReads(bool b) {
    if (mRdr)
        mRdr->EnableReads(b);
}

float StandardStream::GetTime() {
    if (!mChannels.empty() && mSampleRate != 0) {
#ifdef HX_NATIVE
        return mLastStreamTime + sAudioOffsetMs;
#else
        return mLastStreamTime;
#endif
    } else
        return mStartMs;
}

float StandardStream::GetJumpBackTotalTime(float time) const {
    bool foundJump = true;
    float lastTime = 0.0f;
    float loopback = 0.0f;

    int count = (int)mJumpInstances.size() - 1;
    int i = count;
    while (i >= 0) {
        const JumpInstance& jump = mJumpInstances[i];
        float jumpTime = jump.unk8;
        if (time >= jumpTime) {
            loopback = jump.unkc;
            lastTime = jumpTime;
            break;
        }
        foundJump = false;
        i--;
    }

    if (foundJump && mJumpFromSamples != mJumpToSamples) {
        float totalLoopback = loopback + lastTime;
        float jumpFromMs = SampToMs(mJumpFromSamples);
        float jumpToMs = SampToMs(mJumpToSamples);
        if (totalLoopback < jumpFromMs) {
            float adjustedTime = (time - lastTime) + totalLoopback;
            if (adjustedTime >= jumpFromMs) {
                loopback = loopback + (jumpToMs - jumpFromMs);
            }
        }
    }

    return loopback;
}

float StandardStream::GetInSongTime() {
    float time = GetTime();
    return time + GetJumpBackTotalTime(time);
}

void StandardStream::SetVolume(int chan, float vol) {
    MILO_ASSERT_RANGE(chan, 0, mChanParams.size(), 0x41E);
    static Symbol _default("_default");
    mChanParams[chan]->mFaders.FindLocal(_default, true)->SetVolume(vol);
}

float StandardStream::GetVolume(int chan) const {
    MILO_ASSERT_RANGE(chan, 0, mChanParams.size(), 0x444);
    return mChanParams[chan]->mFaders.GetVolume();
}

void StandardStream::SetPan(int chan, float pan) {
    MILO_ASSERT_RANGE(chan, 0, mChanParams.size(), 0x426);
    mChanParams[chan]->mPan = pan;
    if (!mChannels.empty()) {
        mChannels[chan]->SetPan(pan);
    }
}

float StandardStream::GetPan(int chan) const {
    MILO_ASSERT_RANGE(chan, 0, mChanParams.size(), 0x44C);
    return mChanParams[chan]->mPan;
}

void StandardStream::SetFXSend(int channel, FxSend *send) {
    MILO_ASSERT_RANGE(channel, 0, mChanParams.size(), 0x4D9);
    mChanParams[channel]->mFxSend = send;
    if (!mChannels.empty()) {
        mChannels[channel]->SetFXSend(send);
    }
}

void StandardStream::SetSpeed(float speed) {
    mSpeed = speed;
    for (int i = 0; i < mChannels.size(); i++) {
        UpdateSpeed(i);
    }
    mTimer.SetSpeed(speed);
}

void StandardStream::LoadMarkerList(const char *cc) {
    ClearMarkerList();
    FileStream stream(cc, FileStream::kRead, 1);
    int i94 = 0;
    stream >> i94;
    int i98 = 0;
    stream >> i98;
    for (int i = 0; i < i98; i++) {
        Marker marker;
        stream >> marker.name;
        stream >> marker.position;
        marker.posMS = ((float)marker.position * 1000.0f) / (float)i94;
        AddMarker(marker);
    }
}

void StandardStream::ClearMarkerList() { mMarkerList.clear(); }
void StandardStream::AddMarker(const Marker &marker) { mMarkerList.push_back(marker); }
int StandardStream::MarkerListSize() const { return mMarkerList.size(); }

bool StandardStream::MarkerAt(int idx, Marker &marker) const {
    if (idx >= MarkerListSize() || idx < 0)
        return false;
    else {
        marker = mMarkerList[idx];
        return true;
    }
}

void StandardStream::SetJump(float fromMs, float toMs, const char *file) {
    MILO_ASSERT(toMs >= 0, 0x2C0);
    MILO_ASSERT(fromMs >= 0 || fromMs == kStreamEndMs, 0x2C1);
    if (IsPastStreamJumpPointOfNoReturn()) {
        MILO_NOTIFY(
            "Trying to set loop points when we're already past the point of no return!"
        );
    }
    mJumpFromMs = fromMs;
    mJumpToMs = toMs;
    mJumpFile = file;
    if (!mJumpFile.empty()) {
        mJumpFile += ".";
        mJumpFile += mExt.Str();
    }
    if (GetSampleRate() == 0)
        mJumpSamplesInvalid = true;
    else
        setJumpSamplesFromMs(fromMs, toMs);
}

void StandardStream::SetJump(String &s1, String &s2) {
    for (int i = 0; i < mMarkerList.size(); i++) {
        if (mMarkerList[i].name == s1) {
            mStartMarker = mMarkerList[i];
        }
        if (mMarkerList[i].name == s2) {
            mEndMarker = mMarkerList[i];
        }
    }
    SetJump(mStartMarker.posMS, mEndMarker.posMS, nullptr);
}

bool StandardStream::CurrentJumpPoints(Marker &start, Marker &end) {
    if (mJumpFromSamples == 0) {
        return false;
    } else {
        start = mStartMarker;
        end = mEndMarker;
        return true;
    }
}

void StandardStream::ClearJump() {
    mJumpFromSamples = 0;
    mJumpFromMs = 0;
    mJumpToSamples = 0;
    mJumpToMs = 0;
}

void StandardStream::EnableSlipStreaming(int channel) {
    MILO_ASSERT(mChannels.empty(), 0x4AF);
    MILO_ASSERT_RANGE(channel, 0, mChanParams.size(), 0x4B0);
    mChanParams[channel]->mSlipEnabled = true;
}

void StandardStream::SetSlipOffset(int channel, float offset) {
    MILO_ASSERT_RANGE(channel, 0, mChanParams.size(), 0x4B7);
    mChannels[channel]->SetSlipOffset(offset);
}

void StandardStream::SlipStop(int channel) {
    MILO_ASSERT_RANGE(channel, 0, mChanParams.size(), 0x4BE);
    mChannels[channel]->SlipStop();
}

float StandardStream::GetSlipOffset(int channel) {
    MILO_ASSERT_RANGE(channel, 0, mChanParams.size(), 0x4CF);
    return mChannels[channel]->GetSlipOffset();
}

void StandardStream::SetSlipSpeed(int channel, float speed) {
    MILO_ASSERT_RANGE(channel, 0, mChanParams.size(), 0x4C5);
    MILO_ASSERT(mChanParams[channel]->mSlipEnabled, 0x4C6);
    mChanParams[channel]->mSlipSpeed = speed;
    if (!mChannels.empty()) {
        UpdateSpeed(channel);
    }
}

FaderGroup &StandardStream::ChannelFaders(int channel) {
    MILO_ASSERT_RANGE(channel, 0, mChanParams.size(), 0x48D);
    return mChanParams[channel]->mFaders;
}

void StandardStream::AddVirtualChannels(int i) {
    MILO_ASSERT(mChannels.empty(), 0x495);
    mVirtualChans += i;
}

void StandardStream::RemapChannel(int i1, int i2) {
    mChanMaps.push_back(std::make_pair(i1, i2));
}

float StandardStream::GetRawTime() {
    float bytes = mChannels[0]->GetBytesPlayed() / 2;
    return (bytes / (float)mSampleRate) * 1000.0f + mStartMs;
}

void StandardStream::SetADSR(int chan, const ADSRImpl &adsr) {
    MILO_ASSERT_RANGE(chan, 0, mChanParams.size(), 0x43A);
    mChanParams[chan]->mADSR = adsr;
    if (!mChannels.empty()) {
        mChannels[chan]->SetADSR(adsr);
    }
}

void StandardStream::SetJumpSamples(int fromSamples, int toSamples, const char *file) {
    MILO_ASSERT(toSamples >= 0, 0x2F6);
    MILO_ASSERT(fromSamples >= 0 || fromSamples == kStreamEndSamples, 0x2F7);
    MILO_ASSERT(file || fromSamples > toSamples || fromSamples == kStreamEndSamples, 0x2F8);
    MILO_ASSERT(mJumpFromSamples == 0, 0x2FA);
    mJumpFromSamples = fromSamples;
    mJumpToSamples = toSamples;
    mJumpFile = file;
    if (!mJumpFile.empty()) {
        mJumpFile += ".";
        mJumpFile += mExt.Str();
    }
    mJumpSamplesInvalid = false;
}

const char *StandardStream::GetSoundDisplayName() {
    if (!IsPlaying()) {
        return gNullStr;
    } else if (mFile) {
        return MakeString("StandardStream: %s", FileGetName(mFile->Filename().c_str()));
    } else {
        return MakeString("StandardStream: --no file--");
    }
}

void StandardStream::SynthPoll() { PollStream(); }

void StandardStream::PollStream() {
    mFrameTimer.Restart();

    float lastMs = mFrameTimer.GetLastMs();
    float minPoll = mState == kBuffering ? 8.0f : 1.0f;
    mRdr->Poll(Max(lastMs * mThrottle, minPoll));

    std::for_each(
        mChannels.begin(), mChannels.end(), std::mem_fun(&StreamReceiver::Poll)
    );

    switch (mState) {
    case kInit:
    case kReady:
        break;
    case kBuffering:
        if (StuffChannels()) {
            mState = kReady;
        }
        break;
    case kPlaying:
    case kSuspended:
    case kStopped:
        StuffChannels();
        if (mChannels[0]->mDoneBufferCounter > mChannels[0]->mNumBuffers + 2) {
            mState = kFinished;
        }
        break;
    case kFinished:
        break;
    default:
        MILO_FAIL("bad state logic.");
        break;
    }

    if (mState != kInit && mJumpFromSamples != 0) {
        if (mJumpFromSamples < 0) {
            if (mRdr->Done()) {
                DoJump();
            }
        } else if (mJumpFromSamples > 0) {
            if (mJumpFromSamples < mJumpToSamples) {
                if (mCurrentSamp >= mJumpFromSamples && mCurrentSamp < mJumpToSamples) {
                    DoJump();
                }
            } else if (mJumpFromSamples > mJumpToSamples) {
                if (mCurrentSamp >= mJumpFromSamples) {
                    DoJump();
                }
            }
        }
    }

    UpdateVolumes();
    UpdateTime();
}

void StandardStream::UpdateTime() {
    if (mChannels.empty() || mSampleRate == 0) {
        mLastStreamTime = mStartMs;
        return;
    }

    float rawTime = GetRawTime();

#ifdef HX_NATIVE
    // In headless mode (no real audio device), the audio callback fires very
    // slowly, making rawTime lag far behind wall-clock time. Use an independent
    // wall-clock timer (not mTimer, which gets drift-corrected toward rawTime)
    // to detect this. Once detected, bypass drift correction permanently.
    if (mState == kPlaying && !mUseTimerFallback) {
        if (!mWallClockStarted) {
            mWallClock.Start();
            mWallClockStarted = true;
        }
        mWallClock.Split();
        float wallElapsed = mWallClock.Ms();
        if (wallElapsed > 500.0f) {
            float audioElapsed = rawTime - mStartMs;
            if (audioElapsed < wallElapsed * 0.1f) {
                // Audio output is < 10% real-time — switch to wall-clock timing
                mUseTimerFallback = true;
                // Sync mTimer to match current wall-clock elapsed time from song start
                mTimer.Reset(mStartMs + wallElapsed);
            }
        }
    }
    if (mUseTimerFallback) {
        mLastStreamTime = mTimer.Ms();
        return;
    }
#endif

    float quantized = floorf(rawTime * 0.1875f + 0.5f) * 5.3333335f;

    float timerMs = mTimer.Ms();
    float adjusted = timerMs - (rawTime - quantized);
    float adjustedQuantized = floorf(adjusted * 0.1875f);

    if (quantized != adjustedQuantized * 5.3333335f) {
        float drift = quantized - adjusted;
        if (drift < 0.0f) {
            drift += 5.3333335f;
        }

        if (fabsf(drift) < 5.3333335f) {
            drift *= 0.1f;
        }

        mTimer.Reset(mTimer.Ms() + drift);

        if (fabsf(drift) > 50.0f && sReportLargeTimerErrors) {
            MILO_LOG("timer error is large: %f\n", drift);
        }
    }

    mLastStreamTime = mTimer.Ms();
}

void StandardStream::UpdateTimeByFiltering() {
    if (mChannels.empty() || mSampleRate == 0) {
        mLastStreamTime = mStartMs;
        return;
    }

    float drift = GetRawTime() - mTimer.Ms();

    if (fabsf(drift) > 50.0f) {
        if (sReportLargeTimerErrors) {
            MILO_LOG("timer error is large: %f\n", drift);
        }
    } else {
        drift *= 0.1f;
    }

    mTimer.Reset(mTimer.Ms() + drift);
    mLastStreamTime = mTimer.Ms();
}


void StandardStream::Init(float f1, float f2, Symbol s, bool b4) {
    ClearJumpMarkers();
    mBufSecs = f2;
    mGetInfoOnly = false;
    mState = kInit;
    mSampleRate = 0;
    if (mBufSecs == 0) {
        SystemConfig("synth")->FindData("stream_buf_size", mBufSecs);
    }
    mFileStartMs = f1;
    mStartMs = f1;
    mLastStreamTime = f1;
    mTimer.Reset(f1);
    mFloatSamples = false;
    if (!b4) {
        MILO_ASSERT(mChanParams.empty(), 0x6E);
        mChanParams.resize(32);
        for (int i = 0; i < 32; i++) {
            mChanParams[i] = new ChannelParams();
        }
        mVirtualChans = 0;
        mSpeed = 1;
    } else {
        while (mChanParams.size() < 32) {
            mChanParams.push_back(new ChannelParams());
        }
    }
    mJumpFromMs = 0;
    mJumpFromSamples = 0;
    mJumpToMs = 0;
    mJumpToSamples = 0;
    mCurrentSamp = 0;
    mThrottle = SystemConfig("synth", "oggvorbis")->FindFloat("throttle");
    if (mPollingEnabled) {
        StartPolling();
    }
    mRdr = TheSynth->NewStreamDecoder(mFile, this, s);
}

void StandardStream::InitInfo(int i1, int sampleRate, bool floatSamples, int i4) {
    unk154 = i4;
    int numChannels = mVirtualChans + i1;
    unkec = (mInfoChannels / sampleRate);
    mInfoChannels = numChannels;
    auto& _ref2 = mSampleRate;
    if (!mGetInfoOnly) {
        if (_ref2 == 0) {
            mFloatSamples = floatSamples;
            _ref2 = sampleRate;
            int bufBytes = mBufSecs * sampleRate * 2.0f;
#ifndef HX_NATIVE
            MILO_ASSERT(bufBytes % (2*kStreamBufSize) == 0, 0x13F);
#endif
            bufBytes >>= 0xE;
            SystemConfig("synth", "iop")->FindInt("max_slip");
            for (int i = 0; i < numChannels; i++) {
                if (unk158) {
                    mChannels.push_back(
                        new StreamReceiverFile(bufBytes, mChanParams[i]->mSlipEnabled)
                    );
                } else {
                    mChannels.push_back(
                        StreamReceiver::New(
                            bufBytes, sampleRate, mChanParams[i]->mSlipEnabled, i
                        )
                    );
                }
            }
#ifdef HX_WEB
            UpdateWebDebugLabels();
#endif
            for (int i = 0; i < mVirtualChans; i++) {
                void *buf = MemAlloc(
                    mFloatSamples ? 0x1000 : 0x800, __FILE__, 0x159, "stream mVirtBufs"
                );
                mVirtBufs.push_back(buf);
            }
            mState = kBuffering;
        } else {
            MILO_ASSERT(numChannels == mChannels.size(), 0x161);
            MILO_ASSERT(_ref2 == sampleRate, 0x162);
            MILO_ASSERT(mFloatSamples == floatSamples, 0x163);
        }
        if (mJumpSamplesInvalid) {
            setJumpSamplesFromMs(mJumpFromMs, mJumpToMs);
        }
        int i;
        MILO_ASSERT(mChanParams.size() >= numChannels, 0x16C);
        for (i = 0; i < numChannels; i++) {
            mChannels[i]->SetPan(mChanParams[i]->mPan);
            UpdateSpeed(i);
            mChannels[i]->SetADSR(mChanParams[i]->mADSR);
        }
        for (; i < mChanParams.size(); i++) {
            delete mChanParams[i];
        }
        mChanParams.resize(numChannels);
        mCurrentSamp = MsToSamp(mFileStartMs);
        if (mCurrentSamp != 0) {
            mRdr->Seek(mCurrentSamp);
        }
        mFaders->SetDirty();
        UpdateFXSends();
    }
}

void StandardStream::ClearJumpMarkers() {
    mStartMarker.position = 0;
    mStartMarker.name = "";
    mEndMarker.position = 0;
    mEndMarker.name = "";
    mJumpInstances.clear();
}

void StandardStream::Destroy() {
    RELEASE(mRdr);
    DeleteAll(mChannels);
}

int StandardStream::MsToSamp(float ms) const {
    MILO_ASSERT(mSampleRate, 0x459);
    return mSampleRate * ms / 1000.0f;
}

float StandardStream::SampToMs(int samples) const {
    MILO_ASSERT(mSampleRate, 0x460);
    float ms = (float)samples / (float)mSampleRate;
    return ms * 1000.0f;
}

void StandardStream::UpdateVolumes() {
    static Symbol _parent("_parent");
    static Symbol _default("_default");

    // If the master faders are dirty, propagate to each channel's _parent local fader
    if (mFaders->Dirty()) {
        float masterVol = mFaders->GetVolume();
        for (std::vector<ChannelParams *>::iterator it = mChanParams.begin();
             it != mChanParams.end(); ++it) {
            (*it)->mFaders.FindLocal(_parent, true)->SetVolume(masterVol);
        }
        mFaders->ClearDirty();
    }

    // Apply per-channel volume to StreamReceiver
    for (int i = 0; i < mChannels.size(); i++) {
        if (mChanParams[i]->mFaders.Dirty()) {
            float vol = mChanParams[i]->mFaders.GetVolume();
            float ratio = DbToRatio(vol);
            ClampEq(ratio, 0.0f, 1.0f);
            mChannels[i]->SetVolume(ratio);
            mChanParams[i]->mFaders.ClearDirty();
        }
    }
}

void StandardStream::UpdateFXSends() {
    for (int i = 0; i < mChannels.size(); i++) {
        mChannels[i]->SetFXSend(mChanParams[i]->mFxSend);
    }
}

void StandardStream::UpdateSpeed(int chn) {
    MILO_ASSERT_RANGE(chn, 0, mChanParams.size(), 0x4A2);
    mChannels[chn]->SetSpeed(mSpeed);
    if (mChanParams[chn]->mSlipEnabled) {
        mChannels[chn]->SetSlipSpeed((float)mSpeed * mChanParams[chn]->mSlipSpeed);
    }
}

bool StandardStream::StuffChannels() {
    bool ret = true;
    for (int i = 0; i < mChannels.size(); i++) {
        if (!mChannels[i]->Ready())
            ret = false;
    }
    if (mRdr->Done() && mJumpFromSamples == 0) {
        std::for_each(
            mChannels.begin(), mChannels.end(), std::mem_fun(&StreamReceiver::EndData)
        );
    }
    return ret;
}

float StandardStream::GetBufferAheadTime() const {
    float time = 0;
    if (mSampleRate != 0) {
        time = SampToMs(mCurrentSamp);
    }
    return time;
}

// TODO: implement
#ifdef HX_NATIVE
int StandardStream::ConsumeData(void **v, int numSamples, int startSamp) {
    if (mGetInfoOnly)
        return 0;
    int numChannels = mChannels.size();
    int realChannels = numChannels - mVirtualChans;
    MILO_ASSERT(numChannels != 0, 0x1A9);
    if (startSamp >= 0 && startSamp != mCurrentSamp) {
        MILO_LOG("sample mismatch: expected %i, got %i\n", mCurrentSamp, startSamp);
        mCurrentSamp = startSamp;
    }

    int samplesToConsume = numSamples;
    if (mJumpFromSamples != 0 && mJumpFromSamples != kStreamEndSamples) {
        MILO_ASSERT(mCurrentSamp <= mJumpFromSamples, 0x1CF);
        int remaining = mJumpFromSamples - mCurrentSamp;
        if (remaining < samplesToConsume) {
            samplesToConsume = remaining;
        }
    }

    // Flow control: limit to what the ring buffers can accept.
    // Without this, decoded audio is silently dropped when the buffer is full,
    // causing the Vorbis decoder to advance past unconsumed data.
    for (int i = 0; i < (int)mChannels.size(); i++) {
        StreamReceiverNative *rcvr = static_cast<StreamReceiverNative *>(mChannels[i]);
        int availSamples = rcvr->AvailableWriteBytes() / 2; // bytes → samples (16-bit)
        if (availSamples < samplesToConsume) {
            samplesToConsume = availSamples;
        }
    }
    if (samplesToConsume <= 0)
        return 0;

    int bytesPerSample = mFloatSamples ? 4 : 2;
    int bufSize = samplesToConsume * bytesPerSample;

    // Vorbis decoder always outputs float PCM (vorbis_synthesis_pcmout returns float**),
    // but mFloatSamples=false so bytesPerSample=2. Convert float→int16 before sending
    // to StreamReceiverNative which stores int16 in its ring buffer.
    int16_t *convBuf = (int16_t *)alloca(samplesToConsume * sizeof(int16_t));

    for (int i = 0; i < realChannels; i++) {
        int chanIdx = i;
        for (int j = 0; j < (int)mChanMaps.size(); j++) {
            if (mChanMaps[j].first == i) {
                chanIdx = mChanMaps[j].second;
                break;
            }
        }
        float *src = (float *)v[i];
        for (int s = 0; s < samplesToConsume; s++) {
            float clamped = src[s];
            if (clamped > 1.0f) clamped = 1.0f;
            if (clamped < -1.0f) clamped = -1.0f;
            convBuf[s] = (int16_t)(clamped * 32767.0f);
        }
        mChannels[chanIdx]->WriteData(convBuf, samplesToConsume * 2);
    }
    for (int i = 0; i < mVirtualChans; i++) {
        float *src = (float *)v[realChannels + i];
        for (int s = 0; s < samplesToConsume; s++) {
            float clamped = src[s];
            if (clamped > 1.0f) clamped = 1.0f;
            if (clamped < -1.0f) clamped = -1.0f;
            convBuf[s] = (int16_t)(clamped * 32767.0f);
        }
        memcpy(mVirtBufs[i], convBuf, samplesToConsume * 2);
    }
    mCurrentSamp += samplesToConsume;
    return samplesToConsume;
}
#else
int StandardStream::ConsumeData(void **v, int numSamples, int startSamp) {
    if (mGetInfoOnly)
        return 0;
    int numChannels = mChannels.size();
    int realChannels = numChannels - mVirtualChans;
    MILO_ASSERT(numChannels != 0, 0x1A9);
    if (startSamp >= 0 && startSamp != mCurrentSamp) {
        MILO_LOG("sample mismatch: expected %i, got %i\n", mCurrentSamp, startSamp);
        mCurrentSamp = startSamp;
    }
    void *pcm[0x1E];
    MILO_ASSERT(numChannels < DIM(pcm), 0x1B3);

    int i;
    for (i = 0; i < numChannels; i++) {
        if (i < realChannels)
            pcm[i] = v[i];
        else
            pcm[i] = mVirtBufs[i - realChannels];
    }

    int samplesToConsume = numSamples;
    if (samplesToConsume >= 0x800)
        samplesToConsume = 0x800;

    if (mJumpFromSamples > 0) {
        if (mJumpFromSamples < mJumpToSamples) {
            if (mCurrentSamp < mJumpToSamples) {
                if (mCurrentSamp > mJumpFromSamples) {
                    samplesToConsume = 0;
                } else {
                    int remaining = mJumpFromSamples - mCurrentSamp;
                    if ((unsigned int)remaining < (unsigned int)samplesToConsume)
                        samplesToConsume = remaining;
                }
            }
        } else if (mJumpFromSamples > mJumpToSamples) {
            MILO_ASSERT(mCurrentSamp <= mJumpFromSamples, 0x1CF);
            int remaining = mJumpFromSamples - mCurrentSamp;
            if ((unsigned int)remaining < (unsigned int)samplesToConsume)
                samplesToConsume = remaining;
        }
    }

    for (std::vector<StreamReceiver *>::iterator it = mChannels.begin();
         it != mChannels.end(); ++it) {
        int bytes = (unsigned int)(*it)->BytesWriteable() >> 1;
        if (bytes < samplesToConsume)
            samplesToConsume = bytes;
    }

    if ((unsigned int)samplesToConsume != 0) {
        bool floatSamples = mFloatSamples;
        std::vector<std::pair<int, int> >::iterator mapIt = mChanMaps.begin();
        int copySize = samplesToConsume * (floatSamples ? 4 : 2);
        while (mapIt != mChanMaps.end()) {
            memcpy(pcm[mapIt->second], pcm[mapIt->first], copySize);
            mapIt++;
        }

        short convBuf[0x800];
        int chIdx = 0;
        while (chIdx < numChannels) {
            void *data;
            if (mFloatSamples) {
                if ((unsigned int)samplesToConsume != 0) {
                    int j = 0;
                    while (j < samplesToConsume) {
                        float f = ((float *)pcm[chIdx])[j] * 32767.0f;
                        f = Clamp(-32767.0f, 32767.0f, f);
                        convBuf[j] = (short)f;
                        j++;
                    }
                }
                data = convBuf;
            } else {
                data = pcm[chIdx];
            }
            mChannels[chIdx]->WriteData(data, samplesToConsume << 1);
            chIdx++;
        }
    }

    mCurrentSamp += samplesToConsume;
    return samplesToConsume;
}
#endif

void StandardStream::setJumpSamplesFromMs(float fromMs, float toMs) {
    mJumpFromSamples = kStreamEndSamples;
    mJumpToSamples = 0;
    if (kStreamEndMs != fromMs) {
        mJumpFromSamples = MsToSamp(fromMs);
    }
    if (toMs != 0.0f) {
        mJumpToSamples = MsToSamp(toMs);
        if (SampToMs(mJumpToSamples) < toMs) {
            mJumpToSamples++;
        }
    }
}

bool StandardStream::IsPastStreamJumpPointOfNoReturn() {
    if (mJumpFromSamples == 0)
        return false;
    if (mChannels.empty())
        return false;
    return mCurrentSamp >= mJumpFromSamples;
}

void StandardStream::DoJump() {
    MILO_ASSERT(mJumpFromSamples != 0, 0x316);
    if (!mJumpFile.empty()) {
        delete mFile;
        delete mRdr;
        mFile = NewFile(mJumpFile.c_str(), 2);
        if (!mFile)
            MILO_FAIL("\nCould not open %s", mJumpFile.c_str());
        mRdr = TheSynth->NewStreamDecoder(mFile, this, mExt);
        mFileStartMs = SampToMs(mJumpToSamples);
        mCurrentSamp = 0;
        ClearJump();
    } else {
        if (mJumpFromSamples != mJumpToSamples) {
            if (mRdr)
                mRdr->Seek(mJumpToSamples);
        }
        mCurrentSamp = mJumpToSamples;
    }
    JumpInstance ji;
    ji.unk0 = mJumpFromMs;
    ji.unk4 = mJumpToMs;
    if (mJumpInstances.empty()) {
        ji.unkc = mJumpToMs - mJumpFromMs;
        ji.unk8 = mJumpFromMs;
    } else {
        ji.unkc = (mJumpToMs - mJumpFromMs) + mJumpInstances.back().unkc;
        ji.unk8 = (mJumpFromMs - mJumpInstances.back().unk0) + mJumpInstances.back().unk4;
    }
    mJumpInstances.push_back(ji);
}
