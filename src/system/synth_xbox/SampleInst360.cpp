#include "synth_xbox/SampleInst360.h"
#include "synth_xbox/Voice.h"

SampleInst360::SampleInst360(SynthSample360 *sample, bool loop, int startSample, int endSample)
    : SampleInst(sample) {
    mVoice = new Voice(sample->IsXMA(), sample->GetNumChannels(), false);
    mVoice->SetSampleRate(sample->GetSampleRate());
    mVoice->SetData((const void *)sample->GetDataAddr(), sample->GetNumBytes(), sample->GetNumSamples());
    if (loop) {
        mVoice->SetLoopRegion(startSample, endSample);
    }
}

SampleInst360::~SampleInst360() {
    Voice *voice = mVoice;
    if (voice) {
        delete voice;
    }
}

bool SampleInst360::IsPlaying() const { return false; }

void SampleInst360::SetFXCore(FXCore core) {}

void SampleInst360::StartImpl() { mVoice->Start(); }

void SampleInst360::StopImpl(bool b) { mVoice->Stop(b); }

void SampleInst360::SetVolumeImpl(float vol) { mVoice->SetVolume(vol); }

void SampleInst360::SetPanImpl(float pan) { mVoice->SetPan(pan); }

void SampleInst360::SetSpeedImpl(float speed) { mVoice->SetSpeed(speed); }

void SampleInst360::Pause(bool b) { mVoice->Pause(b); }

void SampleInst360::SetADSR(const ADSRImpl &adsr) {
    mVoice->mAttackRate = adsr.GetAttackRate();
    mVoice->mReleaseRate = adsr.GetReleaseRate();
}
