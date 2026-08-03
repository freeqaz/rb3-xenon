#include "rndobj/Cam.h"
#include "Rnd.h"
#include "Utl.h"
#include "math/Mtx.h"
#include "math/Rot.h"

#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/System.h"
#include "rndobj/Draw.h"
#include "rndobj/HiResScreen.h"
#include "rndobj/Rnd_NG.h"
#include "rndobj/Trans.h"
#ifdef HX_NATIVE
#include "platform/NativeSettings.h"
#endif

// Transpose is inline in math/Mtx.h

#ifndef HX_NATIVE
static_assert(sizeof(Frustum) == 0x60, "Frustum size mismatch");
#endif

#ifdef HX_NATIVE
RndCam *RndCam::sCurrent;
#endif
float RndCam::sDefaultNearPlane = 1;
float RndCam::sMaxFarNearPlaneRatio = 1000;
// Y/Z flip: Milo (X=right, Y=forward, Z=up) → D3D/WebGPU (X=right, Y=up, Z=forward)
// Runtime-initialized by dynamic initializer ??__EsFlipYZ@@YAXXZ (0x82EDCAE0)
static Transform sFlipYZ(Hmx::Matrix3(1, 0, 0, 0, 0, 1, 0, 1, 0), Vector3(0, 0, 0));

RndCam::RndCam()
    : mNearPlane(sDefaultNearPlane), mFarPlane(mNearPlane * sMaxFarNearPlaneRatio),
      mYFov(0.6024178), mAspectRatio(1), mZRange(0.0f, 1.0f),
      mScreenRect(0.0f, 0.0f, 1.0f, 1.0f), mTargetTex(this), mViewProjMatrix(Hmx::Matrix4::ID()) {
    UpdateLocal();
}

RndCam::~RndCam() {
    if (sCurrent == this)
        sCurrent = nullptr;
}

unsigned int RndCam::ProjectZ(float) { return 0; }

// Forward declaration: retail keeps every SetFrustum call site out-of-line
// (see the definition + comment below OnSetFrustum). SyncProperty's three
// SYNC_PROP_SET call sites need the same noinline trampoline -- without a
// forward declaration here, /Ob2 auto-inlines SetFrustum's full clamp body
// into each branch (measured: base 1048 B vs target 856 B, 3 duplicated
// ~64-byte clamp/store clusters at idx 54-73/115-132/172-190 in objdiff).
static void _outline_SetFrustum(RndCam *cam, float n, float f, float y, float a);

BEGIN_HANDLERS(RndCam)
    HANDLE(set_frustum, OnSetFrustum)
    HANDLE(set_z_range, OnSetZRange)
    HANDLE(far_plane, OnFarPlane)
    HANDLE(set_screen_rect, OnSetScreenRect)
    HANDLE_SUPERCLASS(RndTransformable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(RndCam)
    SYNC_SUPERCLASS(RndTransformable)
    SYNC_PROP_SET(
        near_plane, mNearPlane, _outline_SetFrustum(this, _val.Float(), mFarPlane, mYFov, 1)
    )
    SYNC_PROP_SET(
        far_plane, mFarPlane, _outline_SetFrustum(this, mNearPlane, _val.Float(), mYFov, 1)
    )
    SYNC_PROP_SET(
        y_fov,
        mYFov * RAD2DEG,
        _outline_SetFrustum(this, mNearPlane, mFarPlane, _val.Float() * DEG2RAD, 1)
    )
    SYNC_PROP(z_range, mZRange)
    SYNC_PROP_MODIFY(screen_rect, mScreenRect, UpdateLocal())
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(RndCam)
    SAVE_REVS(0xC, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndTransformable)
    bs << mNearPlane << mFarPlane << mYFov;
    bs << mScreenRect << mZRange;
    bs << mTargetTex;
END_SAVES

BEGIN_COPYS(RndCam)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndTransformable)
    CREATE_COPY(RndCam)
    BEGIN_COPYING_MEMBERS
        if (ty != kCopyFromMax) {
            COPY_MEMBER(mNearPlane)
            COPY_MEMBER(mFarPlane)
            COPY_MEMBER(mYFov)
            COPY_MEMBER(mScreenRect)
            COPY_MEMBER(mZRange)
            COPY_MEMBER(mTargetTex)
        }
    END_COPYING_MEMBERS
    UpdateLocal();
