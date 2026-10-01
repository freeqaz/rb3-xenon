#include "FxSendMeterEffect.h"
#include "FxSend.h"
#include "MeterEffect.h"
#include "macros.h"
#include "xdk/xaudio2/xaudio2.h"

FxSendMeterEffect360::FxSendMeterEffect360() : FxSend360(this), unkb0(0) {}

FxSendMeterEffect360::~FxSendMeterEffect360() {
    delete unkb0;
    unkb0 = 0;
}

// Retail 0x82B63EC8 / 0x82B63ED0 / 0x82B63ED8, slots 27 / 22 / 26 of
// FxSendMeterEffect360's vtable: `addi r3,r3,0x64` (the FxSend360 base) and a
// tail call to FxSend360::SyncEffectParams / Refresh / UpdateVolumes.
void FxSendMeterEffect360::Recreate(std::vector<FxSend *> &sends) {
    FxSend360::Refresh(sends);
}

void FxSendMeterEffect360::UpdateMix() { FxSend360::UpdateVolumes(); }

void FxSendMeterEffect360::OnParametersChanged() { FxSend360::SyncEffectParams(); }

IUnknown *FxSendMeterEffect360::CreateFx() {
    return static_cast<CXAPOBase *>(new MeterEffect());
}

// Retail @82B34FE0 (0x58).
void FxSendMeterEffect360::SyncEffectParams(IXAudio2SubmixVoice *voice) const {
    int val;
    if (unkb0) {
        val = *unkb0;
    }
    voice->SetEffectParameters(0, &val, sizeof(val), 0);
}

// Retail @82B35478 (0x114): rebuilds the channel LevelData list and hands the
// meter XAPO a pointer to the vector's data block (same trick as the master
// MeterEffect in Synth360::PreInit).
void FxSendMeterEffect360::InitParams(IXAudio2SubmixVoice *voice, int chans) {
    std::vector<LevelData> *channels = &mChannels;
    channels->erase(channels->begin(), channels->end());
    if (chans != 1) {
        if (chans == 2) {
            LevelData left("left");
            LevelData right("right");
            channels->push_back(left);
            channels->push_back(right);
        }
    } else {
        LevelData mono("center");
        channels->push_back(mono);
    }
    delete unkb0;
    unkb0 = 0;
    unkb0 = new int;
    *unkb0 = *(int *)&mChannels;
    voice->SetEffectParameters(0, &unkb0, 4, 0);
}
