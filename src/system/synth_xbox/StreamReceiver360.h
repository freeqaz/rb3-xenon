#pragma once
#include <list>
#include "synth/StreamReceiver.h"
#include "synth/ADSR.h"
#include "synth_xbox/Voice.h"

class FxSend;
class FxSend360;

class StreamReceiver360 : public StreamReceiver {
public:
    StreamReceiver360(int sampleRate, int numBuffers, bool slip);
    virtual ~StreamReceiver360();
    virtual void SetVolume(float);
    virtual void SetPan(float);
    virtual void SetSpeed(float);
    virtual void SetADSR(const ADSRImpl &);
    virtual void Tag();
    virtual void Poll();
    virtual void SetSlipOffset(float);
    virtual void SlipStop();
    virtual void SetSlipSpeed(float);
    virtual float GetSlipOffset();
    virtual void SetFXSend(FxSend *);
    virtual bool SendDoneImpl();

    static void Init();

protected:
    virtual int GetPlayCursor();
    virtual void PauseImpl(bool);
    virtual void PlayImpl();
    virtual void StartSendImpl(unsigned char *, int, int);

private:
    void UpdateADSR();

    // Pending voice list at 0x802C (8 bytes)
#ifdef HX_NATIVE
    std::list<Voice *> mPendingVoices; // 0x802C
#else
    stlpmtx_std::list<Voice *, stlpmtx_std::StlNodeAlloc<Voice *>> mPendingVoices; // 0x1802c
#endif

    unsigned char *mStreamBuf; // 0x18034
    Voice *mSlipVoice;          // 0x18038
    Voice *mVoice;             // 0x1803c
    int mSampleRate;           // 0x18040
    int mNumBufs;              // 0x18044
    float mVolume;             // 0x18048
    float mPan;                // 0x1804c
    float mSpeed;              // 0x18050
    ADSRImpl mADSR;            // 0x18054
    int mFxSend;                // 0x1807c
    bool mTagged;               // 0x18080
};

StreamReceiver *New360Receiver(int, int, bool, int);
