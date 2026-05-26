#pragma once
#include <math/Color.h>
#include <rndobj/Rnd.h>

class DebugMeter {
public:
    DebugMeter(float x, float y, float w, float h, const Hmx::Color &c)
        : x(x), y(y), width(w), height(h), color(c) {}
    void DrawBar(
        float startNorm,
        float endNorm,
        Hmx::Color color,
        float scaleX = 1.0f,
        float scaleY = 0.0f
    );
    void DrawLine(float, Hmx::Color, float, float);
    void DrawText(char const *, float, float, Hmx::Color);
    void Draw();

    float GetX() const { return x; }
    float GetY() const { return y; }
    float GetWidth() const { return width; }
    float GetHeight() const { return height; }

private:
    float x;
    float y;
    float width;
    float height;
    Hmx::Color color;
};