END_COPYS

INIT_REVS(12, 0)

BEGIN_LOADS(RndCam)
    LOAD_REVS(bs)
    ASSERT_REVS(12, 0)
    if (d.rev > 10) {
        Hmx::Object::Load(bs);
    }
    RndTransformable::Load(bs);
    if (d.rev < 10) {
        RndDrawable::DumpLoad(bs);
    }
    if (d.rev == 8) {
        ObjPtrList<Hmx::Object> objList(this, kObjListNoNull);
        int x;
        bs >> x >> objList;
    }
    bs >> mNearPlane;
    bs >> mFarPlane;
    bs >> mYFov;
    if (d.rev < 0xC) {
        mYFov = ConvertFov(mYFov, 0.75f);
    }
    if (d.rev < 2) {
        int x;
        bs >> x;
    }
    bs >> mScreenRect;
    if (d.rev > 0 && d.rev < 3) {
        int x;
        bs >> x;
    }
    if (d.rev > 3) {
        bs >> mZRange;
    }
    if (d.rev > 4) {
        bs >> mTargetTex;
    }
    if (d.rev == 6) {
        int x;
        bs >> x;
    }
    UpdateLocal();
END_LOADS

void RndCam::UpdateLocal() {
    float ratio = (mScreenRect.h / mScreenRect.w) * mAspectRatio;
    if (mTargetTex) {
        ratio *= (float)mTargetTex->Height() / (float)mTargetTex->Width();
    } else {
#ifdef HX_NATIVE
        float aspectOvr = NativeSettings::Get().aspectOverride;
        if (aspectOvr > 0)
            ratio *= 1.0f / aspectOvr;  // override the display aspect
        else
#endif
        ratio *= TheRnd.YRatio();
    }
    mLocalFrustum.Set(mNearPlane, mFarPlane, mYFov, ratio);
    mLocalProjectXfm.m.Zero();
    mLocalProjectXfm.v.Zero();
    mInvLocalProjectXfm.m.Zero();
    mInvLocalProjectXfm.v.Zero();
    if (mYFov == 0) {
        mInvLocalProjectXfm.m.y.z = -ratio;
        mLocalProjectXfm.m.x.x = 1;
        mLocalProjectXfm.m.z.y = -1.0f / ratio;
        mInvLocalProjectXfm.m.x.x = 1;
    } else {
        float thetan = tanf(mYFov * 0.5f);
        mLocalProjectXfm.m.y.z = 1;
        mInvLocalProjectXfm.m.z.y = 1;
        mLocalProjectXfm.m.x.x = ratio / thetan;
        mLocalProjectXfm.m.z.y = -1.0f / thetan;
        mInvLocalProjectXfm.m.x.x = thetan / ratio;
        mInvLocalProjectXfm.m.y.z = -thetan;
    }
    UpdatedWorldXfm();
    mAspect = TheRnd.GetAspect();
}

void RndCam::UpdatedWorldXfm() {
    const Transform &xfm = WorldXfm();
    Invert(xfm, mInvWorldXfm);
    Multiply(mLocalFrustum, xfm, mWorldFrustum);
    Multiply(mInvWorldXfm, mLocalProjectXfm, mWorldProjectXfm);
    Multiply(mInvLocalProjectXfm, xfm, mInvWorldProjectXfm);
}

