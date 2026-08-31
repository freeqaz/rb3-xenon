#include "FFT.h"
#define _USE_MATH_DEFINES
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <cstdlib>
#include "xdk/LIBCMT/vectorintrinsics.h"

// External declarations
int FFTComplex(float* data, long size, long inverse, float* context);

// VMX constants
extern "C" {
    extern unsigned char __vmx_3f800000bf8000003f800000bf800000[];
    extern unsigned char __vmx_bf8000003f800000bf8000003f800000[];
    extern unsigned char __vmx_00000000000000000000000000000000[];
}
int fft_pingpong(float* data, unsigned long size, long sign, float* context);
int fft_square_matrix(float* data, long size, long inverse, float* context);
int fft_recursive(float* data, unsigned long size, long sign, float* context);
int fft_scalar(float* a, float* b, unsigned long size, long sign, float* twiddle);
int fft_altivec(float* a, float* b, unsigned long size, long sign, float* twiddle);
int fft_real_forward_altivec(float* data, long size, float* context);
void SquareComplexTransposeVector(float* data, long n);
int fft_matrix_forward_columnwise(float* data, long size, float* context);
int fft_matrix_inverse_columnwise(float* data, long size, float* scratch);

// Lazily-grown ping-pong scratch buffer shared by fft_pingpong / fft_recursive.
struct FftScratch {
    void* buf;
    unsigned long size;
};
static FftScratch g_fftScratch;

// Real-input forward FFT (scalar). Computes a half-length complex FFT then
// recombines the bins. data holds size real samples (treated as size/2 complex
// pairs); context is FFTComplex scratch.
int fft_real_forward_scalar(float* data, unsigned long size, float* context) {
    if (size < 2) {
        return 0;
    }
    int ret = FFTComplex(data, (long)(size >> 1), -1, context);
    {
        if (ret == 0) {
            float inv_n = 1.0f / (float)(double)(long long)(unsigned int)size;
            double sin_2a = sin(inv_n * (float)(2.0 * M_PI));
            float sin_a = (float)sin(inv_n * (float)M_PI);

            // DC / Nyquist bins.
            double c = 1.0;
            double s = 0.0;
            float ss = (float)sin_2a;
            float re0 = data[0];
            float im0 = data[1];
            data[1] = re0 - im0;
            data[0] = im0 + re0;

            double cc = (double)sin_a * (double)sin_a * 2.0;

            unsigned int count = size >> 2;
            float* lo = data + 2;
            float* hi = data + size;
            for (unsigned int k = 0; k < count; ++k) {
                float hi_im = hi[-1];
                float lo_im = lo[1];
                float diff_im = lo_im - hi_im;
                float lo_re = lo[0];
                float hi_re = hi[-2];
                float sum_im = hi_im + lo_im;
                float sum_re = hi_re + lo_re;
                float diff_re = lo_re - hi_re;

                // Advance the twiddle via the trig recurrence.
                double upd_c = s * ss + c * cc;
                double upd_s = s * cc - c * ss;
                float neg_diff_im = -diff_im;
                c = c - upd_c;
                s = s - upd_s;

                double a = (double)sum_im * c + (double)sum_re;
                double b = (double)diff_im - (double)diff_re * c;
                double d = (double)neg_diff_im - (double)diff_re * c;
                double e = (double)sum_re - (double)sum_im * c;

                a = a - (double)diff_re * s;
                b = b - (double)sum_im * s;
                d = d - (double)sum_im * s;
                e = e + (double)diff_re * s;

                lo[0] = (float)a * 0.5f;
                lo[1] = (float)b * 0.5f;
                hi[-1] = (float)d * 0.5f;
                hi[-2] = (float)e * 0.5f;

                lo += 2;
                hi -= 2;
            }
        }
    }
    return ret;
}

