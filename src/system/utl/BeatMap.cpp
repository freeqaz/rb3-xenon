#include "utl/BeatMap.h"
#include <algorithm>
#include "os/Endian.h"
#include "os/File.h"

BeatMap gDefaultBeatMap;
BeatMap *TheBeatMap = &gDefaultBeatMap;

BeatMap::BeatMap() {}

void SetTheBeatMap(BeatMap *bmap) { TheBeatMap = bmap; }

void ResetTheBeatMap() { TheBeatMap = &gDefaultBeatMap; }

bool BeatInfoCmp(const BeatInfo &info, int tick) { return info.mTick < tick; }

// M4: real SongData::AddBeat (native rb3-hit) feeds beat events into the
// BeatMap. Our tree's BeatMap.cpp lacked AddBeat (declared in BeatMap.h);
// add the real body. Genuine retail function — ungated for the homing scan
// (X360 A/B verified net-neutral+).
bool BeatMap::AddBeat(int tick, int level) {
    if (mInfos.empty() || mInfos.back().mTick < tick) {
        mInfos.push_back(BeatInfo(tick, level));
        return true;
    } else
        return false;
}

float BeatMap::BeatToTick(float f1) const {
    if (mInfos.empty())
        return f1 * 480.0f;
    else {
        int i2;
        if (f1 < 0) {
            i2 = 0;
        } else {
            if (f1 > mInfos.size() - 2) {
                i2 = mInfos.size() - 2;
            } else {
                i2 = f1;
            }
        }

        const BeatInfo &r30 = mInfos[i2];
        const BeatInfo &r31 = mInfos[i2 + 1];
        int k1 = mInfos[i2].mTick;
        return static_cast<float>(f1 - i2) * static_cast<float>(r31.mTick - r30.mTick)
            + k1;
    }
}

int BeatMap::IsDownbeat(int i1) const {
    if (mInfos.empty())
        return i1 % 4 == 0;
    else if (i1 >= mInfos.size()) {
        return false;
    } else
        return mInfos[i1].mLevel > 0;
}

// https://decomp.me/scratch/h18a4
// matches in retail with the right inline settings
float BeatMap::Beat(int tick) const {
    if (mInfos.empty())
        return (float)tick / 480.0f;
    else {
        int i2;
        if (tick <= mInfos[0].mTick)
            i2 = 0;
        else {
            i2 = mInfos.size();
            if (tick >= mInfos[i2 - 1].mTick)
                i2 = mInfos.size() - 2;
            else {
                const BeatInfo *lowerInfo =
                    &*std::lower_bound(mInfos.begin(), mInfos.end(), tick, BeatInfoCmp);
                i2 = lowerInfo - &mInfos.front() - 1;
            }
        }
        return Interpolate(tick, i2);
    }
}

// also matches in retail with the right inline settings
float BeatMap::Beat(float tick) const {
    if (mInfos.empty())
        return tick / 480.0f;

    int firstTick = mInfos[0].mTick;
    int i2 = tick;
    if (i2 <= firstTick)
        i2 = 0;
    else if (i2 >= mInfos[mInfos.size() - 1].mTick)
        i2 = mInfos.size() - 2;
    else {
        int sp08 = i2;
        const BeatInfo *lowerInfo =
            &*std::lower_bound(mInfos.begin(), mInfos.end(), sp08, BeatInfoCmp);
        i2 = lowerInfo - &mInfos.front() - 1;
    }
    return Interpolate(tick, i2);
}

// Debug dump of a mono 16-bit PCM buffer as a .wav file (the GameMic "do_record"
// path). The RIFF header fields are little-endian, so every size and the samples
// themselves are byte-swapped on the way out.
namespace {
    const char *kWavRiffID = "RIFF";
    const char *kWavWaveID = "WAVE";
    const char *kWavFormatID = "fmt ";
    const char *kWavDataID = "data";

    struct WavChunkHeader {
        int mID;
        unsigned int mSize;
    };

    struct WavFormat {
        unsigned short mFormatTag;
        unsigned short mChannels;
        unsigned int mSampleRate;
        unsigned int mByteRate;
        unsigned short mBlockAlign;
        unsigned short mBitsPerSample;
    };
}

void WriteWav(const char *file, int sampleRate, const void *data, int bytes) {
    int fd = FileOpen(file, 0x301);

    WavChunkHeader riff;
    riff.mSize = EndianSwap((unsigned int)(bytes + 0x24));
    riff.mID = *(const int *)kWavRiffID;
    FileWrite(fd, &riff, sizeof(riff));

    int wave = *(const int *)kWavWaveID;
    FileWrite(fd, &wave, sizeof(wave));

    WavChunkHeader fmt;
    fmt.mSize = EndianSwap((unsigned int)sizeof(WavFormat));
    fmt.mID = *(const int *)kWavFormatID;
    FileWrite(fd, &fmt, sizeof(fmt));

    WavFormat format;
    format.mFormatTag = EndianSwap((unsigned short)1);
    format.mChannels = EndianSwap((unsigned short)1);
    format.mSampleRate = EndianSwap((unsigned int)sampleRate);
    format.mBlockAlign = EndianSwap((unsigned short)2);
    format.mBitsPerSample = EndianSwap((unsigned short)16);
    format.mByteRate = EndianSwap((unsigned int)(sampleRate * 2));
    FileWrite(fd, &format, sizeof(format));

    WavChunkHeader dataHdr;
    dataHdr.mSize = EndianSwap((unsigned int)bytes);
    dataHdr.mID = *(const int *)kWavDataID;
    FileWrite(fd, &dataHdr, sizeof(dataHdr));

    const short *samples = (const short *)data;
    for (int i = 0; i < bytes / 2; i++) {
        short sample = samples[i];
        short swapped = (sample << 8) | ((unsigned short)sample >> 8);
        FileWrite(fd, &swapped, sizeof(swapped));
    }
    FileClose(fd);
}