void RndCam::Select() {
    RndCam *cur = sCurrent;
    if (cur) {
        if (cur->TargetTex() && cur != this) {
            cur->TargetTex()->FinishDrawTarget();
        }
    }
    WorldXfm();
    sCurrent = this;
    if (TheRnd.GetAspect() != mAspect) {
        UpdateLocal();
    }
#ifdef HX_NATIVE
    if (mTargetTex) {
        mTargetTex->MakeDrawTarget();
    } else {
        TheRnd.MakeDrawTarget();
    }
    int width = mTargetTex ? mTargetTex->Width() : TheRnd.Width();
    int height = mTargetTex ? mTargetTex->Height() : TheRnd.Height();
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
    NgRnd::Viewport vp;
    vp.X = (unsigned int)((float)width * r.x);
    vp.Y = (unsigned int)((float)height * r.y);
    vp.Width = (unsigned int)((float)width * r.w);
    vp.Height = (unsigned int)((float)height * r.h);
    vp.MinZ = mZRange.x;
    vp.MaxZ = mZRange.y;
    TheNgRnd.SetViewport(vp);
#endif
}

Transform RndCam::GetInvViewXfm() {
    Transform out;
    Multiply(sFlipYZ, WorldXfm(), out);
    return out;
}

void RndCam::GetCamFrustum(Vector3 &origin, Vector3 (&dirs)[4]) {
    const Transform &xfm = WorldXfm();
    origin = xfm.v;
    static Vector2 sCorners[4] = {
        Vector2(0, 0), Vector2(0, 1), Vector2(1, 0), Vector2(1, 1)
    };
    for (int i = 0; (unsigned int)i < 4; i++) {
        ScreenToWorld(sCorners[i], mFarPlane, dirs[i]);
        Subtract(dirs[i], origin, dirs[i]);
    }
}

void RndCam::SetViewProj(const Hmx::Matrix4 &mtx) {
    mViewProjMatrix = mtx;
}

void RndCam::SetTargetTex(RndTex *tex) {
    if (sCurrent == this) {
        if (mTargetTex) {
            mTargetTex->FinishDrawTarget();
        }
    }
    mTargetTex = tex;
    UpdateLocal();
}

void RndCam::Init() {
    REGISTER_OBJ_FACTORY(RndCam);
    if (SystemConfig()) {
        DataArray *cfg = SystemConfig("rnd");
        cfg->FindData("cam_default_near_plane", sDefaultNearPlane, true);
        cfg->FindData("cam_max_far_near_ratio", sMaxFarNearPlaneRatio, true);
    }
    DataRegisterFunc("cam_get_default_near_plane", OnGetDefaultNearPlane);
    DataRegisterFunc("cam_get_max_far_near_ratio", OnGetMaxFarNearPlaneRatio);
}

void RndCam::SetFrustum(float near, float far, float yfov, float f4) {
    if (far - 0.0001f > sMaxFarNearPlaneRatio * near) {
        MILO_NOTIFY_ONCE(
            "%s: %f/%f plane ratio exceeds %d",
            Name(),
            far,
            near,
            (int)sMaxFarNearPlaneRatio
        );
        if (far == mFarPlane) {
            near = far / sMaxFarNearPlaneRatio;
        } else {
            far = sMaxFarNearPlaneRatio * near;
        }
    }
    mNearPlane = near;
    mFarPlane = far;
    mYFov = yfov;
    mAspectRatio = f4;
    UpdateLocal();
}

