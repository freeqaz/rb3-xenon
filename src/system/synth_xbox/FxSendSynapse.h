#pragma once

namespace DSP {

struct SynapseBand {
    char enabled;       // 0x00
    char pad[3];        // 0x01-0x03
    float freq;         // 0x04 - center frequency
    float gain;         // 0x08 - gain in dB
    float q;            // 0x0c - quality factor
    float coeff0;       // 0x10 - biquad filter coefficient
    float coeff1;       // 0x14 - biquad filter coefficient
    float coeff2;       // 0x18 - biquad filter coefficient
};  // size = 0x1c

struct SynapseAPOParams {
    SynapseAPOParams();

    SynapseBand bands[3];     // 0x00 - 0x53
    float lowCutoffFreq;      // 0x54 - low cutoff frequency (default 20 Hz)
    float highCutoffFreq;     // 0x58 - high cutoff frequency (default 40 Hz)
};

}  // namespace DSP
