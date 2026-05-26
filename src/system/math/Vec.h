#pragma once
#include "os/Debug.h"
#include "utl/BinStream.h"
#include "utl/TextStream.h"
#include "math/Utl.h"
#include <cmath>

class Vector2 {
public:
    Vector2() {}
    Vector2(float xx, float yy) : x(xx), y(yy) {}

    void Set(float xx, float yy) {
        x = xx;
        y = yy;
    }

    Vector2 &operator*=(float f) {
        x *= f;
        y *= f;
        return *this;
    }

    void Zero() { x = y = 0; }

    Vector2 &operator/=(float f) {
        x /= f;
        y /= f;
        return *this;
    }

    Vector2 &operator+=(const Vector2 &v) {
        x += v.x;
        y += v.y;
        return *this;
    }

    bool operator==(const Vector2 &v) const { return x == v.x && y == v.y; }

    bool operator!() const { return x == 0.0f && y == 0.0f; }

    float x;
    float y;
};

inline BinStream &operator<<(BinStream &bs, const Vector2 &vec) {
    bs << vec.x << vec.y;
    return bs;
}

inline BinStream &operator>>(BinStream &bs, Vector2 &vec) {
    bs >> vec.x >> vec.y;
    return bs;
}

class Vector3 {
protected:
    static Vector3 sX;
    static Vector3 sY;
    static Vector3 sZ;
    static Vector3 sZero;

public:
    float x;
    float y;
    float z;

    Vector3() {}
    Vector3(float f1, float f2, float f3) : x(f1), y(f2), z(f3) {}
    // used during decompression of CharBones vectors
    Vector3(short *s) {
        x = s[0] * 0.000030518509f * 1300.0f;
        y = s[1] * 0.000030518509f * 1300.0f;
        z = s[2] * 0.000030518509f * 1300.0f;
    }

    void Set(float f1, float f2, float f3) {
        x = f1;
        y = f2;
        z = f3;
    }
    void Zero() { x = y = z = 0; }

    Vector3 &operator+=(const Vector3 &v) {
        x += v.x;
        y += v.y;
        z += v.z;
        return *this;
    }

    Vector3 &operator-=(const Vector3 &v) {
        x -= v.x;
        y -= v.y;
        z -= v.z;
        return *this;
    }

    Vector3 &operator*=(float f) {
        x *= f;
        y *= f;
        z *= f;
        return *this;
    }

    Vector3 &operator*=(const Vector3 &v) {
        x *= v.x;
        y *= v.y;
        z *= v.z;
        return *this;
    }

    Vector3 &operator/=(float f) {
        x /= f;
        y /= f;
        z /= f;
        return *this;
    }

    const float &operator[](int i) const {
        MILO_ASSERT_RANGE(i, 0, 3, 0x122);
        return *(&x + i);
    }

    float &operator[](int i) {
        MILO_ASSERT_RANGE(i, 0, 3, 0x127);
        return *(&x + i);
    }

    //   public: static const Vector3& GetXAxis();
    //   public: static const Vector3& GetYAxis();
    //   public: static const Vector3& GetZAxis();
    static const Vector3 &GetZero() { return sZero; }
    static const Vector3 &ZeroVec() { return sZero; }

    bool operator==(const Vector3 &v) const { return x == v.x && y == v.y && z == v.z; }
    bool IsZero() const { return x == 0 && y == 0 && z == 0; }
    bool operator!=(const Vector3 &v) const { return x != v.x || y != v.y || z != v.z; }

private:
    u32 PAD; // should NEVER be used!!!! for simd alignment!!!
};

class ShortVector3 {
public:
    ShortVector3() {}
    ShortVector3(short *) {}

    void Set(const Vector3 &);
    static short ToShort(float);

    short x;
    short y;
    short z;
};

inline BinStream &operator<<(BinStream &bs, const Vector3 &vec) {
    bs << vec.x << vec.y << vec.z;
    return bs;
}

inline BinStream &operator>>(BinStream &bs, Vector3 &vec) {
    bs >> vec.x >> vec.y >> vec.z;
    return bs;
}

TextStream &operator<<(TextStream &, const Vector3 &);

// 16-byte padded Vector3 for structs that need XMVECTOR-compatible stride
struct PaddedJointPos {
    float x, y, z, _pad;
    PaddedJointPos() {}
    PaddedJointPos(const Vector3 &v) : x(v.x), y(v.y), z(v.z), _pad(0) {}
    operator Vector3 &() { return *(Vector3 *)&x; }
    operator const Vector3 &() const { return *(const Vector3 *)&x; }
    PaddedJointPos &operator=(const Vector3 &v) {
        x = v.x;
        y = v.y;
        z = v.z;
        return *this;
    }
    void Zero() { x = y = z = 0; }
    void Set(float a, float b, float c) {
        x = a;
        y = b;
        z = c;
    }
    float &operator[](int i) { return ((Vector3 &)*this)[i]; }
    float operator[](int i) const { return ((const Vector3 &)*this)[i]; }
};

class Vector4 {
protected:
    static Vector4 sX;
    static Vector4 sY;
    static Vector4 sZ;
    static Vector4 sW;
    static Vector4 sZero;

public:
    float x;
    float y;
    float z;
    float w;

    Vector4() {}
    Vector4(float f1, float f2, float f3, float f4) : x(f1), y(f2), z(f3), w(f4) {}
    void Set(float f1, float f2, float f3, float f4) {
        x = f1;
        y = f2;
        z = f3;
        w = f4;
    }

