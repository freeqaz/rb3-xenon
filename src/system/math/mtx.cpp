#include "math/Mtx.h"

Hmx::Matrix2 Hmx::Matrix2::sID(Vector2(1, 0), Vector2(0, 1));
Hmx::Matrix3 Hmx::Matrix3::sID(Vector3(1, 0, 0), Vector3(0, 1, 0), Vector3(0, 0, 1));
Hmx::Matrix4 Hmx::Matrix4::sID(
    Vector4(1, 0, 0, 0), Vector4(0, 1, 0, 0), Vector4(0, 0, 1, 0), Vector4(0, 0, 0, 1)
);

Transform Transform::sID(Hmx::Matrix3::GetIdentity(), Vector3(0, 0, 0));

Hmx::Matrix4::Matrix4(const Transform &tf) {
    x.x = tf.m.x.x;
    x.y = tf.m.x.y;
    x.z = tf.m.x.z;
    x.w = 0.0f;
    y.x = tf.m.y.x;
    y.y = tf.m.y.y;
    y.z = tf.m.y.z;
    y.w = 0.0f;
    z.x = tf.m.z.x;
    z.y = tf.m.z.y;
    z.z = tf.m.z.z;
    z.w = 0.0f;
    w.x = tf.v.x;
    w.y = tf.v.y;
    w.z = tf.v.z;
    w.w = 1.0f;
}

// Computes determinant of 3x3 matrix and returns its reciprocal (1/det)
// Used for matrix inversion - returns 0 if matrix is singular
float Det(const Hmx::Matrix3 &m) {
    float det = (m.y.y * m.z.z - m.y.z * m.z.y) * m.x.x
                - (m.y.x * m.z.z - m.y.z * m.z.x) * m.x.y
                + (m.y.x * m.z.y - m.y.y * m.z.x) * m.x.z;
    if (det == 0) {
        return det;
    }
    return 1.0f / det;
}

void Invert(const Hmx::Matrix3 &min, Hmx::Matrix3 &mout) {
    float det = (min.y.y * min.z.z - min.y.z * min.z.y) * min.x.x
                - (min.y.x * min.z.z - min.z.x * min.y.z) * min.x.y
                + (min.y.x * min.z.y - min.z.x * min.y.y) * min.x.z;
    float mult = 0.0f;
    if (det != 0.0f) {
        mult = 1.0f / det;
    }
    mout.Set(
        (min.z.z * min.y.y - min.y.z * min.z.y) * mult,
        -((min.z.z * min.x.y - min.x.z * min.z.y) * mult),
        (min.y.z * min.x.y - min.x.z * min.y.y) * mult,
        -((min.z.z * min.y.x - min.y.z * min.z.x) * mult),
        (min.z.z * min.x.x - min.x.z * min.z.x) * mult,
        -((min.y.z * min.x.x - min.x.z * min.y.x) * mult),
        (min.z.y * min.y.x - min.z.x * min.y.y) * mult,
        -((min.z.y * min.x.x - min.z.x * min.x.y) * mult),
        (min.y.y * min.x.x - min.x.y * min.y.x) * mult
    );
}

void Multiply(const Hmx::Matrix3 &a, const Hmx::Matrix3 &b, Hmx::Matrix3 &out) {
    out.Set(
        a.x.x * b.x.x + a.x.y * b.y.x + a.x.z * b.z.x,
        a.x.x * b.x.y + a.x.y * b.y.y + a.x.z * b.z.y,
        a.x.x * b.x.z + a.x.y * b.y.z + a.x.z * b.z.z,
        a.y.x * b.x.x + a.y.y * b.y.x + a.y.z * b.z.x,
        a.y.x * b.x.y + a.y.y * b.y.y + a.y.z * b.z.y,
        a.y.x * b.x.z + a.y.y * b.y.z + a.y.z * b.z.z,
        a.z.x * b.x.x + a.z.y * b.y.x + a.z.z * b.z.x,
        a.z.x * b.x.y + a.z.y * b.y.y + a.z.z * b.z.y,
        a.z.x * b.x.z + a.z.y * b.y.z + a.z.z * b.z.z
    );
}

