#include "rndobj/TexBlender.h"
#include "Utl.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Draw.h"
#include "rndobj/Mat.h"
#include "rndobj/Mesh.h"
#include "rndobj/Tex.h"
#include "rndobj/ShaderMgr.h"
#include "rndobj/Cam.h"
#include "rndobj/Shader.h"
#include "rndobj/Rnd_NG.h"
#include "rndobj/PostProc.h"
#include <algorithm>

struct BlendSorter {
    bool operator()(
        const std::pair<RndTexBlendController *, float> &a,
        const std::pair<RndTexBlendController *, float> &b
    ) const {
        return a.second < b.second;
    }
};

#pragma region Hmx::Object

RndTexBlender::RndTexBlender()
    : mBaseMap(this), mNearMap(this), mFarMap(this), mOutputTextures(this),
      mControllerList(this), mOwner(this), mControllerInfluence(1), mRenderedStates(0),
      unkc0(true) {}

BEGIN_HANDLERS(RndTexBlender)
    HANDLE(get_render_textures, OnGetRenderTextures)
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(RndTexBlender)
    SYNC_PROP(base_map, mBaseMap)
    SYNC_PROP(near_map, mNearMap)
    SYNC_PROP(far_map, mFarMap)
    SYNC_PROP(output_texture, mOutputTextures)
    SYNC_PROP(controller_list, mControllerList)
    SYNC_PROP(owner, mOwner)
    SYNC_PROP(controller_influence, mControllerInfluence)
    SYNC_SUPERCLASS(RndDrawable)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(RndTexBlender)
    SAVE_REVS(2, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndDrawable)
    bs << mOutputTextures;
    bs << mBaseMap;
    bs << mNearMap;
    bs << mFarMap;
    bs << mControllerList;
    bs << mOwner;
    bs << mControllerInfluence;
END_SAVES

BEGIN_COPYS(RndTexBlender)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndDrawable)
    CREATE_COPY(RndTexBlender)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mOutputTextures)
        COPY_MEMBER(mBaseMap)
        COPY_MEMBER(mNearMap)
        COPY_MEMBER(mFarMap)
        COPY_MEMBER(mControllerList)
        COPY_MEMBER(mOwner)
        COPY_MEMBER(mControllerInfluence)
    END_COPYING_MEMBERS
    mRenderedStates = 0;
END_COPYS

INIT_REVS(2, 0)

#ifndef HX_NATIVE
// Retail (0x8248B388) splits the revision into a file-static {altRev, rev}
// pair and reads straight off `bs` (no BinStreamRev).
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_TexBlender;
BEGIN_LOADS(RndTexBlender)
    int rev;
    bs >> rev;
    gRevs_TexBlender.rev = getHmxRev(rev);
    gRevs_TexBlender.altRev = getAltRev(rev);
    Hmx::Object::Load(bs);
    RndDrawable::Load(bs);
    bs >> mOutputTextures;
    bs >> mBaseMap;
    bs >> mNearMap;
    bs >> mFarMap;
    bs >> mControllerList;
    bs >> mOwner;
    if (gRevs_TexBlender.rev > 1)
        bs >> mControllerInfluence;
    else
        mControllerInfluence = 0.7071068f;
    mRenderedStates = 0;
END_LOADS
#else
BEGIN_LOADS(RndTexBlender)
    LOAD_REVS(bs);
    ASSERT_REVS(2, 0);
    Hmx::Object::Load(bs);
    RndDrawable::Load(bs);
    bs >> mOutputTextures;
    bs >> mBaseMap;
    bs >> mNearMap;
    bs >> mFarMap;
    bs >> mControllerList;
    bs >> mOwner;
    if (d.rev > 1)
        bs >> mControllerInfluence;
    else
        mControllerInfluence = 0.7071068f;
    mRenderedStates = 0;
END_LOADS
#endif

#pragma endregion
#pragma region RndDrawable

float RndTexBlender::GetDistanceToPlane(const Plane &plane, Vector3 &vec) {
    if (mOwner) {
        return mOwner->GetDistanceToPlane(plane, vec);
    } else
        return 0;
}

bool RndTexBlender::MakeWorldSphere(Sphere &sphere, bool b) {
    if (mOwner) {
        return mOwner->MakeWorldSphere(sphere, b);
    } else
        return false;
}

// Retail compiles these notifies out but still evaluates their non-inline
// arguments (the PathName calls), so the match build sinks them into
// MiloStripEval; native keeps the real notify.
#ifdef HX_NATIVE
#define TEXBLENDER_NOTIFY MILO_NOTIFY_ONCE
#else
#define TEXBLENDER_NOTIFY(...) MiloStripEval(__VA_ARGS__)
#endif