float RndCam::WorldToScreen(const Vector3 &w, Vector2 &s) const {
#ifdef HX_NATIVE
    const_cast<RndCam *>(this)->UpdatedWorldXfm();

    Transform viewXfm;
    Hmx::Matrix4 projMtx;
    GetViewProjectXfms(viewXfm, projMtx);

    Vector3 viewPos;
    Multiply(w, viewXfm, viewPos);

    float clip[4];
    float pos[4] = { viewPos.x, viewPos.y, viewPos.z, 1.0f };
    const float proj[16] = {
        projMtx.x.x, projMtx.x.y, projMtx.x.z, projMtx.x.w,
        projMtx.y.x, projMtx.y.y, projMtx.y.z, projMtx.y.w,
        projMtx.z.x, projMtx.z.y, projMtx.z.z, projMtx.z.w,
        projMtx.w.x, projMtx.w.y, projMtx.w.z, projMtx.w.w,
    };

    for (int j = 0; j < 4; j++) {
        float sum = 0.0f;
        for (int k = 0; k < 4; k++) {
            sum += pos[k] * proj[k * 4 + j];
        }
        clip[j] = sum;
    }

    float ndcX = clip[0];
    float ndcY = clip[1];
    if (clip[3] != 0.0f) {
        float invW = 1.0f / clip[3];
        ndcX *= invW;
        ndcY *= invW;
    }

    s.x = (ndcX + 1.0f) * 0.5f;
    s.y = (ndcY + 1.0f) * 0.5f;
    s.Set(s.x * mScreenRect.w + mScreenRect.x, s.y * mScreenRect.h + mScreenRect.y);
    return viewPos.z;
#else
    Vector3 projectedVec;
    Multiply(w, mWorldProjectXfm, projectedVec);
    if (projectedVec.z) {
        float scale = 1.0f / projectedVec.z;
        s.x = projectedVec.x * scale;
        s.y = projectedVec.y * scale;
    } else {
        s.x = projectedVec.x;
        s.y = projectedVec.y;
    }
    s.x = (s.x + 1.0f) / 2.0f;
    s.y = (s.y + 1.0f) / 2.0f;
    s.Set(s.x * mScreenRect.w + mScreenRect.x, s.y * mScreenRect.h + mScreenRect.y);
    return projectedVec.z;
#endif
}

// Converts screen coordinates to world space at a given depth
void RndCam::ScreenToWorld(const Vector2 &v2, float f, Vector3 &vout) const {
#ifdef HX_NATIVE
    const_cast<RndCam *>(this)->UpdatedWorldXfm();

    Transform viewXfm;
    Hmx::Matrix4 projMtx;
    GetViewProjectXfms(viewXfm, projMtx);

    float ndcX = (((v2.x - mScreenRect.x) / mScreenRect.w) * 2.0f) - 1.0f;
    float ndcY = (((v2.y - mScreenRect.y) / mScreenRect.h) * 2.0f) - 1.0f;

    Vector3 viewPos;
    if (mYFov == 0.0f) {
        viewPos.x = (ndcX - projMtx.z.x) / projMtx.x.x;
        viewPos.y = (ndcY - projMtx.z.y) / projMtx.y.y;
        viewPos.z = f;
    } else {
        viewPos.z = f;
        viewPos.x = (ndcX - projMtx.z.x) * viewPos.z / projMtx.x.x;
        viewPos.y = (ndcY - projMtx.z.y) * viewPos.z / projMtx.y.y;
    }

    Transform invView;
    Invert(viewXfm, invView);
    Multiply(viewPos, invView, vout);
#else
    // Normalize screen coords [0,1] to NDC [-1,1], scale by depth
    float x = (((v2.x - mScreenRect.x) / mScreenRect.w) * 2.0f - 1.0f) * f;
    float y = (((v2.y - mScreenRect.y) / mScreenRect.h) * 2.0f - 1.0f) * f;
    // Assignment order affects codegen - do not reorder
    vout.z = f;
    vout.y = y;
    vout.x = x;
    Multiply(vout, mInvWorldProjectXfm, vout);
#endif
}

