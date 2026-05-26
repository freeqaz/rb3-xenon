#include "rndobj/ShadowMap.h"
#include "Memory.h"
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
    if (GetGfxMode() != kNewGfx || sLightCam == NULL || sShadowTex == NULL)
        return false;

    RndEnviron *e = env != NULL ? env : RndEnviron::Current();

    RndLight *light = NULL;
    ObjPtrList<RndLight>::iterator it;
    for (it = e->LightsApprox().begin(); it != e->LightsApprox().end(); ++it) {
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
        const Transform &lw = light->WorldXfm();
        lightXfm.m.y.z = sphere.center.z - lw.v.z;
        lightXfm.m.y.y = sphere.center.y - lw.v.y;
        lightXfm.m.y.x = sphere.center.x - lw.v.x;
        Normalize(lightXfm.m, lightXfm.m);
    }

    float tanHalfFov = (float)std::tan(PI / 8.0f);
    float dist = sphere.radius / tanHalfFov;
    float farPlane = sphere.radius + dist;
    float nearPlane = dist - sphere.radius;

    Vector3 offset;
    Multiply(Vector3(0.0f, -dist, 0.0f), lightXfm.m, offset);
    Add(lightXfm.v, offset, lightXfm.v);

    sLightCam->SetWorldXfm(lightXfm);
    sLightCam->SetFrustum(nearPlane, farPlane, PI / 4.0f, 1.0f);
    sLightCam->Select();

    Rnd::DrawMode oldMode = TheRnd.GetDrawMode();
    TheRnd.SetDrawMode(Rnd::kDrawExtrude);
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
