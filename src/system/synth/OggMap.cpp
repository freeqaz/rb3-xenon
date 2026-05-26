#include "synth/OggMap.h"
#include "math/Utl.h"
#include "os/Debug.h"

int OggMap::GetSongLengthSamples() { return mGran * mLookup.size(); }

void OggMap::GetSeekPos(int sampTarget, int &seekPos, int &actSamp) {
    if (mLookup.empty()) {
        seekPos = actSamp = 0;
    } else {
        int i14 = sampTarget / mGran;
        int maxLookupIdx = mLookup.size() - 1;
        int idx = Clamp(0, maxLookupIdx, i14);
        seekPos = mLookup[idx].first;
        actSamp = mLookup[idx].second;
    }
}

OggMap::~OggMap() { mLookup.clear(); }

OggMap::OggMap() : mGran(1000), mLookup() {
    mLookup.push_back(std::pair<int, int>(0, 0));
}

void OggMap::Read(BinStream &bs) {
    int version;
    bs >> version;
    if (version < 0xb)
        MILO_FAIL("Incorrect oggmap version.");
    bs >> mGran >> mLookup;
}
