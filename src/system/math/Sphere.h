#pragma once
#include "math/Vec.h"

class Sphere {
public:
    Sphere() : radius(0) {}
    Sphere(const Vector3 &v, float f) { Set(v, f); }
    void Zero() {
        center.Zero();
        radius = 0.0f;
    }
    float GetRadius() const { return radius; }

    void Set(const Vector3 &vec, float f) {
        center = vec;
        radius = f;
    }

    void GrowToContain(const Sphere &);

    Vector3 center; // 0x0
    float radius; // 0x10
};

#include "utl/BinStream.h"
inline BinStream &operator<<(BinStream &bs, const Sphere &s) {
    bs << s.center << s.radius;
    return bs;
}

inline BinStream &operator>>(BinStream &bs, Sphere &s) {
    bs >> s.center >> s.radius;
    return bs;
}
