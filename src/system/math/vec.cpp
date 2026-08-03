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
void ScaleAddEq(Transform &tf1, const Transform &tf2, float f) {
    ScaleAddEq(tf1.m, tf2.m, f);
    ScaleAdd(tf1.v, tf2.v, f, tf1.v);
}
