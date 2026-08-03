// Decompiled from assembly
#include <cmath>

namespace DSP {

// RBJ biquad cookbook low-pass coefficients.
// coeffs layout: b0=coeffs[0], b1=coeffs[1], b2=coeffs[2],
//                a0=coeffs[3], a1=coeffs[4], a2=coeffs[5]
// On return the feedforward/feedback taps are normalized by a0 and a0 is set to 1.
void LowpassCoefficients(float *const coeffs, float sampleRate, float freq, float q) {
    if (freq > sampleRate * 0.5) {
        coeffs[3] = 1.0f;
        coeffs[0] = 1.0f;
        coeffs[5] = 0.0f;
        coeffs[4] = 0.0f;
        coeffs[2] = 0.0f;
        coeffs[1] = 0.0f;
        return;
    }

    double w0 = freq * 6.2831854820251465 / sampleRate;
    double sinw = sin(w0);
    double cosw = cos(w0);
    double alpha = sinw / (q * 2.0f);

    coeffs[4] = (float)(cosw * -2.0);
    coeffs[0] = (float)((1.0 - cosw) * 0.5);
    coeffs[2] = (float)((1.0 - cosw) * 0.5);
    coeffs[1] = (float)(1.0 - cosw);
    coeffs[3] = (float)(alpha + 1.0);
    coeffs[5] = (float)(1.0 - alpha);

    for (int i = 0; i < 3; i++) {
        coeffs[i] /= coeffs[3];
    }
    for (int i = 4; i < 6; i++) {
        coeffs[i] /= coeffs[3];
    }
    coeffs[3] = 1.0f;
}

// RBJ biquad cookbook high-pass coefficients.
void HighpassCoefficients(float *const coeffs, float sampleRate, float freq, float q) {
    if (freq > sampleRate * 0.5) {
        coeffs[3] = 1.0f;
        coeffs[5] = 0.0f;
        coeffs[4] = 0.0f;
        coeffs[2] = 0.0f;
        coeffs[1] = 0.0f;
        coeffs[0] = 0.0f;
        return;
    }

    double w0 = freq * 6.2831854820251465 / sampleRate;
    double sinw = sin(w0);
    double cosw = cos(w0);
    double alpha = sinw / (q * 2.0f);

    coeffs[4] = (float)(cosw * -2.0);
    coeffs[0] = (float)((1.0 + cosw) * 0.5);
    coeffs[1] = (float)(-(1.0 + cosw));
    coeffs[2] = (float)((1.0 + cosw) * 0.5);
    coeffs[3] = (float)(alpha + 1.0);
    coeffs[5] = (float)(1.0 - alpha);

    for (int i = 0; i < 3; i++) {
        coeffs[i] /= coeffs[3];
    }
    for (int i = 4; i < 6; i++) {
        coeffs[i] /= coeffs[3];
    }
    coeffs[3] = 1.0f;
}

} // namespace DSP
