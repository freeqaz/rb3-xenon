#include "math/DoubleExponentialSmoother.h"

DoubleExponentialSmoother::DoubleExponentialSmoother()
    : mLevel(0), mPrevLevel(0), mTrend(0), mAlpha(0), mBeta(0) {}

DoubleExponentialSmoother::DoubleExponentialSmoother(float level, float alpha, float beta)
    : mLevel(level), mPrevLevel(level), mTrend(0), mAlpha(alpha), mBeta(beta) {}

void DoubleExponentialSmoother::Smooth(float value, float delta) {
    float newAlpha = Max(0.0f, mAlpha * delta);
    float newBeta = Max(0.0f, mBeta * delta);
    newAlpha = Min(newAlpha, 1.0f);
    newBeta = Min(newBeta, 1.0f);
    float oldPrev = mPrevLevel;
    mPrevLevel = newAlpha * (value - mLevel) + mLevel;
    mTrend = ((mPrevLevel - oldPrev) - mTrend) * newBeta + mTrend;
    mLevel = mTrend + mPrevLevel;
}

void Vector2DESmoother::SetSmoothParameters(float alpha, float beta) {
    mX.SetCoeffs(alpha, beta);
    mY.SetCoeffs(alpha, beta);
}

void Vector2DESmoother::ForceValue(Vector2 v) {
    mX.SetParams(v.x, v.x, 0);
    mY.SetParams(v.y, v.y, 0);
}

Vector2 Vector2DESmoother::Value() const { return Vector2(mX.mLevel, mY.mLevel); }

Vector3DESmoother::Vector3DESmoother(Vector3 v, float alpha, float beta)
    : mX(v.x, alpha, beta), mY(v.y, alpha, beta), mZ(v.z, alpha, beta) {}

void Vector3DESmoother::SetSmoothParameters(float alpha, float beta) {
    mX.SetCoeffs(alpha, beta);
    mY.SetCoeffs(alpha, beta);
    mZ.SetCoeffs(alpha, beta);
}

Vector3 Vector3DESmoother::Value() const {
    return Vector3(mX.mLevel, mY.mLevel, mZ.mLevel);
}

void Vector3DESmoother::ForceValue(Vector3 v) {
    mX.SetParams(v.x, v.x, 0);
    mY.SetParams(v.y, v.y, 0);
    mZ.SetParams(v.z, v.z, 0);
}

void Vector2DESmoother::Smooth(Vector2 v, float dt, bool normalize) {
    mX.Smooth(v.x, dt);
    mY.Smooth(v.y, dt);
    if (normalize) {
        float x = mX.mLevel;
        float y = mY.mLevel;
        float len = std::sqrt(x * x + y * y);
        if (len != 0.0f) {
            float inv = 1.0f / len;
            mX.mLevel = x * inv;
            mY.mLevel = y * inv;
        }
        mX.mTrend = 0;
        mY.mTrend = 0;
    }
}

void Vector3DESmoother::Smooth(Vector3 v, float dt, bool normalize) {
    mX.Smooth(v.x, dt);
    mY.Smooth(v.y, dt);
    mZ.Smooth(v.z, dt);
    if (normalize) {
        Vector3 val(mX.mLevel, mY.mLevel, mZ.mLevel);
        Normalize(val, val);
        mX.mLevel = val.x;
        mY.mLevel = val.y;
        mZ.mLevel = val.z;
        mX.mTrend = 0;
        mY.mTrend = 0;
        mZ.mTrend = 0;
    }
}