void RndCam::GetViewProjectXfms(Transform &viewXfm, Hmx::Matrix4 &projMtx) const {
    Multiply(mInvWorldXfm, sFlipYZ, viewXfm);

#ifdef HX_NATIVE
    // Apply camera position offsets in view space.
    // After the Y/Z flip: X=right, Y=up, Z=forward (into the scene).
    // Forward offset: positive Z = closer to subject (camera advances).
    // Height offset: positive Y = camera moves up.
    // Lateral offset: positive X = camera moves right.
    {
        auto &s = NativeSettings::Get();
        if (s.camForwardOffset != 0)
            viewXfm.v.z += s.camForwardOffset;
        if (s.camHeightOffset != 0)
            viewXfm.v.y -= s.camHeightOffset;  // negate: view Y is inverted
        if (s.camLateralOffset != 0)
            viewXfm.v.x += s.camLateralOffset;
    }
#endif

    projMtx.Zero();
    float nearPlane = mNearPlane;
    float farPlane = mFarPlane;

#ifdef HX_NATIVE
    // Apply runtime near/far overrides for tuning
    {
        auto &s = NativeSettings::Get();
        if (s.nearPlaneOverride > 0)
            nearPlane = s.nearPlaneOverride;
        if (s.farPlaneOverride > 0)
            farPlane = s.farPlaneOverride;
    }
#endif

    float farRatio;
    if (mYFov == 0) {
        projMtx.w.w = 1.0f;
        farRatio = 1.0f / (farPlane - nearPlane);
    } else {
        projMtx.z.w = 1.0f;
        farRatio = farPlane / (farPlane - nearPlane);
    }

    Hmx::Rect screenRect = mScreenRect;
    Hmx::Rect hiRect = TheHiResScreen.ScreenRect(this, screenRect);

    float cx = mScreenRect.w * 0.5f + mScreenRect.x;
    float cy = mScreenRect.h * 0.5f + mScreenRect.y;

    float left = Max(hiRect.x, 0.0f);
    float bottom = Max(hiRect.y, 0.0f);
    float right = Min(hiRect.x + hiRect.w, 1.0f);
    float top = Min(hiRect.y + hiRect.h, 1.0f);

    float l = (left - cx) * 2.0f;
    float b = (bottom - cy) * 2.0f;
    float r = (right - cx) * 2.0f;
    float t = (top - cy) * 2.0f;

    float width = r - l;
    float height = t - b;

    projMtx.z.z = farRatio;
    projMtx.x.x = (mScreenRect.w * mLocalProjectXfm.m.x.x * 2.0f) / width;
#ifdef HX_NATIVE
    // ⛔ THE VERTICAL FOV TERM WAS READ FROM THE WRONG MATRIX SLOT, AND IT MADE
    //    THE ENGINE'S PROJECTION DEGENERATE.
    //
    // The X360 line below reads mLocalProjectXfm.v.x -- the TRANSLATION
    // component. UpdateLocal (:162-163 in DC3's copy, :160-161 here) does
    // `mLocalProjectXfm.v.Zero()` and then only ever writes `m.*`, so v.x is
    // ALWAYS EXACTLY ZERO. projMtx.y.y is therefore always 0, i.e. nothing in
    // view space contributes to NDC y, and every triangle collapses onto a
    // line. MEASURED at X3 bring-up on crowd_female01: with this line as-is the
    // engine's GetViewProjectXfms path returned
    //     [ 1.8107  0  0  0 ][ 0  0  0  0 ][ -0  0  1.0011  1 ][ 0  0 -1.1123  0 ]
    // -- an entirely zero second row -- and the frame came back a flat clear
    // colour with the draws demonstrably issued and every texture uploaded.
    //
    // The vertical FOV factor lives in m.z.y: UpdateLocal writes
    // `mLocalProjectXfm.m.z.y = -1/thetan` (and `-1/ratio` in the ortho
    // branch). ★ DC3 HAS ALREADY FOUND AND FIXED THIS EXACT LINE --
    // dc3-decomp/src/system/rndobj/Cam.cpp:468-472 carries the corrected slot
    // and a comment ending "(was incorrectly mLocalProjectXfm.v.x, which is
    // always zero)". rb3-Wii's Cam.cpp writes the same m.z.y in UpdateLocal.
    // So three independent decomps agree on where the value is stored, and
    // xenon is alone in reading it from somewhere else.
    //
    // ⚠ GATED, NOT CORRECTED OUTRIGHT, and the reason is honest rather than
    // timid: `RndCam::GetViewProjectXfms` does not appear as a matched function
    // in build/45410914/report.json's `default/Cam` unit (84.7% fn-matched), so
    // this build has NO objdiff evidence about the retail body either way. The
    // native side gets the demonstrably-correct value; the X360 side keeps
    // exactly the bytes it compiles today (no /D is passed there, so HX_NATIVE
    // is never defined and its token stream is unchanged). Promoting the fix
    // unconditionally is an objdiff A/B against the real function, and it is
    // written up as owed work in docs/plans/x3-first-render-2026-08-01.md.
    projMtx.y.y = (-(mScreenRect.h * mLocalProjectXfm.m.z.y) * 2.0f) / height;
#else
    projMtx.y.y = (-(mScreenRect.h * mLocalProjectXfm.v.x) * 2.0f) / height;
#endif
    projMtx.z.y = (t + b) / height;
    projMtx.z.x = -((r + l) / width);
    projMtx.w.z = -(nearPlane * farRatio);

#ifdef HX_NATIVE
    // Apply FOV scale — scales the projection matrix elements that encode FOV.
    // x.x controls horizontal FOV, y.y controls vertical FOV.
    // A scale > 1 narrows the FOV (zooms in), < 1 widens it (zooms out).
    {
        float fovScale = NativeSettings::Get().fovScale;
        if (fovScale != 1.0f) {
            projMtx.x.x *= fovScale;
            projMtx.y.y *= fovScale;
        }
    }
#endif
}

