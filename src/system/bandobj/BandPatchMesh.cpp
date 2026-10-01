// Retail inlines the two-arg ObjPtr ctor at this TU's owner-only sites: the
// MeshPair(Hmx::Object *) temporaries built by PropSync<MeshPair> (0x8234CB30) and
// ObjVector<MeshPair>::resize (0x8234CF80) store {mOwner, mObject = 0, vtable}
// inline with no `bl ??0?$ObjPtr@...`. Documented per-TU lever, obj/Object.h.
#define RB3_TU_OBJPTR_FORCEINLINE_CTOR
#include "bandobj/BandPatchMesh.h"
#include "bandobj/BandCharDesc.h"
#include "math/Rot.h"
#include "obj/Dir.h"
#include "os/Debug.h"
#include "os/File.h"
#include "rndobj/Mat.h"
#include "rndobj/MeshDeform.h"
#include "utl/BinStream.h"
#include "utl/MemMgr.h"
#include <algorithm>
#include <cmath>
#include <cstddef>

#ifdef HX_NATIVE
// The X360 reciprocal-square-root estimate intrinsic AddUV uses; the host has no
// such instruction, so give it the exact value the estimate approximates (AddUV
// refines it with one Newton step either way).
static inline double __frsqrte(double x) { return 1.0 / std::sqrt(x); }
#else
double __frsqrte(double);
#endif

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
// kMVSlotBase) are the retail X360 byte constants. On X360, Vector3 carries a
// trailing SIMD pad (sizeof 0x10), so MeshVert is 0x3c: twin flag at 0x2f
// (= offsetof(MeshVert, unk27)), face-list overallocation base at 0x3a
// (= sizeof - 2), arena slot base 0x40 (= sizeof + 4). The old 0x32/0x27/0x38
// values were the Wii/MWCC 12-byte-Vector3 layout — refuted against retail asm
// (stb 0x2f twin-flag stores, addi +0x1d face-list base, addi +0x1e slot size).
#ifdef HX_NATIVE
// Host layout (LP64 pointer, host Vector3): derive the same three quantities from
// the struct, as the rb3-Wii oracle's native arm does. On X360 these expressions
// evaluate to exactly the literals below (unk30 at 0x38, unk27 at 0x2f).
static const size_t kMVFaceList =
    offsetof(BandPatchMesh::MeshVert, unk30) + sizeof(unsigned short);
static const size_t kMVTwinFlag = offsetof(BandPatchMesh::MeshVert, unk27);
static const size_t kMVSlotBase = kMVFaceList + 6;
#else
static const size_t kMVFaceList = 0x3a;
static const size_t kMVTwinFlag = 0x2f;
static const size_t kMVSlotBase = 0x40;
#endif

void BandPatchMesh::MeshVert::SetVert(
    const BandPatchMesh::MeshVert *mvert, const RndMesh::Vert *vert
) {
    mVert = vert;
    unk4 = mvert->unk4;
    unk10 = mvert->unk10;
    unk1c = mvert->unk1c;
    unk26 = mvert->unk26;
}

void BandPatchMesh::MeshVert::SetVert(const RndMesh::Vert *vert) {
    mVert = vert;
    ZeroOut();
}

