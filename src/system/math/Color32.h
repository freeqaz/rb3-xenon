#pragma once
#include "math/Color.h"

// rb3-Wii canonically defines Hmx::Color32 inside math/Color.h. Adding it there
// directly perturbs codegen in unrelated units that include the (very hot)
// math/Color.h — e.g. UIButton::OnMsg regressed 100% -> 98% from the extra
// inline COMDATs shifting MSVC's inlining decisions. To keep math/Color.h
// byte-identical to main, Color32 lives in this dedicated header that only the
// ported track/bandobj headers that actually need it pull in.
namespace Hmx {
    class Color32 {
    public:
        union {
            uint color;
            struct {
                u8 a, b, g, r;
            };
        };

        Color32() { Clear(); }
        Color32(int i) { color = i; }
        Color32(const Color32 &other) { color = other.color; }
        Color32(const Hmx::Color &col) { color = col.PackAlpha(); }
        Color32(float r, float g, float b, float a) {
            color = ((int)(a * 255.0f) & 0xFF) << 24 | ((int)(b * 255.0f) & 0xFF) << 16
                | ((int)(g * 255.0f) & 0xFF) << 8 | ((int)(r * 255.0f) & 0xFF);
        }
        void Clear() { color = -1; }
        void Set(Hmx::Color &col) { color = col.PackAlpha(); }
        void Set(float r, float g, float b, float a) {
            color = ((int)(a * 255.0f) & 0xFF) << 24 | ((int)(b * 255.0f) & 0xFF) << 16
                | ((int)(g * 255.0f) & 0xFF) << 8 | ((int)(r * 255.0f) & 0xFF);
        }
        Color32 &operator=(const Color32 &other) {
            color = other.color;
            return *this;
        }
        bool operator==(const Color32 &other) const { return color == other.color; }
        bool operator!=(const Color32 &other) const { return color != other.color; }
        void SetAlpha(float f) { a = f * 255.0f; }
        int FullColor() const { return color; }
        int Opaque() const { return color | 0xFF000000; }

        float fr() const { return r * 0.0039215688593685627f; }
        float fg() const { return g * 0.0039215688593685627f; }
        float fb() const { return b * 0.0039215688593685627f; }
        float fa() const { return a * 0.0039215688593685627f; }
    };
}
