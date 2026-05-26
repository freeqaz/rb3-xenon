#pragma once
#include <cstring>
#include "utl/BinStream.h"

class ChunkID {
public:
    ChunkID() { memcpy(mStr, "????", 4); }
    ChunkID(const char *str) { memcpy(mStr, str, 4); }
    ChunkID(BinStream &bs) { bs.Read(mStr, 4); }

    void Load(BinStream &bs) { bs.Read(mStr, 4); }

    bool operator==(const ChunkID &id) { return strncmp(mStr, id.mStr, 4) == 0; }
    bool operator!=(const ChunkID &id) { return !(*this == id); }

    const char *Str() const { return mStr; }

private:
    char mStr[4];
};

extern const ChunkID kListChunkID;
extern const ChunkID kRiffChunkID;
extern const ChunkID kMidiChunkID;
extern const ChunkID kMidiHeaderChunkID;
extern const ChunkID kMidiTrackChunkID;
extern const ChunkID kWaveChunkID;
extern const ChunkID kWaveFormatChunkID;
extern const ChunkID kWaveDataChunkID;
extern const ChunkID kWaveFactChunkID;
extern const ChunkID kWaveInstChunkID;
extern const ChunkID kWaveSampleChunkID;
extern const ChunkID kWaveCueChunkID;
extern const ChunkID kWaveLabelChunkID;
extern const ChunkID kWaveTextChunkID;
extern const ChunkID kWaveAdditionalChunkID;
