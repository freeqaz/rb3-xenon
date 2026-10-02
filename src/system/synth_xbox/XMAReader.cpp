// XMAReader (retail RTTI .?AVXMAReader@@, vtable 0x82197138).
// Retail .text 0x82B6A384-0x82B6B4F0, right after FxSendSynapse360.
// Written here: the ctor, dtor, Seek, Done, FinishSeek and the table sum. Not
// yet written: Poll (0x82B6AEB0) and the 916-B body at 0x82B6AA98, which drive
// the XMA hardware decoder.
#include "synth_xbox/XMAReader.h"
#include "../../Memory.h"
#include "os/File.h"
#include "utl/Std.h"
#include <algorithm>

// 0x82B6A568
XMAReader::XMAReader(File *file, StandardStream *stream)
    : mFile(file), mStream(stream), mPlayback(0), unk10(0), mBlockSize(0), unk18(0),
      unk1c(0), unk20(0), unk30(0), mBlockOffset(-1), mSampleOffset(0), unk58(0), unk5c(0),
      unk70(true), unk71(false), mDone(false) {
    for (int i = 0; i < 2; i++) {
        mPhysicalBuffers[i] = 0;
        unk50[i] = 0;
    }
    mReadBuffer = new char[20000];
    mFile->ReadAsync(mReadBuffer, 20000);
}

// 0x82B6A828
XMAReader::~XMAReader() {
    if (mPlayback)
        XMAPlaybackDestroy(mPlayback);
    delete[] mReadBuffer;
    DeleteAll(mBlocks);
    for (int i = 0; i < 2; i++) {
        if (mPhysicalBuffers[i])
            PhysicalFree(mPhysicalBuffers[i]);
    }
}

// 0x82B6A738: position on the block holding `sample`.
void XMAReader::Seek(int sample) {
    std::vector<int>::iterator it =
        std::lower_bound(mSeekTable.begin(), mSeekTable.end(), sample);
    int block = it - mSeekTable.begin();
    int blockStart = block ? mSeekTable[block - 1] : 0;
    mSampleOffset = sample - blockStart;
    mBlockOffset = mBlockSize * block;
}

// 0x82B6A3E0
bool XMAReader::Done() { return mDone; }

// Slots 3 and 5 are the shared `blr` and `li r3,0; blr`.
void XMAReader::EnableReads(bool) {}
bool XMAReader::Fail() { return false; }

// 0x82B6A518
int XMAReader::TableSum() const {
    int sum = 0;
    for (unsigned int i = 0; i < unk24.size(); i++)
        sum += unk24[i];
    return sum;
}

// 0x82B6A3E8
bool XMAReader::FinishSeek() {
    int bytesRead;
    if (mFile->ReadDone(bytesRead)) {
        for (unsigned int i = 0; i < unk24.size(); i++)
            XMAPlaybackFlushData(mPlayback, i);
        for (int i = 0; i < 2; i++)
            unk50[i] = 0;
        unk58 = unk5c = mBlockOffset / mBlockSize;
        mFile->Seek(unk10 + mBlockOffset, 0);
        if (mBlockOffset == 0)
            unk30 = 0;
        else
            unk30 = mSeekTable[mBlockOffset / mBlockSize - 1];
        mDone = false;
        mBlockOffset = -1;
        return true;
    }
    return false;
}
