#include "math/Interp.h"
#include "math/Vec.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include <cmath>

Interpolator::~Interpolator() {}

void ATanInterpolator::Sync() {
    float run = mP1.x - mP0.x;
    float slope;
    if (std::fabs(run) < 0.000001f) {
        slope = 0;
    } else {
        slope = mSeverity / run * 2.0f;
    }
    mSlope = slope;
    mB = -(mP0.x * mSlope) - mSeverity;
    MILO_ASSERT_FMT(
        mSeverity > 0.001f, "ATanInterpolator: severity (%f) too small.", mSeverity
    );
    float tanned = atan(-mSeverity);
    float rise = mP1.y - mP0.y;
    mOffset = rise * 0.5f + mP0.y;
    mScale = rise / (-tanned - tanned);
}

float ATanInterpolator::Eval(float f1) const {
    float tanned = atan(mSlope * f1 + mB);
    return mScale * tanned + mOffset;
}

ATanInterpolator::ATanInterpolator() : mP0(0, 0), mP1(1, 1) {
    mSeverity = 2.0f;
    Sync();
}

ATanInterpolator::ATanInterpolator(const char *, const char *) : mP0(0, 0), mP1(1, 1) {
    mSeverity = 2.0;
    Sync();
}

void ATanInterpolator::Reset(const Vector2 &y, const Vector2 &x, float sev) {
    mP0 = y;
    mP1 = x;
    mSeverity = sev;
    Sync();
}

void ATanInterpolator::Reset(const DataArray *a) {
    float sev = a->Size() > 5 ? a->Float(5) : 10.0f;
    float f2 = a->Float(2);
    float f4 = a->Float(4);
    Vector2 vecX(f4, f2);
    float f1 = a->Float(1);
    float f3 = a->Float(3);
    Vector2 vecY(f3, f1);
    mSeverity = sev;
    mP0 = vecY;
    mP1 = vecX;
    Sync();
}

// Additive port from rb3-Wii math/Interp.cpp (see Interp.h for why these are
// new leaf classes rather than extensions to Interpolator/ATanInterpolator).

LinearInterpolator::LinearInterpolator() {}

LinearInterpolator::LinearInterpolator(float y0, float y1, float x0, float x1) {
    Reset(y0, y1, x0, x1);
}

void LinearInterpolator::Reset(float y0, float y1, float x0, float x1) {
    float run = x1 - x0;
    mX0 = x0;
    mX1 = x1;
    mY0 = y0;
    mY1 = y1;
    if (std::fabs(run) < 0.000001f)
        mSlope = 0.0f;
    else
        mSlope = (y1 - y0) / run;
    mB = -mX0 * mSlope + mY0;
}

void LinearInterpolator::Reset(const DataArray *data) {
    Reset(data->Float(1), data->Float(2), data->Float(3), data->Float(4));
}

float LinearInterpolator::Eval(float f) const { return mSlope * f + mB; }

ExpInterpolator::ExpInterpolator(float f1, float f2, float f3, float f4, float f5) {
    Reset(f1, f2, f3, f4, f5);
}

void ExpInterpolator::Reset(float f1, float f2, float f3, float f4, float f5) {
    float run = f4 - f3;
    mX0 = f3;
    mX1 = f4;
    mY0 = f1;
    mY1 = f2;
    if (std::fabs(run) < 0.000001f)
        mInvRun = 1.0f;
    else
        mInvRun = 1.0f / run;
    mPower = f5;
    mRise = f2 - f1;
}

void ExpInterpolator::Reset(const DataArray *data) {
    Reset(
        data->Float(1),
        data->Float(2),
        data->Float(3),
        data->Float(4),
        (data->Size() > 5) ? data->Float(5) : 2.0f
    );
}

float ExpInterpolator::Eval(float f) const {
    double pow_res = pow((double)(mInvRun * (f - mX0)), (double)mPower);
    return (float)pow_res * mRise + mY0;
}

InvExpInterpolator::InvExpInterpolator(float f1, float f2, float f3, float f4, float f5) {
    Reset(f1, f2, f3, f4, f5);
}

void InvExpInterpolator::Reset(float f1, float f2, float f3, float f4, float f5) {
    float run = f4 - f3;
    mX0 = f3;
    mX1 = f4;
    mY0 = f1;
    mY1 = f2;
    if (std::fabs(run) < 0.000001f)
        mInvRun = 1.0f;
    else
        mInvRun = 1.0f / run;
    mPower = f5;
    mRise = f2 - f1;
}

void InvExpInterpolator::Reset(const DataArray *data) {
    Reset(
        data->Float(1),
        data->Float(2),
        data->Float(3),
        data->Float(4),
        (data->Size() > 5) ? data->Float(5) : 2.0f
    );
}

float InvExpInterpolator::Eval(float f) const {
    double a = -((double)(mInvRun * (f - mX0)) - 1.0);
    double pow_res = std::pow(a, (double)mPower);
    return (float)((1.0 - pow_res) * mRise + mY0);
}
