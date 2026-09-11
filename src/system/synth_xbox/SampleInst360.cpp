#include "synth_xbox/SampleInst360.h"
#include "synth_xbox/Voice.h"

SampleInst360::SampleInst360(SynthSample360 *sample, bool loop, int startSample, int endSample)
    : SampleInst(sample) {
    // Retail 0x82B6DFB8 passes (IsXMA(), 0, 0): no channel-count call at all.
    mVoice = new Voice(sample->IsXMA(), false, false);
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

// Retail 0x82B6E118 is `lwz r3, 0x54(r3); b ?IsPlaying@Voice@@` -- it forwards to the
// voice; the `return false` this tree carried made every sample instance report
// stopped (lane W4-D).
bool SampleInst360::IsPlaying() const { return mVoice->IsPlaying(); }

void SampleInst360::SetFXCore(FXCore core) {}

void SampleInst360::StartImpl() { mVoice->Start(); }

void SampleInst360::StopImpl(bool) { mVoice->Stop(); } // retail 0x82B6E108: `lwz r3, 0x54(r3); b Voice::Stop`

void SampleInst360::SetVolumeImpl(float vol) { mVoice->SetVolume(vol); }

void SampleInst360::SetPanImpl(float pan) { mVoice->SetPan(pan); }

void SampleInst360::SetSpeedImpl(float speed) { mVoice->SetSpeed(speed); }

void SampleInst360::Pause(bool b) { mVoice->Pause(b); }

void SampleInst360::SetADSR(const ADSRImpl &adsr) {
    mVoice->mAttackRate = adsr.GetAttackRate();
    mVoice->mReleaseRate = adsr.GetReleaseRate();
}
