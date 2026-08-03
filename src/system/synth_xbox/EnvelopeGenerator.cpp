#include "synth_xbox/EnvelopeGenerator.h"
#include "math/Decibels.h"
#include "os/Debug.h"

XAPO_REGISTRATION_PROPERTIES
ATG::CSampleXAPOBase<EnvelopeGenerator, EnvelopeGeneratorParams>::m_regProps;

EnvelopeGenerator::EnvelopeGenerator() : unk8c(0) {
    EnvelopeGeneratorParams p;
    p.unk0 = 0;
    p.unk4 = 0;
    p.unk8 = 0;
    p.unkc = 0;
    unk84 = 0;
    unk88 = 0;
    unk90 = 0;
    SetParameters(&p, sizeof(EnvelopeGeneratorParams));
}

void EnvelopeGenerator::OnSetParameters(const EnvelopeGeneratorParams &p) {
    unk84 = p.unk0 * 48000;
    unk88 = p.unk4 * 48000;
    if (p.unk8 > 0.5f) {
        unk90 = 2;
    }
}

void EnvelopeGenerator::DoProcess(
    const EnvelopeGeneratorParams &params, float *__restrict buffer, unsigned int,
    unsigned int numChannels
) {
    int nch = numChannels;
    if (nch != 1 && nch != 2) {
        return;
    }

    if (unk90 == 1)
        return;

    if (unk90 == 3) {
        if (nch == 1) {
            for (unsigned int i = 0; i < 256; i++) {
                buffer[i] = 0;
            }
        } else {
            for (unsigned int i = 0; i < 256 * 2; i++) {
                buffer[i] = 0.0f;
            }
        }
        return;
    }

    float gain = unk8c;
    float clampedDb = RatioToDb(gain);
    if (clampedDb < -60.0f)
        clampedDb = -60.0f;

    int durationSamples = (unk90 == 0) ? unk84 : unk88;
    float deltaDb = 256.0f * 60.0f / (float)durationSamples;
    if (unk90 == 2) {
        deltaDb = -deltaDb;
    }

    int rampFrames = 256;
    if (deltaDb > -clampedDb) {
        float frac = clampedDb / deltaDb;
        frac = -frac;
        rampFrames = (int)(frac * 256.0f);
        deltaDb = -clampedDb;
    } else if (unk90 == 2 && deltaDb + clampedDb < -60.0f) {
        float negDelta = -deltaDb;
        float scaled = (clampedDb - -60.0f) * 256.0f;
        rampFrames = (int)(scaled / negDelta);
    }

    float ramp = DbToRatio(deltaDb + clampedDb);
    float gainStep = (ramp - gain) / (float)rampFrames;
    float zero = 0.0f;
    float one = 1.0f;
    int i = 0;
    if (rampFrames > 0) {
        float *stereo = buffer;
        float *mono = buffer;
        for (; i < rampFrames; i++) {
            if (nch == 1) {
                mono[0] = mono[0] * gain;
            } else {
                stereo[0] = stereo[0] * gain;
                stereo[1] = stereo[1] * gain;
            }
            gain = gainStep + gain;
            mono += 1;
            stereo += 2;
        }
    }

    // Tested on `i`, NOT on rampFrames -- equivalent (i == rampFrames when the
    // ramp loop ran, i == 0 otherwise, and 0 < 256), but codegen-load-bearing:
    //  * on the rampFrames<=0 path `i` is the constant 0, so MSVC folds this
    //    branch away and the `ble` above jumps straight past it (retail does);
    //    that in turn keeps 0.0f/1.0f materialised before the ramp loop.
    //  * it makes the mono fill loop's entry guard below the IDENTICAL
    //    comparison, so dominator redundancy elimination drops it.
    // Spelling it `rampFrames < 256` costs 8 bytes across three sites.
    if (i < 256) {
        if (unk90 == 0) {
            gain = one;
            unk90 = 1;
        } else {
            unk90 = 3;
            gain = zero;
            if (nch == 1) {
                // Signed index is load-bearing: retail drops this loop's entry
                // guard entirely because it is the SAME comparison as the
                // dominating `rampFrames < 256` test (`cmpwi ..,0x100`/`bge`),
                // so dominator-based redundancy elimination removes it. An
                // `unsigned` index emits `cmplwi` instead — a different opcode,
                // not recognised as redundant, leaving an extra cmplwi/bge pair.
                // (The stereo loop below keeps its guard because its bound 512
                // does not match the dominating test.)
                for (int j = i; j < 256; j++) {
                    buffer[j] = 0;
                }
            } else {
                for (int j = i; j < 256 * 2; j++) {
                    buffer[j] = 0.0f;
                }
            }
        }
    }

    EnvelopeGeneratorParams copy = params;
    copy.unkc = (unk90 == 3) ? one : zero;
    SetParameters(&copy, sizeof(EnvelopeGeneratorParams));
    unk8c = gain;
}
