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

void DxCam::Select() {
    TheNgStats->mCams++;
    auto& _ref0 = mTargetTex;
    RndCam::Select();
    if (_ref0 != nullptr) {
        _ref0->MakeDrawTarget();
    } else {
        TheDxRnd.MakeDrawTarget();
    }
    Transform view;
    Hmx::Matrix4 proj;
    GetViewProjectXfms(view, proj);
    SetViewport();
    auto _tmp1 = GetGfxMode();
    if (_ref0 != nullptr) {
        RndTex::Type type = _ref0->GetType();
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
    if (_tmp1 == kNewGfx) {
        Hmx::Matrix4 viewProj = Hmx::operator*(view, proj);
        SetViewProj(viewProj);
        Transform invView = GetInvViewXfm();
        TheShaderMgr.SetVConstant(kVS_ViewProjMatrix, mViewProjMatrix);
        Hmx::Matrix4 invViewMtx(invView);
        TheShaderMgr.SetVConstant((VShaderConstant)0x10, invViewMtx);
        Hmx::Rect tmp = TheHiResScreen.ScreenRect();
        Hmx::Rect rect;
        rect.x = tmp.x;
        rect.y = tmp.y;
        rect.w = tmp.w;
        rect.h = tmp.h;
        TheShaderMgr.SetVConstant((VShaderConstant)0x46, (const Vector4 &)rect);
        Hmx::Rect tmp2 = TheHiResScreen.ScreenRect();
        Hmx::Rect rect2;
        rect2.x = tmp2.x;
        rect2.y = tmp2.y;
        rect2.w = tmp2.w;
        rect2.h = tmp2.h;
        TheShaderMgr.SetPConstant((PShaderConstant)0x46, (const Vector4 &)rect2);
    }
}

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
    if (TheHiResScreen.IsActive()) {
        Hmx::Rect tileRect;
        TheHiResScreen.CurrentTileRect(mScreenRect, r, tileRect);
    } else {
        float x = mScreenRect.x;
        float y = mScreenRect.y;
        float x2 = mScreenRect.w + x;
        float y2 = mScreenRect.h + y;
        r.x = Max(0.0f, x);
        r.y = Max(0.0f, y);
        x2 = Max(0.0f, x2);
        y2 = Max(0.0f, y2);
        r.x = Min(1.0f, r.x);
        r.y = Min(1.0f, r.y);
        x2 = Min(1.0f, x2);
        y2 = Min(1.0f, y2);
        r.w = x2 - r.x;
        r.h = y2 - r.y;
    }
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