void RndTexBlender::DrawShowing() {
    if (TheRnd.DrawMode() != Rnd::kDrawNormal)
        return;

    if (!(TheRnd.ProcCmds() & kProcessWorld) && TheRnd.ProcCmds() != kProcessNone)
        return;

    if (!mOutputTextures)
        return;

    if ((mOutputTextures->GetType() & RndTex::kRenderedNoZ) != RndTex::kRenderedNoZ) {
        TEXBLENDER_NOTIFY(
            "%s: \"%s\" must be renderable with no z-buffer",
            PathName(this),
            mOutputTextures->Name()
        );
        return;
    }

    if (mOutputTextures->Height() * mOutputTextures->Width() > 0x40000) {
        TEXBLENDER_NOTIFY(
            "%s: \"%s\" is %d x %d, must be no larger than 512 x 512",
            PathName(this),
            mOutputTextures->Name(),
            mOutputTextures->Height(),
            mOutputTextures->Width()
        );
    }

    std::vector<std::pair<RndTexBlendController *, float> > nearList;
    std::vector<std::pair<RndTexBlendController *, float> > farList;
    std::vector<std::pair<RndTexBlendController *, float> > customList;

    float influence = mControllerInfluence;
    for (ObjPtrList<RndTexBlendController>::iterator it = mControllerList.begin();
         it != mControllerList.end();
         ++it) {
        RndTexBlendController *ctrl = *it;
        float blendAmount;
        RndTexBlendController::BlendState state = ctrl->GetBlendState(blendAmount, influence);
        switch (state) {
        case RndTexBlendController::kBlendNear:
            nearList.push_back(std::pair<RndTexBlendController *, float>(ctrl, blendAmount));
            break;
        case RndTexBlendController::kBlendFar:
            farList.push_back(std::pair<RndTexBlendController *, float>(ctrl, blendAmount));
            break;
        case RndTexBlendController::kBlendCustom:
            customList.push_back(std::pair<RndTexBlendController *, float>(ctrl, blendAmount));
            break;
        }
    }

    if (!unkc0 && nearList.empty() && farList.empty() && customList.empty()
        && mRenderedStates == 1) {
        return;
    }

    unkc0 = false;
    RndCam *cam = TheRnd.GetDefaultCam();
    RndCam *savedCam = RndCam::Current();

    RndTex *busyTex = savedCam->TargetTex();
    if (busyTex) {
        TEXBLENDER_NOTIFY(
            "%s: Cannot render to texture (%s) while already rendering to texture (%s).",
            PathName(busyTex),
            PathName(this),
            PathName(busyTex)
        );
    }

    cam->SetTargetTex(mOutputTextures);
    cam->Select();

    if (mBaseMap) {
        RndMat *mat = TheShaderMgr.GetWork();
        SetupMaterial(mat, mBaseMap);
        mat->SetAlpha(1.0f);
        TheNgRnd.DrawRect(
            Hmx::Rect(
                0.0f, 0.0f, (float)mOutputTextures->Width(), (float)mOutputTextures->Height()
            ),
            mat,
            kDrawRectShader,
            Hmx::Color(1.0f, 1.0f, 1.0f, 1.0f),
            nullptr,
            nullptr
        );
        mRenderedStates = 1;
    }

    std::sort(nearList.begin(), nearList.end(), BlendSorter());
    std::sort(farList.begin(), farList.end(), BlendSorter());

    // The near and far passes have DrawBlendList's shape: no mesh null test,
    // and the faces are drawn through each mesh's geometry owner.
    RndTex *nearTex = mNearMap;
    if (nearTex && !nearList.empty()) {
        mRenderedStates |= kTexNear;
        RndMat *mat = TheShaderMgr.GetWork();
        Transform xfm;
        xfm.Reset();
        TheShaderMgr.SetVConstant(kVS_ViewProjMatrix, Hmx::Matrix4(xfm));
        TheShaderMgr.SetTransform(xfm);
        SetupMaterial(mat, nearTex);
        mat->SetBlend(RndMat::kBlendSrcAlpha);

        float lastAlpha = -1.0f;
        for (std::vector<std::pair<RndTexBlendController *, float> >::iterator it =
                 nearList.begin();
             it != nearList.end();
             ++it) {
            float alpha = it->second;
            RndTexBlendController *ctrl = it->first;
            if (alpha != lastAlpha) {
                mat->SetAlpha(alpha);
                RndShader::SelectConfig(mat, kUnwrapUVShader, false);
                lastAlpha = alpha;
            }
            RndMesh *mesh = ctrl->Mesh();
            if (mesh->IsSkinned()) {
                TEXBLENDER_NOTIFY(
                    "%s: \"%s\" should not be a skinned mesh", PathName(this), mesh->Name()
                );
            }
            mesh->GetGeomOwner()->DrawFaces();
        }
        mat->SetAlpha(1.0f);
        if (RndCam::Current()) {
            TheShaderMgr.SetVConstant(
                kVS_ViewProjMatrix, RndCam::Current()->GetViewProjMatrix()
            );
        }
    }

    RndTex *farTex = mFarMap;
    if (farTex && !farList.empty()) {
        mRenderedStates |= kTexFar;
        RndMat *mat = TheShaderMgr.GetWork();
        Transform xfm;
        xfm.Reset();
        TheShaderMgr.SetVConstant(kVS_ViewProjMatrix, Hmx::Matrix4(xfm));
        TheShaderMgr.SetTransform(xfm);
        SetupMaterial(mat, farTex);
        mat->SetBlend(RndMat::kBlendSrcAlpha);

        float lastAlpha = -1.0f;
        for (std::vector<std::pair<RndTexBlendController *, float> >::iterator it =
                 farList.begin();
             it != farList.end();
             ++it) {
            float alpha = it->second;
            RndTexBlendController *ctrl = it->first;
            if (alpha != lastAlpha) {
                mat->SetAlpha(alpha);
                RndShader::SelectConfig(mat, kUnwrapUVShader, false);
                lastAlpha = alpha;
            }
            RndMesh *mesh = ctrl->Mesh();
            if (mesh->IsSkinned()) {
                TEXBLENDER_NOTIFY(
                    "%s: \"%s\" should not be a skinned mesh", PathName(this), mesh->Name()
                );
            }
            mesh->GetGeomOwner()->DrawFaces();
        }
        mat->SetAlpha(1.0f);
        if (RndCam::Current()) {
            TheShaderMgr.SetVConstant(
                kVS_ViewProjMatrix, RndCam::Current()->GetViewProjMatrix()
            );
        }
    }

    DrawBlendList(customList, kTexCustom);

    cam->SetTargetTex(nullptr);
    savedCam->Select();
}

