#pragma once
#include "math/Mtx.h"
#include "math/Sphere.h"
#include "math/Utl.h"
#include "math/Vec.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "rndobj/Rnd.h"
#include "rndobj/Tex.h"
#include "rndobj/Trans.h"
#include "math/Geo.h"
#include "utl/MemMgr.h"

class RndCam : public RndTransformable {
    friend class NgSpotlightDrawer;
    // GamePanel::Enter() calls cam->UpdateLocal() directly; friendship keeps
    // that legal while UpdateLocal stays protected (which is what retail's
    // `IAA` mangling requires). Friendship is compile-time only — no effect
    // on mangling, layout or codegen.
    friend class GamePanel;

public:
    virtual ~RndCam();
    OBJ_CLASSNAME(Cam);
    OBJ_SET_TYPE(Cam);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void UpdatedWorldXfm();
    virtual void Select();
    virtual unsigned int ProjectZ(float);

    float NearPlane() const { return mNearPlane; }
    float FarPlane() const { return mFarPlane; }
    float YFov() const { return mYFov; }

    void SetViewProj(const Hmx::Matrix4 &);
    Transform GetInvViewXfm();
    float WorldToScreen(const Vector3 &, Vector2 &) const;
    void ScreenToWorld(const Vector2 &, float, Vector3 &) const;
    void GetCamFrustum(Vector3 &, Vector3 (&)[4]);
    void SetTargetTex(RndTex *);
    void SetFrustum(float, float, float, float);
    void GetViewProjectXfms(Transform &, Hmx::Matrix4 &) const;
    void GetDepthRangeValues(Vector4 &) const;
    void GetInfiniteViewProj(Hmx::Matrix4 &) const;
    Transform &LocalProjectXfm() { return mLocalProjectXfm; }
    RndTex *TargetTex() const { return mTargetTex; }
    const Frustum &WorldFrustum() const { return mWorldFrustum; }
    const Vector2 &ZRange() const { return mZRange; }
    void SetZRange(float f1, float f2) { mZRange.Set(f1, f2); }
    void SetScreenRect(const Hmx::Rect &rect) {
        mScreenRect = rect;
        UpdateLocal();
    }
    const Hmx::Rect &GetScreenRect() const { return mScreenRect; }
    float CalcScreenHeight(const Sphere &s) {
        float r = mLocalProjectXfm.m.z.y * s.GetRadius();
        float dist = CalcDistTo(s.center);
        if (dist != 0) {
            return fabsf(r / dist) * mScreenRect.h;
        } else {
            return kHugeFloat;
        }
    }
    float CalcDistTo(const Vector3 &v) {
        return Dot(v, WorldXfm().m.y) + mInvWorldXfm.v.y;
    }

    NEW_OBJ(RndCam);
    OBJ_MEM_OVERLOAD(0x1B);
    static void Init();
    static RndCam *Current() { return sCurrent; }
#ifdef HX_NATIVE
    static void ClearCurrent() { sCurrent = nullptr; }
#endif
    static float DefaultNearPlane() { return sDefaultNearPlane; }
    static float MaxFarNearPlaneRatio() { return sMaxFarNearPlaneRatio; }
    const Hmx::Matrix4 &GetViewProjMatrix() const { return mViewProjMatrix; }

protected:
    RndCam();

    // Retail mangles this `IAA` (protected), agreeing with dc3's RndCam — the
    // earlier "retail exposes it publicly" note was wrong (it was inferred
    // from GamePanel::Enter() calling cam->UpdateLocal(), which retail can do
    // via friendship). Access is pure name-mangling with no vtable/layout
    // effect, but objdiff pairs by name, so a public declaration can never
    // pair with the target symbol.
    void UpdateLocal();

    DataNode OnSetFrustum(const DataArray *);
    DataNode OnSetZRange(const DataArray *);
    DataNode OnSetScreenRect(const DataArray *);
    DataNode OnFarPlane(const DataArray *);
    DataNode OnWorldToScreen(const DataArray *);
    DataNode OnScreenToWorld(const DataArray *);

    static DataNode OnGetDefaultNearPlane(DataArray *);
    static DataNode OnGetMaxFarNearPlaneRatio(DataArray *);

    static RndCam *sCurrent;
    static float sDefaultNearPlane;
    static float sMaxFarNearPlaneRatio;

    Transform mInvWorldXfm; // 0xb4
    Transform mLocalProjectXfm; // 0xf4
    Transform mInvLocalProjectXfm; // 0x134
    Transform mWorldProjectXfm; // 0x174
    Transform mInvWorldProjectXfm; // 0x1b4
    Frustum mLocalFrustum; // 0x1f4
    Frustum mWorldFrustum; // 0x254
    /**
     * @brief The near-clipping plane.
     * Original _objects description:
     * "The distance in world coordinates to the near clipping
     * plane. The near/far ratio is limited to 1:1000 to preserve
     * Z-buffer resolution."
     */
    float mNearPlane; // 0x2b4
    /**
     * @brief The far-clipping plane.
     * Original _object description:
     * "The distance in world coordinates to the far clipping
     * plane. The near/far ratio is limited to 1:1000 to preserve
     * Z-buffer resolution. Note that on the PS2, object polys are
     * culled rather than clipped to the far plane."
     */
    float mFarPlane; // 0x2b8
    float mYFov; // 0x2bc
    float mAspectRatio; // 0x2c0
    /**
     * @brief
     * Original _objects description:
     * "The part of the Z-buffer to use, in normalized
     * coordinates. It can be useful to draw a scene where the near
     * and far planes must exceed the 1:1000 ratio (so multiple
     * cameras are used to draw farthest to nearest objects, each
     * using a closer range of the z-buffer) or to leave some
     * z-buffer for HUD overlay objects."
     */
    Vector2 mZRange; // 0x2c4
    Hmx::Rect mScreenRect; // 0x2cc
    ObjPtr<RndTex> mTargetTex; // 0x2dc
    Rnd::Aspect mAspect; // 0x2e8
    Hmx::Matrix4 mViewProjMatrix; // 0x2ec (retail TU5: no mInvViewProjMatrix; vbase at 0x330)
};
