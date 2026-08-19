#pragma once
#include "obj/Data.h"
#include "obj/PropSync.h"
#include "utl/CRC.h"
#include "utl/Str.h"
#include "utl/BinStream.h"
#include "utl/FilePath.h"
#include <vector>

class SampleMarker {
    friend bool PropSync(SampleMarker &, DataNode &, DataArray *, int, PropOp);

public:
    SampleMarker() : name(""), sample(-1) {}
    SampleMarker(const String &str, int i) : name(str), sample(i) {}
    void Save(BinStream &bs) const {
        bs << name;
        bs << sample;
    }
    void Load(BinStream &bs) {
        bs >> name;
        bs >> sample;
    }
    int Sample() const { return sample; }
    const String &Name() const { return name; }

private:
    String name; // 0x0
    int sample; // 0xc
};

// Retail RB3 alloc/free signatures are size-only alloc and pointer-only free.
// DC3's newer engine widened these to carry file/line tracking; RB3 does not.
typedef void *(*SampleDataAllocFunc)(int);
typedef void (*SampleDataFreeFunc)(void *);

class SampleData {
public:
    enum Format {
        kPCM,
        kBigEndPCM,
        kVAG,
        kXMA,
        kATRAC,
        kMP3,
        kNintendoADPCM
    };

    SampleData();
    ~SampleData();
    void Reset();
    void Save(BinStream &) const;
    void Load(BinStream &, const FilePath &);
    void LoadWAV(BinStream &, const FilePath &, bool);
    int SizeAs(Format) const;
    int NumMarkers() const;
    const SampleMarker &GetMarker(int) const;
    void Dealloc();
    int GetSampleRate() const { return mSampleRate; }
    int GetNumSamples() const { return mNumSamples; }
    Format GetFormat() const { return mFormat; }
    int GetSizeBytes() const { return mSizeBytes; }
    bool HasData() const { return (int)(uintptr_t)mData != 0; }
    unsigned int DataAddr() const { return (unsigned int)(uintptr_t)mData; }
#ifdef HX_NATIVE
    // RB3 samples are 16-bit mono; the channel count is not stored on retail
    // (see SizeAs, which has no per-channel multiply). The native FFMPEG XMA
    // decode path keeps a real channel count alongside the matched fields.
    int NumChannels() const { return mNumChannels; }
    void *DataPtr() const { return mData; }
#else
    int NumChannels() const { return 1; }
#endif
    std::vector<SampleMarker> &AccessMarkers() { return mMarkers; }

    static void SetAllocator(SampleDataAllocFunc, SampleDataFreeFunc);

private:
    static SampleDataAllocFunc sAlloc;
    static SampleDataFreeFunc sFree;

    // Retail RB3 layout (cross-checked against the target binary's
    // SampleData::Reset/Save/SizeAs and rb3-Wii's SampleData.h). DC3's newer
    // engine added mCRC@0x0 and mNumChannels@0xc; RB3 has neither.
    int mNumSamples; // 0x0
    int mSampleRate; // 0x4
    int mSizeBytes; // 0x8
    Format mFormat; // 0xc
    void *mData; // 0x10
#ifdef HX_NATIVE
    int mNumChannels;
#endif
    std::vector<SampleMarker> mMarkers; // 0x14
};