void BandPatchMesh::MeshVert::ZeroOut() {
    unk1c.Zero();
    unk4.Zero();
    unk10.Zero();
}

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
    float v50x = mv->unk1c.x;
    float v50y = mv->unk1c.y;
    float v48x = v48.x;
    float v48y = v48.y;
    float v48z = v48.z;
    float newlensq = v48z * v48z + v48x * v48x + v48y * v48y;
    if (newlensq > 0) {
        float ratio = newlensq / lensq;
        double est = __frsqrte(ratio);
        float r = (float)est;
        float recipsq = 0.5f * r * (3.0f - ratio * r * r);
        float dot5 = v48x * mv->unk4.x + v48y * mv->unk4.y + v48z * mv->unk4.z;
        float vry = vr.y;
        float dot4 = v48x * mv->unk10.x + v48y * mv->unk10.y + v48z * mv->unk10.z;
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
        mMeshVerts[i] += (MeshVertSlot)unkc;
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
            ((unsigned short *)((char *)mv + kMVFaceList))[mv->unk30] = i;
            mv->unk30++;
        }
    }
    RndMesh::Vert *base = &mMesh->Verts()[0];
    for (int i = 0; i < unk18.size(); i++) {
        RndMesh::Vert *v1 = unk18[i];
        int vi = v1 - base;
        if (((MeshVert *)mMeshVerts[vi])->unk28 == -1) {
            ((MeshVert *)mMeshVerts[vi])->unk28 = vi;
            int prev = vi;
            for (int j = i + 1; j < unk18.size(); j++) {
                RndMesh::Vert *v2 = unk18[j];
                bool diff = v1->pos.x != v2->pos.x || v1->pos.y != v2->pos.y
                    || v1->pos.z != v2->pos.z;
                if (diff)
                    break;
                int vi2 = unk18[j] - base;
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

// -----------------------------------------------------------------------------------
// BandPatchMesh / MeshPair members (lane W17-BPM), ported from the rb3-Wii oracle
// (src/system/bandobj/BandPatchMesh.cpp:906-975 and 1217-1360). These used to exist
// only as native-link copies in native/src/x20_bandpatchmesh_link.cpp; retail has
// them in this TU (0x8234B270-0x8234D2F0), so the X360 build now emits them too.
//
// Retail-vs-oracle differences, read off retail bytes:
//   * PreRender calls MakeRotMatrixZ(angle, m) (0x82345520) where the oracle has
//     the inline Hmx::Matrix3::RotateAboutZ.
//   * ConstructQuad is inlined into PreRender (retail PreRender calls Construct
//     with (mMeshes[0], tex, true, false, 0) directly).
// -----------------------------------------------------------------------------------

RndTex *BandPatchMesh::MeshPair::OutputTex() const {
    if (mesh && mesh->Mat())
        return mesh->Mat()->GetDiffuseTex();
    else
        return 0;
}

void BandPatchMesh::MeshPair::AddMappingPatch(RndMesh *themesh) {
    patches.push_back();
    patches.back().mPatch = themesh;
}

BandPatchMesh::MeshPair::PatchPair &BandPatchMesh::MeshPair::AddPatch(bool permanent) {
    MILO_ASSERT(!permanent || patches.size() == 0, 0x4BE);
    ObjectDir *dir = mesh.Owner()->Dir();
    const char *name = PatchName();
    RndMesh *mesh = 0;
    if (permanent)
        mesh = dir->Find<RndMesh>(name, false);
    if (!mesh) {
        mesh = Hmx::Object::New<RndMesh>();
        if (permanent) {
            mesh->SetName(PatchName(), dir);
            mesh->SetOrder(0.01f);
        }
    }
    AddMappingPatch(mesh);
    return patches.back();
}

const char *BandPatchMesh::MeshPair::PatchName() const {
    if (mesh)
        return MakeString("%s_patch.mesh", FileGetBase(mesh->Name()));
    else
        return "";
}

BandPatchMesh::BandPatchMesh(Hmx::Object *o)
    : mMeshes(o), mRenderTo(true), mSrc(o, 0), mCategory(0) {}

BandPatchMesh::BandPatchMesh(const BandPatchMesh &mesh)
    : mMeshes(mesh.mMeshes), mRenderTo(mesh.mRenderTo), mSrc(mesh.mSrc),
      mCategory(mesh.mCategory) {}

BandPatchMesh &BandPatchMesh::operator=(const BandPatchMesh &mesh) {
    mSrc = mesh.mSrc;
    mMeshes = mesh.mMeshes;
    mRenderTo = mesh.mRenderTo;
    mCategory = mesh.mCategory;
    return *this;
}

void BandPatchMesh::PostRender() {
    for (ObjVector<MeshPair>::iterator mp = mMeshes.begin(); mp != mMeshes.end();
         ++mp) {
        for (ObjVector<MeshPair::PatchPair>::iterator pp = mp->patches.begin();
             pp != mp->patches.end();
             ++pp) {
            RndMesh *patch = pp->mPatch;
            if (patch && !patch->Dir()) {
                delete patch;
            }
        }
        mp->patches.clear();
    }
}

void BandPatchMesh::PreRender(BandCharDesc *desc, int iii) {
    if (mCategory == 0 || (iii & mCategory)) {
        for (ObjVector<MeshPair>::iterator mp = mMeshes.begin(); mp != mMeshes.end();
             ++mp) {
            MILO_ASSERT(mp->patches.empty(), 0x509);
        }
        if (mSrc) {
            for (ObjVector<MeshPair>::iterator mp = mMeshes.begin();
                 mp != mMeshes.end();
                 ++mp) {
                mp->AddPatch(true);
            }
        }
        ObjectDir *pdir = desc->GetPatchDir();
        if (pdir) {
            for (int i = 0; i < desc->mPatches.size(); i++) {
                BandCharDesc::Patch &patch = desc->mPatches[i];
                if (patch.mCategory & mCategory) {
                    RndMesh *mesh = desc->GetPatchMesh(patch);
                    RndTex *tex = 0;
                    if (patch.mTexture == -1) {
                        if (mesh && mesh->Mat()) {
                            tex = mesh->Mat()->GetDiffuseTex();
                        } else {
                            MILO_WARN(
                                "%s could not find texture from placement mesh, category %d.",
                                PathName(pdir),
                                mCategory
                            );
                        }
                    } else
                        tex = desc->GetPatchTex(patch);
                    if (tex) {
                        if (mesh) {
                            if (patch.mTexture == -1) {
                                if (mMeshes.size() == 1) {
                                    AddMappingPatch(mMeshes[0], mesh);
                                }
                            } else {
                                Transform tf60;
                                if (FindXfm(mesh, patch.mUV, tf60)) {
                                    Hmx::Matrix3 m88;
                                    MakeRotMatrixZ(patch.mRotation, m88);
                                    Multiply(m88, tf60.m, tf60.m);
                                    float sx = patch.mScale.x * 0.5f;
                                    float sy = patch.mScale.y * 0.5f;
                                    tf60.m.x *= sx;
                                    tf60.m.y *= sy;
                                    ProjectPatches(tf60, tex, false);
                                } else {
                                    MILO_WARN(
                                        "Could not project %s onto %s\n",
                                        tex->Name(),
                                        mesh->Name()
                                    );
                                }
                            }
                        } else {
                            if (patch.mMeshName.empty()) {
                                ConstructQuad(tex);
                            } else {
                                MILO_WARN(
                                    "%s: could not find placement mesh %s",
                                    PathName(pdir),
                                    patch.mMeshName.c_str()
                                );
                            }
                        }
                    }
                }
            }
            desc->AddOverlays(*this);
        }
    }
}

void BandPatchMesh::Render(RndTex *tex, RndMat *mat) {
    for (int i = 0; i < mMeshes.size(); i++) {
        RndTex *outputtex = mMeshes[i].OutputTex();
        if (outputtex == tex) {
            for (int j = 0; j < mMeshes[i].patches.size(); j++) {
                BandPatchMesh::MeshPair::PatchPair &ppair = mMeshes[i].patches[j];
                RndMesh *patch = ppair.mPatch;
                if (patch) {
                    RndMat *patchmat = patch->Mat();
                    if (patchmat) {
                        // The oracle reads `patchmat->mColor`; that member is
                        // protected on X360's RndMat, and GetColor() returns it.
                        mat->SetColor(patchmat->GetColor());
                        mat->SetTexWrap(patchmat->GetTexWrap());
                        mat->SetBlend(patchmat->GetBlend());
                        mat->SetDiffuseTex(patchmat->GetDiffuseTex());
                    } else {
                        mat->SetColor(1, 1, 1);
                        mat->SetTexWrap(kTexBorderBlack);
                        mat->SetBlend(RndMat::kPreMultAlpha);
                        mat->SetDiffuseTex(mMeshes[i].patches[j].mTex);
                    }
                    Transform tf88;
                    tf88.Reset();
                    tf88.m.y *= (float)tex->Height() / (float)tex->Width();
                    patch->SetLocalXfm(tf88);
                    patch->SetMat(mat);
                    if (mat->GetDiffuseTex())
                        patch->DrawShowing();
                    patch->SetMat(patchmat);
                    patch->DirtyLocalXfm().Reset();
                }
            }
        }
    }
}

void BandPatchMesh::Compress(BandCharDesc *desc) {
    ObjectDir *pdir = desc->GetPatchDir();
    for (int i = 0; i < mMeshes.size(); i++) {
        for (int j = 0; j < mMeshes[i].patches.size(); j++) {
            RndMesh *patch = mMeshes[i].patches[j].mPatch;
            if (patch) {
                RndTex *tex = mMeshes[i].patches[j].mTex;
                if (tex && pdir && tex->Dir() == pdir) {
                    delete tex;
                }
                if (!patch->Dir())
                    delete patch;
            }
        }
    }
}

void BandPatchMesh::ListDrawChildren(std::list<RndDrawable *> &list) {
    if (mRenderTo) {
        for (int i = 0; i < mMeshes.size(); i++) {
            for (int j = 0; j < mMeshes[i].patches.size(); j++) {
                list.push_back(mMeshes[i].patches[j].mPatch);
            }
        }
    }
}

void BandPatchMesh::AddMappingPatch(BandPatchMesh::MeshPair &pair, RndMesh *mesh) {
    MILO_ASSERT(mRenderTo, 0x761);
    mesh->SetTransParent(0, false);
    mesh->CopyBones(0);
    mesh->SetHasAOCalc(false);
    pair.AddMappingPatch(mesh);
}

void BandPatchMesh::ConstructQuad(RndTex *tex) {
    MILO_ASSERT(mRenderTo, 0x76B);
    if (mMeshes.size() != 1) {
        MILO_WARN(
            "%s: Can't construct quad with %d meshes, must exactly 1",
            PathName(mMeshes.Owner()),
            mMeshes.size()
        );
    } else
        Construct(mMeshes[0], tex, true, false, 0);
}

// Retail 0x823460D0 (rb3-Wii BandPatchMesh.cpp:1428). Defined ahead of Construct:
// retail Construct keeps its 1.0 / 0.0 loop constants in volatile f9 / f10 across
// the call, which the compiler does only for a callee whose register use it has
// already seen in this TU.
void BandPatchMesh::SetRenderToVert(
    RndMesh::Vert &vert, const Vector2 &pos, const Vector2 &uv
) {
    vert.tex = uv;
    vert.pos.Set((pos.x - 0.5f) * 2.0f, (pos.y - 0.5f) * 2.0f, 0);
    vert.norm.Set(0, 0, -1.0f);
    vert.boneWeights.Set(0, 0, 0, 0);
    vert.color.Set(1, 1, 1, 1);
}

struct SortByWorkVertZ {
    bool operator()(BandPatchMesh::MeshVert *v1, BandPatchMesh::MeshVert *v2) {
        return v1->mVert->pos.z < v2->mVert->pos.z;
    }
};

// Inlined into ProjectPatches in retail (the std::sort call carries a zeroed
// comparator byte), so it is defined ahead of it.
void BandPatchMesh::WorkVerts::SortWorkVertsByZ() {
    std::sort(unk10.begin(), unk10.end(), SortByWorkVertZ());
}

// Retail 0x8234BD68 (rb3-Wii BandPatchMesh.cpp:981). Retail-vs-oracle, read off
// retail bytes: the scale is (0.5 / |x|, -0.5 / |y|); the hit point is clipped
// with the out-of-line Interp(start, end, t, end); the seed vertex takes the
// collision plane as its normal and no uv; SortWorkVertsByZ is inlined.
void BandPatchMesh::ProjectPatches(const Transform &xfm, RndTex *tex, bool perm) {
    Segment seg;
    seg.start = xfm.v;
    ScaleAdd(seg.start, xfm.m.z, -100.0f, seg.end);
    Vector2 scale(0.5f / Length(xfm.m.x), -0.5f / Length(xfm.m.y));
    MILO_ASSERT(64 > mMeshes.size(), 0x60A);
    int meshCount = mMeshes.size();
    int meshIndices[64];
    for (int i = 0; i < mMeshes.size(); i++) {
        meshIndices[i] = i;
    }
    RndMesh::sRawCollide = true;
    int hitMeshIdx = -1;
    int hitFaceIdx = 0;
    float t;
    Plane plane;
    for (int i = 0; i < mMeshes.size(); i++) {
        RndMesh *mesh = mMeshes[i].mesh;
        if (mesh) {
            if (!mesh->GetKeepMeshData()) {
                MILO_WARN(
                    "%s patch trying to collide against mesh with no keep_mesh_data",
                    PathName(mesh)
                );
            }
            if (mesh->CollideShowing(seg, t, plane)) {
                hitMeshIdx = i;
                hitFaceIdx = RndMesh::sLastCollide;
                Interp(seg.start, seg.end, t, seg.end);
            }
        }
    }
    RndMesh::sRawCollide = false;
    if (hitMeshIdx == -1)
        return;
    meshCount--;
    meshIndices[hitMeshIdx] = meshIndices[meshCount];
    MeshPair *hitPair = &mMeshes[hitMeshIdx];
    WorkVerts *wv = new WorkVerts(hitPair->mesh, scale);
    wv->SetMeshVerts();
    RndMesh::Vert seedVert;
    MeshVert seedMV;
    seedMV.SetVert(&seedVert);
    seedMV.unk1c.Set(0.5f, 0.5f);
    seedVert.pos = seg.end;
    seedVert.norm = *(Vector3 *)&plane;
    seedMV.unk4 = xfm.m.x;
    seedMV.unk10 = xfm.m.y;
    seedMV.Normalize(1);
    wv->AddFace(hitFaceIdx, &seedMV);
    wv->Project();
    wv->SortWorkVertsByZ();
    WorkVerts *workVerts[64];
    MeshPair *meshPairs[64];
    meshPairs[0] = hitPair;
    workVerts[0] = wv;
    int wvCount = 1;
    for (int j = 0; j < meshCount; j++) {
        MeshPair *cur = &mMeshes[meshIndices[j]];
        if (cur->mesh) {
            WorkVerts *nwv = new WorkVerts(cur->mesh, scale);
            for (int k = 0; k < wvCount; k++) {
                if (nwv->SetSameVerts(workVerts[k])) {
                    nwv->Project();
                    nwv->SortWorkVertsByZ();
                    workVerts[wvCount] = nwv;
                    meshPairs[wvCount] = cur;
                    wvCount++;
                    meshIndices[j--] = meshIndices[--meshCount];
                    break;
                }
            }
            if (nwv->mMeshVerts.size() == 0)
                delete nwv;
        }
    }
    for (int i = 0; i < wvCount; i++) {
        Construct(*meshPairs[i], tex, false, perm, workVerts[i]);
        delete workVerts[i];
    }
}

// Retail 0x8234B6F0 (rb3-Wii BandPatchMesh.cpp:1372). Retail-vs-oracle, read off
// retail bytes: with `perm`, the patch mesh and the generated deform are tagged
// with SetNote (strings 0x82039B1C / 0x82039AE8), which the oracle's bare
// MakeString discards.
void BandPatchMesh::Construct(
    MeshPair &meshpair, RndTex *tex, bool quad, bool perm, WorkVerts *wv
) {
    MILO_ASSERT(quad || wv, 0x77D);
    MeshPair::PatchPair &patchpair = meshpair.AddPatch(perm);
    patchpair.mTex = tex;
    if (mRenderTo) {
        patchpair.mPatch->SetTransParent(0, false);
        patchpair.mPatch->CopyBones(0);
        patchpair.mPatch->SetHasAOCalc(false);
    } else {
        patchpair.mPatch->SetOrder(0.01f);
        patchpair.mPatch->CopyBones(meshpair.mesh);
        patchpair.mPatch->RndTransformable::Copy(meshpair.mesh, Hmx::Object::kCopyDeep);
        patchpair.mPatch->SetHasAOCalc(meshpair.mesh->HasAOCalc());
    }
    if (quad) {
        if (!mRenderTo)
            MILO_WARN("Generating quad patch for non render to!");
        patchpair.mPatch->Verts().resize(4);
        patchpair.mPatch->Faces().resize(2);
        for (int i = 0; i < 4; i++) {
            float y = (i == 1 || i == 2) ? 1.0f : 0.0f;
            float x = (i < 2) ? 1.0f : 0.0f;
            Vector2 v(x, y);
            SetRenderToVert(patchpair.mPatch->Verts(i), v, v);
        }
        patchpair.mPatch->Faces()[0].Set(0, 1, 2);
        patchpair.mPatch->Faces()[1].Set(0, 2, 3);
    } else
        wv->SetVertsAndFaces(patchpair.mPatch, mRenderTo);
    patchpair.mPatch->Sync(0x13F);
    delete RndMeshDeform::FindDeform(patchpair.mPatch);
    if (perm) {
        patchpair.mPatch->SetNote(
            MakeString("Generated by OutfitConfig patch port to %s", meshpair.mesh->Name())
        );
        if (!quad && !mRenderTo) {
            RndMeshDeform *df = RndMeshDeform::FindDeform(meshpair.mesh);
            if (df) {
                RndMeshDeform *newdef = Hmx::Object::New<RndMeshDeform>();
                RndMesh *patch = patchpair.mPatch;
                newdef->SetName(
                    MakeString("%s.deform", FileGetBase(patch->Name())),
                    patchpair.mPatch->Dir()
                );
                newdef->Copy(df, Hmx::Object::kCopyDeep);
                newdef->SetMesh(patchpair.mPatch);
                wv->CopyDeformWeights(newdef, df);
                newdef->SetNote("Generated by OutfitConfig patch porting");
                newdef->SetNote("Generated by OutfitConfig patch porting");
            }
        }
    }
}

// -----------------------------------------------------------------------------------
// The rest of the patch-projection subsystem (lane W17-BPM2), so the native build
// links the real PreRender / ReProject instead of counted stubs. Ported from the
// rb3-Wii oracle (src/system/bandobj/BandPatchMesh.cpp:51-905, 958-964) except
// FindXfm, which follows retail 0x823468E8 (the oracle's copy is garbled: its
// nearest-edge fallback is guarded by `endFace == endFace` and never runs). None
// of these is named in the target map yet, so on X360 they are compiled but not
// scored. They sit after Construct / ProjectPatches so that neither caller sees
// their bodies (retail calls every one of them out of line).
// -----------------------------------------------------------------------------------

void BandPatchMesh::MeshVert::Normalize(int count) {
    MILO_ASSERT(count > 0, 0x7E);
    unk1c /= count;
    Vector3 v40;
    Cross(unk4, unk10, v40);
    Hmx::Quat q50;
    MakeRotQuat(v40, mVert->norm, q50);
    Hmx::Matrix3 m34;
    MakeRotMatrix(q50, m34);
    Multiply(unk4, m34, unk4);
    Multiply(unk10, m34, unk10);
    ::Normalize(unk4, unk4);
    ::Normalize(unk10, unk10);
    Vector3 v5c;
    ::Add(unk4, unk10, v5c);
    ::Normalize(v5c, v5c);
    Vector3 v68;
    Cross(mVert->norm, v5c, v68);
    ::Normalize(v68, v68);
    ::Add(v5c, v68, unk10);
    Subtract(v5c, v68, unk4);
    ::Normalize(unk4, unk4);
    ::Normalize(unk10, unk10);
    unk26 = 0;
    if (unk1c.x < 0)
        unk26 |= 1;
    else if (unk1c.x > 1.0f)
        unk26 |= 2;
    if (unk1c.y < 0)
        unk26 |= 4;
    else if (unk1c.y > 1.0f)
        unk26 |= 8;
}

void BandPatchMesh::WorkVerts::AddFace(int i, MeshVert *mv) {
    RndMesh::Face &curface = mMesh->Faces()[i];
    for (int n = 0; n < 3; n++) {
        SetMeshVertAndTwins(curface[n], mv);
    }
    TryAddFace(i, 3);
}

void BandPatchMesh::WorkVerts::AddEdge(MeshVert *mv0, MeshVert *mv1) {
    int twin = mv1->unk28;
    for (int idx = mv0->unk28; idx != -1; idx = ((MeshVert *)mMeshVerts[idx])->unk2c) {
        MeshVert *mv = (MeshVert *)mMeshVerts[idx];
        for (int i = 0; i < mv->unk30; i++) {
            int faceidx = ((unsigned short *)((char *)mv + kMVFaceList))[i];
            if (unk28[faceidx].mFlags == -1) {
                RndMesh::Face &face = mMesh->Faces()[faceidx];
                for (int j = 0; j < 3; j++) {
                    if (face[j] == idx
                        && ((MeshVert *)mMeshVerts[face[(j + 1) % 3]])->unk28 == twin) {
                        TryAddFace(faceidx, j);
                        break;
                    }
                }
            }
        }
    }
}

int BandPatchMesh::WorkVerts::TryAddFace(int faceidx, int b) {
    unk28[faceidx].mFlags = b;
    unk20.push_back(faceidx);
    // MeshFace states: -1 unadded, 3 don't-test-monotonicity, 4 finished.
    MILO_ASSERT(b != 4, 0x2A1);
    MILO_ASSERT(b != -1, 0x2A2);
    int prevVertCount = unk10.size();
    RndMesh::Face &face = mMesh->Faces()[faceidx];
    int allOut = 0xf;
    MeshVert *verts[3];
    for (int i = 0; i < 3; i++) {
        MeshVert *mv = (MeshVert *)mMeshVerts[face[i]];
        verts[i] = mv;
        if (mv->mVert == 0) {
            MILO_ASSERT(b != 3, 0x2B1);
            AddMeshVertAndTwins(face[i], (MeshVert *)mMeshVerts[face[b]]);
        }
        allOut &= verts[i]->unk26;
    }
    int reject = (allOut != 0) ? 1 : 0;
    if (reject == 0) {
        MeshVert temp;
        temp.SetVert(verts[0], verts[0]->mVert);
        Vector2 v(temp.unk1c);
        if (temp.AddUV(verts[1], unk34, &v) == 0 || temp.AddUV(verts[2], unk34, &v) == 0)
            reject = 1;
    }
    if (reject == 0 && b != 3) {
        int prev = (b == 0) ? 2 : b - 1;
        int next = (b == 2) ? 0 : b + 1;
        MeshVert *vb = verts[b];
        MeshVert *vn = verts[next];
        MeshVert *vp = verts[prev];
        float ey = vn->unk1c.y - vb->unk1c.y;
        float py = vp->unk1c.y - vb->unk1c.y;
        float ex = vn->unk1c.x - vb->unk1c.x;
        float px = vp->unk1c.x - vb->unk1c.x;
        float t = (ex * px + ey * py) / (ex * ex + ey * ey);
        if (t > 1.0f)
            t = 1.0f;
        else if (t < 0)
            t = 0;
        float projx = vb->unk1c.x + t * (vn->unk1c.x - vb->unk1c.x);
        float projy = vb->unk1c.y + t * (vn->unk1c.y - vb->unk1c.y);
        float dot = (vp->unk1c.x - projx) * (vp->unk1c.x - 0.5f)
            + (vp->unk1c.y - projy) * (vp->unk1c.y - 0.5f);
        reject = (dot < 0) ? 1 : 0;
    }
    if (reject != 0) {
        int added = unk10.size() - prevVertCount;
        for (int i = 0; i < added; i++) {
            unk10[unk10.size() - 1]->mVert = 0;
            unk10.pop_back();
        }
        unk20.pop_back();
        if (allOut == 0) {
            unk28[faceidx].mFlags = -1;
        }
        return 0;
    } else {
        unk28[faceidx].mFlags = 4;
        return 1;
    }
}

void BandPatchMesh::WorkVerts::SpreadEdges(int i) {
    MeshVert *meshverts[3];
    RndMesh::Face &curface = mMesh->Faces()[unk20[i]];
    for (int n = 0; n < 3; n++) {
        meshverts[n] = (MeshVert *)mMeshVerts[curface[n]];
    }
    AddEdge(meshverts[1], meshverts[0]);
    AddEdge(meshverts[2], meshverts[1]);
    AddEdge(meshverts[0], meshverts[2]);
}

int BandPatchMesh::WorkVerts::AddUvs(MeshVert *mv1, MeshVert *mv2, const Vector2 *v2) {
    unsigned short *faceidxptr = (unsigned short *)((char *)mv2 + kMVFaceList);
    int ret = 0;
    for (int i = 0; i < mv2->unk30; i++) {
        RndMesh::Face &curface = mMesh->Faces()[faceidxptr[i]];
        for (int j = 0; j < 3; j++) {
            MeshVert *curmv = (MeshVert *)mMeshVerts[curface[j]];
            if (curmv != mv2 && curmv->mVert != 0 && curmv->unk24 != unk0) {
                curmv->unk24 = unk0;
                ret += mv1->AddUV(curmv, unk34, v2);
            }
        }
    }
    return ret;
}

void BandPatchMesh::WorkVerts::SetMeshVertAndTwins(int idx, MeshVert *first) {
    MeshVert *cur = (MeshVert *)mMeshVerts[idx];
    MILO_ASSERT(!cur->mVert, 0x3BA);
    cur->SetVert(&mMesh->Verts(idx));
    unk10.push_back(cur);
    cur->AddUV(first, unk34, 0);
    cur->Normalize(1);
    for (int num = cur->unk28; num != -1; num = ((MeshVert *)mMeshVerts[num])->unk2c) {
        MeshVert *mt = (MeshVert *)mMeshVerts[num];
        if (mt != cur) {
            MILO_ASSERT(!mt->mVert, 0x3DB);
            unk10.push_back(mt);
            mt->SetVert(cur, &mMesh->Verts(num));
        }
    }
}

void BandPatchMesh::WorkVerts::AddMeshVertAndTwins(int idx, MeshVert *first) {
    MeshVert *cur = (MeshVert *)mMeshVerts[idx];
    MILO_ASSERT(!cur->mVert, 0x3EA);
    cur->SetVert(&mMesh->Verts(idx));
    unk10.push_back(cur);
    unk0++;
    cur->AddUV(first, unk34, 0);
    cur->unk24 = unk0;
    first->unk24 = unk0;
    Vector2 v18(cur->unk1c);
    int count = 1;
    for (int num = cur->unk28; num != -1; num = ((MeshVert *)mMeshVerts[num])->unk2c) {
        MeshVert *mt = (MeshVert *)mMeshVerts[num];
        count += AddUvs(cur, mt, &v18);
    }
    cur->Normalize(count);
    for (int num = cur->unk28; num != -1; num = ((MeshVert *)mMeshVerts[num])->unk2c) {
        MeshVert *mt = (MeshVert *)mMeshVerts[num];
        if (mt != cur) {
            MILO_ASSERT(!mt->mVert, 0x41A);
            unk10.push_back(mt);
            mt->SetVert(cur, &mMesh->Verts(num));
        }
    }
}

void BandPatchMesh::WorkVerts::Project() {
    for (int i = 0; i < unk20.size(); i++)
        SpreadEdges(i);
}

struct SortByPointer {
    bool operator()(BandPatchMesh::MeshVert *v1, BandPatchMesh::MeshVert *v2) {
        return v1->mVert < v2->mVert;
    }
};

void BandPatchMesh::WorkVerts::SetVertsAndFaces(RndMesh *mesh, bool renderTo) {
    std::sort(unk10.begin(), unk10.end(), SortByPointer());
    for (int i = 0; i < unk10.size(); i++) {
        unk10[i]->unk24 = i;
    }
    std::sort(unk20.begin(), unk20.end());
    mesh->Verts().resize(unk10.size());
    mesh->Faces().resize(unk20.size());
    if (renderTo) {
        MILO_ASSERT(mMesh->Mat(), 0x475);
        RndTex *dest = mMesh->Mat()->GetDiffuseTex();
        MILO_ASSERT(dest, 0x477);
        unk44.Set(dest->Width(), dest->Height());
        unk44 *= 0.707f;
        unk4c.Set(1.0f / unk44.x, 1.0f / unk44.y);
        unk54.Set(std::fabs(unk3c.x), std::fabs(unk3c.y));
        unk5c.Set(1.0f / unk54.x, 1.0f / unk54.y);
        for (int i = 0; i < mesh->Verts().size(); i++) {
            MeshVert *cur = unk10[i];
            Vector2 v40(0, 0);
            Vector2 v48(0, 0);
            ExtendTwin(cur, v40, v48);
            v40 += cur->mVert->tex;
            v48 += cur->unk1c;
            SetRenderToVert(mesh->Verts(i), v40, v48);
        }
    } else {
        for (int i = 0; i < mesh->Verts().size(); i++) {
            MeshVert *cur = unk10[i];
            mesh->Verts(i) = *cur->mVert;
            mesh->Verts(i).tex = cur->unk1c;
        }
    }
    for (int i = 0; i < mesh->Faces().size(); i++) {
        RndMesh::Face &myface = mMesh->Faces()[unk20[i]];
        for (int j = 0; j < 3; j++) {
            mesh->Faces()[i][j] = ((MeshVert *)mMeshVerts[myface[j]])->unk24;
        }
    }
}

void BandPatchMesh::WorkVerts::ExtendTwin(
    const MeshVert *mv, Vector2 &outDir, Vector2 &outUv
) {
    if (mv->unk27 == 0)
        return;
    float accumX = 0.0f;
    float accumY = 0.0f;
    const MeshVert *anchor = mv;
    const MeshVert *prevTwin = mv;
    const MeshVert *prevOther = mv;
    unsigned short *facePtr = (unsigned short *)((char *)mv + kMVFaceList);
    for (int i = 0; i < mv->unk30; i++) {
        unsigned short faceIdx = facePtr[i];
        if (unk28[faceIdx].mFlags == 4) {
            RndMesh::Face &face = mMesh->Faces()[faceIdx];
            MeshVert *v0 = (MeshVert *)mMeshVerts[face.v2];
            MeshVert *next = (MeshVert *)mMeshVerts[face.v3];
            for (int j = 0; j < 3; j++) {
                MeshVert *curr = (MeshVert *)mMeshVerts[face[j]];
                if (next == mv) {
                    if (curr->unk27 != 0) {
                        float dx = (curr->mVert->tex.x - next->mVert->tex.x) * unk44.x;
                        float dy = (curr->mVert->tex.y - next->mVert->tex.y) * unk44.y;
                        float inv = 1.0f / std::sqrt(dx * dx + dy * dy);
                        accumY = dy;
                        accumX = dx;
                        outDir.x += dx * inv;
                        outDir.y += dy * inv;
                        prevTwin = next;
                        prevOther = v0;
                        anchor = curr;
                    }
                } else if (curr == mv) {
                    if (next->unk27 != 0) {
                        float dx = (curr->mVert->tex.x - next->mVert->tex.x) * unk44.x;
                        float dy = (curr->mVert->tex.y - next->mVert->tex.y) * unk44.y;
                        float inv = 1.0f / std::sqrt(dx * dx + dy * dy);
                        accumY = dy;
                        accumX = dx;
                        outDir.x += dx * inv;
                        outDir.y += dy * inv;
                        prevTwin = next;
                        prevOther = v0;
                        anchor = next;
                    }
                }
                v0 = next;
                next = curr;
            }
        }
    }
    if (prevTwin == prevOther)
        return;
    float dxOther = prevOther->mVert->tex.x - prevTwin->mVert->tex.x;
    float dyOther = prevOther->mVert->tex.y - prevTwin->mVert->tex.y;
    float cross = accumX * dyOther - accumY * dxOther;
    float sign = (cross >= 0.0f) ? 1.0f : -1.0f;
    float ox = outDir.x;
    float oy = outDir.y;
    float invLen = sign / std::sqrt(ox * ox + oy * oy);
    outDir.x = -oy * invLen * unk4c.x;
    outDir.y = ox * invLen * unk4c.y;
    float ax = mv->mVert->tex.y - prevOther->mVert->tex.y;
    float ay = mv->mVert->tex.x - prevOther->mVert->tex.x;
    float bx = mv->mVert->tex.y - anchor->mVert->tex.y;
    float by = mv->mVert->tex.x - anchor->mVert->tex.x;
    float det = ay * bx - ax * by;
    if (std::fabs(det) < 1e-15f) {
        outUv.x = 0.0f;
        outUv.y = 0.0f;
        return;
    }
    float invDet = 1.0f / det;
    float m00 = ay * invDet;
    float m11 = -ax * invDet;
    float m01 = bx * invDet;
    float m10 = -by * invDet;
    float tu = mv->unk1c.x - prevOther->unk1c.x;
    float tv = mv->unk1c.y - prevOther->unk1c.y;
    float au = mv->unk1c.x - anchor->unk1c.x;
    float av = mv->unk1c.y - anchor->unk1c.y;
    float resX = outDir.x * m01 + outDir.y * m00;
    float resY = outDir.x * m10 + outDir.y * m11;
    outUv.x = resY * av + resX * tv;
    outUv.y = resY * au + resX * tu;
}

bool BandPatchMesh::WorkVerts::SetSameVerts(WorkVerts *other) {
    int start = 0;
    int end = 0;
    for (int i = 0; i < other->unk10.size(); i++) {
        MeshVert *mv = other->unk10[i];
        if (mv->unk28 == mv->mVert - &other->mMesh->Verts(0)) {
            float lo = mv->mVert->pos.z - 0.1f;
            float hi = mv->mVert->pos.z + 0.1f;
            while (start < unk18.size() && unk18[start]->pos.z < lo)
                start++;
            if (end < start)
                end = start;
            while (end < unk18.size() && unk18[end]->pos.z < hi)
                end++;
            for (int k = start; k < end; k++) {
                RndMesh::Vert *v = unk18[k];
                float dx = mv->mVert->pos.x - v->pos.x;
                float dy = mv->mVert->pos.y - v->pos.y;
                float dz = mv->mVert->pos.z - v->pos.z;
                if (dx * dx + dy * dy + dz * dz < 0.01f) {
                    mv->unk27 = 1;
                    if (mMeshVerts.empty()) {
                        SetMeshVerts();
                    }
                    int idx = unk18[k] - &mMesh->Verts(0);
                    SetMeshVertAndTwins(idx, mv);
                    ((MeshVert *)mMeshVerts[idx])->unk27 = 1;
                    break;
                }
            }
        }
    }
    int n10 = unk10.size();
    for (int i = 0; i < n10; i++) {
        MeshVert *mv = unk10[i];
        int vIdx = mv->mVert - &mMesh->Verts(0);
        for (int j = 0; j < mv->unk30; j++) {
            unsigned short *faceidxptr = (unsigned short *)((char *)mv + kMVFaceList);
            RndMesh::Face &face = mMesh->Faces()[faceidxptr[j]];
            int prev = face.v3;
            for (int z = 0; z < 3; z++) {
                if (face[z] == vIdx) {
                    MeshVert *partner = (MeshVert *)mMeshVerts[prev];
                    if (partner->mVert) {
                        AddEdge(partner, mv);
                    }
                    break;
                }
                prev = face[z];
            }
        }
    }
    return unk10.size() != 0;
}

void BandPatchMesh::WorkVerts::CopyDeformWeights(RndMeshDeform *to, RndMeshDeform *from) {
    MILO_ASSERT(mMesh == from->Mesh(), 0x49E);
    for (int i = 0; i < unk10.size(); i++) {
        to->CopyWeights(i, unk10[i]->mVert - &mMesh->Verts(0), from);
    }
}

// Retail 0x823468E8. Finds the face whose uv triangle contains `uv` (or, failing
// that, the face with the nearest uv edge), and builds the patch frame there:
// v = the uv-interpolated position, m.z = the uv-interpolated normal, m.x / m.y =
// the position gradients along u / -v, orthonormalised and scaled by half their
// length.
bool BandPatchMesh::FindXfm(RndMesh *mesh, const Vector2 &uv, Transform &xfm) {
    if (mesh->Verts().size() == 0 || mesh->Faces().size() == 0) {
        MILO_NOTIFY("Patches can't project onto %s, has no verts or faces!", mesh->Name());
        return false;
    }
    RndMesh::Face *end = mesh->Faces().end();
    RndMesh::Face *found = mesh->Faces().end();
    for (RndMesh::Face *f = mesh->Faces().begin(); f != end; f++) {
        const RndMesh::Vert *v0 = &mesh->Verts((*f)[2]);
        float firstSign = 0.0f;
        int matched = 0;
        for (; matched < 3; matched++) {
            const RndMesh::Vert *v1 = &mesh->Verts((*f)[matched]);
            float dx = uv.x - v0->tex.x;
            float dy = uv.y - v0->tex.y;
            float cross = (v1->tex.y - v0->tex.y) * dx - dy * (v1->tex.x - v0->tex.x);
            if (firstSign == 0.0f)
                firstSign = cross;
            if (cross * firstSign < 0.0f)
                break;
            v0 = v1;
        }
        if (matched == 3) {
            found = f;
            break;
        }
    }
    if (found == end) {
        float best = 1e30f;
        for (RndMesh::Face *f = mesh->Faces().begin(); f != end; f++) {
            const RndMesh::Vert *v0 = &mesh->Verts((*f)[2]);
            for (int j = 0; j < 3; j++) {
                const RndMesh::Vert *v1 = &mesh->Verts((*f)[j]);
                float ex = v1->tex.x - v0->tex.x;
                float ey = v1->tex.y - v0->tex.y;
                float t = (ex * (uv.x - v0->tex.x) + ey * (uv.y - v0->tex.y))
                    / (ex * ex + ey * ey);
                if (t < 0.0f)
                    t = 0.0f;
                else if (t > 1.0f)
                    t = 1.0f;
                float cx = v0->tex.x + ex * t - uv.x;
                float cy = v0->tex.y + ey * t - uv.y;
                if (MinEq(best, cx * cx + cy * cy))
                    found = f;
                v0 = v1;
            }
        }
    }
    const RndMesh::Vert *tri[3];
    for (int i = 0; i < 3; i++) {
        tri[i] = &mesh->Verts((*found)[i]);
    }
    Hmx::Matrix3 uvMat;
    Hmx::Matrix3 posMat;
    Hmx::Matrix3 normMat;
    Vector3 *uvRows = &uvMat.x;
    Vector3 *posRows = &posMat.x;
    Vector3 *normRows = &normMat.x;
    for (int i = 0; i < 3; i++) {
        uvRows[i].Set(tri[i]->tex.x, tri[i]->tex.y, 1.0f);
        posRows[i] = tri[i]->pos;
        normRows[i] = tri[i]->norm;
    }
    Invert(uvMat, uvMat);
    Hmx::Matrix3 posOut;
    Multiply(uvMat, posMat, posOut);
    Multiply(uvMat, normMat, posMat);
    Vector3 uvw(uv.x, uv.y, 1.0f);
    Multiply(uvw, posOut, xfm.v);
    Multiply(uvw, posMat, xfm.m.z);
    ::Normalize(xfm.m.z, xfm.m.z);
    RndMesh::Vert centerVert;
    MeshVert centerMV;
    centerMV.SetVert(&centerVert);
    centerMV.unk4 = posOut.x;
    centerMV.unk10 = posOut.y;
    centerMV.unk10.x *= -1.0f;
    centerMV.unk10.y *= -1.0f;
    centerMV.unk10.z *= -1.0f;
    centerVert.norm = xfm.m.z;
    centerMV.Normalize(1);
    float scaleX = Length(posOut.x) * 0.5f;
    float scaleY = Length(posOut.y) * 0.5f;
    xfm.m.x.x = centerMV.unk4.x * scaleX;
    xfm.m.x.y = centerMV.unk4.y * scaleX;
    xfm.m.x.z = centerMV.unk4.z * scaleX;
    xfm.m.y.x = scaleY * centerMV.unk10.x;
    xfm.m.y.y = centerMV.unk10.y * scaleY;
    xfm.m.y.z = centerMV.unk10.z * scaleY;
    return true;
}

bool BandPatchMesh::ReProject() {
    PostRender();
    if (mSrc)
        ProjectPatches(mSrc->LocalXfm(), 0, true);
    PostRender();
    return mRenderTo;
}

BinStream &operator>>(BinStream &bs, BandPatchMesh::MeshPair &mp) {
    bs >> mp.mesh;
    return bs;
}

// Retail reads and writes the rev pair through ONE base register (0x82CBE45C,
// offsets +0 / +4), i.e. internal-linkage statics, not the external class
// statics BandPatchMesh.h declares (those would each carry their own
// relocation). File scope is also what the scatter hosts' `#define gRev
// gRev_<Host>` renames exist for.
// Declared in retail's .bss order: gAltRev at +0, gRev at +4 (same pattern as
// rndobj/Font.cpp, rndobj/PostProc.cpp, ui/UIPicture.cpp, bandobj/BandCamShot.cpp).
static unsigned short gAltRev = 0;
static unsigned short gRev = 0;

// Retail 0x8234D1A8.
BinStream &operator>>(BinStream &bs, BandPatchMesh &mesh) {
    int rev;
    bs >> rev;
    gRev = getHmxRev(rev);
    gAltRev = getAltRev(rev);
    bs >> mesh.mSrc;
    if (gRev > 3)
        bs >> mesh.mMeshes;
    else {
        mesh.mMeshes.resize(1);
        bs >> mesh.mMeshes[0].mesh;
    }
    if (gRev < 1) {
        Symbol s;
        bs >> s;
    }
    if (gRev < 4) {
        Symbol s;
        bs >> s;
    }
    if (gRev > 1) {
        if (gRev > 2)
            bs >> mesh.mRenderTo;
        else {
            Symbol s;
            bs >> s;
            mesh.mRenderTo = !s.Null();
        }
    }
    if (gRev > 3)
        bs >> mesh.mCategory;
    return bs;
}

// -----------------------------------------------------------------------------------
// MeshPair CUSTOM_PROPSYNC (lane CY-3).
//
// Retail fn_8234AC78 (0x8234AC78-0x8234AD88, 272 B) is THIS function, not the
// SkeletonClip::MoveRating PropSync the target map claimed.  Retail evidence:
// the two dispatch strings the body loads are literally "mesh" (0x820398F4) and
// "patches" (0x8201AB64), member 1 is passed at +0x0 to
// ??$PropSync@VRndMesh@@@@YA_N... (i.e. an ObjPtr<RndMesh>) and member 2 at
// +0xc, which is exactly MeshPair's { ObjPtr<RndMesh> mesh; ObjVector<PatchPair>
// patches; } layout.  The rb3-Wii oracle (src/system/bandobj/BandPatchMesh.cpp
// :1502) has the identical two SYNC_PROPs.
//
// The old pairing scored 100.0 only because objdiff masks relocation arguments,
// so MoveRating's 2-property PropSync (member 1 at +0x0, member 2 at +0xc --
// STLport String is 12 B, so mExpected really sits at 0xc) is a byte-exact
// SHAPE TWIN of this one; the property strings and the two callees are the only
// differences and all four are masked relocs.
// -----------------------------------------------------------------------------------
BEGIN_CUSTOM_PROPSYNC(BandPatchMesh::MeshPair::PatchPair)
    SYNC_PROP(patch, o.mPatch)
    SYNC_PROP(tex, o.mTex)
END_CUSTOM_PROPSYNC

BEGIN_CUSTOM_PROPSYNC(BandPatchMesh::MeshPair)
    SYNC_PROP(mesh, o.mesh)
    SYNC_PROP(patches, o.patches)
END_CUSTOM_PROPSYNC

// Retail 0x8234CD38 (rb3-Wii BandPatchMesh.cpp:1507).
BEGIN_CUSTOM_PROPSYNC(BandPatchMesh)
    SYNC_PROP(meshes, o.mMeshes)
    SYNC_PROP(src, o.mSrc)
    SYNC_PROP(render_to, o.mRenderTo)
    SYNC_PROP(category, o.mCategory)
END_CUSTOM_PROPSYNC

// Writers for the ObjVector<BandPatchMesh> that OutfitConfig::Save streams
// (`bs << mPatches`). Retail HAS these -- OutfitConfig::Save's mPatches call
// reaches a vector writer which must reach a BandPatchMesh element writer --
// but neither is named in the map, so unlike the rest of this lane's work these
// two bodies are NOT read off retail bytes.
//
// They mirror the operator>> family at the newest revision (BandPatchMesh's rev
// ceiling is 4, per the reader's `can't load new version %d > %d` guard), which
// is the same construction used for the OutfitConfig element writers. They are
// unconstrained by the X360 metric -- objdiff masks the bl target -- and exist
// because OutfitConfig::Save does not otherwise LINK in the native build. A
// declaration alone was tried first and failed the native gate with exactly
// this undefined reference.
BinStream &operator<<(BinStream &bs, const BandPatchMesh::MeshPair &mp) {
    bs << mp.mesh;
    return bs;
}

BinStream &operator<<(BinStream &bs, const BandPatchMesh &mesh) {
    bs << packRevs(0, 4);
    bs << mesh.mSrc;
    bs << mesh.mMeshes;
    bs << mesh.mRenderTo;
    bs << mesh.mCategory;
    return bs;
}
