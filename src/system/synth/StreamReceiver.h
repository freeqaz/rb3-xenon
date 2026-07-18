#pragma once
#include "synth/ADSR.h"
#include "utl/MemMgr.h"

class StreamReceiver;
typedef StreamReceiver *StreamReceiverFactoryFunc(int, int, bool, int);

#define kStreamRcvrBufSize 0x18000

class StreamReceiver {
    friend class StandardStream;
public:
    enum State {
        kInit = 0,
        kReady = 1,
        kPlaying = 2,
        kStopped = 3,
    };
    StreamReceiver(int, bool);
    virtual ~StreamReceiver();
    virtual void SetVolume(float) = 0;
    virtual void SetPan(float) = 0;
    virtual void SetSpeed(float) = 0;
    virtual void SetADSR(const ADSRImpl &) {}
    virtual void Tag() {}
    virtual void Poll();
    virtual void SetSlipOffset(float) = 0;
    virtual void SlipStop() = 0;
    virtual void SetSlipSpeed(float) = 0;
    virtual float GetSlipOffset() = 0;
    virtual void SetFXSend(class FxSend *) {}
    virtual int GetPlayCursor() = 0;
    virtual void PauseImpl(bool) = 0;
    virtual void PlayImpl() = 0;
    virtual void StartSendImpl(unsigned char *, int, int) = 0;
    virtual bool SendDoneImpl() = 0;
#ifdef HX_NATIVE
    /** True when the audio output has consumed all buffered data.
     *  Used by Poll() to pace mDoneBufferCounter after mEndData. */
    virtual bool IsOutputDrained() const { return true; }
#endif

    MEM_OVERLOAD(StreamReceiver, 0x23);

    int BytesWriteable();
    bool Ready();
    void EndData();
    void Play();
    void Stop();
    u64 GetBytesPlayed();
    void WriteData(const void *, int);

    static StreamReceiver *New(int, int, bool, int);

protected:
#ifdef HX_NATIVE
public:
    static StreamReceiverFactoryFunc *sFactory;
protected:
#else
    static StreamReceiverFactoryFunc *sFactory;
#endif

    bool mSlipEnabled; // 0x4
    unsigned char mBuffer[kStreamRcvrBufSize]; // 0x5
    int mNumBuffers; // 0x18008
    int mRingFreeSpace; // 0x1800c
    State mState; // 0x18010
    int mSendTarget; // 0x18014
    bool mWantToSend; // 0x18018
    bool mSending; // 0x18019
    int mBuffersSent; // 0x1801c
    bool mStarving; // 0x18020
    bool mEndData; // 0x18021
    int mDoneBufferCounter; // 0x18024
    int mLastPlayCursor; // 0x18028
};
