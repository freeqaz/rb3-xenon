#pragma once
#include "obj/Data.h"
#include "utl/MemMgr.h"
#include "math/Vec.h"

// Retail RB3 layout (verified against the pinned Interp.cpp .text span
// 0x824E2E10-0x824E3510): the base Interpolator carries mY0/mY1/mX0/mX1 and the
// leaf classes append their own coefficients. ATanInterpolator holds an
// embedded LinearInterpolator (mXMapping) and reads it through the vtable in
// Eval, exactly as rb3-Wii's math/Interp.{h,cpp}. This differs from the newer
// dc3-decomp Interp.h (Vector2 mP0/mP1 + Sync) — dc3 is a later engine revision;
// RB3 retail matches the rb3-Wii form byte-for-byte.

class Interpolator {
public:
    Interpolator() {}
    virtual float Eval(float) = 0;
    virtual void Reset(const DataArray *) = 0;
    virtual ~Interpolator() {}

    float Y0() const { return mY0; }
    float Y1() const { return mY1; }
    float X0() const { return mX0; }
    float X1() const { return mX1; }

    MEM_OVERLOAD(Interpolator, 0x28);

    float mY0, mY1, mX0, mX1; // 0x4 0x8 0xc 0x10
};

class LinearInterpolator : public Interpolator {
public:
    LinearInterpolator(float, float, float, float);
    LinearInterpolator() {}
    virtual float Eval(float f) { return mSlope * f + mB; }
    virtual void Reset(const DataArray *);
    virtual ~LinearInterpolator() {}

    void Reset(float, float, float, float);

    float mSlope, mB; // 0x14 0x18
};

class ExpInterpolator : public Interpolator {
public:
    ExpInterpolator(float, float, float, float, float);
    virtual float Eval(float);
    virtual void Reset(const DataArray *);
    virtual ~ExpInterpolator() {}

    void Reset(float, float, float, float, float);

    float mPower; // 0x14
    float mRise; // 0x18
    float mInvRun; // 0x1c
};

class InvExpInterpolator : public Interpolator {
public:
    InvExpInterpolator(float, float, float, float, float);
    virtual float Eval(float);
    virtual void Reset(const DataArray *);
    virtual ~InvExpInterpolator() {}

    void Reset(float, float, float, float, float);

    float mPower; // 0x14
    float mRise; // 0x18
    float mInvRun; // 0x1c
};

class ATanInterpolator : public Interpolator {
public:
    ATanInterpolator(float, float, float, float, float);
    ATanInterpolator();
    virtual float Eval(float);
    virtual void Reset(const DataArray *);
    virtual ~ATanInterpolator() {}

    void Reset(float, float, float, float, float);

    LinearInterpolator mXMapping; // 0x14
    float mScale; // 0x30
    float mOffset; // 0x34
    float mSeverity; // 0x38
};
