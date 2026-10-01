#include "rndobj/VelocityBuffer.h"
#include "math/Mtx.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/BaseMaterial.h"
#include "rndobj/Cam.h"
#include "rndobj/Mat.h"
#include "rndobj/Tex.h"
#include "rndobj/Utl.h"
#include "rndobj/Rnd.h"
#include "math/Utl.h"
#include "rndobj/Rnd_NG.h"
#include "rndobj/ShaderMgr.h"
#include "rndobj/Shader.h"
#include "rndobj/Stats_NG.h"
#include "rnddx9/RenderState.h"

RndVelocityBuffer RndVelocityBuffer::sSingleton;

bool RndXfmCache::GetXfms(
    const RndMesh * __restrict mesh,
    unsigned int startIndex,
    unsigned int numBones,
    const float *&outFloats
) const {
    bool valid;
    const float *floats;
    unsigned int endIndex = startIndex + numBones;
    if ((endIndex > unk1b580)
        || (mMeshPtrs[endIndex - 1] != mesh)
        || (mMeshPtrs[startIndex] != mesh)) {
        floats = nullptr;
        valid = false;
    } else {
        valid = true;
        floats = (const float *)(&unk1f40[startIndex * 12]);
    }
    outFloats = floats;
    return valid;
}

bool RndXfmCache::CacheXfms(
    const RndMesh * __restrict mesh,
    const float * __restrict boneFloats,
    unsigned int numBones,
    unsigned int &outKey
) {
    outKey = 0xffffffff;
    if (unk1b580 + numBones <= 0x7d0) {
        outKey = unk1b580;
        int startIndex = unk1b580;

        // Copy bone transform floats (12 floats/ints per bone = 3 float4 rows)
        unsigned int totalFloats = numBones * 12;
        unsigned int count = 0;
        float *dst = (float *)&unk1f40[startIndex * 12];
        if (totalFloats != 0) {
            int diff = (int)boneFloats - (int)dst;
            do {
                count++;
                *dst = *(float *)((int)dst + diff);
                dst++;
            } while (count < totalFloats);
        }

        // Store mesh pointers (one per bone slot) and per-bone indices
        {
            int *indices = &unk19640[startIndex];
            const RndMesh **meshPtrs = &mMeshPtrs[startIndex];
            if (numBones != 0) {
                --indices;
                unsigned int m = numBones;
                --meshPtrs;
                int idx = 0;
                unsigned int n = numBones;
                do {
                    ++meshPtrs;
                    *meshPtrs = mesh;
                } while (--n != 0);
                do {
                    ++indices;
                    *indices = idx;
                    idx++;
                } while (--m != 0);
            }
        }

        unk1b580 = startIndex + numBones;
    }
    return outKey < 2000U;
}

RndVelocityBuffer::RndVelocityBuffer()
    : unk36be8(0), mActiveXfmCacheIndex(0), mFrame(0), mVelocityTex(nullptr), mMat(nullptr),
      mLastFrameCamera(nullptr) {
    memset(&mViewProjXfm, 0, 0xa4);
}

void RndVelocityBuffer::CacheCameraSettings(RndCam *camera) {
    MILO_ASSERT(camera, 0x88);
    Transform tfa0;
    Hmx::Matrix4 me0;
    camera->GetViewProjectXfms(tfa0, me0);
    mViewProjXfm = tfa0 * me0;
    camera->GetDepthRangeValues(mDepthRangeValues);
    camera->GetCamFrustum(mFrustumNear, (Vector3 (&)[4])mFrustumCorners);
    mCam = camera;
}

bool RndVelocityBuffer::AdvanceFrame(RndCam *cam) {
    mFrameAdvanced = false;
    mActiveXfmCacheIndex ^= 1;
    mFrame++;
    if (cam != mLastFrameCamera) {
        mLastFrameCamera = cam;
        mFrame = 0;
    }
    return (unsigned int)mFrame >= 2;
}

