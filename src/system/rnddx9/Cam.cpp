#include "rnddx9/Cam.h"
#include "math/Mtx.h"
#include "os/Debug.h"
#include "os/System.h"
#include "rndobj/HiResScreen.h"
#include "rndobj/Rnd_NG.h"
#include "rndobj/ShaderMgr.h"
#include "rndobj/Stats_NG.h"
#include "rndobj/Tex.h"
#include "rnddx9/Rnd.h"
#include "xdk/d3d9i/d3d9.h"

Vector3 Hmx::Matrix4::Col3(int col) const {
    return Vector3(x[col], y[col], z[col]);
}

DxCam::DxCam() {}

// The screen rect reaches the shader as a Vector4 built from a temporary
// (retail: one shared temp slot, the four floats copied h/w/y/x into a local).
static inline Vector4 RectToVector4(const Hmx::Rect &r) {
    return Vector4(r.x, r.y, r.w, r.h);
}

void DxCam::Select() {
#ifdef HX_NATIVE
    TheNgStats->mCams++;
#endif
    RndCam::Select();
    if (mTargetTex != nullptr) {
        mTargetTex->MakeDrawTarget();
    } else {
        TheDxRnd.MakeDrawTarget();
    }
    Transform view;
    Hmx::Matrix4 proj;
    GetViewProjectXfms(view, proj);
    SetViewport();
    if (mTargetTex != nullptr) {
        RndTex::Type type = mTargetTex->GetType();
        bool isShadowMap = false;
        float depth = 1.0f;
        if (type == RndTex::kShadowMap) {
            isShadowMap = true;
        } else {
            depth = 0.0f;
        }
        UINT clearColor = 0;
        UINT clearFlags = 0;
        bool setClear = (type & RndTex::kRendered) && !(type & 0x20);
        if (setClear) {
            clearFlags = 0x30;
        }
        if (!isShadowMap) {
            clearFlags |= 0xf;
        }
        if (type == RndTex::kDepthVolumeMap) {
            clearColor = 0xFF000000;
        }
        auto _tmp0 = TheDxRnd.Device();
        D3DDevice_Clear(
            _tmp0, 0, nullptr, clearFlags, clearColor, depth, 0, 0
        );
    }
    // RB3 always uploads the view constants (retail has no GetGfxMode test).
    mViewProjMatrix = Hmx::operator*(view, proj);
    Transform invView = GetInvViewXfm();
    TheShaderMgr.SetVConstant(kVS_ViewProjMatrix, mViewProjMatrix);
    TheShaderMgr.SetVConstant((VShaderConstant)0x10, Hmx::Matrix4(invView));
    Vector4 rect = RectToVector4(TheHiResScreen.ScreenRect());
    TheShaderMgr.SetVConstant((VShaderConstant)0x46, rect);
    Vector4 rect2 = RectToVector4(TheHiResScreen.ScreenRect());
    TheShaderMgr.SetPConstant((PShaderConstant)0x46, rect2);
}

// 0x8273DC00 (called only from DxCam::Select). RB3 has no hi-res-screenshot
// tiling path here.
void DxCam::SetViewport() {
    int width, height;
    if (mTargetTex != nullptr) {
        width = mTargetTex->Width();
        height = mTargetTex->Height();
    } else {
        width = TheDxRnd.Width();
        height = TheDxRnd.Height();
    }
    Hmx::Rect r;
    float x = mScreenRect.x;
    float y = mScreenRect.y;
    float x2 = mScreenRect.w + x;
    float y2 = mScreenRect.h + y;
    r.x = Max(0.0f, x);
    r.y = Max(0.0f, y);
    x2 = Max(0.0f, x2);
    y2 = Max(0.0f, y2);
    r.x = Min(r.x, 1.0f);
    r.y = Min(r.y, 1.0f);
    x2 = Min(x2, 1.0f);
    y2 = Min(y2, 1.0f);
    r.w = x2 - r.x;
    r.h = y2 - r.y;
    MILO_ASSERT((r.x >= 0.f) && (r.x <= 1.f), 0x43);
    MILO_ASSERT((r.y >= 0.f) && (r.y <= 1.f), 0x44);
    MILO_ASSERT((r.w >= 0.f) && (r.w <= 1.f), 0x45);
    MILO_ASSERT((r.h >= 0.f) && (r.h <= 1.f), 0x46);
    NgRnd::Viewport vp;
    vp.X = (unsigned int)((float)width * r.x);
    vp.Y = (unsigned int)((float)height * r.y);
    vp.Width = (unsigned int)((float)width * r.w);
    vp.Height = (unsigned int)((float)height * r.h);
    vp.MinZ = mZRange.x;
    vp.MaxZ = mZRange.y;
    TheNgRnd.SetViewport(vp);
}

unsigned int DxCam::ProjectZ(float z) {
    float f = ((z - mNearPlane) / z)
        * (mFarPlane / (mFarPlane - mNearPlane))
        * (mZRange.y - mZRange.x) + mZRange.x;
    if (TheDxRnd.ReverseZ()) {
        f = 1.0f - f;
    }
    return (unsigned int)(f * 16777215.0f);
}
