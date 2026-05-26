#pragma once
#include "os/Debug.h"
#include <cmath>

#define kSmallFloat 0.0001f
#define kHugeFloat 1.0e30f
// idk where else to put this one
#define kDbSilence -96.0f

inline float ScaleUShortToFloat01(unsigned short u) { return u / 65535.0f; }

inline int CountBits(int num) {
    int temp_r0;
    int var_r3;
    int var_r4;

    var_r3 = num;
    var_r4 = 0;
    while (var_r3 != 0U) {
        temp_r0 = var_r3 & 1;
        var_r3 = (int)((unsigned int)var_r3 >> 1U);
        var_r4 += temp_r0;
    }
    return var_r4;
}

inline int HighestBit(int num) {
    if (num == 0) {
        return 0;
    } else {
        int bit = 0x80000000;
        while (!(bit & num)) {
            bit >>= 1;
        }
        return bit;
    }
}

inline int LowestBit(int num) {
    if (num == 0U) {
        return 0;
    } else {
        int bit = 1;
        while (!(bit & num)) {
            bit <<= 1;
        }
        return bit;
    }
}

inline int Round(float f) {
    if (f > (float)0.0) {
        return (int)((float)0.5 + f);
    } else {
        return (int)(f - (float)0.5);
    }
}

template <class T>
inline T Min(T x, T y) {
    return (y < x) ? y : x;
}

// float specialization for the use of fsel instructions
template <>
inline float Min(float x, float y) {
    return (x - y < 0) ? x : y;
}

template <class T>
inline T Max(T x, T y) {
    return (x < y) ? y : x;
}

// float specialization for the use of fsel instructions
template <>
inline float Max(float x, float y) {
    return (x - y < 0) ? y : x;
}

template <class T>
inline T Min(T x, T y, T z) {
    return Min(x, Min(y, z));
}

template <class T>
inline T Max(T x, T y, T z) {
    return Max(x, Max(y, z));
}

template <class T>
inline T Clamp(T min, T max, T value) {
    return value > max ? max : (value < min ? min : value);
}

// float specialization for the use of fsel instructions
template <>
inline float Clamp(float min, float max, float value) {
    return Min(Max(min, value), max);
}

template <class T>
inline bool ClampEq(T &value, const T &min, const T &max) {
    if (value < min) {
        value = min;
        return true;
    } else if (value > max) {
        value = max;
        return true;
    }
    return false;
}

// float specialization for the use of fsel instructions
template <>
inline bool ClampEq(float &value, const float &min, const float &max) {
    float tmp = value;
    value = Min(Max(min, value), max);
    return tmp != value;
}

template <class T>
inline bool MinEq(T &x, const T &y) {
    if (y < x) {
        x = y;
        return true;
    }
    return false;
}

// float specialization for the use of fsel instructions
template <>
inline bool MinEq(float &x, const float &y) {
    float tmp = x;
    x = Min(x, y);
    return x != tmp;
}

template <class T>
inline bool MaxEq(T &x, const T &y) {
    if (x < y) {
        x = y;
        return true;
    }
    return false;
}

// float specialization for the use of fsel instructions
template <>
inline bool MaxEq(float &x, const float &y) {
    float tmp = x;
    x = Max(x, y);
    return x != tmp;
}

inline float Abs(float x) { return fabsf(x); }

template <class T>
inline const T Abs(T x) {
    if (x > 0)
        return x;
    else
        return -x;
}

inline bool IsNaN(float f) { return !(f == f); }

inline int Mod(int num, int modbase) {
    if (modbase == 0)
        return 0;
    int div = num % modbase;
    if (div < 0)
        return div + modbase;
    else
        return div;
}

inline bool NearlyOne(float f) { return fabs(f - 1.0f) < 0.0001f; }
inline bool NearlyZero(float f) { return fabs(f) < 0.0001f; }
inline bool NearlyEqual(float f1, float f2) { return fabs(f1 - f2) < 0.0001f; }

float Mod(float, float);

inline float ModRange(float f1, float f2, float f3) { return Mod(f3 - f1, f2 - f1) + f1; }

inline float Interp(float a, float b, float t) { return t * (b - a) + a; }

inline void Interp(float a, float b, float t, float &fres) { fres = t * (b - a) + a; }

inline void Interp(bool a, bool b, float t, bool &bres) { bres = t < 1.0f ? a : b; }

inline float InverseLerp(float min, float max, float value) {
    // Prevent divide-by-zero from zero-sized range
    if (max != min) {
        return (value - min) / (max - min);
    } else {
        return 1.0f;
    }
}

inline bool PowerOf2(int num) {
    if (num < 0)
        return false;
    else if (num == 0)
        return true;
    else
        return (num & num - 1) == 0;
}

inline float Limit(float f1, float f2, float f3, int &i) {
    float fsub = f2 - f1;
    int floored = (int)(float)floor((f3 - f1) / fsub);
    i = floored;
    return -(floored * fsub - f3);
}

inline float Sigmoid(float t) {
    MILO_ASSERT(t >= 0 && t <= 1, 0x1DB);
    float tsq = t * t;
    return Clamp<float>(0, 1, tsq * 3.0f - tsq * 2.0f * t);
}
