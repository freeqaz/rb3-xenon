#include "synth/OggMap.h"
#include "math/Utl.h"
#include "os/Debug.h"

int OggMap::GetSongLengthSamples() { return mGran * mLookup.size(); }

void OggMap::GetSeekPos(int sampTarget, int &seekPos, int &actSamp) {
    // retail leaves both outputs untouched when there is no lookup table
    if (!mLookup.empty()) {
        int idx = sampTarget / mGran;
        int maxLookupIdx = mLookup.size() - 1;
        if (idx < 0)
            idx = 0;
        else if (idx > maxLookupIdx)
            idx = maxLookupIdx;
        seekPos = mLookup[idx].first;
        actSamp = mLookup[idx].second;
    }
}

OggMap::~OggMap() { mLookup.clear(); }

OggMap::OggMap() : mGran(1000), mStream(false), unk28(0), unk30(0), mLookup() {
    mLookup.push_back(std::pair<int, int>(0, 0));
}

void OggMap::Read(BinStream &bs) {
    int version;
    bs >> version;
    if (version < 0xb)
        MILO_FAIL("Incorrect oggmap version.");
    bs >> mGran >> mLookup;
}
