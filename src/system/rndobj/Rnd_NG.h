#pragma once
#include "OcclusionQueryMgr.h"
#include "math/Color.h"
#include "math/Geo.h"
#include "rndobj/Cam.h"
#include "rndobj/Flare.h"
#include "rndobj/Rnd.h"
#include "rndobj/ShaderOptions.h"
#include "rndobj/SoftParticleBuffer.h"
#include "rndobj/Tex.h"

struct LargeQuadRenderData;

class NgRnd : public Rnd {
public:
    // size 0x18
    struct Viewport {
        Viewport() : X(0), Y(0), Width(0), Height(0), MinZ(0), MaxZ(0) {}
        unsigned int X; // 0x0
        unsigned int Y; // 0x4
        unsigned int Width; // 0x8
        unsigned int Height; // 0xc
        float MinZ; // 0x10
        float MaxZ; // 0x14
    };
    struct RndPointTest {
        RndFlare *mFlare; // 0x0
        unsigned int mPointQueryIdx; // 0x4
        unsigned int mAreaQueryIdx; // 0x8
    };

    NgRnd();
    virtual ~NgRnd();

    virtual void PreInit();
    virtual void Init();
    virtual void ReInit();
    virtual void Terminate();
    virtual void Clear(unsigned int, const Hmx::Color &) {}
    virtual void SetShadowMap(RndTex *, RndCam *, const Hmx::Color *);
    virtual void RemovePointTest(RndFlare *);
    virtual RndTex *GetShadowMap() { return mShadowMap; }
    virtual RndCam *GetShadowCam() { return mShadowCam; }
    virtual void DoPostProcess();

    virtual void SetViewport(const Viewport &v) { mViewport = v; }
    virtual const Viewport &GetViewport() const { return mViewport; }
    virtual void
    DrawRect(const Hmx::Rect &, RndMat *, ShaderType, const Hmx::Color &, const Hmx::Color *, const Hmx::Color *) {
    }
    virtual void DrawRectDepth(
        const Vector3 &, const Vector3 (&)[4], const Vector4 &, RndMat *, ShaderType
    ) {}
    virtual bool Offscreen() const { return false; }
    virtual RndTex *PreProcessTexture() { return nullptr; } // 0x12c
    virtual RndTex *PostProcessTexture() { return nullptr; }
    virtual RndTex *PreDepthTexture() { return nullptr; }
    virtual void Suspend() {}
    virtual void Resume() {}
    virtual RndSoftParticleBuffer *ParticleBuffer() { return mParticleBuffer; }
    virtual void CreateLargeQuad(int, int, LargeQuadRenderData &);
    virtual void
    DrawLargeQuad(const LargeQuadRenderData &, const Transform &, RndMat *, ShaderType);
    virtual void SetVertShaderTex(RndTex *, int);

protected:
    virtual void ResetStats();
    virtual float UpdateOverlay(RndOverlay *, float);

    Viewport mViewport; // 0x1e0
    bool mLowRes;
    RndTex *mShadowMap; // 0x1fc
    RndCam *mShadowCam; // 0x200
    RndOcclusionQueryMgr *mOcclusionQueryMgr; // 0x204
    RndSoftParticleBuffer *mParticleBuffer; // 0x208
    std::vector<RndPointTest> mPointTestQueries; // 0x20c
    bool mInited; // 0x218
};

extern NgRnd &TheNgRnd;
