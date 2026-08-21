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
    // RB3-360 retail vtable order: PauseImpl @0x30, PlayImpl @0x34, GetPlayCursor
    // @0x38 -- verified against the target binary (StreamReceiver::Stop dispatches
    // slot 0x30, and the retail StreamReceiver::Play at 0x8272A3C0 dispatches 0x30
    // then 0x34). DC3 hoisted GetPlayCursor above PauseImpl, shifting both by one
    // slot. rb3-Wii's StreamReceiver has the same PauseImpl/PlayImpl/GetPlayCursor
    // relative order as RB3-360, corroborating that DC3 is the one that moved it.
    //
    // ⚠ DO NOT "FIX" THIS ORDER TO MATCH tools/vtable_order_sweep.py. The sweep
    // reports StreamReceiver360 as PERMUTED with slots 13/14 swapped, and that is
    // a FALSE POSITIVE driven by a wrong map name, not a defect here. Adjudicated
    // on retail bytes 2026-08-21:
    //   * retail's StreamReceiver360 vtable @0x8219754C really does hold
    //     0x82B6BAE8 at slot 13 and 0x82B6BAF8 at slot 14, and
    //     target_symbol_map.json names those GetPlayCursor and PlayImpl
    //     respectively -- which is what the sweep is reporting.
    //   * BUT both are 16-byte tail-call thunks with BYTE-IDENTICAL bodies except
    //     the branch displacement, so which name belongs to which is exactly the
    //     kind of assignment ICF makes arbitrary. Their branch targets resolve to
    //     an unrelated symbol and to no map entry at all.
    //   * The CALL SITE settles it. StreamReceiver::Play dispatches 0x30 with
    //     `li r4,0` (an argument -> PauseImpl(bool), consistent), and dispatches
    //     0x34 with NO argument and DISCARDS the result. `Play()` calling
    //     GetPlayCursor() and throwing the int away is not a plausible reading;
    //     the play path calling PlayImpl() is. So 0x34 is PlayImpl, as declared
    //     here, and the MAP has the two thunk names swapped.
    // Fixing the map is a separate, measured lane (a map edit's delta is mostly
    // un-pairing, so it is not a free rename). Until then the sweep's PERMUTED
    // verdict on StreamReceiver360 is KNOWN and EXPECTED.
    virtual void PauseImpl(bool) = 0;
    virtual void PlayImpl() = 0;
    virtual int GetPlayCursor() = 0;
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