void RndCam::GetDepthRangeValues(Vector4 &v) const {
    float near = mZRange.x;
    float zratio = 1.0f / (mZRange.y - mZRange.x);
    v.Set(mNearPlane, mFarPlane, zratio, zratio * near);
}

void RndCam::GetInfiniteViewProj(Hmx::Matrix4 &m4) const {
    Transform tfa0;
    Hmx::Matrix4 me0;
    GetViewProjectXfms(tfa0, me0);
    me0.z.z = 1;
    me0.w.z = -mNearPlane;
    m4 = tfa0 * me0;
}

DataNode RndCam::OnGetDefaultNearPlane(DataArray *) { return sDefaultNearPlane; }
DataNode RndCam::OnGetMaxFarNearPlaneRatio(DataArray *) { return sMaxFarNearPlaneRatio; }

// Retail keeps this call to RndCam::SetFrustum out-of-line (it is compiled as
// a real function elsewhere in this TU for other callers). Since SetFrustum's
// definition appears earlier in this file, /Ob2 auto-inlines it here unless we
// force it back out through a noinline wrapper (same lever as
// _outline_GetAccomplishmentProgress in AccomplishmentPanel.cpp).
__declspec(noinline) static void
_outline_SetFrustum(RndCam *cam, float n, float f, float y, float a) {
    cam->SetFrustum(n, f, y, a);
}

DataNode RndCam::OnSetFrustum(const DataArray *da) {
    float nearPlane, farPlane, yFov, temp;
    static Symbol near_plane("near_plane");
    static Symbol far_plane("far_plane");
    static Symbol y_fov("y_fov");
    if (!da->FindData(near_plane, nearPlane, false))
        nearPlane = mNearPlane;
    if (!da->FindData(far_plane, farPlane, false))
        farPlane = mFarPlane;
    if (da->FindData(y_fov, yFov, false))
        temp = yFov * DEG2RAD;
    else
        temp = mYFov;
    yFov = temp;
    _outline_SetFrustum(this, nearPlane, farPlane, yFov, 1.0f);
    return 0;
}

DataNode RndCam::OnSetZRange(const DataArray *da) {
    SetZRange(da->Float(2), da->Float(3));
    return 0;
}

DataNode RndCam::OnSetScreenRect(const DataArray *da) {
    Hmx::Rect r(da->Float(2), da->Float(3), da->Float(4), da->Float(5));
    SetScreenRect(r);
    return 0;
}

DataNode RndCam::OnFarPlane(const DataArray *) { return mFarPlane; }

DataNode RndCam::OnWorldToScreen(const DataArray *a) {
    Vector3 w(a->Float(2), a->Float(3), a->Float(4));
    Vector2 s;
    float ret = WorldToScreen(w, s);
    *a->Var(5) = s.x;
    *a->Var(6) = s.y;
    return ret;
}

DataNode RndCam::OnScreenToWorld(const DataArray *a) {
    Vector2 v2(a->Float(2), a->Float(3));
    Vector3 vout;
    ScreenToWorld(v2, a->Float(4), vout);
    *a->Var(5) = vout.x;
    *a->Var(6) = vout.y;
    *a->Var(7) = vout.z;
    return 0;
}