void Multiply(const Transform &a, const Transform &b, Transform &out) {
#ifdef HX_NATIVE
    // Native fix: the PPC decomp below has decompiler mis-mappings in the translation
    // computation (b.m.y.y/b.m.z.y swapped, b.m.y.z/b.m.z.y swapped, b.v.y/b.v.z swapped).
    // These produce identical PPC assembly but wrong x86 results for non-trivial rotations.
    // Use the mathematically correct formula: out.v = a.v * b.m + b.v
    Multiply(a.m, b.m, out.m);
    out.v.x = a.v.x * b.m.x.x + a.v.y * b.m.y.x + a.v.z * b.m.z.x + b.v.x;
    out.v.y = a.v.x * b.m.x.y + a.v.y * b.m.y.y + a.v.z * b.m.z.y + b.v.y;
    out.v.z = a.v.x * b.m.x.z + a.v.y * b.m.y.z + a.v.z * b.m.z.z + b.v.z;
#else
    float fVar1 = a.v.y;
    float fVar2 = a.v.x;
    float fVar3 = a.v.z;
    float fVar4 = b.m.z.y;

    float fVar10 = b.m.x.y * fVar2 + b.m.y.y * fVar1;

    float fVar5;
    if (&b != &out) {
        float bzx = b.m.z.x;
        float byx = b.m.y.x;
        float bxx = b.m.x.x;
        float fVar8 = b.m.z.z * fVar3 + b.m.x.z * fVar2 + b.m.y.z * fVar1;
        out.v.z = fVar8;
        out.v.y = fVar4 * fVar3 + fVar10;
        out.v.x = fVar2 * bxx + byx * fVar1 + bzx * fVar3;
        fVar5 = b.v.z;
        float bvx = b.v.x;
        out.v.y += b.v.y;
        out.v.x += bvx;
        fVar5 = fVar5 + fVar8;
    } else {
        float fVar6 = b.m.y.z;
        float fVar7 = b.m.x.z;
        float fVar8 = b.m.z.z;
        float bvz = b.v.z;
        float fVar9 = b.v.y;

        out.v.x = b.v.x + fVar2 * b.m.x.x + b.m.y.x * fVar1 + b.m.z.x * fVar3;
        out.v.y = fVar9 + fVar4 * fVar3 + fVar10;
        fVar5 = bvz + fVar8 * fVar3 + fVar7 * fVar2 + fVar6 * fVar1;
    }
    out.v.z = fVar5;
    Multiply(a.m, b.m, out.m);
#endif
}


void FastInvert(const Hmx::Matrix3 &min, Hmx::Matrix3 &mout) {
    float xdot = Dot(min.x, min.x);
    if (xdot != 0)
        xdot = 1.0f / xdot;
    float ydot = Dot(min.y, min.y);
    if (ydot != 0)
        ydot = 1.0f / ydot;
    float zdot = Dot(min.z, min.z);
    if (zdot != 0)
        zdot = 1.0f / zdot;
    mout.Set(
        min.x.x * xdot,
        min.y.x * ydot,
        min.z.x * zdot,
        min.x.y * xdot,
        min.y.y * ydot,
        min.z.y * zdot,
        min.x.z * xdot,
        min.y.z * ydot,
        min.z.z * zdot
    );
}

QuatXfm::QuatXfm(const Transform &tf) : v(tf.v), q(tf.m) {}

void Transform::LookAt(const Vector3 &target, const Vector3 &up) {
    m.z.Set(target.x - v.x, target.y - v.y, target.z - v.z);
    m.y = up;
    Normalize(m, m);
}

// Matrix4 operator*(Matrix4, Matrix4) is inline in Mtx.h

float Det(const Hmx::Matrix4 &m) {
    float a11 = m.y.y, a12 = m.y.z, a13 = m.y.w;
    float a21 = m.z.y, a22 = m.z.z, a23 = m.z.w;
    float a31 = m.w.y, a32 = m.w.z, a33 = m.w.w;

    // Cofactor expansion along row 0, reusing a single Matrix3 for each minor
    Hmx::Matrix3 minor(a11, a12, a13, a21, a22, a23, a31, a32, a33);
    float det = Det(minor) * m.x.x;

    float a10 = m.y.x, a20 = m.z.x, a30 = m.w.x;
    minor.Set(a10, a12, a13, a20, a22, a23, a30, a32, a33);
    det = -(Det(minor) * m.x.y - det);

    minor.Set(a10, a11, a13, a20, a21, a23, a30, a31, a33);
    det = Det(minor) * m.x.z + det;

    minor.Set(a10, a11, a12, a20, a21, a22, a30, a31, a32);
    det = -(Det(minor) * m.x.w - det);

    return det;
}