#pragma endregion
#pragma region RndTexBlender

RndMat *RndTexBlender::SetupMaterial(RndMat *mat, RndTex *tex) {
    mat->SetZMode(kZModeDisable);
    mat->SetBlend(RndMat::kBlendSrc);
    // No cull setting: retail stores only ZMode, Blend and TexWrap here.
    mat->SetTexWrap(kTexWrapClamp);
    mat->SetDiffuseTex(tex);
    return mat;
}

void RndTexBlender::DrawBlendList(
    const std::vector<std::pair<RndTexBlendController *, float> > &list,
    TexState state
) {
    // Retail picks mNearMap only for kTexNear; every other state (including
    // kTexCustom, the only caller's state) reads mFarMap.
    RndTex *texmap = (state == kTexNear) ? mNearMap : mFarMap;

    if ((texmap || state == kTexCustom) && !list.empty()) {
        mRenderedStates |= state;

        RndMat *mat = TheShaderMgr.GetWork();
        Transform xfm;
        xfm.Reset();
        TheShaderMgr.SetVConstant(kVS_ViewProjMatrix, Hmx::Matrix4(xfm));
        TheShaderMgr.SetTransform(xfm);
        SetupMaterial(mat, texmap);
        mat->SetBlend(RndMat::kBlendSrcAlpha);

        float lastAlpha = -1.0f;
        for (std::vector<std::pair<RndTexBlendController *, float> >::const_iterator it =
                 list.begin();
             it != list.end();
             ++it) {
            RndTexBlendController *controller = it->first;
            float alpha = it->second;
            if (state == kTexCustom) {
                mat->SetDiffuseTex(controller->Tex());
            }
            if (alpha != lastAlpha || state == kTexCustom) {
                mat->SetAlpha(alpha);
                RndShader::SelectConfig(mat, kUnwrapUVShader, false);
                lastAlpha = alpha;
            }
            // No null test on the mesh, and the faces are drawn through the
            // geometry owner (retail: vcall slot 0x38 on mesh+0x110).
            RndMesh *mesh = controller->Mesh();
            if (mesh->IsSkinned()) {
                TEXBLENDER_NOTIFY(
                    "%s: \"%s\" should not be a skinned mesh", PathName(this), mesh->Name()
                );
            }
            mesh->GetGeomOwner()->DrawFaces();
        }
        mat->SetAlpha(1.0f);
        if (RndCam::Current()) {
            TheShaderMgr.SetVConstant(
                kVS_ViewProjMatrix, RndCam::Current()->GetViewProjMatrix()
            );
        }
    }
}

#undef TEXBLENDER_NOTIFY

DataNode RndTexBlender::OnGetRenderTextures(DataArray *) {
    return GetRenderTexturesNoZ(Dir());
}

// sw2 scatter-include (default/TexBlender <- rndobj/AmbientOcclusion.cpp)
#define gRev gRev_AmbientOcclusion
#define gAltRev gAltRev_AmbientOcclusion
#include "rndobj/AmbientOcclusion.cpp"
#undef gRev
#undef gAltRev
