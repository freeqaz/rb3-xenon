// XMAReader (retail RTTI .?AVXMAReader@@, vtable 0x82197138).
// Retail .text 0x82B6A384-0x82B6B4F0, right after FxSendSynapse360.
// Every body written from the retail asm.
#include "synth_xbox/XMAReader.h"
#include "../../Memory.h"
#include "math/Utl.h"
#include "os/File.h"
#include "synth/StandardStream.h"
#include "utl/BinStream.h"
#include "utl/BufStream.h"
#include "utl/Std.h"
#include "synth_xbox/XMAReaderBlock.h"
#include <algorithm>

// 0x82B6A568
XMAReader::XMAReader(File *file, StandardStream *stream)
    : mFile(file), mStream(stream), mPlayback(0), unk10(0), mBlockSize(0), unk18(0),
      mDataSize(0), unk20(0), unk30(0), mBlockOffset(-1), mSampleOffset(0), mReadBlock(0), mSubmitBlock(0),
      mFirstSubmit(true), mLockRequested(false), mDone(false) {
    for (int i = 0; i < 2; i++) {
        mPhysicalBuffers[i] = 0;
        mBufferState[i] = 0;
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
            mBufferState[i] = 0;
        mReadBlock = mSubmitBlock = mBlockOffset / mBlockSize;
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

// 0x82B6AA98
bool XMAReader::Init() {
    if (!mReadBuffer)
        return true;
    int bytesRead;
    if (mFile->ReadDone(bytesRead)) {
        BufStream bs(mReadBuffer, 20000, true);
        int version;
        bs >> version;
        bs >> unk10;
        if (version < 2) {
            int numStreams;
            bs >> numStreams;
            unk24.resize(numStreams, 1);
        } else {
            bs >> unk24;
        }
        bs >> unk20;
        bs >> mBlockSize;
        bs >> unk18;
        bs >> mDataSize;
        bs >> mSeekTable;
        delete mReadBuffer;
        mReadBuffer = 0;
        mFile->Seek(unk10, 0);
        for (int i = 0; i < 2; i++)
            mPhysicalBuffers[i] = PhysicalAllocTracked(mBlockSize, 4, "XMABuffer(phys)");
        mStream->InitInfo(TableSum(), unk20, false, -1);
        mBlocks.resize(TableSum(), 0);
        for (unsigned int i = 0; i < mBlocks.size(); i++)
            mBlocks[i] = new XMAReaderBlock(0x10000);
        std::vector<XMA_PLAYBACK_INIT> inits;
        for (unsigned int i = 0; i < unk24.size(); i++) {
            XMA_PLAYBACK_INIT init;
            init.sampleRate = unk20;
            init.outputBufferSizeInSamples = 0xf80 / unk24[i];
            init.channelCount = unk24[i];
            init.subframesToDecode = 8;
            inits.push_back(init);
        }
        XMAPlaybackCreate(unk24.size(), inits.begin(), 0, &mPlayback, 0, 0);
    }
    return mReadBuffer == 0;
}

// 0x82B6AEB0. With the decoder's modify lock held: pull decoded samples into
// the per-channel FIFOs (splitting stereo streams across two), hand the span
// every FIFO can supply to the stream (or drop it while a seek is still
// skipping samples), then keep the two physical buffers cycling through
// read -> submit -> drained.
void XMAReader::Poll(float) {
    if (!Init())
        return;
    if (!mLockRequested) {
        XMAPlaybackRequestModifyLock(mPlayback);
        mLockRequested = true;
    }
    if (!XMAPlaybackQueryModifyLockObtained(mPlayback))
        return;
    mLockRequested = false;
    for (unsigned int i = 0; i < unk24.size(); i++)
        XMAPlaybackGetErrorBits(mPlayback, 0);
    if (mBlockOffset < 0 || FinishSeek()) {
        unsigned int s = 0;
        int block = 0;
        for (; s < unk24.size(); s++) {
            short *data;
            int samples = XMAPlaybackQueryAvailableData(mPlayback, s, (void **)&data);
            XMAReaderBlock *first = mBlocks[block];
            int room = first->FreeSpace() / 2;
            if (room < samples)
                samples = room;
            if (unk24[s] == 2) {
                first->FreeSpace();
                mBlocks[block + 1]->FreeSpace();
                for (int j = 0; j < samples; j++) {
                    first->Write(&data[j * 2], 2, true);
                    mBlocks[block + 1]->Write(&data[j * 2 + 1], 2, true);
                }
            } else if (samples != 0) {
                first->Write(data, samples * 2, true);
            }
            XMAPlaybackConsumeDecodedData(mPlayback, s, samples, (void **)&data);
            block += unk24[s];
        }

        int samples;
        do {
            samples = 0x8000;
            for (int i = 0; i < TableSum(); i++) {
                int n = mBlocks[i]->ReadableBytes() / 2;
                if (n < samples)
                    samples = n;
            }
            if (samples == 0)
                break;
            if (mBlockOffset == -1) {
                if (mSampleOffset != 0) {
                    // Retail zero-extends the skip to 64 bits before the
                    // 32-bit compare (`clrrwi r11,r10,0`).
                    __int64 skip = (unsigned int)mSampleOffset;
                    if ((int)skip < samples)
                        samples = (int)skip;
                    mSampleOffset -= samples;
                } else {
                    std::vector<void *> ptrs(TableSum());
                    for (int i = 0; i < TableSum(); i++)
                        ptrs[i] = mBlocks[i]->ReadPtr(samples * 2);
                    samples = mStream->ConsumeData(&ptrs[0], samples, unk30);
                }
            }
            for (int i = 0; i < TableSum(); i++)
                mBlocks[i]->Consume(samples * 2);
            unk30 = samples + unk30;
            if (unk30 >= mSeekTable.back())
                mDone = true;
        } while (samples != 0);

        int submit = mSubmitBlock % 2;
        if (mBufferState[submit] == 2) {
            bool ready = true;
            for (unsigned int i = 0; i < unk24.size(); i++) {
                if (!XMAPlaybackQueryReadyForMoreData(mPlayback, i)) {
                    ready = false;
                    break;
                }
            }
            if (ready) {
                int left = mDataSize - mSubmitBlock * mBlockSize;
                int size = left < mBlockSize ? left : mBlockSize;
                for (unsigned int i = 0; i < unk24.size(); i++) {
                    int offset = 0;
                    if (mFirstSubmit)
                        offset = i << 11;
                    int len = size - offset;
                    if (len >= 0)
                        XMAPlaybackSubmitData(
                            mPlayback, i, (char *)mPhysicalBuffers[submit] + offset, len
                        );
                }
                mBufferState[submit] = 3;
                mSubmitBlock++;
                mFirstSubmit = false;
            }
        }

        int read = mReadBlock % 2;
        if (mBufferState[read] == 3) {
            bool pending = false;
            for (unsigned int i = 0; i < unk24.size(); i++) {
                if (XMAPlaybackQueryInputDataPending(mPlayback, i, mPhysicalBuffers[read])) {
                    pending = true;
                    break;
                }
            }
            if (!pending)
                mBufferState[read] = 0;
        }
        if (mBufferState[read] == 0 && mBlockOffset == -1) {
            mFile->ReadAsync(mPhysicalBuffers[read], mBlockSize);
            mBufferState[read] = 1;
        }
        if (mBufferState[read] == 1) {
            int bytesRead;
            if (mFile->ReadDone(bytesRead)) {
                mBufferState[read] = 2;
                mReadBlock++;
            }
        }
    }
    XMAPlaybackResumePlayback(mPlayback);
}
