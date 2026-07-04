#include "bandobj/BandPatchMesh.h"
#include "os/Debug.h"
#include "utl/MemMgr.h"
#include <algorithm>
#include <cmath>

// Minimal port of BandPatchMesh.cpp from the rb3-Wii MWCC decomp (matching TU
// src/system/bandobj/BandPatchMesh.cpp) to MSVC X360. Only the worklist target
// functions and the helpers required to compile + emit them are ported here:
//
//   * BandPatchMesh::MeshVert::AddUV                  (0x82332BC0)
//   * stlpmtx_std::__unguarded_partition<...,SortByZ> (0x82334AF8, via the
//     std::sort(unk18, SortByZ()) instantiation in the WorkVerts ctor)
//   * BandPatchMesh::WorkVerts::SetMeshVerts          (0x82337AA0)
//
// The MeshVert per-vert arena layout literals (kMVFaceList/kMVTwinFlag/
// kMVSlotBase) are the Wii/retail-target byte constants — the retail X360 build
// uses the same MWCC 4-byte-pointer MeshVert layout, so the raw literals match.
static const size_t kMVFaceList = 0x32;
static const size_t kMVTwinFlag = 0x27;
static const size_t kMVSlotBase = 0x38;

int BandPatchMesh::MeshVert::AddUV(
    const BandPatchMesh::MeshVert *mv, const Vector2 &vr, const Vector2 *vp
) {
    MILO_ASSERT(this != mv, 0x55);
    MILO_ASSERT(mv->mVert, 0x57);
    Vector3 v48;
    Subtract(mVert->pos, mv->mVert->pos, v48);
    float lensq = LengthSquared(v48);
    float dot = Dot(mv->mVert->norm, v48);
    ScaleAddEq(v48, mv->mVert->norm, -dot);
    float v50y = mv->unk1c.y;
    float v50x = mv->unk1c.x;
    float v48x = v48.x;
    float v48y = v48.y;
    float v48z = v48.z;
    float newlensq = v48z * v48z + v48x * v48x + v48y * v48y;
    if (newlensq > 0) {
        float ratio = newlensq / lensq;
        float r = 1.0f / std::sqrt(ratio);
        float recipsq = 0.5f * r * (3.0f - ratio * r * r);
        float dot4 = v48x * mv->unk10.x + v48y * mv->unk10.y + v48z * mv->unk10.z;
        float vry = vr.y;
        float dot5 = v48x * mv->unk4.x + v48y * mv->unk4.y + v48z * mv->unk4.z;
        v50x += recipsq * vr.x * dot5;
        v50y += recipsq * vry * dot4;
    } else if (lensq > 0)
        return 0;
    if (vp) {
        float dx = vp->x - v50x;
        float dy = vp->y - v50y;
        if (dx * dx + dy * dy > 0.25f)
            return 0;
    }
    unk1c.x += v50x;
    unk1c.y += v50y;
    unk4 += mv->unk4;
    unk10 += mv->unk10;
    return 1;
}

struct SortByZ {
    bool operator()(RndMesh::Vert *v1, RndMesh::Vert *v2) {
        if (v1->pos.z != v2->pos.z)
            return v1->pos.z < v2->pos.z;
        else if (v1->pos.y != v2->pos.y)
            return v1->pos.y < v2->pos.y;
        else
            return v1->pos.x < v2->pos.x;
    }
};

BandPatchMesh::WorkVerts::WorkVerts(RndMesh *mesh, const Vector2 &v2)
    : unkc(0), mMesh(mesh), unk34(v2), unk3c((1.0f / v2.x), (1.0f / v2.y)) {
    unk0 = 0;
    MemDoTempAllocations m;
    unk18.resize(mMesh->Verts().size());
    for (int i = 0; i < unk18.size(); i++) {
        unk18[i] = &mMesh->Verts(i);
    }
    std::sort(unk18.begin(), unk18.end(), SortByZ());
}

BandPatchMesh::WorkVerts::~WorkVerts() { delete[] unkc; }

void BandPatchMesh::WorkVerts::SetMeshVerts() {
    MILO_ASSERT(mMeshVerts.empty(), 0x10C);
    MemDoTempAllocations m;
    unk10.reserve(mMesh->Verts().size());
    unk20.reserve(mMesh->Faces().size());
    unk28.resize(mMesh->Faces().size());
    for (int i = 0; i < unk28.size(); i++) {
        unk28[i].mFlags = -1;
    }
    mMeshVerts.resize(mMesh->Verts().size());
    for (int i = 0; i < mMeshVerts.size(); i++) {
        mMeshVerts[i] = 0;
    }
    for (int i = 0; i < mMesh->Faces().size(); i++) {
        RndMesh::Face &curface = mMesh->Faces()[i];
        for (int j = 0; j < 3; j++) {
            ((int &)mMeshVerts[curface[j]])++;
        }
    }
    int count = 0;
    for (int i = 0; i < mMeshVerts.size(); i++) {
        int c = (int)mMeshVerts[i];
        mMeshVerts[i] = count;
        count += (((c + 1) & ~1) - 2) * 2 + kMVSlotBase;
    }
    unkc = new char[count];
    for (int i = 0; i < mMeshVerts.size(); i++) {
        mMeshVerts[i] = (unsigned int)((char *)unkc + (int)mMeshVerts[i]);
        MeshVert *v = (MeshVert *)mMeshVerts[i];
        *((unsigned char *)v + kMVTwinFlag) = 0;
        v->unk28 = -1;
        v->unk2c = -1;
        v->unk30 = 0;
        v->mVert = 0;
        v->unk24 = 0;
    }
    for (int i = 0; i < mMesh->Faces().size(); i++) {
        RndMesh::Face &curface = mMesh->Faces()[i];
        for (int j = 0; j < 3; j++) {
            MeshVert *mv = (MeshVert *)mMeshVerts[curface[j]];
            int n = mv->unk30;
            ((unsigned short *)((char *)mv + kMVFaceList))[n] = i;
            mv->unk30 = n + 1;
        }
    }
    RndMesh::Vert *base = &mMesh->Verts()[0];
    for (int i = 0; i < unk18.size(); i++) {
        RndMesh::Vert *v1 = unk18[i];
        int vi = v1 - base;
        MeshVert *mv = (MeshVert *)mMeshVerts[vi];
        if (mv->unk28 == -1) {
            mv->unk28 = vi;
            int prev = vi;
            for (int j = i + 1; j < unk18.size(); j++) {
                RndMesh::Vert *v2 = unk18[j];
                bool diff = v1->pos.x != v2->pos.x || v1->pos.y != v2->pos.y
                    || v1->pos.z != v2->pos.z;
                if (diff)
                    break;
                int vi2 = v2 - base;
                ((MeshVert *)mMeshVerts[vi2])->unk28 = vi;
                *((unsigned char *)mMeshVerts[vi2] + kMVTwinFlag) = 1;
                ((MeshVert *)mMeshVerts[prev])->unk2c = vi2;
                prev = vi2;
            }
            if (prev != vi) {
                *((unsigned char *)mMeshVerts[vi] + kMVTwinFlag) = 1;
            }
        }
    }
}
