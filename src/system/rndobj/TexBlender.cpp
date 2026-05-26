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
    SYNC_SUPERCLASS(Hmx::Object)
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

void RndTexBlender::DrawShowing() {
    if (TheRnd.GetDrawMode() != Rnd::kDrawNormal)
        return;

    ProcessCmd cmds = TheRnd.ProcCmds();
    if (!(cmds & kProcessWorld)) {
        if (cmds != kProcessNone)
            return;
    }

    RndTex *outputTex = mOutputTextures;
    if (!outputTex)
        return;

    if ((outputTex->GetType() & RndTex::kRenderedNoZ) != RndTex::kRenderedNoZ) {
        MILO_NOTIFY_ONCE(
            "%s: \"%s\" must be renderable with no z-buffer",
            PathName(this),
            outputTex->Name()
        );
        return;
    }

    if (outputTex->Width() * outputTex->Height() > 0x40000) {
        MILO_NOTIFY_ONCE(
            "%s: \"%s\" is %d x %d, must be no larger than 512 x 512",
            PathName(this),
            outputTex->Name(),
            outputTex->Height(),
            outputTex->Width()
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

    if (savedCam->TargetTex()) {
        MILO_NOTIFY_ONCE(
            "%s: Cannot render to texture (%s) while already rendering to texture (%s).",
            PathName(savedCam->TargetTex()),
            PathName(this),
            PathName(savedCam->TargetTex())
        );
    }

    cam->SetTargetTex(outputTex);
    cam->Select();

    if (mBaseMap) {
        RndMat *mat = TheShaderMgr.GetWork();
        SetupMaterial(mat, mBaseMap);
        mat->SetAlpha(1.0f);
        Hmx::Rect rect(0.0f, 0.0f, (float)outputTex->Width(), (float)outputTex->Height());
        Hmx::Color color(1.0f, 1.0f, 1.0f, 1.0f);
        TheNgRnd.DrawRect(rect, mat, kDrawRectShader, color, nullptr, nullptr);
        mRenderedStates = 1;
    }

    std::sort(nearList.begin(), nearList.end(), BlendSorter());
    std::sort(farList.begin(), farList.end(), BlendSorter());

    RndTex *nearTex = mNearMap;
    if (nearTex && !nearList.empty()) {
        mRenderedStates |= kTexNear;
        RndMat *mat = TheShaderMgr.GetWork();

        Transform xfm;
        xfm.Reset();
        Hmx::Matrix4 viewProjMtx(xfm);
        TheShaderMgr.SetVConstant(kVS_ViewProjMatrix, viewProjMtx);
        TheShaderMgr.SetTransform(xfm);
        SetupMaterial(mat, nearTex);
        mat->SetBlend(BaseMaterial::kBlendSrcAlpha);

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
                MILO_NOTIFY_ONCE(
                    "%s: \"%s\" should not be a skinned mesh",
                    PathName(this),
                    mesh->Name()
                );
            }
            mesh->DrawFacesInRange(0, -1);
        }
        mat->SetAlpha(1.0f);
        RndCam *cur = RndCam::Current();
        if (cur) {
            TheShaderMgr.SetVConstant(kVS_ViewProjMatrix, cur->GetViewProjMatrix());
        }
    }

    RndTex *farTex = mFarMap;
    if (farTex && !farList.empty()) {
        mRenderedStates |= kTexFar;
        RndMat *mat = TheShaderMgr.GetWork();

        Transform xfm;
        xfm.Reset();
        Hmx::Matrix4 viewProjMtx(xfm);
        TheShaderMgr.SetVConstant(kVS_ViewProjMatrix, viewProjMtx);
        TheShaderMgr.SetTransform(xfm);
        SetupMaterial(mat, farTex);
        mat->SetBlend(BaseMaterial::kBlendSrcAlpha);

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
                MILO_NOTIFY_ONCE(
                    "%s: \"%s\" should not be a skinned mesh",
                    PathName(this),
                    mesh->Name()
                );
            }
            mesh->DrawFacesInRange(0, -1);
        }
        mat->SetAlpha(1.0f);
        RndCam *cur = RndCam::Current();
        if (cur) {
            TheShaderMgr.SetVConstant(kVS_ViewProjMatrix, cur->GetViewProjMatrix());
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
    mat->SetCull(kCullNone);
    mat->SetTexWrap(kTexWrapClamp);
    mat->SetDiffuseTex(tex);
    return mat;
}

void RndTexBlender::DrawBlendList(
    const std::vector<std::pair<RndTexBlendController *, float> > &list,
    TexState state
) {
    RndTex *texmap = (state != 2) ? mNearMap : mFarMap;

    bool texValid = (texmap != nullptr);
    if ((texValid || (state == 8)) && (!list.empty())) {
        mRenderedStates |= state;

        RndMat *mat = TheShaderMgr.GetWork();
        float f31 = 1.0f;
        float f29 = -1.0f;

        Transform xfm;
        xfm.Reset();
        Hmx::Matrix4 viewProjMtx = Hmx::Matrix4(xfm);
        TheShaderMgr.SetVConstant(kVS_ViewProjMatrix, viewProjMtx);
        TheShaderMgr.SetTransform(xfm);
        SetupMaterial(mat, texmap);

        mat->SetBlend(BaseMaterial::kBlendSrcAlpha);

        for (std::vector<std::pair<RndTexBlendController *, float> >::const_iterator it =
                 list.begin();
             it != list.end();
             ++it) {
            RndTexBlendController *controller = it->first;
            float alpha = it->second;

            if (state == 8) {
                mat->SetDiffuseTex(controller->Tex());
            }

            if (alpha != f29 || state == 8) {
                mat->SetAlpha(alpha);
                RndShader::SelectConfig(mat, (ShaderType)0x16, false);
                f29 = alpha;
            }

            RndMesh *mesh = controller->Mesh();
            if (mesh) {
                if (mesh->IsSkinned()) {
                    MILO_NOTIFY_ONCE(
                        "%s: \"%s\" should not be a skinned mesh",
                        PathName(this),
                        mesh->Name()
                    );
                }
                mesh->DrawFacesInRange(0, -1);
            }
        }

        mat->SetAlpha(f31);

        RndCam *cam = RndCam::Current();
        if (cam) {
            TheShaderMgr.SetVConstant(kVS_ViewProjMatrix, cam->GetViewProjMatrix());
        }
    }
}

DataNode RndTexBlender::OnGetRenderTextures(DataArray *) {
    return GetRenderTexturesNoZ(Dir());
}
