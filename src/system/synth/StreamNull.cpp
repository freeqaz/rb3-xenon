#include "synth/StreamNull.h"
#include "synth/Faders.h"
#include "utl/Std.h"

StreamNull::StreamNull(float f) : mFaders(nullptr) {
    mTimer.Reset(f);
    mChannelFaders.resize(20);
    FOREACH (it, mChannelFaders) {
        *it = new FaderGroup(nullptr);
    }
}

StreamNull::~StreamNull() { DeleteAll(mChannelFaders); }

bool StreamNull::IsFinished() const { return !mTimer.Running(); }
void StreamNull::Play() { mTimer.Start(); }
void StreamNull::Stop() { mTimer.Stop(); }
bool StreamNull::IsPlaying() const { return mTimer.Running(); }

void StreamNull::Resync(float f) {
    mTimer.Stop();
    mTimer.Reset(f);
}

float StreamNull::GetTime() { return mTimer.Ms(); }

void StreamNull::SetSpeed(float speed) { mTimer.SetSpeed(speed); }

FaderGroup &StreamNull::ChannelFaders(int idx) { return *mChannelFaders[idx]; }
