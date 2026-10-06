// Retail's ObjPtr ctor policy in this TU is per-site, so it is spelled per site
// (obj/Object.h, ObjPtrInlineOwner) instead of with the per-TU
// RB3_TU_OBJPTR_FORCEINLINE_CTOR switch. Inline three-store form:
// BandPatchMesh::mSrc, MeshPair::mesh (the MeshPair(Hmx::Object *) temporaries
// in PropSync<MeshPair> 0x8234CB30 and ObjVector<MeshPair>::resize 0x8234CF80)
// and PatchPair::mPatch. Out of line: PatchPair::mTex, which retail's
// PatchPair(Hmx::Object *) ctor constructs with `bl ??0?$ObjPtr@VRndTex@@@@`.
// The per-TU switch inlined mTex too (PatchPair ctor 45.0).
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

// Minimal BandPatchMesh.cpp (TU
// src/system/bandobj/BandPatchMesh.cpp) for MSVC X360. Only the worklist target
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
// the struct. On X360 these expressions
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

// Retail 0x823454A0: an out-of-line 2x2 inverse, called only by ExtendTwin. No
// symbol survives; the name is ours, the signature is fixed by the retail body. It
// refuses (and leaves `out` untouched) when |det| < eps, else writes adj/det.
// All four inputs are read before the first store, so `out` may alias `m`
// (ExtendTwin inverts in place). It must be external, not static or inline:
// under /O1 a once-called static/inline body is inlined, and retail calls it.
bool Invert(const Hmx::Matrix2 &m, Hmx::Matrix2 &out, float eps) {
    if (std::fabs(m.x.x * m.y.y - m.x.y * m.y.x) < eps)
        return false;
    float inv = 1.0f / (m.y.y * m.x.x - m.x.y * m.y.x);
    float xx = m.y.y * inv;
    float xy = -(m.x.y * inv);
    float yx = -(m.y.x * inv);
    float yy = m.x.x * inv;
    out.x.x = xx;
    out.x.y = xy;
    out.y.x = yx;
    out.y.y = yy;
    return true;
}

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

