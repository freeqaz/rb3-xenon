#include "synth_xbox/StreamReceiver360.h"
#include "synth_xbox/FxSend.h"
#include "os/Debug.h"
#include "math/Utl.h"
#include "utl/PoolAlloc.h"
#include "utl/Std.h"
#include "utl/MemMgr.h"

extern void *_MemAllocTemp(int, const char *, int, const char *, int);
extern "C" void XMemCpy(void *, const void *, int);

extern "C" int lbl_8316C864;
extern "C" int lbl_8316C860;

StreamReceiver360::StreamReceiver360(int sampleRate, int numBuffers, bool slip)
    : StreamReceiver(numBuffers, slip), mStreamBuf(0), mSlipVoice(0), mVoice(0),
      mSampleRate(sampleRate), mNumBufs(numBuffers), mVolume(1.0f), mPan(0.0f), mSpeed(1.0f),
      mFxSend(0), mTagged(false) {
    mStreamBuf = (unsigned char *)_MemAllocTemp(
        numBuffers << 14, "StreamReceiver.cpp", 0x33, "StreamBuffer", 0);

    Voice *mem = (Voice *)PoolAlloc(
        0x7c, 0x7c, "e:\\lazer_build_gmc1\\system\\src\\synth360\\Voice.h", 0x28, "Voice"
    );
    mVoice = mem ? new (mem) Voice(false, 1, false) : 0;

    mVoice->SetData(mStreamBuf, numBuffers << 14, 0);
    mVoice->SetLoopRegion(0, -1);
    mVoice->SetSampleRate(mSampleRate);

    if (!mSlipEnabled) {
        mSlipVoice = mVoice;
    } else {
        mVoice->SetVolume(0.0f);
    }
}

StreamReceiver360::~StreamReceiver360() {
    if (mVoice != 0) {
        delete mVoice;
    }
    if (mSlipEnabled && mSlipVoice != 0) {
        delete mSlipVoice;
    }
    DeleteAll(mPendingVoices);
    MemFree(mStreamBuf);
}

void StreamReceiver360::SetVolume(float f) {
    mVolume = f;
    if (mSlipVoice == 0) return;
    mSlipVoice->SetVolume(f);
}

void StreamReceiver360::SetPan(float f) {
    mPan = f;
    if (mSlipVoice == 0) return;
    mSlipVoice->SetPan(f);
}

void StreamReceiver360::SetSpeed(float f) {
    mSpeed = f;
    mVoice->SetSpeed(f);
}

void StreamReceiver360::SetADSR(const ADSRImpl &adsr) {
    memcpy(&mADSR, &adsr, sizeof(ADSRImpl));
    UpdateADSR();
}

void StreamReceiver360::Tag() {
    mTagged = true;
    if (mSlipVoice) {
        int val;
        Voice *target;
        if (mVoice != 0) {
            mSlipVoice->mTagState = 1;
            val = 2;
            target = mVoice;
        } else {
            if (mSlipVoice == 0) return;
            val = 3;
            target = mSlipVoice;
        }
        target->mTagState = val;
        return;
    }
    if (mVoice == 0) return;
    mVoice->mTagState = 4;
}

void StreamReceiver360::Poll() {
    StreamReceiver::Poll();
    if (mVoice != 0 && mVoice->IsPlaying()) {
        lbl_8316C864 = lbl_8316C864 + 1;
    }
    if (mSlipVoice != 0 && mSlipVoice->IsPlaying()) {
        lbl_8316C860 = lbl_8316C860 + 1;
    }
    while (mPendingVoices.begin() != mPendingVoices.end()) {
        if (mPendingVoices.front()->IsPlaying()) break;
        Voice *v = mPendingVoices.front();
        if (v != 0) {
            delete v;
        }
        mPendingVoices.erase(mPendingVoices.begin());
    }
}

