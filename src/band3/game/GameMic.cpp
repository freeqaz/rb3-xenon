#include "game/GameMic.h"
#include "math/Utl.h"
#include "obj/Data.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "synth/Synth.h"
#include "utl/FileStream.h"
#include "utl/MemStream.h"
#include "utl/WaveFile.h"
#include <time.h>

void WriteWav(const char *, int, const void *, int);

// Pitch-detector slot in use, one per GameMic (retail 0x82E030A8).
bool gIdxTaken[6];

void GameMic::SetEnablePitchDetection(bool enable) {
    if (mDetector) {
        mDetector->mEnablePitchDetection = enable;
    }
}

void GameMic::AccessContinuousSamples(const short *&s, int &i) const {
    s = mSamplesContinuous;
    i = mNumSamplesContinuous;
}

Mic *GameMic::GetMyMic() { return TheSynth->GetMic(mMicID); }

void GameMic::ThreadProcessOneFrame() {
    float livePitch = 0.0f;
    float energy = 0.0f;
    TheTaskMgr.Seconds(TaskMgr::kRealTime);
    clock();
    int droppedSamples = 0;
    if (!mPlaybackSampleRate && mMicID != -1) {
        Mic *mic = GetMyMic();
        droppedSamples = mic->GetDroppedSamples();
        short *recentBuf = mic->GetRecentBuf(mNumSamplesRecent);
        if (mNumSamplesRecent > 0x2000)
            mNumSamplesRecent = 0x2000;
        memcpy(mSamplesRecent, recentBuf, mNumSamplesRecent * 2);
        short *continuousBuf = mic->GetContinuousBuf(mNumSamplesContinuous);
        if (mNumSamplesContinuous > 0x2000)
            mNumSamplesContinuous = 0x2000;
        memcpy(mSamplesContinuous, continuousBuf, mNumSamplesContinuous * 2);
    }
    Mic *myMic = GetMyMic();
    float outc = 0.0f;
    float micGain = myMic->unk8;
    const char *micName = myMic->GetName().Str();
    mDetector->AnalyzeBlock(
        micName,
        mSamplesRecent,
        mNumSamplesRecent,
        myMic->GetSensitivity(),
        micGain,
        livePitch,
        energy,
        outc
    );
    energy = Clamp(0.0f, 1.0f, energy / (mMicVolumeClamp * 500.0f));
    float rate = (energy > mEnergy) ? 0.3f : 0.1f;
    mPitch = livePitch;
    mEnergy = rate * energy + (1.0f - rate) * mEnergy;
    if (mWriteWav && TheTaskMgr.Seconds(TaskMgr::kRealTime) >= 0.0f) {
        if (droppedSamples > 0 && droppedSamples < 48000) {
            short *zeros = new short[droppedSamples];
            memset(zeros, 0, droppedSamples * 2);
            mStoredAudio->Write(zeros, droppedSamples * 2);
            delete[] zeros;
        }
        mStoredAudio->Write(mSamplesContinuous, mNumSamplesContinuous * 2);
    }
    if (!mPlaybackSampleRate && (mMicID == -1 || !GetMyMic()->IsConnected())) {
        mPitch = 0.0f;
        mEnergy = 0.0f;
    }
}

int GameMic::GetDataSampleRate() {
    if (mPlaybackSampleRate) {
        return mPlaybackSampleRate;
    }
    if (mMicID != -1) {
        return GetMyMic()->GetSampleRate();
    }
    return 16000;
}

GameMic::~GameMic() {
    if (mWriteWav) {
        WriteWav(
            "mic_output_fonix.wav",
            16000,
            mStoredAudio->Buffer(),
            mStoredAudio->Size()
        );
    }
    RELEASE(mStoredAudio);
    mPlaybackSampleRate = 0;
    gIdxTaken[mFonixIdx] = false;
    delete mDetector;
}

void GameMic::Update() {
    ThreadProcessOneFrame();
    if (mPlaybackSampleRate) {
        float sampleRate = GetDataSampleRate();
        int maxSamples;
        int desired = TheTaskMgr.Seconds(TaskMgr::kRealTime) * sampleRate;
        maxSamples = mStoredAudio->Size() / 2;
        int tellSamples = mStoredAudio->Tell() / 2;
        if (desired < tellSamples) {
            desired = tellSamples;
        } else if (desired > maxSamples) {
            desired = maxSamples;
        }
        mNumSamplesContinuous = desired - ((unsigned int)mStoredAudio->Tell() >> 1);
        MinEq(mNumSamplesContinuous, 8192);
        mStoredAudio->Read(mSamplesContinuous, mNumSamplesContinuous * 2);
        for (int i = 0; i < mNumSamplesContinuous; i++) {
            mSamplesContinuous[i] = (mSamplesContinuous[i] << 8)
                | ((unsigned short)mSamplesContinuous[i] >> 8);
        }
    }
    mLastEnergy = mEnergy;
    mLastPitch = mPitch;
    if (mMicID != -1) {
        mUSB = GetMyMic()->GetType() != 1;
    }
}

int GameMic::SetInputFile(const char *filename) {
    int sampleRate;
    if (!filename) {
        if (mMicID != -1)
            sampleRate = GetMyMic()->GetSampleRate();
        else
            sampleRate = 16000;
        mPlaybackSampleRate = 0;
    } else {
        mWriteWav = false;
        FileStream fs(filename, FileStream::kRead, true);
        WaveFile wav(fs);
        mPlaybackSampleRate = wav.SamplesPerSec();
        WaveFileData data(wav);
        if (!mStoredAudio) {
            mStoredAudio = new MemStream();
        }
        mStoredAudio->Resize(
            (int)(wav.BitsPerSample() * wav.NumChannels() * wav.NumSamples()) / 8
        );
        data.Read(
            (void *)mStoredAudio->Buffer(),
            (int)(wav.BitsPerSample() * wav.NumChannels() * wav.NumSamples()) / 8
        );
        sampleRate = mPlaybackSampleRate;
    }
    delete mDetector;
    mDetector = nullptr;
    mDetector = new PitchDetector(sampleRate);
    return sampleRate;
}

GameMic::GameMic(int id)
    : mMicID(id), mUSB(1), mPlayback(1), mWriteWav(0), mPlaybackSampleRate(0),
      mStoredAudio(0), mDetector(0), mNullMic(0), mMicVolumeClamp(1),
      mNumSamplesRecent(0), mNumSamplesContinuous(0), mSpursActive(0) {
    mFonixIdx = -1;
    for (int i = 0; i < 6; i++) {
        if (!gIdxTaken[i]) {
            mFonixIdx = i;
            gIdxTaken[i] = true;
            break;
        }
    }
    MILO_ASSERT(mFonixIdx != -1, 0x5D);
    mWriteWav = DataVariable("do_record").Int() != 0;
    SetInputFile(nullptr);
    mEnergy = mLastEnergy = 0;
    mPitch = mLastPitch = -1;
    if (mWriteWav) {
        mStoredAudio = new MemStream();
        mStoredAudio->Reserve(0x1c00000);
    }
    memset(mSamplesRecent, 0, sizeof(mSamplesRecent));
    memset(mSamplesContinuous, 0, sizeof(mSamplesContinuous));
}
