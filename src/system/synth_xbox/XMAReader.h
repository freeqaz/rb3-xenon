#pragma once
#include "synth/StreamReader.h"
#include "xdk/xaudio2/xmaplayback.h"
#include "synth_xbox/XMAReaderBlock.h"
#include <vector>

class File;
class StandardStream;

// RB3-360 XMA stream decoder (retail RTTI .?AVXMAReader@@ @82C75A3C).
// Synth360::NewStreamDecoder allocates 0x74 bytes for it and constructs with
// (File *, StandardStream *). Retail .text 0x82B6A384-0x82B6B4F0; ctor 0x82B6A568.
// Members are named from what the written bodies do with them.
class XMAReader : public StreamReader {
public:
    XMAReader(File *, StandardStream *);
    virtual ~XMAReader();
    virtual void Poll(float);
    virtual void Seek(int);
    virtual void EnableReads(bool);
    virtual bool Done();
    virtual bool Fail();
    // NOT virtual: retail's ??_7XMAReader@@6B@ @0x82197138 is SIX slots and all
    // six are spoken for by body shape -- [0] deleting dtor, [1]/[2] substantial
    // (Poll/Seek), [3] the bare-`blr` hub (an EMPTY EnableReads(bool)), [4]
    // `lbz r3,0x72(r3); blr` (Done) and [5] `li r3,0; blr` (Fail returns false).
    // The bound is hard: slot 6 would be 0x82197150, which holds 0xffffffff and
    // is not an image VA. There is no slot left for Init. (This class is not in
    // objects.json, so this is a declaration-accuracy fix and is metric-neutral
    // by construction -- our build emits no XMAReader vtable at all.)
#ifdef HX_NATIVE
    virtual void Init();
#else
    // 0x82B6AA98: parse the header out of the first read and create the XMA
    // playback object; true once the header has been consumed.
    bool Init();
#endif

    // 0x82B6A518: the sum of the 0x24 table.
    int TableSum() const;
    // 0x82B6A3E8: once the outstanding read has landed, flush every stream and
    // reposition the file on the block Seek chose. Name descriptive.
    bool FinishSeek();

    File *mFile; // 0x4
    StandardStream *mStream; // 0x8
    XMAPLAYBACK *mPlayback; // 0xc
    int unk10; // 0x10
    int mBlockSize; // 0x14
    int unk18; // 0x18
    int mDataSize; // 0x1c, bytes of XMA data after the header
    int unk20; // 0x20
    std::vector<int> unk24; // 0x24
    int unk30; // 0x30
    std::vector<int> mSeekTable; // 0x34, ascending sample offsets
    int mBlockOffset; // 0x40
    int mSampleOffset; // 0x44
    void *mPhysicalBuffers[2]; // 0x48, PhysicalFree'd
    // Per physical buffer: 0 free, 1 read pending, 2 read landed, 3 submitted.
    int mBufferState[2]; // 0x50
    int mReadBlock; // 0x58, next block to read into a physical buffer
    int mSubmitBlock; // 0x5c, next block to submit to the decoder
    std::vector<XMAReaderBlock *> mBlocks; // 0x60
    char *mReadBuffer; // 0x6c, 20000 bytes
    bool mFirstSubmit; // 0x70, stream i starts at i * 0x800 in the first block
    bool mLockRequested; // 0x71, XMAPlaybackRequestModifyLock outstanding
    bool mDone; // 0x72
};
