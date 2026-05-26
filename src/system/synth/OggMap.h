#pragma once
#include "utl/BinStream.h"
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
    /** The LUT of ogg samples.
        pair's first int = the seek position
        pair's second int = the active sample. */
    std::vector<std::pair<int, int> > mLookup; // 0x8
};