void StreamReceiver360::SetSlipOffset(float f) {
    MILO_ASSERT(mSlipEnabled, 0xC5);
    SlipStop();
    Voice *v = (Voice *)PoolAlloc(0x7c, 0x7c, "e:\\lazer_build_gmc1\\system\\src\\synth360\\Voice.h", 0x28, "Voice");
    if (v) {
        v = new (v) Voice(false, 1, false);
    }
    mSlipVoice = v;
    if (mTagged) {
        Tag();
    }
    mSlipVoice->SetData(mStreamBuf, mNumBufs << 14, 0);
    mSlipVoice->SetLoopRegion(0, -1);
    mSlipVoice->SetSampleRate(mSampleRate);
    int cursor = GetPlayCursor();
    int halfCursor = cursor / 2;
    int halfBuf = (mNumBufs << 14) / 2;
    int startSamp;
    if (halfBuf == 0) {
        startSamp = 0;
    } else {
        float fOff = f * 0.001f;
        int offset = (int)(fOff * (float)mSampleRate);
        startSamp = (offset + halfCursor) % halfBuf;
        if (startSamp < 0) startSamp += halfBuf;
    }
    mSlipVoice->SetStartSamp(startSamp);
    mSlipVoice->SetVolume(mVolume);
    mSlipVoice->SetPan(mPan);
    mSlipVoice->SetSpeed(mSpeed);
    UpdateADSR();
    SetFXSend((FxSend *)mFxSend);
    mSlipVoice->Start();
}

void StreamReceiver360::SlipStop() {
    MILO_ASSERT(mSlipEnabled, 0xEC);
    if (mSlipVoice != 0) {
        mSlipVoice->Stop(false);
        mPendingVoices.push_back(mSlipVoice);
        mSlipVoice = 0;
    }
}

void StreamReceiver360::SetSlipSpeed(float f) {
    MILO_ASSERT(mSlipEnabled, 0xFA);
    if (mSlipVoice != 0) {
        mSlipVoice->SetSpeed(f);
    }
}

float StreamReceiver360::GetSlipOffset() {
    MILO_ASSERT(mSlipEnabled, 0x102);
    if (mSlipVoice != 0) {
        int mainAddr = mVoice->GetAddr();
        int slipAddr = mSlipVoice->GetAddr();
        float halfBuf = (float)(mNumBufs << 14) * 0.5f;
        float neg = -halfBuf;
        float slipOff = Mod((float)(slipAddr - mainAddr) - neg, halfBuf - neg) + neg;
        return ((slipOff * 0.5f) / (float)mSampleRate) * 1000.0f;
    }
    return 0.0f;
}

int StreamReceiver360::GetPlayCursor() {
    return mVoice->GetAddr();
}

void StreamReceiver360::PauseImpl(bool b) {
    mVoice->Pause(b);
    if (mSlipEnabled && mSlipVoice != 0) {
        mSlipVoice->Pause(b);
    }
}

void StreamReceiver360::PlayImpl() {
    mVoice->Start();
}

void StreamReceiver360::StartSendImpl(unsigned char *buf, int len, int idx) {
    XMemCpy((idx << 14) + mStreamBuf, buf, len);
}

bool StreamReceiver360::SendDoneImpl() {
    return true;
}

void StreamReceiver360::SetFXSend(FxSend *fx) {
    mFxSend = (int)fx;
    if (mSlipVoice) {
        FxSend360 *fx360 = dynamic_cast<FxSend360 *>(fx);
        mSlipVoice->SetSend(fx360);
    }
}

void StreamReceiver360::UpdateADSR() {
    if (mSlipVoice != 0) {
        mSlipVoice->mAttackRate = mADSR.GetAttackRate();
        mSlipVoice->mReleaseRate = mADSR.GetReleaseRate();
    }
}

StreamReceiver *New360Receiver(int numBuffers, int sampleRate, bool slip, int) {
    return new StreamReceiver360(sampleRate, numBuffers, slip);
}

void StreamReceiver360::Init() {
    StreamReceiver::sFactory = New360Receiver;
}