// Iterative radix-2 Cooley-Tukey complex FFT (scalar, ping-pong buffers).
// a/b are the two scratch buffers of size complex pairs; sign is +/-1 to pick
// the transform direction; twiddle is a precomputed cos/sin table. The final
// pass divides by size when sign > 0 (inverse normalization).
// Builds a quarter-symmetric cos/sin twiddle table. table holds n/2 complex
// (cos,sin) pairs; only the first quarter is computed via trig, the rest filled
// by the (-sin, cos) symmetry. Small-n cases (n < 4) are special-cased.
int CalculateSinCosTable(long n, float* table) {
    if (n < 4) {
        table[0] = 1.0f;
        table[1] = 0.0f;
        if (n == 2) {
            table[3] = 0.0f;
            table[2] = -1.0f;
        }
        return 0;
    }

    long count = n / 4;
    long half = n / 2;
    double twoPi = 6.2831854820251465;
    long j = 0;
    for (long i = 0; i < count; ++i, j += 2) {
        float angle = (float)((double)i * twoPi / (double)n);
        float cv = (float)cos(angle);
        float sv = (float)sin(angle);
        table[j] = cv;
        table[j + 1] = sv;
        table[j + half] = -sv;
        table[j + half + 1] = cv;
    }
    return 0;
}

// Runs a complex FFT through the shared ping-pong scratch buffer, growing it on
// demand. Small transforms (< 16 pts) use the scalar kernel; larger ones the
// AltiVec kernel. Returns 0xc on allocation failure.
int fft_pingpong(float* data, unsigned long size, long sign, float* context) {
    int err = 0;
    if (g_fftScratch.size < size) {
        void* old = g_fftScratch.buf;
        g_fftScratch.size = size;
        if (old != 0) {
            free(old);
        }
        void* p = malloc(size << 3);
        g_fftScratch.buf = p;
        if (p == 0) {
            err = 0xc;
            g_fftScratch.buf = 0;
            g_fftScratch.size = 0;
        }
    }
    float* buf = (float*)g_fftScratch.buf;
    int result = err;
    if (err == 0) {
        if (size < 0x10) {
            result = fft_scalar(data, buf, size, sign, context);
        } else {
            result = fft_altivec(data, buf, size, sign, context);
        }
    }
    return result;
}

// Top-level complex FFT dispatcher. Sizes above 0x8000 are decomposed by
// transform length parity: even log2 -> square-matrix path, odd log2 ->
// recursive path. Small sizes go straight through the ping-pong buffer.
int FFTComplex(float* data, long size, long inverse, float* context) {
    if (size <= 0x8000) {
        return fft_pingpong(data, (unsigned long)size, inverse, context);
    }

    long power = 1;
    if (size == 1) {
        power = 0;
    } else {
        long p2 = 2;
        if (size > 2) {
            do {
                p2 *= 2;
                power += 1;
            } while (p2 < size);
        }
    }

    if ((power & 1) == 0) {
        return fft_square_matrix(data, size, inverse, context);
    }
    return fft_recursive(data, (unsigned long)size, inverse, context);
}

// Square-matrix decomposition of a power-of-two complex FFT. Requires size to
// be an exact power of two with an EVEN log2 (so the data can be viewed as a
// square 2^(power/2) x 2^(power/2) complex matrix); returns 22 otherwise.
// Forward (inverse == -1) transforms columnwise then transposes; the inverse
// direction transposes first and then transforms columnwise.
int fft_square_matrix(float* data, long size, long inverse, float* context) {
    long p = 1;
    long power;
    if (size == 1) {
        power = 0;
    } else {
        long p2 = 2;
        if (size > 2) {
            do {
                p2 *= 2;
                p += 1;
            } while (p2 < size);
        }
        power = p;
    }

    if ((1 << power) != size) {
        return 22;
    }
    if ((power & 1) != 0) {
        return 22;
    }

    int ret;
    if (inverse == -1) {
        ret = fft_matrix_forward_columnwise(data, size, context);
        if (ret == 0) {
            SquareComplexTransposeVector(data, 1 << (power / 2));
        }
    } else {
        SquareComplexTransposeVector(data, 1 << (power / 2));
        ret = fft_matrix_inverse_columnwise(data, size, context);
    }
    return ret;
}

