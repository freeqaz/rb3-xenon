#pragma once

namespace DSP {

// One pitch-corrected voice. Field meanings follow SynapseAPO::OnSetParameters,
// which hands each to the matching Synapse::SetVoice* setter.
struct SynapseBand {
    bool enabled;          // 0x00
    char pad[3];           // 0x01-0x03
    float noteHz;          // 0x04 - target note
    float gain;            // 0x08
    float transposition;   // 0x0c
    float amount;          // 0x10
    float proximityEffect; // 0x14
    float proximityFocus;  // 0x18
};  // size = 0x1c

struct SynapseAPOParams {
    // Inline: retail 0x82B6A1D0 is a COMDAT, so SyncEffectParams cannot rely
    // on its register usage and keeps `this` in a callee-saved register.
    SynapseAPOParams() {
        for (int i = 0; i < 3; i++) {
            bands[i].noteHz = 220.0f;
            bands[i].gain = 0.0f;
            bands[i].enabled = 0;
            bands[i].transposition = 0.0f;
        }
        attackSmoothing = 20.0f;
        releaseSmoothing = 40.0f;
    }

    SynapseBand bands[3];   // 0x00 - 0x53
    float attackSmoothing;  // 0x54 (default 20 ms)
    float releaseSmoothing; // 0x58 (default 40 ms)
};

}  // namespace DSP
