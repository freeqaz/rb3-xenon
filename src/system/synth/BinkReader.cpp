#include "synth/BinkReader.h"
#include "os/Block.h"
#include "os/Debug.h"
#include "os/Timer.h"
#include "utl/Symbol.h"
#include "utl/MakeString.h"

// External declarations - C functions from Bink SDK
extern "C" {
void BinkInit(void);
void BinkSetSoundTrack(int, int);
BINK *BinkOpen(File *, unsigned int);
void BinkSetVideoOnOff(BINK *, int);
const char *BinkGetError(void);
void BinkNextFrame(BINK *);
unsigned int BinkGetTrackData(BINKTRACK *, void *);
BINKTRACK *BinkOpenTrack(BINK *, unsigned char);
void BinkCloseTrack(BINKTRACK *);
void BinkClose(BINK *);
void BinkGoto(void *bink, unsigned int frame, int mode);
}

// MemAlloc/MemFree come in transitively from utl/MemMgr.h (via os/File.h). On
// X360 that header also defines the retail call-site arity macros, so the call
// sites below pass only the retail args.

int BinkReader::sHeap = 0;

BinkReader::BinkReader(File *file, StandardStream *stream)
    : mFile(file), mStream(stream), mDecodeTrack(0), mSamplesReady(0),
      mSampleCurrent(0), mSamplesJump(0), mState(0), mHeap(sHeap) {
    // Initialize Bink library
    BinkInit();
    BinkSetSoundTrack(0, 0);

    // Open the Bink file
    mBink = BinkOpen(file, 0x2804400);

    if (mBink != nullptr) {
        mState = kInit;
        BinkSetVideoOnOff(mBink, 0);
    } else {
        const char *err = BinkGetError();
        TheDebug.Notify(MakeString("Error opening Bink audio file: %s", err));
        mState = kFail;
    }
}

BinkReader::~BinkReader() {
    if (mState > 1 && mBink->NumTracks > 0) {
        for (unsigned char i = 0; i < mBink->NumTracks; i++) {
            if (mBinkTracks[i]) {
                BinkCloseTrack(mBinkTracks[i]);
            }
            if (mPCMBuffers[i]) {
#ifdef HX_NATIVE
                MemFree(mPCMBuffers[i], "unknown", 0, "unknown");
#else
                MemFree(mPCMBuffers[i]);
#endif
            }
        }
    }
    BinkClose(mBink);
}

void BinkReader::Poll(float) {
    START_AUTO_TIMER("bink_audio");

    int state = mState;

    switch (state) {
    case kFail: {
        MILO_FAIL("BinkReader::Poll() failed!");
        break;
    }
    case kPlaying: {
        TheBlockMgr.Poll();

        if (mSamplesReady > 0) {
            int iSamplesConsumed = mStream->ConsumeData(
                mPCMOffsets, mSamplesReady, mSampleCurrent
            );

            MILO_ASSERT(iSamplesConsumed <= mSamplesReady, 0x9B);

            mSampleCurrent += iSamplesConsumed;
            mSamplesReady -= iSamplesConsumed;

            for (unsigned char i = 0; i < mBink->NumTracks; i++) {
                mPCMOffsets[i] = (void *)((char *)mPCMOffsets[i] + iSamplesConsumed * 2);
            }

            if (mDecodeTrack == mBink->NumTracks) {
                mState = (mBink->FrameNum == mBink->Frames) ? kDone : kPlaying;
                if (mState == kPlaying) {
                    BinkNextFrame(mBink);
                }
                mDecodeTrack = 0;
            }
        }

        if (mSamplesReady <= 0) {
            unsigned int trackData = 0;
            int remainingBuffer = 0xB400;
            do {
                if (mDecodeTrack == mBink->NumTracks)
                    break;

                trackData = BinkGetTrackData(
                    mBinkTracks[mDecodeTrack], mPCMBuffers[mDecodeTrack]
                );
                remainingBuffer -= trackData;

                mPCMOffsets[mDecodeTrack] =
                    (void *)((char *)mPCMBuffers[mDecodeTrack] + mSamplesJump * 2);
                mDecodeTrack++;
            } while (remainingBuffer > 0);

            if (mDecodeTrack == mBink->NumTracks) {
                unsigned int prevJump = mSamplesJump;
                mSamplesJump = 0;
                mSamplesReady = (trackData >> 1) - prevJump;
                mSampleCurrent += prevJump;

                int newState =
                    (mBink->FrameNum == mBink->Frames) ? kDone : kPlaying;
                mState = newState;

                if (remainingBuffer > 0 && newState == kPlaying) {
                    BinkNextFrame(mBink);
                    mDecodeTrack = 0;
                }
            }
        }
        break;
    }
    case kSetup: {
        mState = kPlaying;
        Init();
        break;
    }
    case kInit: {
        MILO_ASSERT(mBink->NumTracks < BINK_AUDIO_CHANNEL_MAX, 0x5E);

        if (mBink->NumTracks == 0) {
            mState = kDone;
        }

        for (unsigned char i = 0; i < mBink->NumTracks; i++) {
            BINKTRACK *hBinkTrack = BinkOpenTrack(mBink, i);
            mBinkTracks[i] = hBinkTrack;
            MILO_ASSERT(hBinkTrack->Bits == 16, 0x73);
            MILO_ASSERT(hBinkTrack->Frequency == 44100, 0x74);
            MILO_ASSERT(hBinkTrack->Channels == 1, 0x75);

#ifdef HX_NATIVE
            void *buf =
                MemAlloc(hBinkTrack->MaxSize, "BinkReader.cpp", 0x78, "Bink Audio", 0x80);
#else
            // Retail/match: 2-arg (size, align). align=0x80 non-zero →
            // parenthesized bypass of the align-0-forcing macro.
            void *buf = (MemAlloc)(hBinkTrack->MaxSize, 0x80);
#endif
            mPCMOffsets[i] = buf;
            mPCMBuffers[i] = buf;
        }
        mState = kSetup;
        break;
    }
    }

    if (mBink->BinkError != 0) {
        mState = kFail;
    }
}

void BinkReader::Seek(int targetSample) {
    if (mBink != nullptr && mState != kFail) {
        float kfBinkFreq = (float)mBinkTracks[0]->Frequency;
        float kfBinkRate =
            (float)mBink->FrameRate / (float)mBink->FrameRateDiv;

        unsigned int kiSampleFrame = (unsigned int)(
            ((double)((float)targetSample / kfBinkFreq) - 0.75) *
                (double)kfBinkRate +
            1.0
        );
        MILO_ASSERT(kiSampleFrame < mBink->Frames, 0x102);
        BinkGoto(mBink, kiSampleFrame, 1);

        unsigned int samplesAfterSeek = (unsigned int)(
            (double)(float)(
                ((float)(mBink->FrameNum - 1) * (1.0f / kfBinkRate) + 0.75f) * kfBinkFreq
            )
        );
        mSamplesJump = targetSample - samplesAfterSeek;
        mSampleCurrent = samplesAfterSeek;

        MILO_ASSERT(mSamplesJump < (kfBinkFreq / kfBinkRate), 0x10B);
        mState = kPlaying;
    }
}

void BinkReader::Init() {
    MILO_ASSERT(mStream, 0x114);
    mStream->InitInfo(mBink->NumTracks, mBinkTracks[0]->Frequency, false, -1);
}
