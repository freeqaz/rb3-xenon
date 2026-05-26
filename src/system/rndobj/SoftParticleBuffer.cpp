#include "rndobj/SoftParticleBuffer.h"
#include "Rnd_NG.h"
#include "math/Vec.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rnddx9/RenderState.h"
#include "rndobj/Cam.h"
#include "rndobj/Rnd.h"
#include "rndobj/ShaderMgr.h"
#include "rndobj/Tex.h"

RndSoftParticleBuffer::RndSoftParticleBuffer() : unk38(4), mSoftParticleDrawList(this) {
    for (int i = 0; i < 2; i++) {
        mSurfaces[i] = nullptr;
    }
    unsigned int w = TheNgRnd.Width() >> 2;
    unsigned int h = TheNgRnd.Height() >> 2;
    AllocateData(w, h, TheNgRnd.Bpp());
}

RndSoftParticleBuffer::~RndSoftParticleBuffer() { FreeData(); }

void RndSoftParticleBuffer::FreeData() {
    TheNgRnd.UnregisterPostProcessor(this);
    for (int i = 0; i < 2; i++) {
        RELEASE(mSurfaces[i]);
    }
}

void RndSoftParticleBuffer::AllocateData(
    unsigned int w, unsigned int h, unsigned int bpp
) {
    if (w && h && bpp) {
        for (int i = 0; i < 2; i++) {
            MILO_ASSERT(mSurfaces[i] == NULL, 0xC6);
            mSurfaces[i] = Hmx::Object::New<RndTex>();
            mSurfaces[i]->SetBitmap(w, h, bpp, RndTex::kRenderedNoZ, false, nullptr);
        }
    }
    TheNgRnd.RegisterPostProcessor(this);
}

void RndSoftParticleBuffer::BlurSurface() {
    RndMat *workMat = TheShaderMgr.GetWork();
    workMat->MarkDirty(2);
    workMat->SetBlend(BaseMaterial::kBlendSrc);
    workMat->SetZMode(kZModeDisable);
    workMat->SetTexWrap(kTexWrapClamp);

    static float kBlurOffsets[10] = {
        -1.5f, 0.25f,
        -0.5f, 0.3f,
         0.5f, 0.25f,
         1.5f, 0.1f,
         2.5f, 0.0f
    };

    for (unsigned int pass = 0; pass < 2; pass++) {
        RndTex *srcTex = mSurfaces[(pass - 1) & 1];
        RndTex *dstTex = mSurfaces[pass & 1];

        workMat->SetDiffuseTex(srcTex);
        workMat->MarkDirty(2);

        float texW = (float)(long long)srcTex->Width();
        float texH = (float)(long long)srcTex->Height();
        float invW = 1.0f / texW;
        float invH = 1.0f / texH;

        for (int i = 0; i < 5; i++) {
            float weight = kBlurOffsets[i * 2 + 0];
            float offset = kBlurOffsets[i * 2 + 1];

            float scaleU = (pass & 1) ? 0.5f : offset;
            float scaleV = (pass & 1) ? offset : 0.5f;

            Vector4 uvScale(scaleU * invW, scaleV * invH, 1.0f, 1.0f);
            TheShaderMgr.SetPConstant((PShaderConstant)(0x8a + i), uvScale);

            Vector4 uvWeight(weight, weight, weight, weight);
            TheShaderMgr.SetPConstant((PShaderConstant)(0x9a + i), uvWeight);
        }

        TheShaderMgr.SetNumTaps(5);
        Hmx::Rect rect(0.0f, 0.0f, (float)dstTex->Width(), (float)dstTex->Height());
        dstTex->MakeDrawTarget();
        TheNgRnd.DrawRect(rect, workMat, kBlurShader, Hmx::Color(1, 1, 1), nullptr, nullptr);
        TheShaderMgr.SetNumTaps(1);
        dstTex->FinishDrawTarget();
    }
}

void RndSoftParticleBuffer::DoPost() {
    ((bool *)&TheShaderMgr)[0x3f] = false;
    if (!mSoftParticleDrawList.empty()) {
        if (TheNgRnd.PreDepthTexture() != nullptr && mSurfaces[0]) {
            RndCam *curCam = RndCam::Current();
            RndCam *cam = TheRnd.mWorldCamCopy;
            cam->SetTargetTex(mSurfaces[0]);
            cam->Select();
            Rnd::DrawMode savedMode = TheRnd.GetDrawMode();
            TheRnd.mDrawMode = (Rnd::DrawMode)7;
            TheShaderMgr.SetPConstant((PShaderConstant)9, TheNgRnd.PreDepthTexture());
            TheRenderState.SetTextureFilter(9, (RndRenderState::FilterMode)0, false);
            TheRenderState.SetTextureClamp(9, (RndRenderState::ClampMode)2);
            Vector4 depthRange;
            cam->GetDepthRangeValues(depthRange);
            TheShaderMgr.SetPConstant((PShaderConstant)0x59, depthRange);
            FOREACH(it, mSoftParticleDrawList) {
                (*it)->Draw();
            }
            TheRnd.mDrawMode = savedMode;
            cam->SetTargetTex(nullptr);
            curCam->Select();
            BlurSurface();
            TheShaderMgr.unk3f = true;
            TheShaderMgr.SetPConstant(kPS_EnvironMap, mSurfaces[0]);
            TheRenderState.SetTextureFilter(kPS_EnvironMap, (RndRenderState::FilterMode)1, false);
            TheRenderState.SetTextureClamp(kPS_EnvironMap, (RndRenderState::ClampMode)2);
        }
    }
    while (!mSoftParticleDrawList.empty()) {
        mSoftParticleDrawList.pop_back();
    }
}

void RndSoftParticleBuffer::Queue(RndDrawable *drawable, BaseMaterial::Blend blend) {
    if (blend != (BaseMaterial::Blend)unk38) return;
    Hmx::Object *target = static_cast<Hmx::Object *>(drawable);
    ObjPtrList<RndDrawable>::iterator found;
    ObjPtrList<RndDrawable>::iterator it;
    for (it = mSoftParticleDrawList.begin(); it != mSoftParticleDrawList.end(); ++it) {
        if (static_cast<Hmx::Object *>(*it) == target) {
            found = it;
            goto check;
        }
    }
    found = ObjPtrList<RndDrawable>::iterator(0);
check:
    if (!found) {
        mSoftParticleDrawList.insert(mSoftParticleDrawList.end(), drawable);
    }
}