void RndVelocityBuffer::AllocateData(
    unsigned int ui1, unsigned int ui2, unsigned int ui3
) {
    MILO_ASSERT(mVelocityTex == NULL, 0x45);
    mVelocityTex = Hmx::Object::New<RndTex>();
    mVelocityTex->SetBitmap(ui1 / 2, ui2 / 2, ui3, RndTex::kRendered, false, nullptr);
    MILO_ASSERT(mMat == NULL, 0x4A);
    mMat = Hmx::Object::New<RndMat>();
    mMat->SetPerPixelLit(false);
    mMat->SetBlend(RndMat::kBlendSrc);
    mMat->SetZMode(kZModeDisable);
}

void RndVelocityBuffer::FreeData() {
    RELEASE(mVelocityTex);
    RELEASE(mMat);
}

void RndVelocityBuffer::ResetFrame() { mFrame = 0; }

void RndVelocityBuffer::CacheTransform(
    RndMesh * __restrict mesh,
    const float * __restrict boneFloats,
    unsigned int numBones
) {
    if ((TheRnd.ProcCmds() & kProcessWorld) > 0) {
        int cacheIdx = mActiveXfmCacheIndex;
        unsigned int outKey;
        bool ok = mXfmCaches[cacheIdx].CacheXfms(mesh, boneFloats, numBones, outKey);
        if (ok) {
            mesh->mMotionCache.mCacheKey[cacheIdx] = outKey;
        }
    }
}

void RndVelocityBuffer::DrawMesh(RndMesh *mesh) const {
    MILO_ASSERT(mesh, 0x123);
    auto _tmp2 = mesh->Showing();
    MILO_ASSERT(_tmp2, 0x124);
    MILO_ASSERT(TheRnd.DrawMode() == Rnd::kDrawVelocity, 0x125);

    RndMat *mat = mesh->Mat();
    if (mesh && mat != nullptr && mat->GetZMode() != kZModeTransparent) {
        auto _val0 = mXfmCaches;
        mesh->mMotionCache.mShouldCache = true;
        unsigned int cacheIdx = mActiveXfmCacheIndex;
        int numBones = mesh->NumBones();
        if (numBones <= 0) numBones = 1;

        unsigned int prevKey = mesh->mMotionCache.mCacheKey[cacheIdx ^ 1];

        const float *prevFloats = nullptr;
        unsigned char prevOk = (unsigned char)(_val0[cacheIdx ^ 1].GetXfms(mesh, prevKey, numBones, prevFloats));
        if (prevOk) {
            unsigned int currKey = mesh->mMotionCache.mCacheKey[cacheIdx];
            int numBonesActual = mesh->NumBones();
            const float *currFloats = nullptr;
            unsigned char currOk = (unsigned char)(_val0[cacheIdx].GetXfms(mesh, currKey, numBones, currFloats));
            if (currOk) {
                if (numBonesActual <= 40) {
                    TheShaderMgr.SetMeshInfo(numBonesActual, false);
                    RndShader::SelectConfig(mMat, kVelocityObjectShader, false);
                    TheShaderMgr.SetVConstant((VShaderConstant)9, prevFloats, numBones * 3);
                    TheShaderMgr.SetVConstant((VShaderConstant)0x81, currFloats, numBones * 3);
                    TheShaderMgr.SetVConstant((VShaderConstant)0, unk36bec[cacheIdx ^ 1]);
                    TheShaderMgr.SetVConstant((VShaderConstant)4, unk36bec[cacheIdx]);
                    TheShaderMgr.SetPConstant((PShaderConstant)8, (const Vector4 &)mDepthRangeValues);
                    mesh->GetGeomOwner()->DrawFaces();
#ifdef HX_NATIVE
                    TheNgStats->mMotionBlurs++;
#endif
                } else {
                    auto _tmp3 = PathName(mesh->Mat());
                    MILO_NOTIFY_ONCE(
                        "%s (%s): Has too many bones to apply object motion blur (%d bones of max %d)",
                        (char *)PathName(mesh), _tmp3, numBonesActual, 40
                    );
                }
            }
        }
    }
}

