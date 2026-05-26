#pragma once
#include "utl/MemMgr.h"
#include <vector>
#include "math/Vec.h"
#include "math/Color.h"
#include "math/Geo.h"
#include "rndobj/Cam.h"
#include "utl/Str.h"

class Drawable {
public:
    Drawable() {}
    virtual ~Drawable() {}
    virtual void Draw() = 0;
    virtual void DrawFixedZ(float) = 0;

    static void *operator new(size_t);
    static void operator delete(void *);
};

class Line : public Drawable {
public:
    Line(const Vector3 &va, const Vector3 &vb, const Hmx::Color &clr, bool b)
        : mA(va), mB(vb), mCol(clr), mZBuf(b) {}
    virtual void Draw();
    virtual void DrawFixedZ(float);

    Vector3 mA;
    Vector3 mB;
    Hmx::Color mCol;
    bool mZBuf;
};

class ScreenLine : public Drawable {
public:
    ScreenLine(const Vector2 &va, const Vector2 &vb, const Hmx::Color &clr, bool b)
        : mA(va), mB(vb), mCol(clr), mZBuf(b) {}
    virtual void Draw();
    virtual void DrawFixedZ(float);

    Vector2 mA;
    Vector2 mB;
    Hmx::Color mCol;
    bool mZBuf;
};

class RectFilled2D : public Drawable {
public:
    RectFilled2D(const Hmx::Rect &r, const Hmx::Color &c) : mRect(r), mCol(c) {}
    virtual void Draw();
    virtual void DrawFixedZ(float);

    Hmx::Rect mRect; // 0x4
    Hmx::Color mCol; // 0x14
};

class DrawString : public Drawable {
public:
    DrawString(const char *cc, const Vector2 &v, const Hmx::Color &clr)
        : mPos(v), mText(cc), mCol(clr) {}
    virtual void Draw();
    virtual void DrawFixedZ(float);

    Vector2 mPos;
    String mText;
    Hmx::Color mCol;
};

class DrawString3D : public Drawable {
public:
    DrawString3D(const char *cc, const Vector3 &v, const Hmx::Color &clr)
        : mPos(v), mText(cc), mCol(clr) {}
    virtual void Draw();
    virtual void DrawFixedZ(float);

    Vector3 mPos;
    String mText;
    Hmx::Color mCol;
};

class DrawSphere : public Drawable {
public:
    DrawSphere(const Vector3 &v, float r, const Hmx::Color &clr)
        : mCenter(v), mRadius(r), mCol(clr) {}
    virtual void Draw();
    virtual void DrawFixedZ(float);

    Vector3 mCenter; // 0x4
    float mRadius; // 0x14
    Hmx::Color mCol; // 0x18
};

class RndGraph {
public:
    void Reset();
    void AddLine(const Vector3 &, const Vector3 &, const Hmx::Color &, bool);
    void AddScreenLine(const Vector2 &, const Vector2 &, const Hmx::Color &, bool);
    void AddRectFilled2D(const Hmx::Rect &, const Hmx::Color &);
    void AddSphere(const Vector3 &, float, const Hmx::Color &);
    void AddString(const char *, const Vector2 &, const Hmx::Color &);
    void AddString3D(const char *, const Vector3 &, const Hmx::Color &);
    void AddScreenString(const char *, const Vector2 &, const Hmx::Color &);

    static void Init();
    static void Terminate();
    static void ResetAll();
    static void DrawAll();
    static RndGraph *Get(const void *);
    static void Free(const void *, bool);
    static void SetCamera(RndCam *);
    static RndGraph *GetOneFrame();

protected:
    RndGraph(const void *cv) : mEnable(1), mDrawFixedZ(0), mZ(0.0f), mId((void *)cv) {}
    ~RndGraph();

    void Draw();

    MEM_OVERLOAD(RndGraph, 0x1C);

private:
    bool mEnable; // 0x0
    bool mDrawFixedZ; // 0x1
    float mZ; // 0x4
    void *mId; // 0x8
    std::vector<Drawable *> mStuff; // 0xc
};

struct FakeGraph {
    void *mId; // 0x0
    void (*mDrawCallback)(); // 0x4
};