int fft_scalar(float* a, float* b, unsigned long size, long sign, float* twiddle) {
    float* src = a;
    float* dst = b;

    int p = 1;
    int power;
    if ((long)size == 1) {
        power = 0;
    } else {
        int p2 = 2;
        if ((long)size > 2) {
            do {
                p2 *= 2;
                p += 1;
            } while (p2 < (long)size);
        }
        power = p;
    }

    if ((unsigned int)(1 << power) != (unsigned int)size) {
        return 0x16;
    }

    int stage = power - 1;
    int blk = 1;
    if (stage > 0) {
        unsigned int half = (unsigned int)size >> 1;
        int stride4 = (int)size * 4;
        int stride8 = (int)size * 8;
        do {
            unsigned int group = 0;
            if (half != 0) {
                int blk8 = blk * 8;
                float* tw = twiddle;
                do {
                    float wr = tw[0];
                    float wi = tw[1] * (float)(double)(long long)sign;
                    if (blk > 0) {
                        int ctr = blk;
                        do {
                            float* hi = (float*)((char*)src + stride4);
                            float hi_im = hi[1];
                            float l_im = src[1];
                            float h_re = hi[0];
                            float l_re = src[0];
                            float t_re = l_re - h_re;
                            dst[0] = h_re + l_re;
                            src += 2;
                            dst[1] = l_im + hi_im;
                            float t_im = l_im - hi_im;
                            *(float*)((char*)dst + blk8) = t_re * wr - t_im * wi;
                            *(float*)((char*)dst + blk8 + 4) = t_re * wi + t_im * wr;
                            dst += 2;
                            ctr -= 1;
                        } while (ctr != 0);
                    }
                    group += blk;
                    dst = (float*)((char*)dst + blk8);
                    tw = (float*)((char*)tw + blk8);
                } while (group < half);
            }
            char* next_dst = (char*)src - stride4;
            src = (float*)((char*)dst - stride8);
            stage -= 1;
            blk *= 2;
            dst = (float*)next_dst;
        } while (stage > 0);
    }

    if (power & 1) {
        dst = src;
    }

    unsigned int group = 0;
    unsigned int half = (unsigned int)size >> 1;
    if (sign > 0) {
        double scale = 1.0 / (double)(long long)(unsigned int)size;
        if (half != 0) {
            int blk8 = blk * 8;
            float* tw = twiddle;
            do {
                float wr = tw[0];
                float wi = tw[1] * (float)(double)(long long)sign;
                if (blk > 0) {
                    int stride4 = (int)size * 4;
                    int ctr = blk;
                    do {
                        float h_re = *(float*)((char*)src + stride4);
                        float* hi = (float*)((char*)src + stride4);
                        float l_re = src[0];
                        float t_re = l_re - h_re;
                        float t_im = src[1] - hi[1];
                        float p_re = t_im * wi;
                        float p_im = t_im * wr;
                        dst[0] = (float)((double)(h_re + l_re) * scale);
                        float l_im = src[1];
                        src += 2;
                        dst[1] = (float)((double)(l_im + hi[1]) * scale);
                        *(float*)((char*)dst + blk8) = (float)((double)(t_re * wr - p_re) * scale);
                        *(float*)((char*)dst + blk8 + 4) = (float)((double)(t_re * wi + p_im) * scale);
                        dst += 2;
                        ctr -= 1;
                    } while (ctr != 0);
                }
                group += blk;
                dst = (float*)((char*)dst + blk8);
                tw = (float*)((char*)tw + blk8);
            } while (group < half);
        }
    } else if (half != 0) {
        int blk8 = blk * 8;
        float* tw = twiddle;
        do {
            float wr = tw[0];
            float wi = tw[1] * (float)(double)(long long)sign;
            if (blk > 0) {
                int stride4 = (int)size * 4;
                int ctr = blk;
                do {
                    float* hi = (float*)((char*)src + stride4);
                    float t_im = src[1] - hi[1];
                    float h_re = *(float*)((char*)src + stride4);
                    float l_re = src[0];
                    float t_re = l_re - h_re;
                    dst[0] = h_re + l_re;
                    float l_im = src[1];
                    src += 2;
                    dst[1] = l_im + hi[1];
                    *(float*)((char*)dst + blk8) = t_re * wr - t_im * wi;
                    *(float*)((char*)dst + blk8 + 4) = t_re * wi + t_im * wr;
                    dst += 2;
                    ctr -= 1;
                } while (ctr != 0);
            }
            group += blk;
            dst = (float*)((char*)dst + blk8);
            tw = (float*)((char*)tw + blk8);
        } while (group < half);
    }

    return 0;
}

