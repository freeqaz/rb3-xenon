#include "synth/SampleInst.h"
#include "math/Decibels.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "synth/SynthSample.h"

SampleInst::SampleInst(SynthSample *sample)
    : mVolume(1), mBankVolume(1), mPan(0), mBankPan(0), mSpeed(1), mBankSpeed(1),
      mSend(this)
#ifdef HX_NATIVE
      ,
      unka0(0), unka1(0), mEventReceiver(this), unk98(-1), mSample(this, sample)
#endif
{
    // Retail's ctor inits nothing past mSend: unka0/unka1 are left to Play(),
    // and there is no mSample (see SampleInst.h).
#ifdef HX_NATIVE
    if (mSample) {
        mSample->RegisterChild(this);
    }
#endif
}

SampleInst::~SampleInst() {
#ifdef HX_NATIVE
    if (mSample) {
        mSample->UnregisterChild(this);
    }
#endif
}

void SampleInst::Play(float f1) {
    SetVolume(f1);
    Stop(false);
    StartImpl();
#ifdef HX_NATIVE
    StartPolling();
#endif
    unka1 = true;
    unka0 = true;
}

void SampleInst::Stop(bool b1) {
    StopImpl(b1);
#ifdef HX_NATIVE
    CancelPolling();
#endif
}

bool SampleInst::DonePlaying() {
    bool ret = !IsPlaying();
    if (ret) {
        delete this;
    }
    return ret;
}

void SampleInst::SetVolume(float vol) {
    mVolume = DbToRatio(vol);
    UpdateVolume();
}

void SampleInst::SetPan(float pan) {
    mPan = pan;
    SetPanImpl(mPan + mBankPan);
}

void SampleInst::SetSpeed(float spd) {
    mSpeed = spd;
    SetSpeedImpl(mSpeed * mBankSpeed);
}

void SampleInst::SetReverbMixDb(float db) {
    mReverbMixDb = db;
    SetReverbMixDbImpl(mReverbMixDb);
}

void SampleInst::SetReverbEnable(bool b) {
    mReverbEnabled = b;
    SetReverbEnableImpl(mReverbEnabled);
}

void SampleInst::SetSend(FxSend *send) {
    mSend = send;
    SetSendImpl(send);
}

#ifdef HX_NATIVE
void SampleInst::SetEventReceiver(Hmx::Object *rcvr) { mEventReceiver = rcvr; }

void SampleInst::EndLoop() {
    mEventReceiver = nullptr;
    EndLoopImpl();
}
#endif

void SampleInst::UpdateVolume() { SetVolumeImpl(mVolume * mBankVolume); }

void SampleInst::SetBankVolume(float vol) {
    mBankVolume = DbToRatio(vol);
    UpdateVolume();
}

void SampleInst::SetBankPan(float bpan) {
    mBankPan = bpan;
    SetPanImpl(mPan + mBankPan);
}

void SampleInst::SetBankSpeed(float bspd) {
    mBankSpeed = bspd;
    SetSpeedImpl(mSpeed * mBankSpeed);
}

#ifdef HX_NATIVE
void SampleInst::SynthPoll() {
    SynthSample *sample = Sample();
    Hmx::Object *rcvr = GetEventReceiver();
    if (!sample || !rcvr) {
        return;
    }
    double currentSample = (double)sample->LengthMs() * (double)sample->GetSampleRate()
        * (double)GetProgress() * 0.001;
    static Message msg("on_marker_event", 0L);
    std::vector<SampleMarker> markers = sample->AccessMarkers();
    for (auto it = markers.begin(); it != markers.end(); ++it) {
        if ((double)it->Sample() <= currentSample) {
            msg->Node(2) = DataNode(Symbol(it->Name().c_str()));
            rcvr->Handle(msg, false);
        }
    }
}
#endif
