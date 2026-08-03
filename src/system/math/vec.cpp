#include "Vec.h"
#include "Mtx.h"

Vector3 Vector3::sX(1, 0, 0);
Vector3 Vector3::sY(0, 1, 0);
Vector3 Vector3::sZ(0, 0, 1);
Vector4 Vector4::sX(1, 0, 0, 0);
Vector4 Vector4::sY(0, 1, 0, 0);
Vector4 Vector4::sZ(0, 0, 1, 0);
Vector4 Vector4::sW(0, 0, 0, 1);

Vector3 Vector3::sZero(0, 0, 0);
Vector4 Vector4::sZero(0, 0, 0, 0);

// NOTE: ScaleAddEq(Hmx::Matrix3&, const Hmx::Matrix3&, float) is deliberately
// NOT defined in this TU -- see mtx.cpp. Retail keeps tf1/tf2/f live in the
// non-volatile r31/r30/f31 across the call below, which MSVC only does when the
// callee's register footprint is unknown. Defining the Matrix3 overload here
// lets MSVC's whole-TU callee register-usage propagation keep the arguments in
// the volatile r3/r4/f1 instead, which does not match retail.
// The vector half must use the two-reference ScaleAddEq(Vector3&, const
// Vector3&, float), NOT ScaleAdd(tf1.v, tf2.v, f, tf1.v) -- the two are
// semantically identical but NOT schedule-identical (lane DS-4/C, 93.3% -> 100%,
// raw 100%).
//
// ScaleAdd takes a fourth `Vector3 &vres` out-param that ALIASES v1 here, so
// MSVC must assume the store to vres.y may alias the loads of v1.z/v2.z and
// cannot hoist them across it. Retail's stream hoists `lfs a.z` ABOVE the y
// store -- i.e. retail's callee has strictly fewer aliasing constraints, which
// is exactly what dropping the redundant third reference buys. That hoist is
// the whole residual; the instruction multiset was already identical.
//
// ⚠ Do NOT diagnose this from the per-component load ORDER, which is what the
// REGISTER_SWAP label points at. The load order is a scheduler artifact, not a
// source-operand-order artifact: the OLD spelling expanded to three IDENTICAL
// source lines (`vres.C = v2.C * f + v1.C`) and still emitted (a,b) for x and y
// but (b,a) for z. Rewriting per-component operand order therefore cannot steer
// it -- the aliasing freedom is the real lever.
void ScaleAddEq(Transform &tf1, const Transform &tf2, float f) {
    ScaleAddEq(tf1.m, tf2.m, f);
    ScaleAddEq(tf1.v, tf2.v, f);
}
