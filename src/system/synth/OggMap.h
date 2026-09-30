#pragma once
#include "utl/BinStream.h"
#include "utl/MemStream.h"
#include <vector>

/** A collection of ogg samples, meant for quick seeking into an ogg. */
class OggMap {
public:
    OggMap();
    virtual ~OggMap();

    void Read(BinStream &);
    void GetSeekPos(int, int &, int &);
    int GetSongLengthSamples();

private:
    /** Granularity, aka how precise the ogg samples are.
        i.e. if mGran = 1000, the samples are accurate up to the nearest ms. */
    int mGran; // 0x4
    // TU5 (retail 0x82bb1c78 ctor, Read at 0x82bb1d68): a MemStream(false) at
    // +0x8 and two zeroed words precede the lookup table, which Read fills at
    // +0x34. sizeof(OggMap) is therefore 0x40, which is what VorbisReader's
    // former "TU5-inserted" placeholders at +0xc0..+0xec were standing in for.
    MemStream mStream; // 0x8
    int unk28; // 0x28
    int unk2c; // 0x2c (not initialized by the ctor)
    int unk30; // 0x30
    /** The LUT of ogg samples.
        pair's first int = the seek position
        pair's second int = the active sample. */
    std::vector<std::pair<int, int> > mLookup; // 0x34
};
