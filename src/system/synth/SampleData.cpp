#include "synth/SampleData.h"
#include "synth/WavMgr.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/File.h"
#include "utl/BinStream.h"
#include "utl/ChunkStream.h"
#include "utl/WaveFile.h"
#ifdef HX_NATIVE
#include <cstdlib>
#endif
#ifdef HX_FFMPEG
#include "platform/XmaSampleDecoder.h"
#endif

SampleDataAllocFunc SampleData::sAlloc = nullptr;
SampleDataFreeFunc SampleData::sFree = nullptr;
static const unsigned short gSampleDataMaxRev = 0xE;
static const unsigned short gSampleDataMaxAltRev = 0;

SampleData::SampleData() : mData(0), mMarkers() { Reset(); }
SampleData::~SampleData() { Dealloc(); }

void SampleData::SetAllocator(SampleDataAllocFunc a, SampleDataFreeFunc f) {
    sAlloc = a;
    sFree = f;
    TheWavMgr->SetAllocator((WavMgrAllocFunc)a, (WavMgrFreeFunc)f);
}

void SampleData::Dealloc() {
#ifdef HX_NATIVE
    if (!sFree) {
        free(mData);
        mData = 0;
        return;
    }
#endif
    sFree(mData);
    mData = 0;
}

void SampleData::Reset() {
#ifdef HX_NATIVE
    if (sFree)
        sFree(mData);
    else
        free(mData);
#else
    sFree(mData);
#endif
    mData = 0;
    mFormat = kPCM;
    mSizeBytes = 0;
    mSampleRate = 0;
    mNumSamples = 0;
    mMarkers.clear();
}

int SampleData::NumMarkers() const { return mMarkers.size(); }

const SampleMarker &SampleData::GetMarker(int idx) const { return mMarkers[idx]; }

BinStream &operator<<(BinStream &bs, const SampleMarker &s) {
    s.Save(bs);
    return bs;
}

BinStream &operator>>(BinStream &bs, SampleMarker &m) {
    m.Load(bs);
    return bs;
}

void SampleData::Save(BinStream &bs) const {
    SAVE_REVS(0xE, 0);
    bs << mFormat << mNumSamples << mSampleRate << mSizeBytes;
    bool hasData = mData;
    bs << hasData;
    if (hasData) {
        WriteChunks(bs, mData, mSizeBytes, 0x8000);
    }
    bs << mMarkers;
}

void SampleData::LoadWAV(BinStream &bs, const FilePath &fp, bool bigEndian) {
    Reset();
    WaveFile wav(bs);
    if (wav.BitsPerSample() != 0x10) {
        MILO_NOTIFY("Wave file %s is not 16-bit", fp);
        return;
    }
    if (wav.Format() != 1) {
        MILO_NOTIFY("Wave file %s is compressed", fp);
        return;
    }
    mFormat = kPCM;
#ifdef HX_NATIVE
    mNumChannels = wav.NumChannels();
#endif
    mNumSamples = wav.NumSamples();
    mSampleRate = wav.SamplesPerSec();
    mSizeBytes = SizeAs(mFormat);
    mData = sAlloc(mSizeBytes, fp.c_str());
    WaveFileData wavdata(wav);
    wavdata.Read(mData, mSizeBytes);
    for (int i = 0; i < wav.NumMarkers(); i++) {
        mMarkers.push_back(
            SampleMarker(wav.Markers()[i].GetName(), wav.Markers()[i].GetFrame())
        );
    }
}

int SampleData::SizeAs(Format fmt) const {
    if ((unsigned int)fmt <= 7U) {
        switch (fmt) {
        case 1:
            return mNumSamples * 2;
        case 0:
            return mNumSamples * 2;
        case 2:
            return ((mNumSamples + 0x6F) / 0x70) * 0x40;
        case 4:
        case 5:
            return ((mNumSamples + 0x3FF) / 0x400) * 0xC0;
        case 3:
            MILO_NOTIFY("don't know size as XMA");
            return mNumSamples / 5;
        case 6: {
            return 0x60 - (int)((float)(long long)(mNumSamples * 2) * -0.29411763f);
        }
        case 7: {
            return 0x60 - (int)((float)(long long)(mNumSamples * 2) * -0.29411763f);
        }
        }
    } else {
        MILO_ASSERT(0, 0x12B);
        return 0;
    }
    return 0;
}

void SampleData::Load(BinStream &bs, const FilePath &fp) {
    Reset();
    LOAD_REVS(bs);
    if (d.rev > gSampleDataMaxRev) {
        MILO_FAIL("%s can't load new %s version %d > %d", fp, "SampleData", d.rev, gSampleDataMaxRev);
    }
    if (d.altRev > gSampleDataMaxAltRev) {
        MILO_FAIL("%s can't load new %s alt version %d > %d", fp, "SampleData", d.altRev, gSampleDataMaxAltRev);
    }
    int fmt;
    d >> fmt >> mNumSamples >> mSampleRate >> mSizeBytes;
    mFormat = (Format)fmt;
    bool hasData = true;
    if (d.rev >= 0xB) {
        d >> hasData;
    }
    if (hasData) {
#ifdef HX_NATIVE
        if (sAlloc)
            mData = sAlloc(mSizeBytes, fp.c_str());
        else
            mData = malloc(mSizeBytes);
#else
        mData = sAlloc(mSizeBytes, fp.c_str());
#endif
        ReadChunks(bs, mData, mSizeBytes, 0x8000);
    }
    if (d.rev >= 0xE) {
        d >> mMarkers;
    }
#ifdef HX_FFMPEG
    // Decode XMA to PCM at load time so SampleInstNative can play it
    if (mFormat == kXMA && mData && mSizeBytes > 0) {
        void *pcm = nullptr;
        int pcmSize = 0;
        int chans = mNumChannels ? mNumChannels : 1;
        if (DecodeXMAToPCM(mData, mSizeBytes, mNumSamples, mSampleRate, chans, &pcm, &pcmSize)) {
            // Free original XMA data via proper path (handles WavMgr vs sAlloc)
            Dealloc();
            // Allocate decoded PCM with engine allocator (CRC=0 so sFree used on destruction)
            if (sAlloc)
                mData = sAlloc(pcmSize, fp.c_str());
            else
                mData = malloc(pcmSize);
            memcpy(mData, pcm, pcmSize);
            free(pcm);
            mSizeBytes = pcmSize;
            mFormat = kPCM;
            mNumChannels = chans;
            mNumSamples = pcmSize / (2 * chans);
        }
    }
#endif
}
