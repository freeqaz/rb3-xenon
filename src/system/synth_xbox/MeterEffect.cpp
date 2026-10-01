#include "synth_xbox/MeterEffect.h"
#include "synth/FxSend.h"
#include <cmath>

MeterEffect::MeterEffect() : unk90(0) {
    for (int i = 0; i < 6; i++) {
        mStats[0][i] = 0;
        mStats[1][i] = 0;
    }
    MeterEffectParams p;
    p.unk0 = 0;
    SetParameters(&p, sizeof(p));
}

void MeterEffect::OnSetParameters(const MeterEffectParams &params) {
    unk90 = 0;
    for (int i = 0; i < 6; i++) {
        mStats[0][i] = 0;
    }
}

// Retail 0x82B6B768 (MeterEffect's vtable slot 17). Planar buffer: per
// channel, accumulate the sum of squares and track the peak; when the params
// point at a LevelData array, publish the peak (then reset it) and the RMS
// over every frame seen since the last OnSetParameters.
void MeterEffect::DoProcess(
    const MeterEffectParams &params, float *__restrict data, unsigned int frames,
    unsigned int channels
) {
    for (unsigned int c = 0; c < channels; c++) {
        for (unsigned int i = 0; i < frames; i++) {
            float s = data[c * frames + i];
            float a = std::fabs(s);
            mStats[0][c] += s * s;
            if (mStats[1][c] < a) {
                mStats[1][c] = a;
            }
        }
        if (params.unk0) {
            ((LevelData *)params.unk0)[c].mPeak = mStats[1][c];
            mStats[1][c] = 0;
            ((LevelData *)params.unk0)[c].mRMS = std::sqrt(mStats[0][c] / (float)(int)unk90);
        }
    }
    unk90 += frames;
}
