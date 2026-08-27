#include "obj/Data.h"
#include "os/Debug.h"
#include "utl/Symbol.h"
#include <algorithm>
#include <math.h>

// ---------------------------------------------------------------------------
// RB3-360 retail: .text 0x82B816F0..0x82B81DD8, seven COMDATs in source order:
//   0x82B816F0 (356 B) ShiftedDotProduct
//   0x82B81860 (920 B) FindCCPeak
//   0x82B81BF8 ( 32 B) ??__F  clears bit 0 of guard 0x82E12B94  (boost)
//   0x82B81C18 ( 32 B) ??__F  clears bit 1                      (minperiod)
//   0x82B81C38 ( 32 B) ??__F  clears bit 2                      (maxperiod)
//   0x82B81C58 ( 32 B) ??__F  clears bit 3                      (numpeaksmin)
//   0x82B81C78 (352 B) RefinePeriod2
//
// ★ ShiftedDotProduct's fast path is HAND-VECTORISED VMX128 in retail
//   (0x82B81758..0x82B817C0: vmaddfp + primary-opcode-4/5/6 VMX128 loads/stores
//   through a 16-byte stack accumulator at r1-0x20), and it is selected by the
//   FOURTH PARAMETER (`clrlwi. r11, r6, 0x18` at 0x82B816F4) -- not by the
//   `(vlen & 15) == 0` test the Wii DEV oracle uses, where that parameter is
//   marked `/*unused*/` and the fast path is a Gekko paired-single asm block.
//   The 360 fast path cannot be reconstructed from the Wii oracle: it is a
//   different instruction set doing a different (4-wide, not 2-wide) blocking.
//   The scalar path below IS retail's else-arm instruction for instruction
//   (0x82B817FC..0x82B81850). Deliberately left as a partial match rather than
//   guessed at.
// ---------------------------------------------------------------------------

// Computes shifted dot products of buf with itself, output to ss.
// ss[i] = sum_{j} buf[j] * buf[j + i] for i in [0, vlen) where vlen = len/2.
void ShiftedDotProduct(const float *buf, int len, float *ss, bool fast) {
    unsigned int vlen = len / 2;
    MILO_ASSERT((vlen & 15) == 0, 0x135);

    if (fast) {
        // Retail: VMX128, four ss[] outputs per outer iteration. Not
        // reconstructible from the Wii paired-single oracle (see note above).
        for (int i = 0; i < (int)vlen; i += 4) {
            float acc[4];
            acc[0] = acc[1] = acc[2] = acc[3] = 0.0f;
            for (int j = 0; j < vlen; j++) {
                acc[0] += buf[j] * buf[j + i];
                acc[1] += buf[j] * buf[j + i + 1];
                acc[2] += buf[j] * buf[j + i + 2];
                acc[3] += buf[j] * buf[j + i + 3];
            }
            ss[i] = acc[0];
            ss[i + 1] = acc[1];
            ss[i + 2] = acc[2];
            ss[i + 3] = acc[3];
        }
    } else {
        for (int i = 0; i < vlen; i++) {
            float acc = 0.0f;
            for (int j = 0; j < vlen; j++) {
                acc += buf[j] * buf[j + i];
            }
            ss[i] = acc;
        }
    }
}

// Finds the period of the largest cross-correlation peak in dp_data.
int FindCCPeak(const float *dp_data, const float *ss_data, int vlen, int startPeriod) {
    static const DataNode &boost = DataVariable("boost");
    static DataNode &minperiod = DataVariable("minperiod");
    static DataNode &maxperiod = DataVariable("maxperiod");
    static DataNode &numpeaksmin = DataVariable("numpeaksmin");

    int peaks[10];
    float cors[10];
    float goodness[10];
    int num_peaks = 0;
    float bestcor = 0.0f;

    // Scan dp_data for local maxima; reject those whose normalized correlation
    // is too low.
    int half = vlen / 2;
    int max_peaks = half - 1;
    for (int n = startPeriod; n < max_peaks; n++) {
        float dp = dp_data[n];
        if (dp > dp_data[n - 1] && dp > dp_data[n + 1]) {
            float ssa = ss_data[n - 1];
            float ssb = ss_data[n + half - 1];
            float norm = sqrtf(ss_data[half - 1] * (ssb - ssa));
            float ratio = dp / norm;
            if (ratio > 0.75f) {
                if (ratio > bestcor) {
                    bestcor = ratio;
                }
                cors[num_peaks] = ratio;
                peaks[num_peaks] = n;
                num_peaks++;
                if (num_peaks >= 10) {
                    break;
                }
            }
        }
    }

    if (num_peaks == 0 || bestcor < 0.9f) {
        return 0;
    }

    // Boost: weight each peak's correlation by a power of (peak index) to favor
    // shorter periods (higher fundamental frequencies).
    int boost_val = boost.Int(NULL);
    if (boost_val == 0) {
        boost_val = 140;
    }

    for (int i = 0; i < num_peaks; i++) {
        static float bonus_exp = (float)log((float)boost_val / 100.0f) / (float)log(0.5);
        goodness[i] = cors[i] * (float)pow((float)peaks[i], bonus_exp);
    }

    float *best = std::max_element(goodness, goodness + num_peaks);
    int bestIdx = (int)(best - goodness);

    int min_p = minperiod.Int(NULL);
    maxperiod.Int(NULL); // result intentionally unused
    int num_min = numpeaksmin.Int(NULL);
    if (num_min == 0) {
        num_min = 8;
    }
    if (min_p == 0) {
        min_p = 11;
    }

    int period = peaks[bestIdx];
    if (period < min_p && num_peaks <= num_min && bestcor < 0.99f) {
        return 0;
    }
    return period;
}

// Parabolic refinement of a discrete peak period using local correlation values.
float RefinePeriod2(
    const float *buf, const float *autocorr, const float *dp, int vlen, int period
) {
    int half = vlen / 2;
    float alpha = 0.0f;

    int attempt = 0;
    while (attempt < 2 && period > 0) {
        // Load order is retail's (0x82B81CCC..0x82B81CE0): autocorr[period-1],
        // autocorr[period], autocorr[half+period-1], autocorr[half+period],
        // and v2 is differenced before v1.
        float v2v2 = autocorr[period - 1];
        float v1v1 = autocorr[period];
        float wv2 = autocorr[half + period - 1];
        float wv1 = autocorr[half + period];
        float v2 = wv2 - v2v2;
        float v1 = wv1 - v1v1;
        float v1v2 = dp[period];
        float next_dp = dp[period + 1];

        // inner = sum_{j<half} buf[period+j] * buf[period+j+1]
        float inner = 0.0f;
        for (int j = 0; j < half; j++) {
            inner += buf[period + j] * buf[period + j + 1];
        }

        float num = ((next_dp - inner) - v1v2) + v2;
        float denom = (v1 + v2) - 2.0f * inner;
        alpha = num / denom;

        if (alpha > 1.0f) {
            period++;
        } else if (alpha < 0.0f) {
            period--;
        } else {
            break;
        }
        attempt++;
    }

    float result = (float)period + alpha;
    if (result <= 0.0f || fabsf(alpha) > 3.0f) {
        period = 0;
        alpha = 0.0f;
    }
    return (float)period + alpha;
}
