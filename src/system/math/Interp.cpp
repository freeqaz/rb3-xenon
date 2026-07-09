#include "math/Interp.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include <cmath>

// LinearInterpolator ---------------------------------------------------------

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

// ExpInterpolator ------------------------------------------------------------

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
    mRise = f2 - f1;
    mPower = f5;
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

float ExpInterpolator::Eval(float f) {
    double pow_res = pow(mInvRun * (f - mX0), mPower);
    return (float)pow_res * mRise + mY0;
}

// InvExpInterpolator ---------------------------------------------------------

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
    mRise = f2 - f1;
    mPower = f5;
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

float InvExpInterpolator::Eval(float f) {
    float pow_res;
    double a = -(mInvRun * (f - mX0) - 1);
    pow_res = std::pow(a, (double)mPower);
    return (1.0f - pow_res) * mRise + mY0;
}

// ATanInterpolator -----------------------------------------------------------

ATanInterpolator::ATanInterpolator(float y0, float y1, float x0, float x1, float severity)
    : mXMapping(0.0f, 0.0f, 0.0f, 0.0f) {
    Reset(y0, y1, x0, x1, severity);
}

ATanInterpolator::ATanInterpolator() {}

void ATanInterpolator::Reset(float y0, float y1, float x0, float x1, float severity) {
    float negSev = -severity;

    mXMapping.Reset(negSev, severity, x0, x1);
    mX0 = x0;
    mX1 = x1;
    mY0 = y0;
    mY1 = y1;

    float ftan = std::atan(negSev);

    float fneg = y1 - y0;
    float fsub = -ftan - ftan;

    mScale = fneg / fsub;
    mOffset = (y1 - y0) * 0.5f + y0;
    mSeverity = severity;
}

void ATanInterpolator::Reset(const DataArray *data) {
    Reset(
        data->Float(1),
        data->Float(2),
        data->Float(3),
        data->Float(4),
        (data->Size() > 5) ? data->Float(5) : 10.0f
    );
}

float ATanInterpolator::Eval(float f) {
    float ret = std::atan(mXMapping.Eval(f));
    ret *= mScale;
    return ret + mOffset;
}