void Invert(const Hmx::Matrix4 &m, Hmx::Matrix4 &out) {
    float det = Det(m);
    bool small = std::fabs(det) < 0.0001f;
    float invDet;
    if (!small) {
        invDet = 1.0f / det;
    } else {
        invDet = 0.0f;
    }

    float a00 = m.x.x, a01 = m.x.y, a02 = m.x.z, a03 = m.x.w;
    float a10 = m.y.x, a11 = m.y.y, a12 = m.y.z, a13 = m.y.w;
    float a20 = m.z.x, a21 = m.z.y, a22 = m.z.z, a23 = m.z.w;
    float a30 = m.w.x, a31 = m.w.y, a32 = m.w.z, a33 = m.w.w;

    // Pre-computed shared sub-expressions
    float wx_zy = a30 * a21;
    float wy_zx = a31 * a20;
    float zx_wz = a20 * a32;
    float zx_ww = a20 * a33;

    // Cofactors for columns 1,2,3 stored directly to output rows y,z,w
    out.y.Set(
        -(a22 * a33 * a10 - (zx_ww * a12 + a23 * a32 * a10 + -(zx_wz * a13 - (a22 * a30 * a13 - a23 * a30 * a12)))) * invDet,
         (a22 * a33 * a00 + -(zx_ww * a02 - -(a23 * a32 * a00 - (zx_wz * a03 + (a23 * a30 * a02 - a22 * a30 * a03))))) * invDet,
        -(a00 * a33 * a12 - (a00 * a32 * a13 + a33 * a10 * a02 + -(a10 * a32 * a03 - (a03 * a12 * a30 - a02 * a13 * a30)))) * invDet,
         (a23 * a00 * a12 + -(a23 * a10 * a02 - -(a13 * a22 * a00 - (a03 * a22 * a10 + (a13 * a20 * a02 - a03 * a12 * a20))))) * invDet
    );

    out.z.Set(
         (a10 * a33 * a21 + -(a11 * a33 * a20 - -(a10 * a23 * a31 - (a13 * wy_zx + (a11 * a23 * a30 - a13 * wx_zy))))) * invDet,
        -(a00 * a33 * a21 - (a01 * a33 * a20 + a00 * a23 * a31 + -(a03 * wy_zx - (a03 * wx_zy - a01 * a23 * a30)))) * invDet,
         (a33 * a00 * a11 + -(a33 * a10 * a01 - -(a13 * a31 * a00 - (a03 * a31 * a10 + (a13 * a30 * a01 - a03 * a30 * a11))))) * invDet,
        -(a23 * a00 * a11 - (a23 * a10 * a01 + a13 * a00 * a21 + -(a03 * a10 * a21 - (a03 * a11 * a20 - a13 * a20 * a01)))) * invDet
    );

    out.w.Set(
        -(a32 * a21 * a10 - (a32 * a20 * a11 + a31 * a22 * a10 + -(wy_zx * a12 - (wx_zy * a12 - a30 * a22 * a11)))) * invDet,
         (a32 * a21 * a00 + -(a32 * a20 * a01 - -(a31 * a22 * a00 - (wy_zx * a02 + (a30 * a22 * a01 - wx_zy * a02))))) * invDet,
        -(a32 * a00 * a11 - (a31 * a00 * a12 + a32 * a10 * a01 + -(a31 * a10 * a02 - (a30 * a11 * a02 - a30 * a01 * a12)))) * invDet,
         (a22 * a00 * a11 + -(a22 * a10 * a01 - -(a00 * a21 * a12 - (a10 * a21 * a02 + (a01 * a12 * a20 - a02 * a11 * a20))))) * invDet
    );

    // Cofactors for column 0 via operator[] (cols 1,2,3) -> out.x (transposed)
    const Vector4 &row0 = m.x;
    const Vector4 &row1 = m.y;
    const Vector4 &row2 = m.z;
    const Vector4 &row3 = m.w;
#pragma inline_depth(0)

    // c30: minor removing row 3, col 0 -> rows 0,1,2 cols 1,2,3 (sign: -)
    float acc = a21 * a12 * a03;
    acc = -(row2[1] * row1[3] * row0[2] - acc);
    acc = -(row2[2] * row1[1] * row0[3] - acc);
    acc = row2[3] * row1[1] * row0[2] + acc;
    acc = row2[2] * row1[3] * row0[1] + acc;
    acc = -(row2[3] * row1[2] * row0[1] - acc);
    float c30 = acc * invDet;

    // c20: minor removing row 2, col 0 -> rows 0,1,3 cols 1,2,3 (sign: +)
    acc = row3[1] * row1[3] * row0[2];
    acc = -(row3[1] * row1[2] * row0[3] - acc);
    acc = row3[2] * row1[1] * row0[3] + acc;
    acc = -(row3[2] * row1[3] * row0[1] - acc);
    acc = -(row3[3] * row1[1] * row0[2] - acc);
    acc = row3[3] * row1[2] * row0[1] + acc;
    float c20 = acc * invDet;

    // c10: minor removing row 1, col 0 -> rows 0,2,3 cols 1,2,3 (sign: -)
    acc = row3[1] * row2[2] * row0[3];
    acc = -(row3[1] * row2[3] * row0[2] - acc);
    acc = -(row3[2] * row2[1] * row0[3] - acc);
    acc = row3[2] * row2[3] * row0[1] + acc;
    acc = row3[3] * row2[1] * row0[2] + acc;
    acc = -(row3[3] * row2[2] * row0[1] - acc);
    float c10 = acc * invDet;

    // c00: minor removing row 0, col 0 -> rows 1,2,3 cols 1,2,3 (sign: +)
    acc = row3[1] * row2[3] * row1[2];
    acc = -(row3[1] * row2[2] * row1[3] - acc);
    acc = row3[2] * row2[1] * row1[3] + acc;
    acc = -(row3[2] * row2[3] * row1[1] - acc);
    acc = -(row3[3] * row2[1] * row1[2] - acc);
    acc = row3[3] * row2[2] * row1[1] + acc;
    float c00 = acc * invDet;
#pragma inline_depth()

    out.x.Set(c00, c10, c20, c30);
}

// Transpose(Matrix4) moved to Mtx.h as inline
