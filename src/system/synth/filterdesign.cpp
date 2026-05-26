#include "complex.h"
#include <math.h>

extern "C" {
    extern complex* lbl_8316EB70;
    extern char lbl_8316EBA8[0x4008];
    extern char lbl_83172BB0[0x4008];
    extern const double __real_0000000000000000;
    extern const double __real_3f50624dd2f1a9fc;
    extern const double __real_3fe0000000000000;
    extern const double __real_3ff0000000000000;
    extern const double __real_4000000000000000;
    extern const double __real_400921fb60000000;
    extern const double __real_401921fb60000000;
    extern const double __real_bff0000000000000;

    void expand(complex*, int, complex*, ...);
    complex expj(complex*, double);
    double exp(double);
}

complex cexp(complex);

#pragma optimize("gt", on)

void compute_z_mzt(void) {
    char* src_base = (char*)lbl_8316EBA8;
    char* dst_base = (char*)lbl_83172BB0;
    int src_count1;
    int src_count2;
    int loop_count = 0;
    int byte_offset;

    src_count1 = *(int*)(src_base + 0x4000);
    src_count2 = *(int*)(src_base + 0x4004);
    *(int*)(dst_base + 0x4000) = src_count1;
    *(int*)(dst_base + 0x4004) = src_count2;

    if (src_count1 > 0) {
        byte_offset = 0;
        do {
            *(complex*)(dst_base + byte_offset) = cexp(*(complex*)(src_base + byte_offset));
            loop_count++;
            byte_offset += 0x10;
            src_count1 = *(int*)(dst_base + 0x4000);
        } while (loop_count < src_count1);
    }

    src_count2 = *(int*)(dst_base + 0x4004);
    loop_count = 0;

    if (src_count2 > 0) {
        byte_offset = 0x2000;
        do {
            *(complex*)(dst_base + byte_offset) = cexp(*(complex*)(src_base + byte_offset));
            loop_count++;
            byte_offset += 0x10;
            src_count2 = *(int*)(dst_base + 0x4004);
        } while (loop_count < src_count2);
    }
}

void compute_bpres(double arg_sp10, double arg_sp18, double arg_sp20, double arg_sp28,
                   double arg_sp30, double arg_sp38, double arg_sp40, double arg_sp48,
                   double arg_sp50, double arg_sp58, double arg_sp60, double arg_sp68,
                   double arg_sp70,
                   double arg_sp78, complex* arg_sp80, complex* arg_sp90,
                   complex* arg_spA0, complex* arg_sp20B0) {
    double temp_f30 = lbl_8316EB70[6].x * __real_401921fb60000000;
    double temp_f28 = exp(-(temp_f30 / (lbl_8316EB70[0].x * __real_4000000000000000)));
    double omega = temp_f30;
    unsigned char converged = 0;
    double omega_lower = __real_0000000000000000;
    int iteration_count = 0;
    double omega_upper = __real_400921fb60000000;
    double temp_f24 = __real_3f50624dd2f1a9fc;
    double temp_f25 = __real_3fe0000000000000;

    while (1) {
        if (converged != 0) break;

        complex* pole_ptr = &arg_sp80[0];
        expj(pole_ptr, omega);

        arg_sp50 = pole_ptr->x * temp_f28;
        double temp_f0 = arg_sp58 * temp_f28;
        arg_sp58 = temp_f0;

        arg_sp60 = arg_sp50;
        arg_sp68 = -temp_f0;

        expand((complex*)lbl_83172BB0, 2, arg_spA0);

        complex* zero_ptr = &arg_sp90[0];
        expj(zero_ptr, temp_f30);

        double response_ratio = arg_sp78 / arg_sp70;

        if ((float)response_ratio > (float)__real_0000000000000000) {
            omega_upper = omega;
        } else {
            omega_lower = omega;
        }

        if (fabs(response_ratio) < (float)temp_f24) {
            converged = 1;
        }

        iteration_count++;
        omega = (omega_upper + omega_lower) * temp_f25;

        if (iteration_count >= 0x32) {
            break;
        }
    }
}