// VMX constants
extern "C" {
    extern unsigned char __vmx_3f800000bf8000003f800000bf800000[];
    extern unsigned char __vmx_bf8000003f800000bf8000003f800000[];
    extern unsigned char __vmx_00000000000000000000000000000000[];
}


int fft_matrix_forward_columnwise(float* data, long size, float* context) {
    int ret = 0;
    int power = 1;

    // Declare all VMX types upfront to ensure proper register allocation
    XMVECTOR v_zero;
    XMVECTOR v_sign;
    XMVECTOR v_sin2a;
    XMVECTOR v_sin2;
    XMVECTOR v_im_init;
    XMVECTOR v_cos_vec;
    XMVECTOR v_cos_splat;
    XMVECTOR v_sin_vec;
    XMVECTOR v_sin_merged;
    XMVECTOR v_cos_merged;
    XMVECTOR w_re1, w_im1, w_re2, w_im2;
    XMVECTOR pm_swap_v, pm_lo_v, pm_hi_v;
    XMVECTOR d0, d1, d_swap0, d_swap1;
    XMVECTOR sp_sin2, sp_sin2_2, sp_sin2_3;
    XMVECTOR new_re1, new_re2, new_im1, new_im2;
    XMVECTOR t1, t2, p_im1, p_im2;
    XMVECTOR r1, r2;
    XMVECTOR out_lo, out_hi;
    XMVECTOR a, b, hi;

    XMVECTORF32 sv;
    XMVECTORU32 perm_lo;
    XMVECTORU32 perm_hi;
    XMVECTORU32 perm_swap;

    // Calculate power of 2 for size
    if (size == 1) {
        power = 0;
    } else {
        int p2 = 2;
        if (size > 2) {
            do {
                p2 *= 2;
                power += 1;
            } while (p2 < size);
        }
    }

    // Check if size is power of 2
    if ((1 << power) != size) {
        return 0x16;
    }

    // Check data alignment (must be 16-byte aligned)
    if (((unsigned long)data) & 0xF) {
        return 0x16;
    }

    // Calculate dimensions: rows = 2^(power/2), cols = 2^(ceil(power/2))
    int half_power = power / 2;
    int ceil_half_power = half_power;
    if (power & 1) {
        ceil_half_power = half_power + 1;
    }

    int rows = 1 << half_power;
    int cols = 1 << ceil_half_power;

    // Allocate temporary buffer
    float* temp = (float*)malloc(rows * 0x10);
    if (temp == 0) {
        ret = 0xC;
        goto done_twiddle;
    }

    // Load VMX constants
    v_zero = *(XMVECTOR *)__vmx_00000000000000000000000000000000;
    v_sign = *(XMVECTOR *)__vmx_bf8000003f800000bf8000003f800000;

    // Initialize permutation masks - these will be constructed with lis/ori
    perm_lo.u[0] = 0x00010203;
    perm_lo.u[1] = 0x04050607;
    perm_lo.u[2] = 0x10111213;
    perm_lo.u[3] = 0x14151617;

    perm_hi.u[0] = 0x08090A0B;
    perm_hi.u[1] = 0x0C0D0E0F;
    perm_hi.u[2] = 0x18191A1B;
    perm_hi.u[3] = 0x1C1D1E1F;

    perm_swap.u[0] = 0x04050607;
    perm_swap.u[1] = 0x00010203;
    perm_swap.u[2] = 0x0C0D0E0F;
    perm_swap.u[3] = 0x08090A0B;

    // Step 1: Twiddle factor multiplication + row FFT
    int half_rows = rows / 2;
    int half_cols = cols / 2;

    if (half_rows > 0 && half_cols > 0) {
        float* temp2 = (float*)((char*)temp + half_cols * 0x10);
        int col_idx = 0;
        double two_d = 2.0;
        float* data_ptr = (float*)data;
        float one_f = 1.0f;
        float pi_f = (float)M_PI;
        float total = (float)(double)((long long)(int)(rows * cols));

        int iter = 0;
        do {
            // Compute twiddle angles
            float angle1 = ((float)(long long)col_idx * pi_f) / total;
            float angle2 = ((float)(long long)(col_idx + 2) * pi_f) / total;

            // sin² recurrence parameters
            double s1d = sin(angle1);
            float sin2_1 = (float)(s1d * s1d * two_d);
            float sin_2a1 = (float)sin((float)((double)angle1 * two_d));
            sv.f[0] = sin2_1;
            sv.f[2] = sin_2a1;

            double s2d = sin(angle2);
            float sin2_2 = (float)(s2d * s2d * two_d);
            float sin_2a2 = (float)sin((float)((double)angle2 * two_d));
            sv.f[1] = sin2_2;
            sv.f[3] = sin_2a2;
            v_sin2a = __vmrglw(sv.v, sv.v);
            v_sin2 = __vmrghw(sv.v, sv.v);

            // Start overwriting sv for cos vector
            sv.f[0] = one_f;

            v_im_init = v_sign;
            v_im_init = __vmaddfp(v_sin2a, v_im_init, v_zero);

            sv.f[2] = (float)cos(angle1);
            sv.f[3] = (float)cos(angle2);

            v_cos_vec = __lvx(&sv, 0);
            v_cos_splat = __vspltw(v_cos_vec, 0);

            // Phase 3: Overwrite with sin values, load it
            sv.f[2] = (float)s1d;
            sv.f[3] = (float)s2d;

            v_sin_vec = __lvx(&sv, 0);
            v_sin_merged = __vmrglw(v_sin_vec, v_sin_vec);

            // Initialize running twiddle factors
            v_cos_merged = __vmrglw(v_cos_vec, v_cos_vec);
            w_re1 = v_cos_splat;
            w_im1 = v_zero;
            w_re2 = v_cos_merged;
            w_im2 = __vmaddfp(v_sign, v_sin_merged, v_zero);

            float* dst1 = temp;
            float* dst2 = temp2;
            char* src_data = (char*)data_ptr;
            int k = 0;

            if (half_cols > 0) {
                int data_stride = half_rows * 0x10;
                pm_swap_v = *(XMVECTOR*)&perm_swap;
                pm_lo_v = *(XMVECTOR*)&perm_lo;
                pm_hi_v = *(XMVECTOR*)&perm_hi;

                do {
                    // Load first data element (row 0)
                    d0 = __lvx(src_data, 0);
                    src_data += data_stride;

                    // Copy sin² values for this iteration
                    sp_sin2 = v_sin2;
                    sp_sin2_2 = v_sin2;
                    sp_sin2_3 = v_sin2;

                    // Begin twiddle recurrence
                    new_re1 = __vnmsubfp(w_re1, sp_sin2, w_re1);
                    t1 = __vmaddfp(w_re1, d0, v_zero);
                    d_swap0 = __vperm(d0, d0, pm_swap_v);

                    // Load second data element (row 1)
                    d1 = __lvx(src_data, 0);
                    new_re2 = __vnmsubfp(w_re2, sp_sin2_2, w_re2);
                    d_swap1 = __vperm(d1, d1, pm_swap_v);

                    p_im1 = __vnmsubfp(w_im1, sp_sin2, w_im1);
                    t2 = __vmaddfp(w_re2, d1, v_zero);

                    p_im2 = __vnmsubfp(w_im2, sp_sin2_3, w_im2);
                    new_re1 = __vnmsubfp(w_im1, v_im_init, new_re1);
                    r1 = __vmaddfp(w_im1, d_swap0, t1);
                    new_im1 = __vmaddfp(w_re1, v_im_init, p_im1);
                    r2 = __vmaddfp(w_im2, d_swap1, t2);
                    new_re2 = __vnmsubfp(w_im2, v_im_init, new_re2);
                    new_im2 = __vmaddfp(w_re2, v_im_init, p_im2);

                    w_re1 = new_re1;
                    w_re2 = new_re2;
                    w_im1 = new_im1;
                    w_im2 = new_im2;

                    k += 1;

                    // Interleave results and store to temp
                    out_lo = __vperm(r1, r2, pm_lo_v);
                    out_hi = __vperm(r1, r2, pm_hi_v);

                    __stvx(out_lo, dst1, 0);
                    dst1 += 4;
                    __stvx(out_hi, dst2, 0);
                    dst2 += 4;
                    src_data += data_stride;
                } while (k < half_cols);
            }

            // Row FFT on temp buffer halves
            ret = FFTComplex(temp, cols, -1, context);
            if (ret != 0) goto cleanup;

            ret = FFTComplex((float*)((char*)temp + cols * 8), cols, -1, context);
            if (ret != 0) goto cleanup;

            // Deinterleave from temp back to data
            {
                char* src1 = (char*)temp;
                char* src2 = (char*)temp2;
                char* out = (char*)data_ptr;
                k = 0;
                if (half_cols > 0) {
                    int stride = half_rows * 0x10;
                    do {
                        a = __lvx(src1, 0);
                        b = __lvx(src2, 0);
                        k += 1;
                        src1 += 0x10;
                        src2 += 0x10;
                        hi = __vperm(a, b, *(XMVECTOR*)&perm_hi);
                        __stvx(__vperm(a, b, *(XMVECTOR*)&perm_lo), out, 0);
                        out += stride;
                        __stvx(hi, out, 0);
                        out += stride;
                    } while (k < half_cols);
                }
            }

            iter += 1;
            col_idx += 4;
            data_ptr += 4;
        } while (iter < half_rows);
    }

    // Step 2: Column FFT (forward) on each column, cols-1 down to 0
    int col_i = cols - 1;
    if (col_i >= 0) {
        int neg_stride = -rows;
        int stride8 = neg_stride * 8;
        float* col_ptr = (float*)((char*)data + col_i * rows * 8);
        do {
            ret = FFTComplex(col_ptr, rows, -1, context);
            if (ret != 0) goto cleanup;
            col_i -= 1;
            col_ptr = (float*)((char*)col_ptr + stride8);
        } while (col_i >= 0);
    }

done_twiddle:
cleanup:
    free(temp);
    return ret;
}

