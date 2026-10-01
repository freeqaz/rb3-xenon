#include "rnddx9/Part.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rnddx9/Rnd.h"
#include "math/Mtx.h"
#include "rndobj/Cam.h"
#include "rndobj/Shader.h"
#include "rndobj/ShaderMgr.h"
#include "rndobj/Stats_NG.h"
#include "xdk/d3d9i/d3d9.h"
#include "xdk/d3d9i/d3d9types.h"

DxParticleSys::DxParticleSys() {}

// Init (0x82740FF0), DrawParticles (0x82740A08, called only from DrawShowing)
// and DrawShowing (0x82740D30, DxParticleSys vtable slot 5).
D3DVertexDeclaration *DxParticleSys::sVertexDecl;

namespace {
    const D3DVERTEXELEMENT9 sParticleDecl[] = {
        {0, 0x00, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {0, 0x0C, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
        {0, 0x10, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
        D3DDECL_END()
    };
}

void DxParticleSys::Init() {
    REGISTER_OBJ_FACTORY(DxParticleSys)
    MILO_ASSERT(!sVertexDecl, 0x46);
    sVertexDecl = D3DDevice_CreateVertexDeclaration(sParticleDecl);
    DX_ASSERT(sVertexDecl, 0x47);
}

namespace {
    // One vertex as sParticleDecl above describes it: POSITION float3,
    // COLOR d3dcolor, TEXCOORD0 float4 -- 0x20 bytes (retail BeginVertices
    // stride, 0x82740A50). RB3 has no UV-tile animation, so no tile channel.
    // The four TEXCOORD0 slots carry different things depending on whether the
    // particles are velocity-aligned, and the non-aligned path deliberately
    // leaves the fourth one alone.
    struct ParticleVert {
        float x, y, z; // 0x00
        unsigned long color; // 0x0c
        float t0, t1, t2, t3; // 0x10
    };
}

void DxParticleSys::DrawParticles(const Hmx::Color &color) {
    if (mNumActive == 0)
        return;
    MILO_ASSERT(mActiveParticles, 0x227);
    int numActive = mNumActive;
    D3DDevice_SetVertexDeclaration(TheDxRnd.Device(), sVertexDecl);
    ParticleVert *vert = (ParticleVert *)D3DDevice_BeginVertices(
        TheDxRnd.Device(), D3DPT_QUADLIST, numActive * 4, sizeof(ParticleVert)
    );
    DX_ASSERT(vert, 0x22F);
    if (mAlignWithVelocity) {
        for (RndParticle *part = mActiveParticles; part; part = part->next) {
            Hmx::Color c(
                part->col.red * color.red,
                part->col.green * color.green,
                part->col.blue * color.blue,
                part->col.alpha * color.alpha
            );
            vert->x = part->pos.x;
            vert->y = part->pos.y;
            vert->z = part->pos.z;
            vert->color = MakeColor(c);
            vert->t0 = part->size;
            vert->t1 = part->vel.x * 2.0f;
            vert->t2 = part->vel.y * 2.0f;
            vert->t3 = part->vel.z * 2.0f;
            vert++;
        }
    } else {
        // Known residual for this function, 8 rows (99.980 canonical), every
        // one of them an ordering the backend picked and not a value.  Four of
        // them (rows 133/134/139/140) are the red and alpha products of THIS
        // loop's colour being issued in the opposite order: the image computes
        // red, green, alpha, blue; we compute alpha, green, red, blue.  The
        // other loop above is identical source and the image issues it green,
        // blue, red, alpha there -- so the image's own two expansions of one
        // statement disagree, which rules out an argument-order lever.
        // Refuted by build: hoisting the four products into named floats
        // written in the image's order (red, green, alpha, blue) and passing
        // them to the ctor is byte-inert, 8 rows before and after.  The
        // remaining four rows are a commutative fmuls (row 80), an r7/r9
        // volatile swap in the MakeColor byte packing (166/168) and a
        // commutative `add` in the NgStats update (193).
        for (RndParticle *part = mActiveParticles; part; part = part->next) {
            Hmx::Color c(
                part->col.red * color.red,
                part->col.green * color.green,
                part->col.blue * color.blue,
                part->col.alpha * color.alpha
            );
            vert->x = part->pos.x;
            vert->y = part->pos.y;
            vert->z = part->pos.z;
            vert->color = MakeColor(c);
            vert->t0 = part->size;
            vert->t1 = part->angle;
            vert->t2 = part->swingArm;
            vert++;
        }
    }
    D3DDevice_EndVertices(TheDxRnd.Device());
    TheNgStats->mParts += numActive;
    TheNgStats->mPartSys += (unsigned int)numActive != 0;
}

void DxParticleSys::DrawShowing() {
    RndParticleSys::DrawShowing();
    if (!mActiveParticles)
        return;
    Hmx::Color white;
    white.Set(1.0f, 1.0f, 1.0f, 1.0f);
    Hmx::Matrix3 mtx;
    Invert(mRelativeXfm.m, mtx);
    Multiply(RndCam::Current()->WorldXfm().m, mtx, mtx);
    // Half-extent basis: x/y span the quad, z is squashed by the screen aspect.
    mtx.x.x *= 0.5f;
    mtx.x.y *= 0.5f;
    mtx.x.z *= 0.5f;
    mtx.y.x *= 0.5f;
    mtx.y.y *= 0.5f;
    mtx.y.z *= 0.5f;
    mtx.z.x *= 0.5f;
    mtx.z.y *= 0.5f;
    mtx.z.z *= 0.5f;
    mtx.z.x *= mScreenAspect;
    mtx.z.y *= mScreenAspect;
    mtx.z.z *= mScreenAspect;

    bool fancy = mType == kFancy;
    bool aligned = fancy && mAlignWithVelocity;
    bool stretched = aligned && mStretchWithVelocity;
    bool constantArea = stretched && mConstantArea;

    // Each constant goes out through a NAMED Vector4 local, not a temporary:
    // a temporary makes MSVC evaluate the constructor arguments right to left
    // and hold them in FPRs until the whole vector is built, where the target
    // writes each field to its stack slot as it is computed.
    Vector4 stretchParams;
    stretchParams.x = aligned ? 1.0f : 0.0f;
    stretchParams.y = stretched ? 1.0f : 0.0f;
    stretchParams.z = constantArea ? 1.0f : 0.0f;
    stretchParams.w = mStretchScale * 2.0f;
    TheShaderMgr.SetVConstant((VShaderConstant)0x31, stretchParams);

    int noPerspectiveStretch = !(fancy && aligned && stretched && mPerspectiveStretch);
    Vector4 xAxis;
    xAxis.x = mtx.x.x;
    xAxis.y = mtx.x.y;
    xAxis.z = mtx.x.z;
    xAxis.w = noPerspectiveStretch;
    TheShaderMgr.SetVConstant((VShaderConstant)0x2f, xAxis);

    Vector4 zAxis;
    zAxis.x = mtx.z.x;
    zAxis.y = mtx.z.y;
    zAxis.z = mtx.z.z;
    zAxis.w = 1.0f;
    TheShaderMgr.SetVConstant((VShaderConstant)0x30, zAxis);

    TheShaderMgr.SetTransform(mRelativeXfm);
    RndShader::SelectConfig(mMat, kParticlesShader, false);
    DrawParticles(white);
}
