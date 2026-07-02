#pragma once
#include "obj/Data.h"
#include "utl/MemMgr.h"
#include "math/Vec.h"

class Interpolator {
public:
    virtual float Eval(float) const = 0;
    virtual float ClampEval(float f) const { return Eval(f); }
    virtual void Reset(const DataArray *) = 0;
    virtual ~Interpolator();

    MEM_OVERLOAD(Interpolator, 0x28);

protected:
};

class ATanInterpolator : public Interpolator {
public:
    ATanInterpolator();
    ATanInterpolator(const char *, const char *);
    virtual float Eval(float) const;
    virtual void Reset(const DataArray *);

    void Reset(const Vector2 &, const Vector2 &, float);

    float X1() const { return mP1.x; }

protected:
    void Sync();

    Vector2 mP0; // 0x4
    Vector2 mP1; // 0xc
    float mSeverity; // 0x14
    float mSlope; // 0x18
    float mB; // 0x1c
    float mScale; // 0x20
    float mOffset; // 0x24
};

// Additive port from rb3-Wii math/Interp.h — LinearInterpolator/ExpInterpolator/
// InvExpInterpolator are used by Fader::DoFade (synth/Faders.cpp) to build a
// mode-specific curve. The Wii base Interpolator stored mX0/mX1/mY0/mY1 directly
// on the shared base and read them non-virtually; our 360-verified Interpolator
// base (above) carries no data members, so each of these new leaf classes stores
// its own X0/X1/Y0/Y1 pair, exactly like ATanInterpolator does with mP0/mP1.
// FaderTask::Poll resolves the concrete type via Fader::mMode + static_cast
// rather than a new virtual accessor, so the Interpolator/ATanInterpolator
// vtables above are untouched (append-only would still have been a layout
// change per the shared-header contract; this avoids it entirely).
class LinearInterpolator : public Interpolator {
public:
    LinearInterpolator();
    LinearInterpolator(float, float, float, float);
    virtual float Eval(float) const;
    virtual void Reset(const DataArray *);

    void Reset(float, float, float, float);

    float X1() const { return mX1; }
    float Y1() const { return mY1; }

    float mX0, mX1, mY0, mY1;
    float mSlope, mB;
};

class ExpInterpolator : public Interpolator {
public:
    ExpInterpolator(float, float, float, float, float);
    virtual float Eval(float) const;
    virtual void Reset(const DataArray *);

    void Reset(float, float, float, float, float);

    float X1() const { return mX1; }
    float Y1() const { return mY1; }

    float mX0, mX1, mY0, mY1;
    float mPower;
    float mRise;
    float mInvRun;
};

class InvExpInterpolator : public Interpolator {
public:
    InvExpInterpolator(float, float, float, float, float);
    virtual float Eval(float) const;
    virtual void Reset(const DataArray *);

    void Reset(float, float, float, float, float);

    float X1() const { return mX1; }
    float Y1() const { return mY1; }

    float mX0, mX1, mY0, mY1;
    float mPower;
    float mRise;
    float mInvRun;
};