#pragma float_control(precise, on, push)
int fft_matrix_inverse_columnwise(float *data, long size, float *scratch) {
    int ret = 0;
    int exp = 1;

    if (size == 1) {
        exp = 0;
    } else {
        int pow2 = 2;
        if (size > 2) {
            do {
                pow2 *= 2;
                exp += 1;
            } while (pow2 < size);
        }
    }

    if ((1 << exp) != size) {
        return 0x16;
    }

    if (((unsigned long)data) & 0xF) {
        return 0x16;
    }

    int half_exp = exp / 2;
    int ceil_half_exp = half_exp;
    if (exp & 1) {
        ceil_half_exp = half_exp + 1;
    }

    int cols = 1 << half_exp;
    int rows = 1 << ceil_half_exp;

    float *temp = (float *)malloc(cols * 0x10);
    if (temp == 0) {
        ret = 0xC;
        goto done_twiddle;
    }

    // Load VMX constants into persistent registers (v124=sign, v125=zero in target)
    XMVECTOR v_zero = *(XMVECTOR *)__vmx_00000000000000000000000000000000;
    XMVECTOR v_sign = *(XMVECTOR *)__vmx_bf8000003f800000bf8000003f800000;

    // Permutation masks on stack (3 masks: lo, hi, swap)
    XMVECTORU32 perm_lo = { 0x00010203, 0x04050607, 0x10111213, 0x14151617 };
    XMVECTORU32 perm_hi = { 0x08090A0B, 0x0C0D0E0F, 0x18191A1B, 0x1C1D1E1F };
    XMVECTORU32 perm_swap = { 0x04050607, 0x00010203, 0x0C0D0E0F, 0x08090A0B };

    // Step 1: Column FFT (inverse) on each column, cols-1 down to 0
    int col_i = cols - 1;
    if (col_i >= 0) {
        int neg_stride = -rows;
        int stride8 = neg_stride * 8;
        float *col_ptr = (float *)((char *)data + col_i * rows * 8);
        do {
            ret = FFTComplex(col_ptr, rows, 1, scratch);
            if (ret != 0) goto cleanup;
            col_i -= 1;
            col_ptr = (float *)((char *)col_ptr + stride8);
        } while (col_i >= 0);
    }

    // Step 2: Twiddle factor multiplication + row FFT
    int iter = 0;
    int half_rows = rows / 2;

    if (half_rows > 0) {
        int half_cols = cols / 2;
        float *temp2 = (float *)((char *)temp + half_cols * 0x10);
        int col_idx = 0;
        double two_d = 2.0;
        float *data_ptr = (float *)data;
        float one_f = 1.0f;
        float pi_f = (float)M_PI;
        float total = (float)(double)((long long)(int)(rows * cols));

        XMVECTORF32 sv;

        do {
            // Compute twiddle angles
            float angle1 = ((float)(long long)col_idx * pi_f) / total;
            float angle2 = ((float)(long long)(col_idx + 2) * pi_f) / total;

            // sin² recurrence parameters
            double s1d = sin(angle1);
            float sin2_1 = (float)(s1d * s1d * two_d);
            float sin_2a1 = (float)sin((float)((double)angle1 * two_d));
            sv.f[0] = sin2_1;
            sv.f[2] = sin_2a1;

            double s2d = sin(angle2);
            float sin2_2 = (float)(s2d * s2d * two_d);
            float sin_2a2 = (float)sin((float)((double)angle2 * two_d));
            sv.f[1] = sin2_2;
            sv.f[3] = sin_2a2;
            XMVECTOR v_sin2a = __vmrglw(sv.v, sv.v);
            XMVECTOR v_sin2 = __vmrghw(sv.v, sv.v);

            // Start overwriting sv for cos vector
            sv.f[0] = one_f;

            XMVECTOR v_im_init = v_sign;
            v_im_init = __vmaddfp(v_sin2a, v_im_init, v_zero);

            sv.f[2] = (float)cos(angle1);
            sv.f[3] = (float)cos(angle2);

            XMVECTOR v_cos_vec = __lvx(&sv, 0);
            XMVECTOR v_cos_splat = __vspltw(v_cos_vec, 0);

            // Phase 3: Overwrite with sin values, load it
            sv.f[2] = (float)s1d;
            sv.f[3] = (float)s2d;

            XMVECTOR v_sin_vec = __lvx(&sv, 0);
            XMVECTOR v_sin_merged = __vmrglw(v_sin_vec, v_sin_vec);

            // Initialize running twiddle factors
            XMVECTOR v_cos_merged = __vmrglw(v_cos_vec, v_cos_vec);
            XMVECTOR w_re1 = v_cos_splat;
            XMVECTOR w_im1 = v_zero;
            XMVECTOR w_re2 = v_cos_merged;
            XMVECTOR w_im2 = __vmaddfp(v_sign, v_sin_merged, v_zero);

            float *dst1 = temp;
            float *dst2 = temp2;
            char *src_data = (char *)data_ptr;
            int k = 0;

            if (half_cols > 0) {
                int data_stride = half_rows * 0x10;
                XMVECTOR pm_swap_v = *(XMVECTOR *)&perm_swap;
                XMVECTOR pm_lo_v = *(XMVECTOR *)&perm_lo;
                XMVECTOR pm_hi_v = *(XMVECTOR *)&perm_hi;

                do {
                    // Load first data element (row 0)
                    XMVECTOR d0 = __lvx(src_data, 0);
                    src_data += data_stride;

                    // Copy sin² values for this iteration
                    XMVECTOR sp_sin2 = v_sin2;
                    XMVECTOR sp_sin2_2 = v_sin2;
                    XMVECTOR sp_sin2_3 = v_sin2;

                    // Begin twiddle recurrence
                    XMVECTOR new_re1 = __vnmsubfp(w_re1, sp_sin2, w_re1);
                    XMVECTOR t1 = __vmaddfp(w_re1, d0, v_zero);
                    XMVECTOR d_swap0 = __vperm(d0, d0, pm_swap_v);

                    // Load second data element (row 1)
                    XMVECTOR d1 = __lvx(src_data, 0);
                    XMVECTOR new_re2 = __vnmsubfp(w_re2, sp_sin2_2, w_re2);
                    XMVECTOR d_swap1 = __vperm(d1, d1, pm_swap_v);

                    XMVECTOR p_im1 = __vnmsubfp(w_im1, sp_sin2, w_im1);
                    XMVECTOR t2 = __vmaddfp(w_re2, d1, v_zero);

                    XMVECTOR p_im2 = __vnmsubfp(w_im2, sp_sin2_3, w_im2);
                    new_re1 = __vnmsubfp(w_im1, v_im_init, new_re1);
                    XMVECTOR r1 = __vmaddfp(w_im1, d_swap0, t1);
                    XMVECTOR new_im1 = __vmaddfp(w_re1, v_im_init, p_im1);
                    XMVECTOR r2 = __vmaddfp(w_im2, d_swap1, t2);
                    new_re2 = __vnmsubfp(w_im2, v_im_init, new_re2);
                    XMVECTOR new_im2 = __vmaddfp(w_re2, v_im_init, p_im2);

                    w_re1 = new_re1;
                    w_re2 = new_re2;
                    w_im1 = new_im1;
                    w_im2 = new_im2;

                    k += 1;

                    // Interleave results and store to temp
                    XMVECTOR out_lo = __vperm(r1, r2, pm_lo_v);
                    XMVECTOR out_hi = __vperm(r1, r2, pm_hi_v);

                    __stvx(out_lo, dst1, 0);
                    dst1 += 4;
                    __stvx(out_hi, dst2, 0);
                    dst2 += 4;
                    src_data += data_stride;
                } while (k < half_cols);
            }

            // Row FFT on temp buffer halves
            ret = FFTComplex(temp, cols, 1, scratch);
            if (ret != 0) goto cleanup;

            ret = FFTComplex((float *)((char *)temp + cols * 8), cols, 1, scratch);
            if (ret != 0) goto cleanup;

            // Deinterleave from temp back to data
            {
                char *src1 = (char *)temp;
                char *src2 = (char *)temp2;
                char *out = (char *)data_ptr;
                k = 0;
                if (half_cols > 0) {
                    int stride = half_rows * 0x10;
                    do {
                        XMVECTOR a = __lvx(src1, 0);
                        XMVECTOR b = __lvx(src2, 0);
                        k += 1;
                        src1 += 0x10;
                        src2 += 0x10;
                        XMVECTOR hi = __vperm(a, b, *(XMVECTOR *)&perm_hi);
                        __stvx(__vperm(a, b, *(XMVECTOR *)&perm_lo), out, 0);
                        out += stride;
                        __stvx(hi, out, 0);
                        out += stride;
                    } while (k < half_cols);
                }
            }

            iter += 1;
            col_idx += 4;
            data_ptr += 4;
        } while (iter < half_rows);
    }

done_twiddle:
cleanup:
    free(temp);
    return ret;
}
#pragma float_control(pop)


// Real-input forward FFT dispatcher: small transforms (< 32) use the scalar
// kernel, larger ones the AltiVec kernel.
int FFTRealForward(float* data, unsigned long size, float* context) {
    if (size < 0x20) {
        return fft_real_forward_scalar(data, size, context);
    }
    return fft_real_forward_altivec(data, (long)size, context);
}
