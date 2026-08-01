#pragma once
#include "math/Color.h"
#include "math/Mtx.h"
#include "utl/BinStream.h"

class RndColorXfm {
public:
    RndColorXfm();
    void Reset();
    void AdjustHue();
    void AdjustSaturation();
    void AdjustLightness();
    void AdjustContrast();
    void AdjustBrightness();
    void AdjustLevels();
    void AdjustColorXfm();
    void Save(BinStream &) const;
    bool Load(BinStream &);

    RndColorXfm &operator=(const RndColorXfm &c);

    float mHue; // 0x0
    float mSaturation; // 0x4
    float mLightness; // 0x8
    float mContrast; // 0xc
    float mBrightness; // 0x10
    Hmx::Color mLevelInLo; // 0x14
    Hmx::Color mLevelInHi; // 0x24
    Hmx::Color mLevelOutLo; // 0x34
    Hmx::Color mLevelOutHi; // 0x44
    Transform mColorXfm; // 0x54
};

// RB3-360 retail copies an RndColorXfm with a COMPILER-GENERATED block copy:
// the `bl memcpy` is preceded by the SOURCE address (r4) and only then the
// DESTINATION (r3). An explicit `memcpy(this, &c, sizeof(*this))` written in
// source produces the opposite, dst-first, ordering. That 2-instruction swap
// was the entire residual diff in RndPostProc::Copy, and the same expansion
// also completed a function in rndobj/Env (lane NCCC-0731-5f08/f7).
//
// Note: simply DELETING this operator does not work. Transform and Hmx::Matrix3
// carry their own user-defined operator=, so RndColorXfm is not bitwise
// copy-assignable and MSVC emits an out-of-line ??4RndColorXfm@@QAAAAV0@ABV0@@Z
// call instead of the inline 0x94 memcpy retail has. Expressing the body as a
// POD struct assignment is what reproduces retail's own block-copy expansion.
inline RndColorXfm &RndColorXfm::operator=(const RndColorXfm &c) {
    struct Blk {
        char b[sizeof(RndColorXfm)];
    };
    *(Blk *)this = *(const Blk *)&c;
    return *this;
}

// inline BinStream &operator>>(BinStream &bs, RndColorXfm &xfm) {
//     xfm.Load(bs);
//     return bs;
// }
