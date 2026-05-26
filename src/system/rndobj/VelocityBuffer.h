#pragma once
#include "rndobj/Cam.h"
#include "rndobj/Draw.h"
#include "rndobj/Mesh.h"
#include "rndobj/Mat.h"
#include "rndobj/Tex.h"

// size 0x1b584
class RndXfmCache {
    friend class RndVelocityBuffer;

    RndXfmCache() : mMeshPtrs(), unk1f40(), unk19640(), unk1b580(0) {}
    bool GetXfms(const RndMesh * __restrict, unsigned int, unsigned int, const float *&) const;
    bool CacheXfms(const RndMesh * __restrict, const float * __restrict, unsigned int, unsigned int &);

    const RndMesh *mMeshPtrs[2000]; // 0x0
    int unk1f40[24000]; // 0x1f40
    int unk19640[2000]; // 0x19640
    unsigned int unk1b580; // 0x1b580
};

// size 0x40 - wrapper around Vector4[4] to force __ehvec_ctor generation
// NO user-defined constructor — implicit default ctor constructs the Vector4 array
struct ViewProjXfm {
    Vector4 rows[4];
    operator const Hmx::Matrix4 &() const {
        return *(const Hmx::Matrix4 *)this;
    }
};

// size 0x36c84
class RndVelocityBuffer {
public:
    virtual ~RndVelocityBuffer() { FreeData(); }

    void CacheCameraSettings(RndCam *);
    void AllocateData(unsigned int, unsigned int, unsigned int);
    void FreeData();
    void ResetFrame();
    bool Draw(RndCam *, ObjPtrList<RndDrawable> &);
    void DrawMesh(RndMesh *) const;
    void CacheTransform(RndMesh * __restrict, const float * __restrict, unsigned int);

    static RndVelocityBuffer &Singleton() { return sSingleton; }

    float GetUnk36be8() { return unk36be8; };

private:
    RndVelocityBuffer();

    bool AdvanceFrame(RndCam *);

    static RndVelocityBuffer sSingleton;

    Hmx::Matrix4 mViewProjXfm; // 0x8
    Vector4 mDepthRangeValues; // 0x48
    Vector3 mFrustumNear; // 0x58
    PaddedJointPos mFrustumCorners[4]; // 0x68
    RndCam *mCam; // 0xa8
    RndXfmCache mXfmCaches[2]; // 0xac
    Timer mTimer; // 0x36bb8
    float unk36be8; // 0x36be8
    ViewProjXfm unk36bec[2]; // 0x36bec
    int mActiveXfmCacheIndex; // 0x36c6c - which xfmcache
    int mFrame; // 0x36c70 - frame
    RndTex *mVelocityTex; // 0x36c74
    RndMat *mMat; // 0x36c78
    RndCam *mLastFrameCamera; // 0x36c7c
    bool mFrameAdvanced; // 0x36c80
};
