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
    workMat->SetBlend(RndMat::kBlendSrc);
    workMat->SetZMode(kZModeDisable);
    workMat->SetTexWrap(kTexWrapClamp);

    for (unsigned int pass = 0; pass < 2; pass++) {
        // Retail 0x824A8490: the pass's render target is the surface the
        // previous pass sampled, and the surface indexed by the pass itself is
        // the one it reads -- so the second (vertical) pass lands in
        // mSurfaces[0], which DoPost binds. (Both were swapped here.)
        RndTex *dstTex = mSurfaces[(pass - 1) & 1];

        workMat->SetDiffuseTex(mSurfaces[pass & 1]);
        workMat->MarkDirty(2);

        dstTex->MakeDrawTarget();

        float texW = (float)(long long)dstTex->Width();
        float texH = (float)(long long)dstTex->Height();
        Hmx::Rect rect(0.0f, 0.0f, texW, texH);
        float invW = 1.0f / texW;
        float invH = 1.0f / texH;

        // (weight, offset) pairs for the five taps, read out of retail's table
        // (.data 0x82C70EC8 holds the leading 0.1; the other nine are stored on
        // first use): weights 0.1/0.25/0.3/0.25/0.1 sum to 1. The previous flat
        // table here had lost the leading 0.1, so every tap read its offset as
        // the weight and the next weight as the offset. DC3's table differs in
        // tap 0 (0.0, in its .bss).
        static Vector2 kBlurTaps[5] = {
            Vector2(0.1f, -1.5f),
            Vector2(0.25f, -0.5f),
            Vector2(0.3f, 0.5f),
            Vector2(0.25f, 1.5f),
            Vector2(0.1f, 2.5f)
        };

        for (int i = 0; i < 5; i++) {
            float weight = kBlurTaps[i].x;
            float offset = kBlurTaps[i].y;

            float scaleU, scaleV;
            if (!(pass & 1)) {
                scaleU = offset * invW;
                scaleV = invH * 0.5f;
            } else {
                scaleU = invW * 0.5f;
                scaleV = offset * invH;
            }

            // Retail feeds the taps to pixel-shader constants 0x1F+i (UV scale)
            // and 0x2F+i (weight); 0x8A/0x9A are DC3's register numbers.
            Vector4 uvScale(scaleU, scaleV, 1.0f, 1.0f);
            TheShaderMgr.SetPConstant((PShaderConstant)(0x1f + i), uvScale);

            Vector4 uvWeight(weight, weight, weight, weight);
            TheShaderMgr.SetPConstant((PShaderConstant)(0x2f + i), uvWeight);
        }

        TheShaderMgr.SetNumTaps(5);
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
            Rnd::Mode savedMode = TheRnd.DrawMode();
            TheRnd.mDrawMode = (Rnd::Mode)6;
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
    mSoftParticleDrawList.clear();
}

void RndSoftParticleBuffer::Queue(RndDrawable *drawable, RndMat::Blend blend) {
    if (blend != (RndMat::Blend)unk38) return;
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
        mSoftParticleDrawList.push_back(drawable);
    }
}
