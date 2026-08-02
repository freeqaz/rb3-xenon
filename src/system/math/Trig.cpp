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

#ifdef HX_NATIVE
// ⚠ WITHOUT THIS, EVERY Sine() AND Cosine() IN THE PROCESS RETURNS 0.0.
//
// Sine() is not std::sin -- it is a LOOKUP into gBigSinTable, and that table is
// filled by TrigTableInit(). gBigSinTable has static storage, so before the
// init it is legitimately all zeroes and Sine() answers 0.0 for every input.
// Cosine() is defined as Sine(f + pi/2) (Trig.h:10), so it returns 0.0 too.
// Nothing asserts, nothing logs -- the math is simply, silently, wrong.
//
// On X360 that cannot happen: TrigTableInit() is called from SystemInit()
// (os/System.cpp:522/536) and the game always boots through SystemInit.
// EVERY ONE of this repo's 17 native drivers deliberately hand-rolls a reduced
// bring-up instead (main_render.cpp:207-243 documents why: SystemInit ->
// PreInitSystem stands the RENDERER up, which a headless tool must sequence
// itself), and not one of them called TrigTableInit. So the whole native tree
// ran with sin == cos == 0.
//
// MEASURED, X4b: driving a real CharClip on char/crowd/gen/crowd_female01,
// CharBonesMeshes::PoseMeshes calls MakeRotMatrixZ for the six `.rotz` bones
// (L/R knee, forearm, toe). With c == s == 0 that builds
//     [ 0 0 0 ]
//     [ 0 0 0 ]   det = 0
//     [ 0 0 1 ]
// a SINGULAR matrix, so every child collapsed onto its parent and the compose
// below it blew up (bone-length ratios 0.0000, world determinants reaching
// -3.9e14). Quaternion bones were unaffected -- MakeRotMatrix(Quat) is pure
// multiplies -- which is exactly why this hid until a clip drove a rotz channel.
//
// Fixing it in the driver would fix ONE driver. A file-scope initializer fixes
// the class: gBigSinTable is a POD array, so it is zero-initialised at load
// BEFORE any dynamic initializer runs, and filling it here cannot race with
// another translation unit's static ctor reading it -- such a reader would have
// got 0.0 before this existed, so this is strictly an improvement in every
// ordering. SystemInit's later explicit TrigTableInit() stays and is idempotent
// (it recomputes identical values).
//
// X360 is untouched: the whole block is inside #ifdef HX_NATIVE, and the match
// build passes no /D at all, so its token stream is unchanged.
namespace {
    struct TrigTableAutoInit {
        TrigTableAutoInit() { TrigTableInit(); }
    };
    TrigTableAutoInit sTrigTableAutoInit;
}
#endif

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
