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
        // w16-a: a plain indexed loop; MSVC derives the image's
        // `boneFloats - dst` cursor itself (canonical unchanged, 96.296; the
        // old spelling truncated both pointers through `int`).
        float *dst = (float *)&unk1f40[startIndex * 12];
        for (unsigned int i = 0; i < numBones * 12; i++) {
            dst[i] = boneFloats[i];
        }

        // Store mesh pointers (one per bone slot) and per-bone indices.
        //
        // w7-bt (82.2 -> 96.3 canonical): the image's mesh-pointer loop is a
        // counted `for` -- it carries the CTR pass's own dead zero guard
        // (0x826B1658 `cmplwi r6, 0x0` / 0x826B165C `beq` INTO the index
        // loop's body, unreachable because the 0x826B1644 `cmplwi cr6` guard
        // already excluded zero) where a do/while gets no guard at all.  The
        // index loop is the `subic.`/`bne` down-counter (0x826B1670), which it
        // only stays if `m` is declared BEFORE the `for` (declared between the
        // loops it takes the CTR instead, 90.6); the post-increment `*indices++`
        // keeps the strength-reducer's own `subi r8, r10, 0x4` (0x826B164C)
        // where an explicit `--indices` folds into `addi ..., 0x658f`.
        // Refuted: two `for`s in one `if` (83.2, loop 2 takes CTR with an
        // unfolded guard), a pointer-compare `for` (68.7, never CTR),
        // `while (m--)` (90.6), indexing `mMeshPtrs[startIndex + i]` with no
        // local (85.6).
        // RESIDUAL (w7-bt, 96.3 canonical): the image schedules that `subi r8`
        // before the mesh-pointer loop and we sink it to the index loop's
        // preheader (1 insert + 1 delete), and r9/r10/r11 rotate from the
        // 0x826B15F4 reload of unk1b580 onward.
        {
            int *indices = &unk19640[startIndex];
            const RndMesh **meshPtrs = &mMeshPtrs[startIndex];
            int idx = 0;
            if (numBones != 0) {
                unsigned int m = numBones;
                for (unsigned int i = 0; i < numBones; i++) {
                    meshPtrs[i] = mesh;
                }
                do {
                    *indices++ = idx++;
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
    MILO_ASSERT(mesh->Showing(), 0x124);
    MILO_ASSERT(TheRnd.DrawMode() == Rnd::kDrawVelocity, 0x125);

    RndMat *mat = mesh->Mat();
    if (mat != nullptr && mat->GetZMode() != kZModeTransparent) {
        mesh->mMotionCache.mShouldCache = true;
        // The current-frame index is derived from the previous-frame one
        // (two xori), not read back from mActiveXfmCacheIndex.
        unsigned int prevIdx = mActiveXfmCacheIndex ^ 1;
        unsigned int currIdx = prevIdx ^ 1;
        const RndXfmCache &prevCache = mXfmCaches[prevIdx];
        const RndXfmCache &currCache = mXfmCaches[currIdx];
        // NumBones() is read once, after the two cache references: retail
        // forms the currCache address before the size/0x4c divw.  The raw
        // count gates the bone limit below and the clamped one sizes the
        // constant uploads.
        int rawBones = mesh->NumBones();
        unsigned int prevKey = mesh->mMotionCache.mCacheKey[prevIdx];
        unsigned int currKey = mesh->mMotionCache.mCacheKey[currIdx];
        int numBones = Max(1, rawBones);

        const float *prevFloats;
        if (prevCache.GetXfms(mesh, prevKey, numBones, prevFloats)) {
            const float *currFloats;
            if (currCache.GetXfms(mesh, currKey, numBones, currFloats)) {
                if (rawBones <= 40) {
                    TheShaderMgr.SetMeshInfo(rawBones, false);
                    RndShader::SelectConfig(mMat, kVelocityObjectShader, false);
                    TheShaderMgr.SetVConstant((VShaderConstant)9, prevFloats, numBones * 3);
                    TheShaderMgr.SetVConstant((VShaderConstant)0x81, currFloats, numBones * 3);
                    TheShaderMgr.SetVConstant((VShaderConstant)0, unk36bec[prevIdx]);
                    TheShaderMgr.SetVConstant((VShaderConstant)4, unk36bec[currIdx]);
                    TheShaderMgr.SetPConstant((PShaderConstant)8, (const Vector4 &)mDepthRangeValues);
                    // RB3 draws through the mesh's own DrawFaces slot (0x38).
                    mesh->DrawFaces();
#ifdef HX_NATIVE
                    TheNgStats->mMotionBlurs++;
#endif
                } else {
                    // Retail keeps only the PathName(mesh) call of this notify.
                    const char *path = PathName(mesh);
                    MILO_NOTIFY_ONCE(
                        "%s (%s): Has too many bones to apply object motion blur (%d bones of max %d)",
                        mesh->Name(), path, rawBones, 40
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