bool RndVelocityBuffer::Draw(RndCam *cam, ObjPtrList<RndDrawable> &drawList) {
    mFrameAdvanced = false;
    float splitMs = mTimer.SplitMs();
    mTimer.Restart();
    float scale = 41.666668f / (splitMs + 1.0f);
        unk36be8 = scale = Min(2.0f, scale);

    if (cam && cam == mCam) {
        mMat->SetBlend(RndMat::kBlendSrc);
        mMat->SetZMode(kZModeDisable);

        // Retail forms the previous-frame address before the memcpy and holds
        // it (r25) across every call below. Read the index INLINE in both
        // subscripts: a named index local makes MSVC sink prevXfm and
        // rematerialise it from callee-saved copies of the index and the
        // 0x36bec base (one extra saved register, 0xf0 frame). Same finding
        // as dc3-decomp's copy of this function (lane w7-as).
        ViewProjXfm &curXfm = unk36bec[mActiveXfmCacheIndex];
        ViewProjXfm &prevXfm = unk36bec[mActiveXfmCacheIndex ^ 1];
        memcpy(&curXfm, &mViewProjXfm, 0x40);
        // Retail advances the frame BEFORE asking for the pre-depth texture,
        // unconditionally (0x82b855f0: bl AdvanceFrame, then the vtable 0x110
        // PreDepthTexture call); the result is only published inside the branch.
        bool frameReady = AdvanceFrame(cam);
        RndTex *depthTex = TheNgRnd.PreDepthTexture();
        if (depthTex != nullptr) {
            cam->SetTargetTex(mVelocityTex);
            cam->Select();
            TheShaderMgr.SetPConstant((PShaderConstant)9, depthTex);
            TheRenderState.SetTextureFilter(9, (RndRenderState::FilterMode)0, false);
            TheRenderState.SetTextureClamp(9, (RndRenderState::ClampMode)2);
            TheShaderMgr.SetVConstant(kVS_ViewProjMatrix, mViewProjXfm);
            TheShaderMgr.SetPConstant((PShaderConstant)0x86, prevXfm);
            TheNgRnd.DrawRectDepth(
                mFrustumNear,
                (Vector3 (&)[4])mFrustumCorners,
                mDepthRangeValues,
                mMat,
                kVelocityCameraShader
            );

            auto _tmp0 = drawList.size();
            if (_tmp0 != 0) {
                Rnd::Mode savedDrawMode = TheRnd.DrawMode();
#ifdef HX_NATIVE
                TheRnd.SetDrawMode(Rnd::kDrawVelocity);
#else
                // RB3 retail stores 5: its Rnd::Mode has no DC3
                // kDrawOcclusionDepth, so velocity is 5, not 6 (same drift as
                // RndShader::SelectConfig's raw compare).
                TheRnd.SetDrawMode((Rnd::Mode)5);
#endif
                mMat->SetBlend((RndMat::Blend)3);
                mMat->SetZMode(kZModeNormal);
                auto _tmp1 = drawList.end();
                for (ObjPtrList<RndDrawable>::iterator it = drawList.begin();
                     it != _tmp1; ++it) {
                    // Retail calls the non-virtual RndDrawable::Draw (showing
                    // test + cull), not the DrawShowing vcall.
                    (*it)->Draw();
                }
                TheRnd.SetDrawMode(savedDrawMode);
            }

            cam->SetTargetTex(nullptr);
            mFrameAdvanced = frameReady;
        }
        mXfmCaches[mActiveXfmCacheIndex].unk1b580 = 0;
    }

    if (mFrameAdvanced) {
        TheShaderMgr.SetPConstant((PShaderConstant)10, mVelocityTex);
        TheRenderState.SetTextureFilter(10, (RndRenderState::FilterMode)1, false);
        TheRenderState.SetTextureClamp(10, (RndRenderState::ClampMode)2);
    } else {
        TheShaderMgr.SetPConstant((PShaderConstant)10, (RndTex *)nullptr);
    }
    return mFrameAdvanced;
}
