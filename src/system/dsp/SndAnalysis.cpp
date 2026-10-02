#include "obj/Data.h"
#include "os/Debug.h"
#include "utl/Symbol.h"
#include <algorithm>
#include <math.h>
#include "xdk/LIBCMT/vectorintrinsics.h"

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
// ShiftedDotProduct's fast path (selected by the fourth parameter,
// `clrlwi. r11, r6, 0x18` at 0x82B816F4) is VMX128: four ss[] outputs per
// outer iteration, accumulated in a 16-byte stack vector at r1-0x20, with the
// three word-shifted windows built by vperm against the table at 0x8219B0E0.
// The scalar else-arm is 0x82B817FC..0x82B81850.
// ---------------------------------------------------------------------------

// vperm selectors that shift a pair of vectors left by one, two and three words.
static const XMVECTORU32 sShiftPerm[3] = {
    { 0x04050607, 0x08090A0B, 0x0C0D0E0F, 0x10111213 },
    { 0x08090A0B, 0x0C0D0E0F, 0x10111213, 0x14151617 },
    { 0x0C0D0E0F, 0x10111213, 0x14151617, 0x18191A1B },
};

// Computes shifted dot products of buf with itself, output to ss.
// ss[i] = sum_{j} buf[j] * buf[j + i] for i in [0, vlen) where vlen = len/2.
void ShiftedDotProduct(const float *buf, int len, float *ss, bool fast) {
    int vlen = len / 2;
    MILO_ASSERT((vlen & 15) == 0, 0x135);

    if (fast) {
        __declspec(align(16)) float acc[4] = { 0.0f };
        const float *end = buf + vlen;
        const float *src = buf;
        float *out = ss;
        for (int i = 0; i < vlen / 4; i++) {
            XMVECTOR next = __lvx(src, 0);
            for (const float *p = buf; p != end;) {
                XMVECTOR b = __lvx(p, 0);
                XMVECTOR cur = next;
                p += 4;
                XMVECTOR a = __lvx(acc, 0);
                a = __vmaddfp(__vspltw(b, 0), cur, a);
                next = __lvx(p + i * 4, 0);
                a = __vmaddfp(__vspltw(b, 1), __vperm(cur, next, sShiftPerm[0].v), a);
                a = __vmaddfp(__vspltw(b, 2), __vperm(cur, next, sShiftPerm[1].v), a);
                a = __vmaddfp(__vspltw(b, 3), __vperm(cur, next, sShiftPerm[2].v), a);
                __stvx(a, acc, 0);
            }
            for (int k = 0; k < 4; k++) {
                out[k] = acc[k];
                acc[k] = 0.0f;
            }
            src += 4;
            out += 4;
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
        if (dp_data[n] > dp_data[n - 1] && dp_data[n] > dp_data[n + 1]) {
            float norm = sqrtf(ss_data[half - 1] * (ss_data[n + half - 1] - ss_data[n - 1]));
            float ratio = dp_data[n] / norm;
            if (ratio > 0.75f) {
                bestcor = (ratio - bestcor >= 0.0f) ? ratio : bestcor; // retail: fsubs + fsel
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
