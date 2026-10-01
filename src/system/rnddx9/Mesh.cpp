#include "Mesh.h"
#include "Rnd.h"
#include "os/Debug.h"
#include "rndobj/MeshVertCompress.h"
#include "rnddx9/Utl.h"
#include "xdk/D3D9.h"
#include "xdk/d3d9i/d3d9.h"
#include "../../Memory.h"
#include "obj/Task.h"
#include "rndobj/Fur.h"
#include "rndobj/Rnd.h"
#include "rndobj/Shader.h"
#include "rndobj/ShaderMgr.h"
#include "rndobj/VelocityBuffer.h"
#include "rndobj/Wind.h"
#include "math/Mtx.h"

// File scope, not function-local: retail ??0DxMesh (0x827380B0) addresses all
// three arrays off ONE base register (r27, then +0x60 and +0xb8), which needs
// them in the TU's single .data; function-local statics each get their own
// COMDAT section and a fresh lis/addi.
// clang-format off
static D3DVERTEXELEMENT9 sVertexElements[] = {
    { 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
    { 0, 16, D3DDECLTYPE_FLOAT16_2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    { 0, 20, D3DDECLTYPE_DEC4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
    { 0, 24, D3DDECLTYPE_DEC4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT, 0 },
    { 0, 28, D3DDECLTYPE_UDEC4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT, 0 },
    { 0, 32, D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDINDICES, 0 },
    D3DDECL_END()
};
// clang-format on
// clang-format off
static D3DVERTEXELEMENT9 sMutableVertexElements[] = {
    { 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 16, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
    { 0, 48, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
    { 0, 64, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    { 0, 72, D3DDECLTYPE_SHORT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDINDICES, 0 },
    { 0, 80, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT, 0 },
    D3DDECL_END()
};
// clang-format on
// clang-format off
static D3DVERTEXELEMENT9 sMutableSkinnedVertexElements[] = {
    { 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 16, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
    { 0, 32, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT, 0 },
    { 0, 64, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    { 0, 72, D3DDECLTYPE_SHORT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDINDICES, 0 },
    { 0, 80, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT, 0 },
    D3DDECL_END()
};
// clang-format on

DxMesh::DxMesh() : mNumVerts(0), mNumFaces(0), unk1ac(0), unk1b0(0) {
    if (!sVertexDecl) {
        sVertexDecl = D3DDevice_CreateVertexDeclaration(sVertexElements);
        DX_ASSERT(sVertexDecl, 0xA8);
    }
    if (!sMutableVertexDecl) {
        sMutableVertexDecl = D3DDevice_CreateVertexDeclaration(sMutableVertexElements);
        DX_ASSERT(sMutableVertexDecl, 0xAF);
    }
    if (!sMutableSkinnedVertexDecl) {
        sMutableSkinnedVertexDecl =
            D3DDevice_CreateVertexDeclaration(sMutableSkinnedVertexElements);
        DX_ASSERT(sMutableSkinnedVertexDecl, 0xB5);
    }
}

DxMesh::~DxMesh() {
    TheDxRnd.AutoRelease(unk1ac);
    unk1ac = nullptr;
    TheDxRnd.AutoRelease(unk1b0);
    unk1b0 = nullptr;
}

// 0x82737F80: a non-mutable geometry owner clones the source's GPU buffers.
void DxMesh::Copy(const Hmx::Object *src, Hmx::Object::CopyType ty) {
    RndMesh::Copy(src, ty);
    const DxMesh *other = dynamic_cast<const DxMesh *>(src);
    if (other && this == GetGeomOwner() && !mMutable) {
        PhysMemTypeTracker tracker("D3D(phys):Mesh");
        unk1a4.Release();
        mNumVerts = other->mNumVerts;
        if (mNumVerts) {
            D3DVertexBuffer *clone = CloneVertexBuffer(other->unk1a4.buffer);
            unk1a4.SetData(clone, other->unk1a4.size);
        }
        TheDxRnd.AutoRelease(unk1ac);
        unk1ac = nullptr;
        mNumFaces = other->mNumFaces;
        if (mNumFaces) {
            unk1ac = (D3DResource *)CloneIndexBuffer((D3DIndexBuffer *)other->unk1ac);
        }
    }
}

// 0x827378B8 (DxMultiMesh::DrawBatchedNewGfx's index stream): the 16-bit index
// buffer widened once into a 32-bit vertex stream.
D3DVertexBuffer *DxMesh::GetMultimeshFaces() {
    MILO_ASSERT(!Mutable(), 0x1A7);
    if (!unk1b0) {
        unsigned int numIndices = mNumFaces * 3;
        unk1b0 = (D3DResource *)D3DDevice_CreateVertexBuffer(
            numIndices * 4, 0, (D3DPOOL)0
        );
        unsigned int *dst =
            (unsigned int *)D3DVertexBuffer_Lock((D3DVertexBuffer *)unk1b0, 0, 0, 0);
        unsigned short *src =
            (unsigned short *)D3DIndexBuffer_Lock((D3DIndexBuffer *)unk1ac, 0, 0, 0x10);
        for (unsigned int i = 0; i < numIndices; i++) {
            *dst++ = *src++;
        }
        D3DIndexBuffer_Unlock((D3DIndexBuffer *)unk1ac);
        D3DVertexBuffer_Unlock((D3DVertexBuffer *)unk1b0);
    }
    return (D3DVertexBuffer *)unk1b0;
}

// 0x82737440: buffers present, or the mesh is mutable (streamed each draw).
bool DxMesh::CanDraw() const {
    bool hasBuffers = (int)unk1a4.buffer && unk1ac != NULL;
    return hasBuffers || mMutable;
}

u32 DxMesh::VertFVF() const {
    return 0;
}

static const unsigned int kBitsOutput = 32;

void PackVector(
    unsigned int &output,
    const Vector4 &vec,
    unsigned char bitsX,
    unsigned char bitsY,
    unsigned char bitsZ,
    unsigned char bitsW,
    bool normalize
) {
    MILO_ASSERT((bitsX + bitsY + bitsZ + bitsW) == kBitsOutput, 0x39);

    int offsetY = bitsX;
    int offsetZ = bitsY + bitsX;
    int normFactor = normalize ? 1 : 0;
    int kOffsetW = bitsZ + offsetZ;

    int shiftY = bitsY - normFactor;
    int shiftZ = bitsZ - normFactor;
    int shiftX = bitsX - normFactor;
    int shiftW = bitsW - normFactor;

    u32 maskY = (1U << bitsY) - 1;
    u32 maskZ = (1U << bitsZ) - 1;
    u32 maskW = (1U << bitsW) - 1;
    u32 maskX = (1U << bitsX) - 1;
    int maxX = (1 << shiftX) - 1;
    int maxY = (1 << shiftY) - 1;
    int maxZ = (1 << shiftZ) - 1;
    int maxW = (1 << shiftW) - 1;

    MILO_ASSERT(kOffsetW + bitsW == kBitsOutput, 0x4E);

    f32 fy = (f32)(f64)maxY;
    f32 fx = (f32)(f64)maxX;
    f32 fw = (f32)(f64)maxW;
    f32 fz = (f32)(f64)maxZ;

    u32 py = ((u32)(s32)(vec.y * fy)) & maskY;
    u32 px = ((u32)(s32)(vec.x * fx)) & maskX;
    u32 pz = ((u32)(s32)(vec.z * fz)) & maskZ;
    u32 pw = ((u32)(s32)(vec.w * fw)) & maskW;

    output = (pw << kOffsetW) | (pz << offsetZ) | (py << offsetY) | px;
}

static inline unsigned short FloatToHalf(float value) {
    unsigned int raw = *(unsigned int *)&value;
    unsigned int iValue = raw & 0x7FFFFFFF;
    unsigned int sign = (raw >> 16) & 0x8000;
    if (iValue > 0x47FFEFFF) {
        return (unsigned short)(sign | 0x7FFF);
    }
    if (iValue < 0x38800000) {
        unsigned int shift = 113 - (iValue >> 23);
        iValue = (0x800000 | (iValue & 0x7FFFFF)) >> shift;
    } else {
        iValue -= 0x38000000;
    }
    return (unsigned short)(sign | ((((iValue >> 13) & 1) + iValue + 0xFFF) >> 13));
}

void FillCompressedVertex(CompressedVertex_Xbox &compressed, const RndMesh::Vert &vert) {
    // Pack color (ARGB D3DCOLOR format)
    u32 blue = (u32)(vert.color.blue * 255.0f);
    u32 red = (u32)(vert.color.red * 255.0f);
    compressed.mColor =
        (((((u32)(vert.color.alpha * 255.0f) << 8) | (red & 0xFF)) << 8)
        | ((u32)(vert.color.green * 255.0f) & 0xFF)) << 8
        | (blue & 0xFF);

    // Pack bone weights as UDEC4N
    PackVector(
        (unsigned int &)compressed.mBoneIndices, vert.boneWeights, 10, 10, 10, 2, false
    );

    // Copy position as float bit patterns
    *(f32 *)(&compressed.mPosX) = vert.pos.x;
    *(f32 *)(&compressed.mPosY) = vert.pos.y;
    *(f32 *)(&compressed.mPosZ) = vert.pos.z;

    // Pack UV as float16_2
    unsigned short halfU = FloatToHalf(vert.tex.x);
    unsigned short halfV = FloatToHalf(vert.tex.y);
    compressed.mNormal = (halfU << 16) | halfV;

    // Pack normal as DEC4N
    Vector4 normVec(vert.norm.x, vert.norm.y, vert.norm.z, 0.0f);
    PackVector((unsigned int &)compressed.mTangent, normVec, 10, 10, 10, 2, true);

    // Pack tangent as DEC4N
    PackVector((unsigned int &)compressed.mBinormal, vert.tangent, 10, 10, 10, 2, true);

    // Pack bone indices as UBYTE4
    compressed.mBoneWeights = (((int)vert.boneIndices[3] * 0x100
        + (int)vert.boneIndices[2]) * 0x100
        + (int)vert.boneIndices[1]) * 0x100
        + (int)vert.boneIndices[0];
}

// 0x82738B10: one cached transform per bone (one for an unskinned mesh);
// reports whether the cache had to be (re)built.
bool DxMesh::CheckFurTransformCache() {
    int numBones = mBones.size();
    if (numBones == 0) {
        numBones = 1;
    }
    if ((unsigned int)numBones != unk190.size()) {
        unk190.resize(numBones);
        for (int i = 0; i < numBones; i++) {
            unk190[i].Reset();
        }
        return true;
    }
    return false;
}

// 0x82737A10: blend the cached fur transform toward `xfm` (or snap when the
// bone turned or moved too far), then push it along the fur's wind.
void DxMesh::CacheFurTransform(const Transform &xfm, int i, float weight) {
    MILO_ASSERT(unk190.size() > i, 0x1ee);
    Transform &cached = unk190[i];
    float dz = cached.v.z - xfm.v.z;
    float dy = cached.v.y - xfm.v.y;
    float dx = cached.v.x - xfm.v.x;
    if (Dot(xfm.m.y, cached.m.y) >= 0.8660254f
        && dx * dx + dy * dy + dz * dz < 2500.0f) {
        float invWeight = 1.0f - weight;
        cached.m.x *= invWeight;
        cached.m.y *= invWeight;
        cached.m.z *= invWeight;
        cached.v *= invWeight;
        ScaleAddEq(cached, xfm, weight);
    } else {
        cached.m = xfm.m;
        cached.v = xfm.v;
    }
    RndWind *wind = Mat()->GetFur()->GetWind();
    if (wind) {
        Vector3 windForce;
        float windTime = TheTaskMgr.Seconds(TaskMgr::kRealTime);
        wind->GetWind(xfm.v, windTime, windForce);
        Vector3 &cachedPos = cached.v;
        cachedPos.x += windForce.x * 0.05f;
        cachedPos.y += windForce.y * 0.05f;
        cachedPos.z += windForce.z * 0.05f;
    }
}

// 0x82738BF0 (out of line; one caller, SetTransforms): the first fur pass's
// blend weight, 1 on a fresh cache, or -1 when no pass carries fur. RB3 walks
// the pass chain through NextPass() directly, with no dynamic_cast.
float DxMesh::FurWeight(RndMat *mat) {
    while (mat) {
        if (mat->GetFur()) {
            if (CheckFurTransformCache()) {
                return 1.0f;
            }
            return 1.0f / (mat->GetFur()->GetFluidity() * 6.5f + 1.0f);
        }
        mat = mat->NextPass();
    }
    return -1.0f;
}

static float sFurLodBias = -1.0f;

// 0x82737BF0: draws every fur shell of `mat`'s pass and returns the next pass.
// The too-many-bones report is compiled out but still evaluates
// PathName(this) (0x8273D404).
RndMat *DxMesh::DrawFur(RndMat *mat) {
    if (TheRnd.DrawMode() != Rnd::kDrawNormal) {
        return mat->NextPass();
    }
    DxMesh *owner = static_cast<DxMesh *>(GetGeomOwner());
    if (NumBones() * 2 >= 43) {
#ifdef HX_NATIVE
        MILO_NOTIFY_ONCE(
            "%s: Too many bones for fur (%d > %d)", PathName(this), NumBones(), 21
        );
#else
        MiloStripEval(
            "%s: Too many bones for fur (%d > %d)", PathName(this), NumBones(), 21
        );
#endif
        return mat->NextPass();
    }
    RndFur *fur = mat->GetFur();
    int numBones = NumBones();
    if (numBones == 0)
        numBones = 1;
    int furBoneOffset = numBones * 3;
    for (int i = 0; i < numBones; i++) {
        RndShaderMgr &shaderMgr = TheShaderMgr;
        shaderMgr.SetVConstant4x3(
            (VShaderConstant)(kVS_WorldTransform + furBoneOffset + i * 3),
            Hmx::Matrix4(unk190[i])
        );
    }
    fur->Prep(owner, mat);
    DWORD savedLod12 = D3DDevice_GetSamplerState_MipMapLodBias(TheDxRnd.Device(), 0xC);
    DWORD savedLod0 = D3DDevice_GetSamplerState_MipMapLodBias(TheDxRnd.Device(), 0);
    D3DDevice_SetSamplerState_MipMapLodBias(
        TheDxRnd.Device(), 0xC, *(DWORD *)&sFurLodBias
    );
    D3DDevice_SetSamplerState_MipMapLodBias(
        TheDxRnd.Device(), 0, *(DWORD *)&sFurLodBias
    );
    int numPasses = fur->Layers();
    RndMat *next = mat->NextPass();
    for (int i = 0; i < numPasses; i++) {
        fur->Shell(i, owner, mat);
        owner->DrawFaces();
    }
    D3DDevice_SetSamplerState_MipMapLodBias(TheDxRnd.Device(), 0xC, savedLod12);
    D3DDevice_SetSamplerState_MipMapLodBias(TheDxRnd.Device(), 0, savedLod0);
    return next;
}

// 0x82738C78: uploads the world / bone transforms (and caches them for fur and
// the velocity buffer).
void DxMesh::SetTransforms() {
    bool shouldCache = mMotionCache.mShouldCache;
    int numProcessed = 0;
    mMotionCache.mShouldCache = false;
    unsigned int boneCount = mBones.size();
    TheShaderMgr.SetMeshInfo(boneCount, HasAOCalc());
    float fw = FurWeight(Mat());
    bool hasFur;
    if (fw > 0.0f) {
        hasFur = true;
    } else {
        hasFur = false;
    }
    if (boneCount == 0) {
        TheShaderMgr.UpdateCache(WorldXfm(), 0);
        if (hasFur) {
            CacheFurTransform(WorldXfm(), 0, fw);
        }
    } else {
        RndBone *bone = mBones.begin();
        if (bone != mBones.end()) {
            do {
                Transform local;
                Multiply(bone->mOffset, bone->mBone->WorldXfm(), local);
                TheShaderMgr.UpdateCache(local, numProcessed);
                if (hasFur) {
                    CacheFurTransform(local, numProcessed, fw);
                }
                bone++;
                numProcessed++;
            } while (bone != mBones.end());
        }
        if (boneCount >= 1) {
            goto upload;
        }
    }
    boneCount = 1;
upload:
    TheShaderMgr.SetVConstant(
        kVS_WorldTransform, TheShaderMgr.ConstantCache(), boneCount * 3
    );
    if (shouldCache) {
        RndVelocityBuffer::Singleton().CacheTransform(
            this, TheShaderMgr.ConstantCache(), boneCount
        );
    }
}

// 0x82738E38 (DxMesh vtable slot 5). RB3 hands the GEOMETRY OWNER to the
// velocity buffer, reads NextPass() directly and always selects
// kStandardShader.
// NOTE: retail also calls an unidentified rndobj/Mesh helper (0x82418DC0) on
// `this` between SetTransforms and the pass loop; it is not reproduced here.
void DxMesh::DrawShowing() {
    DxMesh *geom = static_cast<DxMesh *>(GetGeomOwner());
    if (!geom->CanDraw()) {
        return;
    }
    if (TheRnd.DrawMode() == (Rnd::Mode)5) {
        RndVelocityBuffer::Singleton().DrawMesh(geom);
        return;
    }
    SetTransforms();
    RndMat *mat = Mat();
    RndMat *next;
    do {
        if (mat) {
            if (mat->GetFur()) {
                mat = DrawFur(mat);
                continue;
            }
            next = mat->NextPass();
        } else {
            next = nullptr;
        }
        RndShader::SelectConfig(mat, kStandardShader, false);
        geom->DrawFaces();
        mat = next;
    } while (mat);
}

// 0x82737DB8 (DxMesh vtable slot 14). Mutable meshes stream their geometry into
// the command buffer every draw; static ones draw from their GPU buffers. RB3
// keeps no draw stats here and ignores BeginIndexedVertices' result.
void DxMesh::DrawFaces() {
    D3DDevice *device = TheDxRnd.Device();
    if (mMutable) {
        if (Faces().empty())
            return;
        D3DDevice_SetVertexDeclaration(
            device, IsSkinned() ? sMutableSkinnedVertexDecl : sMutableVertexDecl
        );
        void *indexData = nullptr;
        void *vertexData = nullptr;
        D3DDevice_BeginIndexedVertices(
            device,
            D3DPT_TRIANGLELIST,
            0,
            Verts().size(),
            Faces().size() * 3,
            D3DFMT_INDEX16,
            0x60,
            &indexData,
            &vertexData
        );
        void *vertexDest = vertexData;
        RndMesh::Face *faceData = Faces().begin();
        RndMesh::Vert *vertData = Verts().begin();
        XMemCpyStreaming_WriteCombined(indexData, faceData, Faces().size() * 6);
        XMemCpyStreaming_WriteCombined(vertexDest, vertData, Verts().size() * 0x60);
        D3DDevice_EndIndexedVertices(device);
    } else {
        D3DDevice_SetIndices(device, (D3DIndexBuffer *)unk1ac);
        D3DDevice_SetStreamSource(device, 0, unk1a4.buffer, 0, 0x24, 1);
        D3DDevice_SetVertexDeclaration(device, sVertexDecl);
        D3DDevice_DrawIndexedVertices(device, D3DPT_TRIANGLELIST, 0, 0, mNumFaces * 3);
        D3DDevice_SetIndices(device, nullptr);
    }
}

// Retail 0x82738768 (DxMesh vtable slot 15), frame 0xD0, one EH funclet
// (0x82738A1C, destroys the tracker at 0x50). Read off the retail bytes; DC3's
// body is the reference but RB3 differs in three places: the compressed-vert path
// does NOT store mNumVerts, the vertex buffer is set through an inline
// SetData (both words stored off the &unk1a4 register), and there is no
// mNumFaces <= 0xFFFF assert.
//
// RESIDUAL (97.40 fuzzy): (1) retail emits TWO dead `stw r24,0x58(r31)` around
// `li r23,0` / `addi r25,r24,0xd8`; this spelling emits none, and the missing
// pair renames verts/fromCompressed r25<->r26. Measured and refuted (each a
// full build, fuzzy): RndMesh* geom + Verts() 96.50, + GetGeomOwner()->mVerts
// 96.50, + mGeomOwner->mVerts 96.50, + geom->mVerts 96.13, reversed decl order
// 96.50, an extra inline `return mVerts` level (DC3's hypothesis) 96.50,
// DxMesh* geom + Verts() 96.50, double static_cast 96.50. The DxMesh* cast
// with geom->mVerts is the best found. (2) the face loop's
// `add r10,begin,r8; add r10,r10,r11` operand order (retail rederives
// &mFaces[i] from the +4-biased dst IV; `dst[i*3+k]` is what gets that IV at
// all -- `dst += 3` 93.39, `dst[i] = face` as Face* 94.45, `*dst++` 93.96).
// (3) `stw r23,0xe8(r30)` two slots late in the vector swap (swap spellings
// `empty.swap`/`mFaces.swap(empty)` 96.13, `mFaces.swap(tmp())` 97.40).
void DxMesh::OnSync(int flags) {
    PhysMemTypeTracker tracker("D3D(phys):Mesh");
    if (this != mGeomOwner) {
        if (Mutable() & 0x1f) {
            mGeomOwner->Sync(flags);
        }
        return;
    }
    RndMesh::OnSync(flags);
    if (mMutable) {
        return;
    }
    // DxMesh*, not RndMesh*: also what lets this read geom->mFaces/mVerts
    // (protected) without friendship.
    DxMesh *geom = static_cast<DxMesh *>(GetGeomOwner());
    VertVector &verts = geom->mVerts;
    if (flags & 0x1f) {
        unsigned int numVerts = 0;
        unsigned int vertSize = 0;
        bool fromCompressed = false;
        int n = verts.size();
        mNumVerts = n;
        if (n != 0) {
            numVerts = n;
            vertSize = sizeof(CompressedVertex_Xbox);
        } else if (mNumCompressedVerts != 0) {
            numVerts = mNumCompressedVerts;
            vertSize = sizeof(CompressedVertex_Xbox);
            fromCompressed = true;
        } else {
            unk1a4.Release();
        }
        if (unk1a4.buffer == NULL || unk1a4.size != vertSize * numVerts) {
            unk1a4.Release();
            if (numVerts != 0) {
                unk1a4.SetData(
                    MakeVertexBuffer(numVerts, vertSize, VertFVF(), false),
                    vertSize * numVerts
                );
            }
        }
        if (unk1a4.buffer != NULL) {
            if (fromCompressed) {
                FillCompressedVerts();
            } else {
                Fill(verts.begin(), verts.end());
            }
        }
    }
    if (flags & 0x20) {
        TheDxRnd.AutoRelease(unk1ac);
        unk1ac = NULL;
        mNumFaces = geom->mFaces.size();
        if (mNumFaces != 0) {
            unk1ac = (D3DResource *)MakeIndexBuffer(mNumFaces, 6, D3DFMT_INDEX16);
            IBLock<> lock((D3DIndexBuffer *)unk1ac, 0);
            unsigned short *dst = (unsigned short *)lock.mDataAddr;
            for (int i = 0; i < mNumFaces; i++) {
                RndMesh::Face &face = geom->mFaces[i];
                dst[i * 3] = face.v1;
                dst[i * 3 + 1] = face.v2;
                dst[i * 3 + 2] = face.v3;
            }
        }
    }
    if ((flags & 0x200) == 0) {
        if ((mMutable & 0x1f) == 0) {
            mVerts.resize(0);
            ClearCompressedVerts();
        }
        if ((mMutable & 0x20) == 0) {
            std::vector<RndMesh::Face>().swap(mFaces);
        }
    }
}

// Retail 0x82737958.
void DxMesh::Fill(RndMesh::Vert *begin, RndMesh::Vert *end) {
    VBLock<CompressedVertex_Xbox> lock(unk1a4.buffer, 0);
    if (begin != end) {
        CompressedVertex_Xbox *dst = (CompressedVertex_Xbox *)lock.mDataAddr;
        do {
            FillCompressedVertex(*dst, *begin);
            begin++;
            dst++;
        } while (begin != end);
    }
}

// Retail 0x827379B8.
void DxMesh::FillCompressedVerts() {
    VBLock<CompressedVertex_Xbox> lock(unk1a4.buffer, 0);
    memcpy(
        lock.mDataAddr,
        mCompressedVerts,
        mNumCompressedVerts * sizeof(CompressedVertex_Xbox)
    );
}

void _fake(void) {
    BufLock<struct D3DVertexBuffer> buf(nullptr, 0);
    BufLock<struct D3DIndexBuffer> buf2(nullptr, 0);
}

// Retail 0x82737F30, 68 B. DECLARED in Mesh.h since the DC3 port and never
// DEFINED anywhere in the tree, while the inline `~VertexBufferData()` right
// above the declaration calls it -- so every TU that destroys a
// VertexBufferData has carried an unresolved external. (It never fired: the
// match build only compiles, and the native build does not glob rnddx9.)
// Body read off the retail bytes, which are unambiguous:
//   lwz r4,0(r3)         -- arg = this->buffer, the FIRST word
//   addi r3, TheDxRnd    -- the global at 0x82E04B38
//   bl ?AutoRelease@DxRnd@@QAAXPAUD3DResource@@@Z   (out-of-line, not inlined)
//   li r11,0; stw r11,0(r31); stw r11,4(r31)        -- zero BOTH words
// i.e. exactly the DX_RELEASE idiom followed by `size = 0`, and it confirms
// the VertexBufferData layout in Mesh.h (buffer at +0, size at +4).
void DxMesh::VertexBufferData::Release() {
    DX_RELEASE(buffer);
    size = 0;
}