    static const Vector4 &GetZero() { return sZero; }
    static const Vector4 &ZeroVec() { return sZero; }

    const float &operator[](int i) const {
        MILO_ASSERT_RANGE(i, 0, 4, 0x1AC);
        return *(&x + i);
    }
};

inline BinStream &operator<<(BinStream &bs, const Vector4 &vec) {
    bs << vec.x << vec.y << vec.z << vec.w;
    return bs;
}
inline BinStream &operator>>(BinStream &bs, Vector4 &vec) {
    bs >> vec.x >> vec.y >> vec.z >> vec.w;
    return bs;
}

inline bool NearlyEqual(const Vector3 &v1, const Vector3 &v2, float max_diff) {
    return std::fabs(v1.x - v2.x) < max_diff && std::fabs(v1.y - v2.y) < max_diff
        && std::fabs(v1.z - v2.z) < max_diff;
}

inline void Scale(const Vector3 &v1, float f, Vector3 &dst) {
    dst.Set(v1.x * f, v1.y * f, v1.z * f);
}

inline void Scale(const Vector3 &v1, const Vector3 &v2, Vector3 &dst) {
    dst.Set(v1.x * v2.x, v1.y * v2.y, v1.z * v2.z);
}

inline void Add(const Vector3 &v1, const Vector3 &v2, Vector3 &dst) {
    dst.Set(v1.x + v2.x, v1.y + v2.y, v1.z + v2.z);
}

inline void Subtract(const Vector3 &v1, const Vector3 &v2, Vector3 &dst) {
    dst.Set(v1.x - v2.x, v1.y - v2.y, v1.z - v2.z);
}

inline float LengthSquared(const Vector3 &v) { return v.x * v.x + v.y * v.y + v.z * v.z; }

inline float Length(const Vector3 &v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

inline float Average(const Vector2 &v) { return (v.x + v.y) / 2; }

inline float Dot(const Vector3 &v1, const Vector3 &v2) {
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

inline float Cross(const Vector2 &v1, const Vector2 &v2) {
    return v1.x * v2.y - v1.y * v2.x;
}

inline void Cross(const Vector3 &v1, const Vector3 &v2, Vector3 &dst) {
    dst.Set(
        v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x
    );
}

inline void Normalize(const Vector3 &in, Vector3 &out) {
    float inv = 0;
    float len = Length(in);
    if (len != 0) {
        inv = 1.0f / len;
    }
    Scale(in, inv, out);
}

inline void NormalizeScale(const Vector3 &in, float scalar, Vector3 &out) {
    float inv = 0;
    float len = Length(in);
    if (len != 0) {
        inv = 1.0f / len;
    }
    Scale(in, inv * scalar, out);
}

inline void Negate(const Vector3 &v, Vector3 &vres) { vres.Set(-v.x, -v.y, -v.z); }

inline void Interp(const Vector2 &v1, const Vector2 &v2, float f, Vector2 &res) {
    res.Set(Interp(v1.x, v2.x, f), Interp(v1.y, v2.y, f));
}

inline void Interp(const Vector3 &v1, const Vector3 &v2, float f, Vector3 &dst) {
    if (f == 0.0f) {
        // Copy all fields including PAD via integer copy to avoid FP operations
        ((u32 *)&dst)[0] = ((u32 *)&v1)[0];
        ((u32 *)&dst)[1] = ((u32 *)&v1)[1];
        ((u32 *)&dst)[2] = ((u32 *)&v1)[2];
        ((u32 *)&dst)[3] = ((u32 *)&v1)[3];
        return;
    }
    if (f == 1.0f) {
        // Copy all fields including PAD via integer copy to avoid FP operations
        ((u32 *)&dst)[0] = ((u32 *)&v2)[0];
        ((u32 *)&dst)[1] = ((u32 *)&v2)[1];
        ((u32 *)&dst)[2] = ((u32 *)&v2)[2];
        ((u32 *)&dst)[3] = ((u32 *)&v2)[3];
        return;
    }
    // Cache z and y components to force specific load order for register allocation.
    // X is accessed inline in the expression to match target instruction scheduling.
    float v1_z = v1.z;
    float v2_z = v2.z;
    float v1_y = v1.y;
    float v2_y = v2.y;
    // Compute dst.x, dst.z, dst.y in this specific order to match target stores
    dst.x = f * (v2.x - v1.x) + v1.x;
    dst.z = f * (v2_z - v1_z) + v1_z;
    dst.y = f * (v2_y - v1_y) + v1_y;
}

inline float Distance(const Vector3 &v1, const Vector3 &v2) {
    Vector3 diff;
    Subtract(v1, v2, diff);
    return Length(diff);
}

inline float DistanceSquared(const Vector3 &v1, const Vector3 &v2) {
    Vector3 diff;
    Subtract(v1, v2, diff);
    return LengthSquared(diff);
}

inline void ScaleAdd(const Vector3 &v1, const Vector3 &v2, float f, Vector3 &vres) {
    vres.x = v2.x * f + v1.x;
    vres.y = v2.y * f + v1.y;
    vres.z = v2.z * f + v1.z;
}

inline void ScaleAddEq(Vector3 &v1, const Vector3 &v2, float f) {
    v1.x += v2.x * f;
    v1.y += v2.y * f;
    v1.z += v2.z * f;
}

// actually defined elsewhere and not in here! (Geo.cpp)
void ClosestPoint(const Vector3 &, const Vector3 &, const Vector3 &, Vector3 *);
