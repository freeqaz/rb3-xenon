#include "rndobj/MeshDeform.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "utl/BinStream.h"
#include "utl/MemMgr.h"
#include "math/Rot.h"

// RB3-360 retail rev storage. Retail's LOAD_REVS keeps NO BinStreamRev: it splits
// the packed rev into two mutable file-scope shorts, and ASSERT_REVS emits nothing.
// The two words must live in ONE aligned(4) aggregate (altRev +0, rev +4) -- MSVC
// does not lay .bss out in declaration order, so two separate statics get other
// globals interleaved between them and will not fold onto one base register.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_MeshDeform;
#define gAltRev gRevs_MeshDeform.altRev
#define gRev gRevs_MeshDeform.rev

#pragma region Hmx::Object

RndMeshDeform::RndMeshDeform()
    : mMesh(this), mBones(this), mVerts(this), mSkipInverse(0), mDeformed(0) {}

RndMeshDeform::~RndMeshDeform() {}

BEGIN_HANDLERS(RndMeshDeform)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(RndMeshDeform)
    SYNC_PROP(mesh, mMesh)
    SYNC_PROP_SET(num_verts, mVerts.NumVerts(), )
    SYNC_PROP_SET(num_bones, (int)mBones.size(), )
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

void operator<<(BinStream &bs, const RndMeshDeform::BoneDesc &desc) {
    bs << desc.mBone;
    bs << desc.unk14 << desc.unk54;
}

BEGIN_SAVES(RndMeshDeform)
    SAVE_REVS(1, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mMesh;
    int numBones = mBones.size();
    bs << numBones;
    for (int i = 0; i < numBones; i++) {
        bs << mBones[i];
    }
    mVerts.Save(bs);
    bs << mMeshInverse;
END_SAVES

BEGIN_COPYS(RndMeshDeform)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(RndMeshDeform)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mMesh)
        const Transform &src = c->mMeshInverse;
        mMeshInverse = src;
        COPY_MEMBER(mBones)
        COPY_MEMBER(mSkipInverse)
        mVerts.Copy(c->mVerts);
    END_COPYING_MEMBERS
END_COPYS

void operator>>(BinStream &bs, RndMeshDeform::BoneDesc &desc) {
    bs >> desc.mBone;
    bs >> desc.unk14 >> desc.unk54;
}

BEGIN_LOADS(RndMeshDeform)
    int rev;
    bs >> rev;
    gRev = getHmxRev(rev);
    gAltRev = getAltRev(rev);
    Hmx::Object::Load(bs);
    bs >> mMesh;
    int num = 0;
    if (gRev < 1) {
        bs >> num;
    }
    int bones;
    bs >> bones;
    if (gRev < 1) {
        mVerts.Clear();
        int i150[64];
        float f250[64];
        for (int i = 0; i < num; i++) {
            int weightIdx = 0;
            for (int j = 0; j < bones; j++) {
                float f74;
                bs >> f74;
                if (f74 != 0) {
                    i150[weightIdx] = j;
                    f250[weightIdx] = f74;
                    weightIdx++;
                }
            }
            mVerts.AppendWeights(weightIdx, i150, f250);
        }
    }
    mBones.resize(bones);
    for (int i = 0; i < bones; i++) {
        bs >> mBones[i];
    }
    if (gRev > 0) {
        mVerts.Load(bs);
    }
    bs >> mMeshInverse;
    // Identity check, row by row. Each Vector3::operator== is an inlined bool-returning
    // call, so retail materializes one bool per row and short-circuits between rows
    // (li 1 / li 0 / clrlwi. / beq). Writing the 12 field compares as a flat && chain
    // instead makes MSVC fold them into a pure branch tree with no materialization,
    // which does NOT match -- see the NCCC-0803 lane notes.
    mSkipInverse =
        mMeshInverse.v == Vector3(0, 0, 0) && mMeshInverse.m.x == Vector3(1, 0, 0)
        && mMeshInverse.m.y == Vector3(0, 1, 0) && mMeshInverse.m.z == Vector3(0, 0, 1);
END_LOADS
#undef gRev
#undef gAltRev

void RndMeshDeform::PreSave(BinStream &bs) {
    if (mMesh) {
        mMesh->SetKeepMeshData(true);
    }
}

void RndMeshDeform::Print() {
    TheDebug << "num_verts " << mVerts.NumVerts() << "\n";
    TheDebug << "mesh_inverse " << mMeshInverse << "\n";
    TheDebug << "skip_inverse " << mSkipInverse << "\n";
    TheDebug << "mesh " << mMesh.Ptr() << "\n";
    for (int i = 0; i < mBones.size(); i++) {
        BoneDesc &cur = mBones[i];
        TheDebug << "bone" << i << ":\n";
        TheDebug << "   " << cur.mBone.Ptr() << "\n";
        TheDebug << "   " << cur.unk14 << "\n";
        TheDebug << "   " << cur.unk54 << "\n";
    }
    int i = 0;
    auto it = mVerts.begin();
    for (; it < mVerts.end(); ++it, ++i) {
        TheDebug << "weights" << i << ": ";
        u8 *cData = (u8 *)it.Data();
        u8 *p = cData;
        for (int j = 0; j < (int)*cData; j++) {
            float w = (float)p[2] * 0.003921568859368563f;
            TheDebug << "(" << p[1] << " " << w << ") ";
            p += 2;
        }
        TheDebug << "\n";
    }
}

