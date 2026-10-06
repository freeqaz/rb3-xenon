#pragma once
#include "movie/BinkSdk.h"
#include "os/File.h"
#include "synth/StreamReader.h"
#include "synth/StandardStream.h"

#define BINK_AUDIO_CHANNEL_MAX 16

// Bink SDK structures.  BINK is in movie/BinkSdk.h.

struct BINKTRACK {
    unsigned int Frequency; // 0x00
    unsigned int Bits; // 0x04
    unsigned int Channels; // 0x08
    int MaxSize; // 0x0C
};

class BinkReader : public StreamReader {
public:
    BinkReader(File *, StandardStream *);
    virtual ~BinkReader();
    virtual void Poll(float);
    virtual void Seek(int);
    virtual void EnableReads(bool enable) { mEnableReads = enable; }
    virtual bool Done() { return mState == 4; }
    virtual bool Fail() { return mState == 5; }

protected:
    virtual void Init();

private:
    enum State {
        kInit = 1,
        kSetup = 2,
        kPlaying = 3,
        kDone = 4,
        kFail = 5
    };

    File *mFile; // 0x04
    StandardStream *mStream; // 0x08
    BINK *mBink; // 0x0C
    BINKTRACK *mBinkTracks[BINK_AUDIO_CHANNEL_MAX]; // 0x10-0x4C
    void *mPCMBuffers[BINK_AUDIO_CHANNEL_MAX]; // 0x50-0x8C
    void *mPCMOffsets[BINK_AUDIO_CHANNEL_MAX]; // 0x90-0xCC
    unsigned char mDecodeTrack; // 0xD0
    bool mEnableReads; // 0xD1 (?)
    int mSamplesReady; // 0xD4
    unsigned int mSampleCurrent; // 0xD8
    unsigned int mSamplesJump; // 0xDC
    int mState; // 0xE0
    int mHeap; // 0xE4

    static int sHeap;
};
