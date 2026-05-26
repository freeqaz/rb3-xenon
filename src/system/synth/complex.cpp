#include "complex.h"
#include <cmath>

complex expj(double d1) {
    struct complex result;
    double dVar1 = sin(d1);
    double dVar2 = cos(d1);

    result.x = dVar2;
    result.y = dVar1;

    return result;
}

complex csqrt(complex cplx) {
    complex result;
    double h;
    double dy;
    double dx;

        auto _tmp0 = hypot(cplx.y, cplx.x);
        dy = (h = _tmp0 - cplx.x) * 0.5;
        if ((dy >= 0.0)) {
        result.y = sqrt(dy);
    } else {
        result.y = 0.0;
    }

    dx = (cplx.y + h) * 0.5;
    result.x = (dx >= 0.0) ? sqrt(dx) : 0.0;

    if (cplx.y < 0.0)
        result.y = -result.y;

    return result;
}

complex cexp(complex cplx) {
    struct complex result;
    complex phase = expj(cplx.y);
    double magnitude = exp(cplx.x);
    phase.x = magnitude * phase.x;
    phase.y = magnitude * phase.y;
    result = phase;
    return result;
}

complex operator/(complex cplx1, complex cplx2) {
    complex result;
    double dVar1 = 1.0 / (cplx2.x * cplx2.x + cplx2.y * cplx2.y);
    result.x = (cplx1.y * cplx2.y + cplx1.x * cplx2.x) * dVar1;
    result.y = (cplx1.y * cplx2.x - cplx1.x * cplx2.y) * dVar1;
    return result;
}

complex operator*(complex cplx1, complex cplx2) {
    complex result;
    result.x = cplx1.x * cplx2.x - cplx2.y * cplx1.y;
    result.y = cplx2.x * cplx1.y + cplx2.y * cplx1.x;
    return result;
}

// Evaluate polynomial using Horner's method.
// coeff[0..degree] are the coefficients, z is the evaluation point.
// Returns: coeff[0] + coeff[1]*z + ... + coeff[degree]*z^degree
complex eval(complex * const coeff, int degree, complex z) {
    complex result;
    complex accum;
    result.x = 0.0;
    result.y = 0.0;
    if (degree >= 0) {
        int count = degree + 1;
        complex *c = &coeff[degree];
        do {
            complex old = result;
            complex cf = *c;
            count--;
            c--;
            complex temp;
            temp.y = old.x * z.y + old.y * z.x;
            temp.x = z.x * old.x - z.y * old.y;
            accum = temp;
            accum.x += cf.x;
            accum.y += cf.y;
            result = accum;
        } while (count != 0);
    }
    return result;
}

// Evaluate rational function: numerator(z) / denominator(z)
complex evaluate(complex *const num, int numDeg, complex *const den, int denDeg, complex z) {
    complex denom = eval(den, denDeg, z);
    complex numer = eval(num, numDeg, z);
    complex result;
    double invMagSq = 1.0 / (denom.x * denom.x + denom.y * denom.y);
    result.x = (numer.y * denom.y + numer.x * denom.x) * invMagSq;
    result.y = (numer.y * denom.x - numer.x * denom.y) * invMagSq;
    return result;
}
