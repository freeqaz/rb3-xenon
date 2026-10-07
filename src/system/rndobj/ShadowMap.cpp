#include "rndobj/ShadowMap.h"
#include "../../Memory.h"
#include "macros.h"
#include "math/Mtx.h"
#include "math/Rot.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/System.h"
#include "rndobj/Cam.h"
#include "rndobj/Lit.h"
#include "rndobj/Rnd.h"
#include "rndobj/Tex.h"
#include <cmath>

RndCam *RndShadowMap::sLightCam;
RndTex *RndShadowMap::sShadowTex;

void RndShadowMap::Terminate() {
    RELEASE(sLightCam);
    RELEASE(sShadowTex);
}

void RndShadowMap::Init() {
    PhysMemTypeTracker tracker("D3D(phys):Global");
    delete sLightCam;
    sLightCam = Hmx::Object::New<RndCam>();
    delete sShadowTex;
    sShadowTex = Hmx::Object::New<RndTex>();
    sShadowTex->SetBitmap(512, 512, 32, RndTex::kShadowMap, false, nullptr);
    sLightCam->SetTargetTex(sShadowTex);
}

void RndShadowMap::EndShadow() { TheRnd.SetShadowMap(nullptr, nullptr, nullptr); }

bool RndShadowMap::PrepShadow(RndDrawable *draw, RndEnviron *env) {
    if (sLightCam == NULL || sShadowTex == NULL)
        return false;

    RndEnviron *e = env != NULL ? env : RndEnviron::Current();

    RndLight *light = NULL;
    ObjPtrList<RndLight>::iterator it;
    // Both passes walk the real lights: a floor spot wins, else the first
    // directional or point light.
    for (it = e->LightsReal().begin(); it != e->LightsReal().end(); ++it) {
        if ((*it)->GetType() == RndLight::kFloorSpot) {
            light = *it;
            break;
        }
    }
    if (light)
        goto found;

    for (it = e->LightsReal().begin(); it != e->LightsReal().end(); ++it) {
        light = *it;
        if (light->GetType() == RndLight::kDirectional || light->GetType() == RndLight::kPoint)
            goto found;
    }

    return false;

found:
    Sphere sphere;
    RndCam *curCam = RndCam::Current();
    if (!draw->MakeWorldSphere(sphere, false)) {
        MILO_NOTIFY_ONCE(
            "Can't self-shadow %s; MakeWorldSphere failed.", PathName(draw)
        );
        return false;
    }

    Transform lightXfm;
    memcpy(&lightXfm.m, &light->WorldXfm().m, sizeof(Hmx::Matrix3));
    lightXfm.v = sphere.center;

    if (light->GetType() == RndLight::kPoint) {
        Subtract(sphere.center, light->WorldXfm().v, lightXfm.m.y);
        Normalize(lightXfm.m, lightXfm.m);
    }

    float tanHalfFov = (float)std::tan(PI / 8.0f);
    float dist = sphere.radius / tanHalfFov;
    float farPlane = sphere.radius + dist;
    float nearPlane = dist - sphere.radius;

    Vector3 offset;
    // This is Multiply(Vector3(0.0f, -dist, 0.0f), lightXfm.m, offset) written
    // out.  x and z of that vector are literal 0.0f, and through the shared
    // overload in Mtx.h /fp:fast reassociates `m.x.c*0 + m.y.c*(-dist) +
    // m.z.c*0` into `(m.x.c + m.z.c)*0 + ...`, a leading `fadds` of two matrix
    // elements.  Retail instead multiplies each zero term on its own
    // (`fmuls f12, f12, f31` on m.z.x at 0xa0(r1), then the -dist term by
    // `fmadds`, then m.x.x at 0x80(r1) by `fmadds ..., f31, ...`), seeding x
    // from m.z.x and y/z from m.x.c.  Accumulator statements pin that order.
    {
        const Vector3 v(0.0f, -dist, 0.0f);
        const Hmx::Matrix3 &m = lightXfm.m;
        float ox = m.z.x * v.z;
        ox += m.y.x * v.y;
        ox += m.x.x * v.x;
        float oy = m.x.y * v.x;
        oy += m.y.y * v.y;
        oy += m.z.y * v.z;
        float oz = m.x.z * v.x;
        oz += m.y.z * v.y;
        oz += m.z.z * v.z;
        offset.Set(ox, oy, oz);
    }
    Add(lightXfm.v, offset, lightXfm.v);

    sLightCam->SetWorldXfm(lightXfm);
    sLightCam->SetFrustum(nearPlane, farPlane, PI / 4.0f, 1.0f);
    sLightCam->Select();

    Rnd::Mode oldMode = TheRnd.DrawMode();
    TheRnd.SetDrawMode(Rnd::kDrawShadowDepth);
    draw->DrawShowing();
    TheRnd.SetDrawMode(oldMode);

    curCam->Select();

    static Hmx::Color sDefaultShadowColor(0.0f, 0.0f, 0.0f, 0.0f);
    const Hmx::Color *shadowColor = &sDefaultShadowColor;
    if (light->GetType() == RndLight::kFloorSpot) {
        shadowColor = &light->GetColor();
    }

    Hmx::Color invertedColor = *shadowColor;
    invertedColor.red = 1.0f - invertedColor.red;
    invertedColor.green = 1.0f - invertedColor.green;
    invertedColor.blue = 1.0f - invertedColor.blue;

    TheRnd.SetShadowMap(sShadowTex, sLightCam, &invertedColor);
    return true;
}