bool BandPatchMesh::MeshVert::AddUV(
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
// BandPatchMesh / MeshPair members (lane W17-BPM)
// in this TU. These used to exist
// only as native-link copies in native/src/x20_bandpatchmesh_link.cpp; retail has
// them in this TU (0x8234B270-0x8234D2F0), so the X360 build now emits them too.
//
// Retail details, read off retail bytes:
//   * PreRender calls MakeRotMatrixZ(angle, m) (0x82345520), not
//     an inline Hmx::Matrix3::RotateAboutZ.
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
    : mMeshes(o), mRenderTo(true), mSrc(ObjPtrInlineOwner(), o), mCategory(0) {}

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
                        // Reads the patch material's color; mColor is
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
                    // Retail also offsets the patch by half a texel
                    // (-0.5 / width, -0.5 / height; pool constant 0x820392FC
                    // = -0.5f), stored to the transform's v.x and v.y.
                    Transform tf88;
                    tf88.Reset();
                    tf88.m.y *= (float)tex->Height() / (float)tex->Width();
                    tf88.v.x = -0.5f / (float)tex->Width();
                    tf88.v.y = -0.5f / (float)tex->Height();
                    RndTransformable *trans = patch;
                    trans->SetLocalXfm(tf88);
                    patch->SetMat(mat);
                    if (mat->GetDiffuseTex()) {
                        // DrawShowing, not Draw: the patch is drawn into the
                        // outfit texture whatever its showing flag says (retail
                        // DxMesh::DrawShowing, fn_82738E38, never tests it).
                        patch->DrawShowing();
                        // Then a patch that lives in a dir is hidden, so the
                        // scene's own Draw() pass does not draw it a second
                        // time. Retail: `lwz 0x20` through the vbase (Dir())
                        // and `stb r11(=0), 0x8(r30)` (mShowing) right after
                        // the slot-0x14 DrawShowing vcall.
                        if (patch->Dir())
                            patch->SetShowing(false);
                    }
                    patch->SetMat(patchmat);
                    trans->DirtyLocalXfm().Reset();
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

// Retail 0x823460D0. Defined ahead of Construct:
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

// Retail 0x8234BD68. Read off
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
                    meshIndices[j--] = meshIndices[--meshCount];
                    wvCount++;
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

// Retail 0x8234B6F0. Read off
// retail bytes: with `perm`, the patch mesh and the generated deform are tagged
// with SetNote (strings 0x82039B1C / 0x82039AE8), not a bare
// MakeString that would be discarded.
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
// links the real PreRender / ReProject instead of counted stubs.
// FindXfm
// follows retail 0x823468E8 (its nearest-edge fallback is not dead: a
// `endFace == endFace` guard would never run it). None
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
        verts[i] = (MeshVert *)mMeshVerts[face[i]];
        if (verts[i]->mVert == 0) {
            MILO_ASSERT(b != 3, 0x2B1);
            AddMeshVertAndTwins(face[i], (MeshVert *)mMeshVerts[face[b]]);
        }
        allOut &= verts[i]->unk26;
    }
    bool reject = allOut != 0;
    if (!reject) {
        MeshVert temp;
        temp.SetVert(verts[0], verts[0]->mVert);
        Vector2 v(temp.unk1c);
        reject = !temp.AddUV(verts[1], unk34, &v) || !temp.AddUV(verts[2], unk34, &v);
    }
    if (!reject && b != 3) {
        int prev = (b == 0) ? 2 : b - 1;
        int next = (b == 2) ? 0 : b + 1;
        const Vector2 &pb = verts[b]->unk1c;
        const Vector2 &pn = verts[next]->unk1c;
        const Vector2 &pp = verts[prev]->unk1c;
        float ex = pn.x - pb.x;
        float ey = pn.y - pb.y;
        float t = Clamp(0.0f, 1.0f, ((pp.x - pb.x) * ex + (pp.y - pb.y) * ey) / (ex * ex + ey * ey));
        Vector2 proj;
        Interp(pb, pn, t, proj);
        reject = (pp.x - 0.5f) * (pp.x - proj.x) + (pp.y - 0.5f) * (pp.y - proj.y) < 0;
    }
    if (reject) {
        for (int i = unk10.size() - prevVertCount; i != 0; i--) {
            unk10.back()->mVert = 0;
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
        unk54.Set(fabsf(unk3c.x), fabsf(unk3c.y));
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

// Retail 0x82346618. Accumulates the normalised render-space direction of every
// edge joining `mv` to a twin vertex across its finished faces, turns the sum
// into the outward perpendicular (signed by the last edge's winding), and solves
// for the uv offset along it in the frame of the last two neighbours.
void BandPatchMesh::WorkVerts::ExtendTwin(
    const MeshVert *mv, Vector2 &outDir, Vector2 &outUv
) {
    if (!mv->unk27)
        return;
    float dx = 0.0f;
    float dy = 0.0f;
    const MeshVert *a = mv;
    const MeshVert *b = mv;
    const MeshVert *c = mv;
    for (int i = 0; i < mv->unk30; i++) {
        int faceIdx = ((unsigned short *)((char *)mv + kMVFaceList))[i];
        if (unk28[faceIdx].mFlags == 4) {
            RndMesh::Face &face = mMesh->Faces()[faceIdx];
            MeshVert *prev2 = (MeshVert *)mMeshVerts[face.v2];
            MeshVert *prev = (MeshVert *)mMeshVerts[face.v3];
            for (int j = 0; j < 3; j++) {
                MeshVert *cur = (MeshVert *)mMeshVerts[face[j]];
                if (prev == mv) {
                    if (cur->unk27) {
                        a = prev;
                        b = prev2;
                        c = cur;
                        dx = unk44.x * (cur->mVert->tex.x - prev->mVert->tex.x);
                        dy = unk44.y * (cur->mVert->tex.y - prev->mVert->tex.y);
                        float inv = __frsqrte(dx * dx + dy * dy);
                        outDir.x += dx * inv;
                        outDir.y += dy * inv;
                    }
                } else if (cur == mv) {
                    if (prev->unk27) {
                        a = prev;
                        b = prev2;
                        c = prev;
                        dx = unk44.x * (cur->mVert->tex.x - prev->mVert->tex.x);
                        dy = unk44.y * (cur->mVert->tex.y - prev->mVert->tex.y);
                        float inv = __frsqrte(dx * dx + dy * dy);
                        outDir.x += dx * inv;
                        outDir.y += dy * inv;
                    }
                }
                prev2 = prev;
                prev = cur;
            }
        }
    }
    if (a == b)
        return;
    float sign = ((a->mVert->tex.y - b->mVert->tex.y) * dx
                  - dy * (a->mVert->tex.x - b->mVert->tex.x))
            >= 0
        ? 1.0f
        : -1.0f;
    float invLen = (float)__frsqrte(outDir.x * outDir.x + outDir.y * outDir.y) * sign;
    outDir.Set(-(unk4c.x * outDir.y * invLen), unk4c.y * outDir.x * invLen);
    Hmx::Matrix2 m;
    m.x.Set(mv->mVert->tex.x - b->mVert->tex.x, mv->mVert->tex.y - b->mVert->tex.y);
    m.y.Set(mv->mVert->tex.x - c->mVert->tex.x, mv->mVert->tex.y - c->mVert->tex.y);
    if (!Invert(m, m, 1e-15f))
        return;
    float r0 = outDir.x * m.x.x + outDir.y * m.y.x;
    float r1 = outDir.x * m.x.y + outDir.y * m.y.y;
    outUv.Set(
        (mv->unk1c.x - b->unk1c.x) * r0 + (mv->unk1c.x - c->unk1c.x) * r1,
        (mv->unk1c.y - b->unk1c.y) * r0 + (mv->unk1c.y - c->unk1c.y) * r1
    );
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
                const Vector3 &p = mv->mVert->pos;
                RndMesh::Vert *v = unk18[k];
                float dx = p.x - v->pos.x;
                float dy = p.y - v->pos.y;
                float dz = p.z - v->pos.z;
                if (dx * dx + dy * dy + dz * dz < 0.1f * 0.1f) { // retail 0x3C23D70B
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
    // Retail reads the two gradient rows once, here, and keeps all six floats
    // in f26-f31 across the calls below for the closing lengths; Length(posOut.x)
    // after the calls reloads them from the stack instead (frame 0x1f0, one
    // saved FPR, 75.2%).
    Vector3 axisX(posOut.x.x, posOut.x.y, posOut.x.z);
    Vector3 axisY(posOut.y.x, posOut.y.y, posOut.y.z);
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
    float scaleX = Length(axisX) * 0.5f;
    float scaleY = Length(axisY) * 0.5f;
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
// patches; } layout -- two SYNC_PROPs, one per member
// (mesh, patches).
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

// Retail 0x8234CD38.
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
