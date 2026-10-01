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
    // One pass: merge every later entry that repeats bone i into entry i (the
    // repeat is replaced by the last entry and re-examined; no break), then
    // validate and accumulate weight i.
    int vertIdx = vertCount;
    for (int i = 0; i < num; i++) {
        for (int j = i + 1; j < num; j++) {
            if (boneIndices[j] == boneIndices[i]) {
                weights[i] += weights[j];
                num--;
                boneIndices[j] = boneIndices[num];
                weights[j] = weights[num];
                j--;
            }
        }
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
        _ref0, mSize, (void *)((char *)_ref0 + mSize), 0, (num * 2) + 1, "RndMeshDeform"
    );
    *newEntry = (u8)num;
    for (int i = 0; i < num; i++) {
        newEntry[i * 2 + 1] = (u8)boneIndices[i];
        float w = weights[i] * scale;
        newEntry[i * 2 + 2] = (u8)(Clamp(0.0f, 1.0f, w) * 255.0f + 0.5f);
    }
    return vertCount;
}

void RndMeshDeform::VertArray::Copy(const RndMeshDeform::VertArray &a) {
    SetSize(a.mSize);
    memcpy(mData, a.mData, mSize);
}

// RB3 members DC3 dropped, needed by BandPatchMesh's patch projection (lane
// W17-BPM2); not
// named in the target map yet (retail CopyVert 0x8240B2A8, SetMesh 0x8240B6B8,
// CopyWeights 0x8240B6F8, FindDeform 0x8240B9F8, all anonymous there).
void *RndMeshDeform::VertArray::FindVert(int vert) {
    u8 *buf = (u8 *)mData;
    while (vert != 0) {
        buf += (buf[0] << 1) + 1;
        vert--;
    }
    MILO_ASSERT(buf <= (u8 *)mData + mSize, 0x37);
    return buf;
}

void RndMeshDeform::VertArray::CopyVert(int to, int from, RndMeshDeform::VertArray &fromArr) {
    MILO_ASSERT(from >= 0 && from < fromArr.NumVerts(), 0x41);
    u8 buf[VertArray::kMaxWeights * 2 + 1];
    u8 *src = (u8 *)fromArr.FindVert(from);
    memcpy(buf, src, *src * 2 + 1);
    if (to > NumVerts()) {
        MILO_FAIL("can't copy vert past end");
        return;
    }
    u8 *dst = (u8 *)FindVert(to);
    int insertLength = *buf * 2 + 1;
    int cutLength = (dst == (u8 *)mData + mSize) ? 0 : *dst * 2 + 1;
    void *out = MemResizeElem(
        mData, mSize, dst, cutLength, insertLength, "RndMeshDeform"
    );
    memcpy(out, buf, *buf * 2 + 1);
}

void RndMeshDeform::CopyWeights(int to, int from, RndMeshDeform *fromMd) {
    mVerts.CopyVert(to, from, fromMd ? fromMd->mVerts : mVerts);
}

void RndMeshDeform::SetMesh(RndMesh *mesh) {
    mMesh = mesh;
    mVerts.Clear();
}

// Retail walks the mesh's ref ring forward (not a reverse
// vector of refs).
RndMeshDeform *RndMeshDeform::FindDeform(RndMesh *m) {
    for (ObjRef::iterator it = m->Refs().begin(); it != m->Refs().end(); ++it) {
        RndMeshDeform *md = dynamic_cast<RndMeshDeform *>(RefPtrOf(it)->RefOwner());
        if (md) {
            MILO_ASSERT(md->Mesh() == m, 0x125);
            return md;
        }
    }
    return 0;
}

bool RndMeshDeform::IsExoBone(RndTransformable *t) {
    if (!t)
        return false;
    return strnicmp("exo_", t->Name(), 4) == 0;
}

void RndMeshDeform::BoneDesc::ExportWorldXfm(Transform &xfm) {
    xfm.Reset();
    RndTransformable *t = mBone;
    while (RndMeshDeform::IsExoBone(t)) {
        Multiply(xfm, t->LocalXfm(), xfm);
        t = t->TransParent();
    }
    Multiply(xfm, unk54, xfm);
}

// 0x8240B208: scale all twelve floats of a Transform in place. Retail keeps it
// out of line; its only caller is Reskin.
__declspec(noinline) void ScaleEq(Transform &t, float f) {
    t.m.x *= f;
    t.m.y *= f;
    t.m.z *= f;
    t.v *= f;
}

// 0x8240D248 (called from CharBonesMeshes, 0x82284A8C). Both diagnostics are
// compiled out but still evaluate PathName(this) (0x8240D3DC, 0x8240D6F4).
void RndMeshDeform::Reskin(SyncMeshCB *cb, bool force) {
    if (!mMesh)
        return;
    if (!cb->HasMesh(mMesh) && !force && mDeformed)
        return;
    cb->SyncMesh(mMesh, 0x1f);
    mDeformed = true;
    std::vector<Transform> xfms;
    MemPushTemp();
    xfms.resize(mBones.size());
    MemPopTemp();
    for (unsigned int i = 0; i < mBones.size(); i++) {
        if (mBones[i].mBone) {
            Transform world;
            mBones[i].ExportWorldXfm(world);
            Multiply(mBones[i].unk14, world, xfms[i]);
        } else {
            xfms[i].Reset();
            MILO_FAIL("%s: null bone %d", PathName(this), i);
        }
    }
    int numVerts = mMesh->Verts().size();
    int vertIdx = 0;
    VertArray &verts = mVerts;
    for (u8 *vert = (u8 *)verts.mData; vert < (u8 *)verts.mData + verts.mSize;
         vert += *vert * 2 + 1) {
        if (vertIdx == numVerts) {
            MILO_FAIL_RTL(
                "%s cannot reskin %s, the vert counts differ mesh:%d me:%d",
                PathName(this),
                mMesh->Name(),
                numVerts,
                mVerts.NumVerts()
            );
            return;
        }
        Transform weighted;
        weighted.m.x.Zero();
        weighted.m.y.Zero();
        weighted.m.z.Zero();
        weighted.v.Zero();
        float totalWeight = 0;
        u8 *pair = vert;
        for (int n = 0; n < *vert; n++) {
            unsigned int bone = pair[1];
            unsigned int weight = pair[2];
            pair += 2;
            float w = weight * (1.0f / 255.0f);
            totalWeight += w;
            ScaleAddEq(weighted, xfms[bone], w);
        }
        ScaleEq(weighted, 1.0f / totalWeight);
        if (!mSkipInverse) {
            Multiply(weighted, mMeshInverse, weighted);
        }
        Vector3 &pos = mMesh->Verts(vertIdx).pos;
        Vector3 &norm = mMesh->Verts(vertIdx).norm;
        Multiply(pos, weighted, pos);
        // A vector perpendicular to the normal: project out the normal from the
        // axis it is least aligned with (norm * -norm[k], then +1 on k).
        Vector3 axis;
        float anx = fabsf(norm.x);
        float any = fabsf(norm.y);
        float anz = fabsf(norm.z);
        if (anx <= any && anx <= anz) {
            Scale(norm, -norm.x, axis);
            axis.x += 1.0f;
        } else if (any < anx && any < anz) {
            Scale(norm, -norm.y, axis);
            axis.y += 1.0f;
        } else {
            Scale(norm, -norm.z, axis);
            axis.z += 1.0f;
        }
        Vector3 cross;
        Cross(norm, axis, cross);
        Multiply(axis, weighted.m, axis);
        Multiply(cross, weighted.m, cross);
        Cross(axis, cross, norm);
        Normalize(norm, norm);
        vertIdx++;
    }
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