#pragma endregion
#pragma region RndMeshDeform

void RndMeshDeform::VertArray::Save(BinStream &bs) {
    bs << mSize;
    bs.Write(mData, mSize);
}

void RndMeshDeform::VertArray::Load(BinStream &bs) {
    int size;
    bs >> size;
    SetSize(size);
    bs.Read(mData, mSize);
}

void RndMeshDeform::VertArray::SetSize(int size) {
    if (mSize != size) {
        mSize = size;
        MemFree(mData);
        mData = MemAlloc(mSize, __FILE__, 0x99, "RndMeshDeform");
    }
}

int RndMeshDeform::VertArray::AppendWeights(int num, int *const boneIndices, float *const weights) {
    MILO_ASSERT(num < VertArray::kMaxWeights, 0x5F);
    // count existing verts
    auto& _ref0 = mData;
    u8 *ptr = (u8 *)_ref0;
    u8 *end = ptr + mSize;
    int vertCount = 0;
    while (ptr < end) {
        vertCount++;
        ptr += (*ptr * 2) + 1;
    }
    float sum = 0.0f;
    // deduplicate bone entries: if two entries share the same bone index, merge them
    for (int i = 1; i < num; i++) {
        for (int j = 0; j < i; j++) {
            if (boneIndices[i] == boneIndices[j]) {
                weights[j] += weights[i];
                num--;
                int last = num;
                boneIndices[i] = boneIndices[last];
                weights[i] = weights[last];
                i--;
                break;
            }
        }
    }
    // validate weights
    int vertIdx = vertCount;
    for (int i = 0; i < num; i++) {
        if (!(weights[i] > 0.0f)) {
            auto _tmp0 = PathName(mParent);
            MILO_NOTIFY(
                "%s vert %d has negative weight %g on bone, won't export",
                _tmp0,
                vertIdx,
                weights[i]
            );
            weights[i] = 0.0f;
        }
        sum += weights[i];
    }
    // Retail emits a single `fabs` here (fn_8240B3F0 @ 0x8240B3F0, idx 81:
    // fsubs / fabs / fcmpu / ble), NOT the Abs<T> template's fcmpu+fneg form.
    if (fabsf(sum - 1.0f) > 0.05f) {
        MILO_NOTIFY(
            "%s vert %d weights sum to %g, not close enough to 1, check the skinning",
            PathName(mParent),
            vertIdx,
            sum
        );
    }
    float scale = 1.0f / sum;
    // append (num*2+1) bytes at end of buffer
    u8 *newEntry = (u8 *)MemResizeElem(
        _ref0, mSize, (void *)((char *)_ref0 + mSize), 0, (num * 2) + 1, __FILE__, 0x85, "RndMeshDeform"
    );
    *newEntry = (u8)num;
    for (int i = 0; i < num; i++) {
        newEntry[i * 2 + 1] = (u8)boneIndices[i];
        float w = weights[i] * scale;
        float clamped = w < 0.0f ? 0.0f : (w > 1.0f ? 1.0f : w);
        newEntry[i * 2 + 2] = (u8)(int)(clamped * 255.0f + 0.5f);
    }
    return vertCount;
}

void RndMeshDeform::VertArray::Copy(const RndMeshDeform::VertArray &a) {
    SetSize(a.mSize);
    memcpy(mData, a.mData, mSize);
}

// ---------------------------------------------------------------------------
// SCATTER TAIL -- X360 ONLY. Same reasoning as rndobj/MeshAnim.cpp's tail:
// obj/Dir.cpp is already emitted by the native obj/ source set (duplicate), and
// band3/meta_band/BandUI.cpp does not compile here (InterstitialMgr has no
// mRandomOverride member -- MEASURED, and a genuine header gap owned by
// band3, not something to paper over from rndobj). Guarding the tail keeps
// RndMeshDeform itself, which rndobj/Rnd.cpp:314 `RndMeshDeform::Init()`
// requires as soon as Rnd::PreInit runs.
// ---------------------------------------------------------------------------
#ifndef HX_NATIVE

// sw2 scatter-include (default/MeshDeform <- obj/Dir.cpp)
#define gRev gRev_Dir
#define gAltRev gAltRev_Dir
#include "obj/Dir.cpp"
#undef gRev
#undef gAltRev

// sw2 scatter-include (default/MeshDeform <- band3/meta_band/BandUI.cpp)
#define gRev gRev_BandUI
#define gAltRev gAltRev_BandUI
#include "band3/meta_band/BandUI.cpp"
#undef gRev
#undef gAltRev

#endif // !HX_NATIVE (scatter tail)
