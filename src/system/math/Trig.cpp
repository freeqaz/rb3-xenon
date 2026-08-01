#include "math/Trig.h"
#include "math/Utl.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include <cmath>

float gBigSinTable[0x200];

// Interleaved table: even slots hold sin(k*i), odd slots hold the forward
// difference to the previous sample (Lookup() reads offset[0]/offset[1] as a
// delta/value pair). The compiler strength-reduces the index form below into a
// single induction pointer, which is why the loop must be written on `i`.
void TrigTableInit() {
    int i = 0;
    do {
        float sineValue = std::sin(0.024543693f * i);
        gBigSinTable[i * 2] = sineValue;
        if (i != 0) {
            gBigSinTable[i * 2 - 1] = sineValue - gBigSinTable[i * 2 - 2];
        }
        i++;
    } while (i < 256);
    // Final delta entry: i == 256, so this fills gBigSinTable[511] from
    // gBigSinTable[510]. In bounds -- the write is NOT past the array end.
    float sineValue = std::sin(0.024543693f * i);
    float *deltaPtr = gBigSinTable + 1;
    deltaPtr[i * 2 - 2] = sineValue - gBigSinTable[i * 2 - 2];
}

void TrigTableTerminate() {}

inline float Lookup(float arg8) {
    float scaledArg = arg8 * 40.743664f;
    int index = (int)scaledArg;
    int idx = (index & 0xFF) * 2;
    float *offset = &gBigSinTable[idx];
    float res = scaledArg - (float)index;
    return (res * offset[1]) + offset[0];
}

float Sine(float arg8) {
    if (arg8 < 0.0f) {
        return -Lookup(-arg8);
    } else
        return Lookup(arg8);
}

float FastSin(float f) {
    if (f < 0.0f) {
        return -gBigSinTable[((int)(-40.743664f * f + 0.49999f) & 0xFF) * 2];
    } else
        return gBigSinTable[((int)(40.743664f * f + 0.49999f) & 0xFF) * 2];
}

DataNode DataSin(DataArray *a) { return (float)sin(DegreesToRadians(a->Float(1))); }
DataNode DataCos(DataArray *da) { return std::cos(DegreesToRadians(da->Float(1))); }
DataNode DataTan(DataArray *da) { return std::tan(DegreesToRadians(da->Float(1))); }

DataNode DataASin(DataArray *da) {
    float f = da->Float(1);
    if (IsNaN(f))
        return 0.0f;
    else
        return RadiansToDegrees(std::asin(f));
}

DataNode DataACos(DataArray *da) {
    float f = da->Float(1);
    if (IsNaN(f))
        return 0.0f;
    else
        return RadiansToDegrees(std::acos(f));
}

DataNode DataATan(DataArray *da) {
    float f = da->Float(1);
    if (IsNaN(f))
        return 0.0f;
    else
        return RadiansToDegrees(std::atan(f));
}

void TrigInit() {
    DataRegisterFunc("sin", DataSin);
    DataRegisterFunc("cos", DataCos);
    DataRegisterFunc("tan", DataTan);
    DataRegisterFunc("asin", DataASin);
    DataRegisterFunc("acos", DataACos);
    DataRegisterFunc("atan", DataATan);
}
