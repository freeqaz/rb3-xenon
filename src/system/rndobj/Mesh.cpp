#include "rndobj/Mesh.h"
#include "rndobj/MeshVertCompress.h"
#include "math/Geo.h"
#include "Utl.h"
#include "math/Mtx.h"
#include "math/Vec.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "obj/PropSync.h"
#include "os/Debug.h"
#include "os/System.h"
#include "rndobj/BaseMaterial.h"
#include "rndobj/Draw.h"
#include "rndobj/Mat.h"
#include "rndobj/MultiMesh.h"
#include "rndobj/Trans.h"
#include "utl/BinStream.h"
#include "utl/ChunkStream.h"
#include "utl/Loader.h"
#include "utl/MakeString.h"
#include "utl/MemMgr.h"
#include "utl/Std.h"
#include "os/Timer.h"
#include "obj/Utl.h"

PatchVerts gPatchVerts;
int MESH_REV_SEP_COLOR = 0x25;

Vector3 TransformNormal(const Vector3 &normal, const Hmx::Matrix3 &mat) {
    Hmx::Matrix3 inv;
    FastInvert(mat, inv);
    float nz = normal.z;
    float ny = normal.y;
    float nx = normal.x;
    Vector3 result;
    result.y = ny * inv.y.y + nz * inv.z.y + nx * inv.x.y;
    result.x = ny * inv.y.x + nz * inv.z.x + nx * inv.x.x;
    result.z = ny * inv.y.z + nz * inv.z.z + nx * inv.x.z;
    return result;
}

void PatchVerts::Clear() {
    mPatchVerts.clear();
    mCentroid.Set(0, 0, 0);
}

void PatchVerts::Add(int vertIdx, RndMesh::VertVector &verts, Vector3 &centroid) {
    int idx = GreaterEq(vertIdx);
    mPatchVerts.insert(mPatchVerts.begin() + idx, vertIdx);
    mCentroid += verts[vertIdx].pos;
    centroid = mCentroid;
    float invCount = 1.0f / (float)(unsigned int)mPatchVerts.size();
    centroid *= invCount;
}

int PatchVerts::GreaterEq(int iii) const {
    if (!(!mPatchVerts.empty() && iii > mPatchVerts.front())) {
        return 0;
    } else {
        if (iii > mPatchVerts.back()) {
            return mPatchVerts.size();
        } else {
            int u5 = 0;
            int u2 = mPatchVerts.size() - 1;
            while (u2 > u5 + 1) {
                int u4 = (u5 + u2) >> 1;
                int curVert = mPatchVerts[u4];
                if (iii > curVert) {
                    u5 = u4;
                }
                if (iii <= curVert) {
                    u2 = u4;
                }
            }
            return u2;
        }
    }
}

bool PatchVerts::HasVert(int vert) const {
    int idx = GreaterEq(vert);
    if (idx < mPatchVerts.size()) {
        return mPatchVerts[idx] == vert;
    } else {
        return false;
    }
}

bool RndMesh::PatchOkay(int numVerts, int numFaces) {
    return (double)numVerts * 4.31 + (double)numFaces * 0.25 < 329.0;
}

void SaveCompressedVertex(const CompressedVertex_Xbox &cv, BinStream &bs) {
    bs << cv.mPosX;
    bs << cv.mPosY;
    bs << cv.mPosZ;
    bs << cv.mColor;
    bs << cv.mNormal;
    bs << cv.mTangent;
    bs << cv.mBinormal;
    bs << cv.mBoneIndices;
    bs << cv.mBoneWeights;
}

/** Calculate the centroid of a triangle face by averaging its three vertex positions. */
void FaceCenter(RndMesh *mesh, RndMesh::Face *face, Vector3 &center) {
    center.z = 0.0f;
    center.y = 0.0f;
    center.x = 0.0f;
    RndMesh::Vert *verts = mesh->mGeomOwner->mVerts.mVerts;
    // Accumulate positions of all three vertices
    for (int i = 0; i < 3; i++) {
        RndMesh::Vert &v = verts[(*face)[i]];
        center.x += v.pos.x;
        center.y += v.pos.y;
        center.z += v.pos.z;
    }
    // Average: divide by 3
    center *= 0.33333334f;
}

bool RndMesh::sRawCollide;
int RndMesh::sLastCollide;

RndMesh::RndMesh()
    : mMat(this), mGeomOwner(this, this), mBones(this), mMutable(0),
      mVolume(kVolumeTriangles), mBSPTree(nullptr), mMultiMesh(nullptr), mHasAOCalc(0),
      mKeepMeshData(0), mCompressedVerts(nullptr), mNumCompressedVerts(0) {
    mMeshVersion = 0x26;
}

RndMesh::~RndMesh() {
#ifdef HX_NATIVE
    extern void CleanupGpuMesh(RndMesh*);
    CleanupGpuMesh(this);
#endif
    RELEASE(mBSPTree);
    RELEASE(mMultiMesh);
    ClearCompressedVerts();
}

BEGIN_HANDLERS(RndMesh)
    HANDLE(compare_edge_verts, OnCompareEdgeVerts)
    HANDLE(attach_mesh, OnAttachMesh)
    HANDLE(get_face, OnGetFace)
    HANDLE(set_face, OnSetFace)
    HANDLE(get_vert_pos, OnGetVertXYZ)
    HANDLE(set_vert_pos, OnSetVertXYZ)
    HANDLE(get_vert_norm, OnGetVertNorm)
    HANDLE(set_vert_norm, OnSetVertNorm)
    HANDLE(get_vert_uv, OnGetVertUV)
    HANDLE(set_vert_uv, OnSetVertUV)
    HANDLE(unitize_normals, OnUnitizeNormals)
    HANDLE(build_from_bsp, OnBuildFromBSP)
    HANDLE(point_collide, OnPointCollide)
    HANDLE(configure_mesh, OnConfigureMesh)
    HANDLE_EXPR(estimated_size_kb, EstimatedSizeKb())
    HANDLE_ACTION(instance_bones, InstanceGeomOwnerBones())
    HANDLE_EXPR(has_instanced_bones, HasInstancedBones())
    HANDLE_EXPR(has_bones, !mBones.empty())
    HANDLE_ACTION(delete_bones, DeleteBones(_msg->Int(2)))
    HANDLE_ACTION(burn_xfm, BurnXfm())
    HANDLE_ACTION(reset_normals, ResetNormals())
    HANDLE_ACTION(tessellate, Tessellate())
    HANDLE_ACTION(clear_ao, ClearAO())
    HANDLE_ACTION(clear_bones, CopyBones(nullptr))
    HANDLE_ACTION(copy_geom_from_owner, CopyGeometryFromOwner())
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(RndTransformable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

bool RndMesh::HasInstancedBones() {
    return mGeomOwner && !mBones.empty() && mGeomOwner->mBones.Owner() == mBones.Owner();
}

BEGIN_CUSTOM_PROPSYNC(RndMesh::Vert)
    SYNC_PROP(pos, o.pos)
    SYNC_PROP(norm, o.norm)
    SYNC_PROP(color, o.color)
    SYNC_PROP(alpha, o.color.alpha)
    SYNC_PROP(tex, o.tex)
END_CUSTOM_PROPSYNC

BEGIN_CUSTOM_PROPSYNC(RndBone)
    SYNC_PROP(bone, o.mBone)
    SYNC_PROP(offset, o.mOffset)
END_CUSTOM_PROPSYNC

bool PropSync(
    RndMesh ::VertVector &vec, DataNode &node, DataArray *prop, int i, PropOp op
) {
    if (op == kPropUnknown0x40)
        return false;
    else if (i == prop->Size()) {
        MILO_ASSERT(op == kPropSize, 0xA7D);
        node = (int)vec.size();
        return true;
    } else {
        RndMesh::Vert &vert = vec[prop->Int(i++)];
        if (i < prop->Size() || op & (kPropGet | kPropSet | kPropSize)) {
            if (vec.size() > 0) {
                if (op & kPropSet) {
                    vec.unkc = true;
                }
                return PropSync(vert, node, prop, i, op);
            } else {
                MILO_NOTIFY_ONCE(
                    "Cannot modify verts (check if keep_mesh_data is set on the mesh)"
                );
            }
            return true;
        } else {
            MILO_NOTIFY("Cannot add or remove verts of a mesh via property system");
        }
        return true;
    }
}

BEGIN_PROPSYNCS(RndMesh)
    SYNC_PROP(mat, mMat)
    SYNC_PROP_MODIFY(geom_owner, mGeomOwner, if (!mGeomOwner) mGeomOwner = this)
    SYNC_PROP_BITFIELD(mutable, mGeomOwner->mMutable, 0xAA1)
    SYNC_PROP_SET(num_verts, Verts().size(), SetNumVerts(_val.Int()))
    SYNC_PROP_SET(num_faces, (int)Faces().size(), SetNumFaces(_val.Int()))
    SYNC_PROP_SET(volume, GetVolume(), SetVolume((Volume)_val.Int()))
    SYNC_PROP_SET(has_valid_bones, HasValidBones(nullptr), _val.Int())
    SYNC_PROP(bones, mBones)
    SYNC_PROP(has_ao_calculation, mHasAOCalc)
    SYNC_PROP_SET(keep_mesh_data, mKeepMeshData, SetKeepMeshData(_val.Int() > 0))
    SYNC_PROP(verts, Verts())
    SYNC_SUPERCLASS(RndTransformable)
    SYNC_SUPERCLASS(RndDrawable)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BinStream &operator<<(BinStream &bs, const RndMesh::Face &face) {
    bs << face.v1 << face.v2 << face.v3;
    return bs;
}

BinStream &operator<<(BinStream &bs, const RndMesh::Vert &vert) {
    bs << vert.pos << vert.norm;
    bs << vert.color << vert.tex << vert.boneWeights << vert.boneIndices[0]
       << vert.boneIndices[1] << vert.boneIndices[2] << vert.boneIndices[3] << vert.tangent;
    return bs;
}

BinStream &operator<<(BinStream &bs, const RndBone &bone) {
    bs << bone.mBone << bone.mOffset;
    return bs;
}

BEGIN_SAVES(RndMesh)
    SAVE_REVS(0x26, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndTransformable)
    SAVE_SUPERCLASS(RndDrawable)
    bs << mMat;
    bs << mGeomOwner << mMutable << mVolume << mBSPTree;
    SaveVertices(bs);
    bs << mFaces << mPatches << mBones;
    bs << mKeepMeshData;
    bs << mHasAOCalc;
END_SAVES

void RndMesh::CopyBones(const RndMesh *mesh) {
    if (mesh)
        mBones = mesh->mBones;
    else
        mBones.clear();
}

BEGIN_COPYS(RndMesh)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndTransformable)
    COPY_SUPERCLASS(RndDrawable)
    CREATE_COPY(RndMesh)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mMat)
        if (ty != kCopyFromMax)
            COPY_MEMBER(mKeepMeshData)
        if (ty == kCopyFromMax)
            mMutable |= c->mMutable;
        else
            COPY_MEMBER(mMutable)
        mHasAOCalc = false;
        if (ty == kCopyShallow || (ty == kCopyFromMax && c->mGeomOwner != c)) {
            mGeomOwner = c->mGeomOwner.Ptr();
            CopyBones(c);
        } else {
            CopyGeometry(c, ty != kCopyFromMax);
            if (ty != kCopyFromMax)
                COPY_MEMBER(mHasAOCalc);
        }
    END_COPYING_MEMBERS
    Sync(0xBF);
END_COPYS

BinStreamRev &operator>>(BinStreamRev &d, RndMesh::Vert &vert) {
    d.stream >> vert.pos;
    float y, z;
    if (d.rev != 10 && d.rev < 0x17) {
        d.stream >> y;
        d.stream >> z;
    }
    d.stream >> vert.norm;
    if (d.rev < MESH_REV_SEP_COLOR)
        d.stream >> vert.color;
    else
        d.stream >> vert.color;
    d.stream >> vert.tex;
    if (d.rev >= MESH_REV_SEP_COLOR) {
        d.stream >> vert.boneWeights;
    }
    if (d.rev != 10 && d.rev < 23) {
        vert.boneWeights.Set((1.0f - y) - z, y, z, 0);
    }
    if (d.rev < 0xB) {
        Vector2 v;
        d.stream >> v;
    }
    if (d.rev > 0x1C) {
        d.stream >> vert.boneIndices[0];
        d.stream >> vert.boneIndices[1];
        d.stream >> vert.boneIndices[2];
        d.stream >> vert.boneIndices[3];
    }
    if (d.rev > 0x1D) {
        d.stream >> vert.tangent;
    }
    return d;
}

BinStream &operator>>(BinStreamRev &d, RndMesh::Face &face) {
    d.stream >> face.v1;
    d.stream >> face.v2;
    d.stream >> face.v3;
    if (d.rev < 1) {
        Vector3 v;
        d.stream >> v;
    }
    return d.stream;
}

BinStream &operator>>(BinStream &bs, RndBone &bone) {
    bs >> bone.mBone;
    bs >> bone.mOffset;
    return bs;
}

template <class T1, class T2>
BinStream &CachedRead(BinStream &, std::vector<T1, T2> &);

#ifdef HX_NATIVE
// Native implementation of CachedRead — reads raw binary data from the stream.
// On Xbox 360, CachedRead reads the raw big-endian data directly into the vector
// via ReadChunks (for chunked streams). On native x86_64, we need to byte-swap
// each element after reading.
#include "utl/ChunkStream.h"

template <class T1, class T2>
BinStream &CachedRead(BinStream &bs, std::vector<T1, T2> &vec) {
    int count;
    bs >> count;
    vec.resize(count);
    if (count > 0) {
        int totalSize = count * sizeof(T1);
        ReadChunks(bs, vec.data(), totalSize, 4096);
        // Byte-swap from big-endian (Xbox 360) to little-endian (native x86_64)
        // We need to swap each element based on its size
        if (sizeof(T1) == 2) {
            unsigned short *p = (unsigned short *)vec.data();
            for (int i = 0; i < count * (int)(sizeof(T1) / 2); i++) {
                p[i] = (p[i] >> 8) | (p[i] << 8);
            }
        } else if (sizeof(T1) == 4) {
            unsigned int *p = (unsigned int *)vec.data();
            for (int i = 0; i < count * (int)(sizeof(T1) / 4); i++) {
                p[i] = __builtin_bswap32(p[i]);
            }
        }
        // For Face (6 bytes = 3 * u16), each u16 needs swapping
        // The sizeof(T1)==2 case doesn't cover Face which is 6 bytes.
        // Handle Face specifically: it contains 3 unsigned shorts.
        if (sizeof(T1) == 6) {
            unsigned short *p = (unsigned short *)vec.data();
            for (int i = 0; i < count * 3; i++) {
                p[i] = (p[i] >> 8) | (p[i] << 8);
            }
        }
    }
    return bs;
}

// Explicit instantiations needed by Mesh.cpp
template BinStream &CachedRead<RndMesh::Face, std::allocator<RndMesh::Face>>(
    BinStream &, std::vector<RndMesh::Face, std::allocator<RndMesh::Face>> &);
template BinStream &CachedRead<unsigned char, std::allocator<unsigned char>>(
    BinStream &, std::vector<unsigned char, std::allocator<unsigned char>> &);
#else
template <class T1, class T2>
BinStream &CachedRead(BinStream &bs, std::vector<T1, T2> &vec) {
    int count;
    bs.ReadEndian(&count, 4);
    vec.resize(count);
    bs.Read(vec.data(), count * sizeof(T1));
    return bs;
}
#endif

INIT_REVS(0x26, 0)

BEGIN_LOADS(RndMesh)
    LOAD_REVS(bs)
    ASSERT_REVS(0x26, 0)
    if (d.rev > 0x19) {
        Hmx::Object::Load(d.stream);
    }
    RndTransformable::Load(d.stream);
    RndDrawable::Load(d.stream);
    if (d.rev < 15) {
        ObjPtrList<Hmx::Object> oList(this);
        int dummy;
        d.stream >> dummy;
        d.stream >> oList;
    }
    int i22 = 0;
    if (d.rev < 0x14) {
        int ib8, ie8;
        d.stream >> ib8;
        d.stream >> ie8;
        if (ib8 == 0 || ie8 == 0) {
            i22 = 0;
        } else if (ib8 == 1) {
            i22 = 2;
        } else if (ie8 == 7) {
            i22 = 3;
        } else {
            i22 = 1;
        }
    }
    if (d.rev < 3) {
        int dummy;
        d.stream >> dummy;
    }
    d.stream >> mMat;
    if (d.rev > 0x1A && d.rev < 0x1C) {
        char buf[0x80];
        d.stream.ReadString(buf, 0x80);
        if (!mMat && buf[0] != '\0') {
            mMat = LookupOrCreateMat(buf, Dir());
        }
    }
    d.stream >> mGeomOwner;
    if (!mGeomOwner) {
        mGeomOwner = this;
    }
    if (d.rev < 0x14 && mMat && (i22 == 0 || mMat->GetZMode() != kZModeDisable)) {
        mMat->SetZMode((ZMode)i22);
    }
    if (d.rev < 0xD) {
        ObjOwnerPtr<RndMesh> mesh(this);
        d.stream >> mesh;
        if (mesh != mGeomOwner) {
            MILO_NOTIFY("Combining face and vert owner of %s", Name());
        }
    }
    if (d.rev < 0xF) {
        ObjPtr<RndTransformable> trans(this);
        d.stream >> trans;
        SetTransParent(trans, false);
        SetTransConstraint((Constraint)2, nullptr, false);
    }
    if (d.rev < 0xE) {
        ObjPtr<RndTransformable> trans1(this);
        ObjPtr<RndTransformable> trans2(this);
        d.stream >> trans1 >> trans2;
    }
    if (d.rev < 3) {
        Vector3 v;
        d.stream >> v;
    }
    if (d.rev < 0xF) {
        Sphere s;
        d.stream >> s;
        SetSphere(s);
    }
    if (d.rev > 4 && d.rev < 8) {
        bool b;
        d >> b;
    }
    if (d.rev > 5 && d.rev < 0x15) {
        String str;
        int x;
        d.stream >> str;
        d.stream >> x;
    }
    if (d.rev > 0xF) {
        d.stream >> mMutable;
    } else if (d.rev > 0xB) {
        bool b;
        d >> b;
        mMutable = b ? 31 : 0;
    }
    if (d.rev > 0x11) {
        d.stream >> (int &)mVolume;
    }
    if (d.rev > 0x12) {
        RELEASE(mBSPTree);
        d.stream >> mBSPTree;
    }
    if (d.rev > 6 && d.rev < 8) {
        bool b;
        d >> b;
    }
    if (d.rev > 8 && d.rev < 0xB) {
        int x;
        d.stream >> x;
    }
    LoadVertices(d);
    if (d.stream.Cached()) {
        CachedRead(d.stream, mFaces);
    } else {
        d >> mFaces;
    }
    if (d.rev > 4 && d.rev < 0x18) {
        int count;
        unsigned short s1, s2;
        d.stream >> count;
        for (; count != 0; count--) {
            d.stream >> s1;
            d.stream >> s2;
        }
    }
    if (d.rev > 0x17) {
        if (d.stream.Cached()) {
            CachedRead(d.stream, mPatches);
        } else {
            d >> mPatches;
        }
    } else if (d.rev > 0x15) {
        mPatches.clear();
        int count;
        unsigned int ui;
        bs >> count;
        for (; count != 0; count--) {
            std::vector<unsigned short> usvec;
            std::vector<unsigned int> uivec;
            d >> ui >> usvec >> uivec;
            mPatches.push_back(ui);
        }
    } else if (d.rev > 0x10)
        d >> mPatches;
    if (d.rev > 0x1C) {
        d >> mBones;
        int max = MaxBones();
        if (mBones.size() > max) {
            MILO_NOTIFY(
                "%s: exceeds bone limit (%d of %d)",
                PathName(this),
                mBones.size(),
                MaxBones()
            );
            mBones.resize(MaxBones());
        }
    } else if (d.rev > 0xD) {
        ObjPtr<RndTransformable> trans(this);
        d.stream >> trans;
        if (trans) {
            mBones.resize(4);
            if (d.rev > 0x16) {
                mBones[0].mBone = trans;
                bs >> mBones[1].mBone >> mBones[2].mBone >> mBones[3].mBone;
                bs >> mBones[0].mOffset >> mBones[1].mOffset >> mBones[2].mOffset
                    >> mBones[3].mOffset;
                if (d.rev < 0x19) {
                    for (auto it = mVerts.begin(); it != mVerts.end(); ++it) {
                        it->boneWeights.Set(
                            ((1.0f - it->boneWeights.x) - it->boneWeights.y)
                                - it->boneWeights.z,
                            it->boneWeights.x,
                            it->boneWeights.y,
                            it->boneWeights.z
                        );
                    }
                }
            } else {
                if (TransConstraint() == RndTransformable::kConstraintParentWorld) {
                    ObjPtr<RndTransformable> &bone = mBones[0].mBone;
                    bone = TransParent();
                } else {
                    mBones[0].mBone = this;
                }
                mBones[0].mOffset.Reset();
                mBones[1].mBone = trans;
                bs >> mBones[2].mBone >> mBones[1].mOffset >> mBones[2].mOffset;
                mBones[3].mBone = nullptr;
            }
            for (int i = 0; i < 4; i++) {
                if (!mBones[i].mBone) {
                    mBones.resize(i);
                    break;
                }
            }
        } else
            mBones.clear();
    }
    RemoveInvalidBones();
    if (d.rev > 0 && d.rev < 4) {
        std::vector<std::vector<unsigned short> > usvec;
        d >> usvec;
    }
    if (d.rev == 0) {
        bool bd4;
        int ic0, ic4, ic8, icc;
        d >> bd4 >> ic0 >> ic4 >> ic8;
        d >> icc;
    }
    if (d.rev == 0x12) {
        if (mGeomOwner == this) {
            SetVolume(mVolume);
            goto yes;
        }
    } else {
    yes:
        if (d.rev >= 0x1E)
            goto next;
    }
    if (mMat && mMat->NormalMap()) {
        MakeTangentsLate(this);
    }
next:
    if (d.rev < 0x1F) {
        SetZeroWeightBones();
    }
    if (d.rev > 0x23) {
        d >> mKeepMeshData;
    }
    if (d.rev < MESH_REV_SEP_COLOR && IsSkinned()) {
        for (auto it = mVerts.begin(); it != mVerts.end(); ++it) {
            it->boneWeights.Set(
                it->color.red, it->color.green, it->color.blue, it->color.alpha
            );
            it->color.Zero();
        }
    }
    if (d.rev > 0x25) {
        d >> mHasAOCalc;
    }
    Sync(0xBF);
END_LOADS

__forceinline TextStream &operator<<(TextStream &ts, RndMesh::Volume v) {
    if (v == RndMesh::kVolumeEmpty)
        ts << "Empty";
    else if (v == RndMesh::kVolumeTriangles)
        ts << "Triangles";
    else if (v == RndMesh::kVolumeBSP)
        ts << "BSP";
    else if (v == RndMesh::kVolumeBox)
        ts << "Box";
    return ts;
}

void RndMesh::Print() {
    TheDebug << "   mat: " << mMat << "\n";
    TheDebug << "   geomOwner: " << mGeomOwner << "\n";
    TheDebug << "   mutable: " << mMutable << "\n";
    TheDebug << "   volume: " << mVolume << "\n";
    TheDebug << "   bones: TODO\n";
    TheDebug << "   geometry: TODO\n";
}

void RndMesh::UpdateSphere() {
    Sphere s;
    if (mBones.empty()) {
        MakeWorldSphere(s, true);
        Transform tf;
        FastInvert(WorldXfm(), tf);
        Multiply(s, tf, s);
    } else
        s.Zero();
    RndDrawable::SetSphere(s);
}

float RndMesh::GetDistanceToPlane(const Plane &p, Vector3 &v) {
    if (Verts().empty())
        return 0;
    else {
        const Transform &world = WorldXfm();
        Vector3 v58;
        Multiply(Verts()[0].pos, world, v58);
        v = v58;
        float dot = p.Dot(v);
        for (auto it = Verts().begin(); it != Verts().end(); ++it) {
            Multiply(it->pos, world, v58);
            float dotted = p.Dot(v58);
            if (std::fabs(dotted) < std::fabs(dot)) {
                dot = dotted;
                v = v58;
            }
        }
        return dot;
    }
}

bool RndMesh::MakeWorldSphere(Sphere &s, bool b) {
    if (b) {
        if (mShowing) {
            Box box;
            CalcBox(this, box);
            Vector3 v68;
            CalcBoxCenter(v68, box);
            s.Set(v68, 0);
            const Transform &worldXfm = WorldXfm();
            for (auto it = Verts().begin(); it != Verts().end(); ++it) {
                Vector3 v50;
                Multiply(it->pos, worldXfm, v50);
                Vector3 v5c;
                Subtract(v50, s.center, v5c);
                s.radius = Max(s.GetRadius(), Dot(v5c, v5c));
            }
            s.radius = sqrtf(s.GetRadius());
            return true;
        }
    } else if (mSphere.GetRadius()) {
        Multiply(mSphere, WorldXfm(), s);
        return true;
    }
    return false;
}

void RndMesh::Mats(std::list<RndMat *> &mats, bool) {
    if (mMat) {
        MatShaderOptions opts = GetDefaultMatShaderOpts(this, mMat);
        mMat->SetShaderOpts(opts);
        mats.push_back(mMat);
    }
}

Vector3 TransformNormal(const Vector3 &, const Hmx::Matrix3 &);

Vector3 RndMesh::SkinVertex(const RndMesh::Vert &vert, Vector3 *vptr) {
    Vector3 ret(0, 0, 0);
    const Transform *xfm;
    if (NumBones() > 0) {
        Transform tf60;
        tf60.Zero();
        for (int i = 0; i < 4; i++) {
            if (vert.boneIndices[i] < NumBones()) {
                int boneIdx = vert.boneIndices[i];
                RndTransformable *curBoneTrans = BoneTransAt(boneIdx);
                if ((&vert.boneWeights.x)[i] != 0.0f && curBoneTrans) {
                    Transform tf90;
                    Multiply(BoneOffsetAt(boneIdx), curBoneTrans->WorldXfm(), tf90);
                    ScaleAddEq(tf60, tf90, (&vert.boneWeights.x)[i]);
                }
            }
        }
        Multiply(vert.pos, tf60, ret);
        if (!vptr) return ret;
        xfm = &tf60;
    } else {
        xfm = &WorldXfm();
        Multiply(vert.pos, *xfm, ret);
        if (!vptr) return ret;
    }
    *vptr = TransformNormal(vert.norm, xfm->m);
    return ret;
}

RndDrawable *RndMesh::CollideShowing(const Segment &seg, float &f, Plane &pl) {
    Segment sega0;
    Transform tf58;
    sLastCollide = -1;
    if (IsSkinned() || sRawCollide)
        sega0 = seg;
    else {
        FastInvert(WorldXfm(), tf58);
        Multiply(seg.start, tf58, sega0.start);
        Multiply(seg.end, tf58, sega0.end);
    }
    if (mGeomOwner->mBSPTree) {
        if (Intersect(sega0, mGeomOwner->mBSPTree, f, pl) && f) {
            Multiply(pl, WorldXfm(), pl);
            return this;
        }
    } else {
        if (GetVolume() == kVolumeTriangles) {
            bool b1 = false;
            f = 1.0f;
            FOREACH (it, Faces()) {
                const Vert &vert0 = Verts(it->v1);
                const Vert &vert1 = Verts(it->v2);
                const Vert &vert2 = Verts(it->v3);
                Triangle tri;
                if (IsSkinned() && !sRawCollide) {
                    tri.Set(
                        SkinVertex(vert0, nullptr),
                        SkinVertex(vert1, nullptr),
                        SkinVertex(vert2, nullptr)
                    );
                } else
                    tri.Set(vert0.pos, vert1.pos, vert2.pos);
                float fintersect;
                if (Intersect(sega0, tri, false, fintersect)) {
                    Interp(sega0.start, sega0.end, fintersect, sega0.end);
                    f *= fintersect;
                    pl = Plane(tri.origin, tri.frame.z);
                    b1 = true;
                    sLastCollide = (it - Faces().begin());
                }
            }
            if (b1) {
                if (!sRawCollide)
                    Multiply(pl, WorldXfm(), pl);
                return this;
            }
        }
    }
    return 0;
}

int RndMesh::CollidePlane(const RndMesh::Face &face, const Plane &plane) {
    bool first = plane.Dot(Verts(face.v1).pos) >= 0;
    bool second = plane.Dot(Verts(face.v2).pos) >= 0;
    bool third = plane.Dot(Verts(face.v3).pos) >= 0;
    if (first == second && second == third) {
        if (first) {
            return 1;
        } else {
            return -1;
        }
    }
    return 0;
}

int RndMesh::CollidePlane(const Plane &plane) {
    int super = RndDrawable::CollidePlane(plane);
    if (super != 0)
        return super;
    Transform tf48;
    FastInvert(WorldXfm(), tf48);
    Plane pl58;
    Multiply(plane, tf48, pl58);
    if (GetVolume() == kVolumeTriangles) {
        if (Faces().empty())
            return -1;
        else {
            std::vector<Face>::iterator faceIt = Faces().begin();
            int faceColl = CollidePlane(*faceIt, pl58);
            if (faceColl == 0)
                return 0;
            else {
                ++faceIt;
                for (; faceIt != Faces().end(); ++faceIt) {
                    if (faceColl != CollidePlane(*faceIt, pl58)) {
                        return 0;
                    }
                }
                return faceColl;
            }
        }
    } else
        return 0;
}

void RndMesh::VertVector::operator=(const RndMesh::VertVector &c) {
    MILO_ASSERT(mCapacity == 0, 0x26D);
    MILO_ASSERT(c.mCapacity == 0, 0x26E);
    if (c.mNumVerts != mNumVerts) {
        delete mVerts;
        mNumVerts = c.mNumVerts;
        mVerts = new Vert[mNumVerts];
    }
    Vert *otherVerts = c.mVerts;
    Vert *otherEnd = &otherVerts[mNumVerts];
    Vert *myVerts = mVerts;
    while (otherVerts != otherEnd) {
        *myVerts++ = *otherVerts++;
    }
}

void RndMesh::VertVector::resize(int n) {
    if (mCapacity > 0) {
        MILO_ASSERT(n <= mCapacity, 0x227);
        mNumVerts = n;
    } else if (n == 0) {
        delete[] mVerts;
        mVerts = nullptr;
        mNumVerts = 0;
    } else if (n != mNumVerts) {
        Vert *oldverts = mVerts;
        Vert *oldit = oldverts;
        Vert *end = oldverts + Min(n, size());
        mVerts = new Vert[n];
        mNumVerts = n;

        Vert *newit = mVerts;
        while (oldit != end) {
            *newit++ = *oldit++;
        }

        delete[] oldverts;
    }
}

int RndMesh::EstimatedSizeKb() const {
    // sizeof(Vert) is 0x50 here
    // but the actual struct is size 0x60
    return (NumVerts() * 0x50 + NumFaces() * (int)sizeof(Face)) / 1024;
}

void RndMesh::ClearCompressedVerts() {
    delete[] mCompressedVerts;
    mCompressedVerts = nullptr;
    mNumCompressedVerts = 0;
}

bool RndMesh::Replace(ObjRef *ref, Hmx::Object *obj) {
    if (RefIs(ref, mGeomOwner)) {
        RndMesh *meshObj;
        if (mGeomOwner == this
            || (meshObj = dynamic_cast<RndMesh *>(obj)) == nullptr) {
            mGeomOwner = this;
        } else {
            mGeomOwner = meshObj->mGeomOwner;
        }
        return true;
    }
    return RndTransformable::Replace(ref, obj);
}

void RndMesh::SetMat(RndMat *mat) { mMat = mat; }

void RndMesh::SetGeomOwner(RndMesh *m) {
    MILO_ASSERT(m, 0x1D7);
    mGeomOwner = m;
}

void RndMesh::SetKeepMeshData(bool keep) {
    if (keep != mKeepMeshData) {
        mKeepMeshData = keep;
        if (!mKeepMeshData) {
            // Clear mesh data and free memory when keep_mesh_data is disabled
            mVerts.clear();
            std::vector<Face>().swap(mFaces);        // Clear and shrink to free memory
            std::vector<unsigned char>().swap(mPatches); // Clear and shrink to free memory
        }
    }
}

void RndMesh::SetNumVerts(int num) {
    Verts().resize(num);
    Sync(0x3F);
}

void RndMesh::SetNumFaces(int num) {
    mGeomOwner->mFaces.resize(num);
    Sync(0x3F);
}

void RndMesh::SetNumBones(int num) {
    if (num > MaxBones()) {
        MILO_NOTIFY("%s: exceeds bone limit of %d", PathName(this), MaxBones());
    }
    mBones.resize(num);
}

void RndMesh::ScaleBones(float f) {
    for (ObjVector<RndBone>::iterator it = mBones.begin(); it != mBones.end(); ++it) {
        it->mOffset.v *= f;
    }
}

void RndMesh::SetZeroWeightBones() {
    if (mBones.size() >= 2) {
        for (int i = 0; i < mVerts.size(); i++) {
            Vert &curvert = mVerts[i];
            if (curvert.boneWeights.y == 0)
                curvert.boneIndices[1] = curvert.boneIndices[0];
            if (curvert.boneWeights.z == 0)
                curvert.boneIndices[2] = curvert.boneIndices[0];
            if (curvert.boneWeights.w == 0)
                curvert.boneIndices[3] = curvert.boneIndices[0];
        }
    }
}

void RndMesh::ResetNormals() {
    if (mGeomOwner != this) {
        MILO_NOTIFY("Must be geom owner to reset normals");
    } else {
        ::ResetNormals(this);
    }
}

void RndMesh::Tessellate() {
    if (mGeomOwner != this) {
        MILO_NOTIFY("Must be geom owner to tessellate");
    } else {
        TessellateMesh(this);
    }
}

void RndMesh::ClearAO() {
    if (mGeomOwner != this) {
        MILO_NOTIFY("Must be geom owner to clear AO");
    } else {
        ::ClearAO(this);
    }
}

int RndMesh::GetBoneIndex(const RndTransformable *t) {
    for (int i = 0; i < mBones.size(); i++) {
        if (mBones[i].mBone == t) {
            return i;
        }
    }
    return -1;
}

void RndMesh::Sync(int flags) {
    if (mKeepMeshData) {
        flags |= 0x200;
    }
    OnSync(flags);
    if (flags | 0x1F) {
        Verts().unkc = 0;
    }
}

bool RndMesh::HasValidBones(unsigned int *boneIdx) const {
    int idx = 0;
    for (ObjVector<RndBone>::const_iterator it = mBones.begin(); it != mBones.end();
         ++it, ++idx) {
        if (!it->mBone) {
            if (boneIdx)
                *boneIdx = idx;
            return false;
        }
    }
    if (boneIdx)
        *boneIdx = mBones.size();
    return true;
}

void RndMesh::BurnXfm() {
    if (mGeomOwner != this) {
        MILO_NOTIFY("Must be geom owner to burn xfm");
    } else {
        for (std::list<RndTransformable *>::iterator it = mChildren.begin();
             it != mChildren.end();
             ++it) {
            Transform xfm;
            Multiply((*it)->LocalXfm(), mLocalXfm, xfm);
            (*it)->SetLocalXfm(xfm);
        }
        ::BurnXfm(this, false);
    }
}

void RndMesh::SetBone(int idx, RndTransformable *bone, bool b3) {
    mBones[idx].mBone = bone;
    if (b3) {
        Transform xfm;
        Invert(bone->WorldXfm(), xfm);
        Multiply(WorldXfm(), xfm, mBones[idx].mOffset);
    }
}

RndMultiMesh *RndMesh::CreateMultiMesh() {
    RndMesh *owner = mGeomOwner;
    if (!owner->mMultiMesh) {
        owner->mMultiMesh = Hmx::Object::New<RndMultiMesh>();
        owner->mMultiMesh->SetMesh(owner);
    }
    owner->mMultiMesh->Instances().resize(0);
    return owner->mMultiMesh;
}

void RndMesh::RemoveInvalidBones() {
    for (ObjVector<RndBone>::iterator it = mBones.begin(); it != mBones.end();) {
        if (it->mBone)
            ++it;
        else {
            it = mBones.erase(it);
        }
    }
}

void RndMesh::CopyGeometryFromOwner() {
    if (mGeomOwner != this) {
        CopyGeometry(mGeomOwner, true);
        Sync(0x3F);
    }
}

void RndMesh::CopyGeometry(const RndMesh *mesh, bool b2) {
    mGeomOwner = this;
    mVerts = mesh->mGeomOwner->mVerts;
    mFaces = mesh->mGeomOwner->mFaces;
    mPatches = mesh->mGeomOwner->mPatches;
    if (b2)
        SetVolume(mesh->mGeomOwner->mVolume);
    mBones = mesh->mBones;
}

void RndMesh::SetVolume(RndMesh::Volume vol) {
    if (mGeomOwner != this)
        mGeomOwner->SetVolume(vol);
    else {
        mVolume = vol;
        RELEASE(mBSPTree);
        if (mVerts.empty() || mFaces.empty())
            return;
        else {
            if (mVolume == kVolumeBox) {
                Box box;
                FOREACH (it, mVerts) {
                    box.GrowToContain(it->pos, &it->pos == &mVerts.begin()->pos);
                }
                mBSPTree = new BSPNode();
                BSPNode *bspIt = mBSPTree;
                for (int i = 0; i < 6; i++) {
                    Vector3 vb0;
                    vb0.Zero();
                    vb0[i % 3] = i > 2 ? -1.0f : 1.0f;
                    const Vector3 &point = i > 2 ? box.mMin : box.mMax;
                    bspIt->plane = Plane(vb0, point);
                    bspIt->left = 0;
                    if (i == 5) {
                        bspIt->right = 0;
                    } else {
                        bspIt->right = new BSPNode();
                        bspIt = bspIt->right;
                    }
                }
            } else if (mVolume == kVolumeBSP) {
                std::list<BSPFace> faces;
                for (int i = mFaces.size() - 1; i >= 0; i--) {
                    Face &curFace = mFaces[i];
                    const Vector3 &v1 = mVerts[curFace.v1].pos;
                    const Vector3 &v2 = mVerts[curFace.v2].pos;
                    const Vector3 &v3 = mVerts[curFace.v3].pos;
                    BSPFace bspFace;
                    bspFace.Set(v1, v2, v3);
                    faces.push_back(bspFace);
                }
                if (!MakeBSPTree(mBSPTree, faces, 0)) {
                    RELEASE(mBSPTree);
                }
                if (mBSPTree) {
                    Box boxa0;
                    FOREACH (it, mVerts) {
                        boxa0.GrowToContain(it->pos, &it->pos == &mVerts.begin()->pos);
                    }
                    if (!CheckBSPTree(mBSPTree, boxa0)) {
                        MILO_LOG("BSP tree outside bounding box");
                        RELEASE(mBSPTree);
                    }
                }
                if (mBSPTree) {
                    int x = 0, y = 0;
                    NumNodes(mBSPTree, x, y);
                    TheDebug << MakeString(
                        "Made BSP tree for \"%s\" (nodes:%d depth:%d)\n", Name(), x, y
                    );
                } else {
                    MILO_WARN("Couldn't make BSP tree for \"%s\"", Name());
                    mVolume = kVolumeEmpty;
                }
            }
        }
    }
}

#ifndef HX_NATIVE
void RndMesh::OnSync(int flags) {
    if (mGeomOwner != this || (flags & 0x80U) || !(flags & 0x20U))
        return;
    mPatches.clear();
    if (PatchOkay(mVerts.size(), mFaces.size())) {
        mPatches.push_back(mFaces.size());
    } else if (flags & 0x100U) {
        int u13 = 0xFFFF;
        int i12 = 0;
        int i4 = 0;
        FOREACH (it, mFaces) {
            i12 = Max(it->v3, Max<u16>(i12, it->v1, it->v2));
            u13 = Min(Min<u16>(u13, it->v1, it->v2), it->v3);
            if (!PatchOkay((i12 - u13) + 1, i4 + 1)) {
                mPatches.push_back(i4);
                i12 = Max(it->v1, it->v2, it->v3);
                u13 = Min(it->v1, it->v2, it->v3);
                i4 = 1;
            } else
                i4++;
        }
        mPatches.push_back(i4);
    } else {
        gPatchVerts.Clear();
        std::vector<Face> faces;
        Vector3 v40(0, 0, 0);
        int i4 = 0;
        while (true) {
            if (mFaces.empty())
                break;
            int u5 = 4;
            float f68 = 0;
            std::vector<Face>::iterator faceIt = mFaces.begin();
            std::vector<Face>::iterator bestFaceIt = mFaces.begin();
            for (; faceIt != mFaces.end(); ++faceIt) {
                int uvar16 = !gPatchVerts.HasVert(faceIt->v1)
                    + !gPatchVerts.HasVert(faceIt->v2) + !gPatchVerts.HasVert(faceIt->v3);
                if (uvar16 < u5) {
                    Vector3 v4c;
                    FaceCenter(this, &*faceIt, v4c);
                    Vector3 diff;
                    Subtract(v4c, v40, diff);
                    f68 = LengthSquared(diff);
                    u5 = uvar16;
                    bestFaceIt = faceIt;
                } else if (uvar16 == u5) {
                    Vector3 v58;
                    FaceCenter(this, &*faceIt, v58);
                    Vector3 diff2;
                    Subtract(v58, v40, diff2);
                    if (MinEq(f68, LengthSquared(diff2))) {
                        u5 = uvar16;
                        bestFaceIt = faceIt;
                    }
                }
            }
            faceIt = bestFaceIt;
            if (!PatchOkay(u5 + gPatchVerts.NumVerts(), i4 + 1)) {
                gPatchVerts.Clear();
                mPatches.push_back(i4);
                i4 = 0;
            }
            for (int i = 0; i < 3; i++) {
                auto& vertIdx = (*faceIt)[i];
                if (!gPatchVerts.HasVert(vertIdx)) {
                    gPatchVerts.Add(vertIdx, mVerts, v40);
                }
            }
            faces.push_back(*faceIt);
            mFaces.erase(faceIt);
            i4++;
            if (mFaces.empty())
                break;
        }
        mPatches.push_back(i4);
        mFaces = faces;
    }
}
#endif

void RndMesh::DeleteBones(bool findRoot) {
    auto& bones = mBones;
    if (bones.empty())
        return;

    std::vector<RndTransformable *> boneTransforms(bones.size(), NULL);
    for (unsigned int i = 0; i < boneTransforms.size(); i++) {
        boneTransforms[i] = bones[i].mBone;
    }

    RndTransformable *root = NULL;
    if (findRoot) {
        RndTransformable *parent = bones[0].mBone;
        while (parent) {
            RndTransformable *transParent = dynamic_cast<RndTransformable *>(parent->TransParent());
            root = parent;
            if (transParent)
                break;
            MILO_ASSERT(parent != parent->TransParent(), 0x5a1);
            parent = parent->TransParent();
        }
    }

    bones.erase(bones.begin(), bones.end());

    for (unsigned int i = 0; i < boneTransforms.size(); i++) {
        if (boneTransforms[i]) {
            delete boneTransforms[i];
        }
    }

    if (root) {
        delete root;
    }
}

void RndMesh::InstanceGeomOwnerBones() {
    if (!mGeomOwner) {
        MILO_WARN("Cannot duplicate bones if mesh is not a Geom Owner!");
        return;
    }

    ObjectDir *dir = dynamic_cast<ObjectDir *>(Dir());
    if (!dir) {
        MILO_NOTIFY("Cannot duplicate bones if parent Dir is not a RndDir.");
        return;
    }

    if (mBones.empty())
        return;

    bool needsCopy = mGeomOwner && mGeomOwner->mBones[0].mBone != mBones[0].mBone;
    if (needsCopy) {
        DeleteBones(true);
        if (!(!mGeomOwner)) {
            mBones = mGeomOwner->mBones;
        } else {
            mBones.erase(mBones.begin(), mBones.end());
        }
    }

    // Find the root bone in the geom owner's hierarchy
    RndTransformable *oldRoot = nullptr;
    for (RndTransformable *parent = mGeomOwner->mBones[0].mBone; nullptr != parent;
         parent = parent->TransParent()) {
        RndTransformable *transParent =
            dynamic_cast<RndTransformable *>(parent->TransParent());
        oldRoot = parent;
        if (transParent)
            break;
    }
    MILO_ASSERT(oldRoot, 0x5e9);

    // Create new root
    RndTransformable *newRoot = Hmx::Object::New<RndTransformable>();
    newRoot->SetName(NextName(oldRoot->Name(), Dir()), Dir());
    newRoot->Copy(oldRoot, Hmx::Object::kCopyDeep);

    // Parent new root under the RndDir
    RndTransformable *dirTrans = dynamic_cast<RndTransformable *>(Dir());
    newRoot->SetTransParent(dirTrans, false);

    // Create new bone transforms for each bone
    for (unsigned int i = 0; i < mBones.size(); i++) {
        RndTransformable *newBone = Hmx::Object::New<RndTransformable>();
        newBone->SetName(NextName(mGeomOwner->mBones[i].mBone->Name(), Dir()), Dir());
        newBone->Copy(mGeomOwner->mBones[i].mBone, Hmx::Object::kCopyDeep);
        mBones[i].mBone = newBone;

        // Find parent in owner hierarchy and reparent
        int parentIdx = mGeomOwner->GetBoneIndex(mGeomOwner->mBones[i].mBone->TransParent());
        RndTransformable *parent;
        parent = !((parentIdx != -1)) ? newRoot : (RndTransformable *)mBones[parentIdx].mBone;
        newBone->SetTransParent(parent, false);
    }
}

DataNode RndMesh::OnCompareEdgeVerts(const DataArray *da) {
    std::vector<int> vec20(Verts().size(), -1);
    std::list<int> vec28;
    std::vector<std::list<int> > vec30(Verts().size());
    for (int i = 0; i < Verts().size(); i++) {
        if (vec20[i] == -1) {
            vec20[i] = i;
            for (int j = i; j < Verts().size(); j++) {
                if (Verts(j).pos == Verts(i).pos) {
                    vec20[j] = i;
                }
            }
        }
    }
    FOREACH (it, Faces()) {
        int i40 = vec20[it->v1];
        int i44 = vec20[it->v2];
        int i48 = vec20[it->v3];
        vec30[i40].push_back(i44);
        vec30[i40].push_back(i48);
        vec30[i44].push_back(i40);
        vec30[i44].push_back(i48);
        vec30[i48].push_back(i40);
        vec30[i48].push_back(i44);
    }
    for (int i = 0; i < Verts().size(); i++) {
        vec30[i].sort();
        vec30[i].unique();
    }
    for (int i = 0; i < Verts().size(); i++) {
        FOREACH (it, vec30[i]) {
            int i10 = 0;
            FOREACH (it2, vec30[*it]) {
                FOREACH (it3, vec30[i]) {
                    if (*it3 != *it) {
                        if (*it3 == *it2) {
                            i10++;
                            break;
                        }
                    }
                }
            }
            if (i10 < 2) {
                vec28.push_back(i);
                break;
            }
        }
    }
    DataArray *array = da->Array(2);
    for (int i = 0; i < array->Size(); i++) {
        RndMesh *curMesh = array->Obj<RndMesh>(i);
        auto debugMsg = MakeString("testing %s\n", curMesh->Name());
        TheDebug << debugMsg;
        FOREACH (it, vec28) {
            if (Verts(*it).pos == curMesh->Verts(*it).pos)
                continue;
            else
                TheDebug << MakeString("   %d doesn't match position\n", *it);
        }
    }
    if (mGeomOwner != this && (mVerts.size() != 0 || mFaces.size() != 0)) {
        if (mGeomOwner)
            mGeomOwner->Name();
        PathName(this);
    }
    return 0;
}

DataNode RndMesh::OnPointCollide(const DataArray *da) {
    BSPNode *tree = GetBSPTree();
    Vector3 v(da->Float(2), da->Float(3), da->Float(4));
    MultiplyTranspose(v, WorldXfm(), v);
    return tree && Intersect(v, tree);
}

DataNode RndMesh::OnBuildFromBSP(const DataArray *a) {
    BuildFromBSP(this);
    return 0;
}

DataNode RndMesh::OnAttachMesh(const DataArray *da) {
    RndMesh *m = da->Obj<RndMesh>(2);
    AttachMesh(this, m);
    delete m;
    return 0;
}

DataNode RndMesh::OnGetVertNorm(const DataArray *da) {
    Vert *v;
    s32 index = da->Int(2);
    MILO_ASSERT(index >= 0 && index < mVerts.size(), 0x858);
    v = &mVerts[index];
    *da->Var(3) = v->norm.x;
    *da->Var(4) = v->norm.y;
    *da->Var(5) = v->norm.z;
    return 0;
}

DataNode RndMesh::OnSetVertNorm(const DataArray *da) {
    Vert *v;
    s32 index = da->Int(2);
    MILO_ASSERT(index >= 0 && index < mVerts.size(), 0x863);
    v = &mVerts[index];
    v->norm.x = da->Float(3);
    v->norm.y = da->Float(4);
    v->norm.z = da->Float(5);
    Sync(31);
    return 0;
}

DataNode RndMesh::OnGetVertXYZ(const DataArray *da) {
    Vert *v;
    s32 index = da->Int(2);
    MILO_ASSERT(index >= 0 && index < mVerts.size(), 0x86F);
    v = &mVerts[index];
    *da->Var(3) = v->pos.x;
    *da->Var(4) = v->pos.y;
    *da->Var(5) = v->pos.z;
    return 0;
}

DataNode RndMesh::OnSetVertXYZ(const DataArray *da) {
    Vert *v;
    s32 index = da->Int(2);
    MILO_ASSERT(index >= 0 && index < mVerts.size(), 0x87A);
    v = &mVerts[index];
    v->pos.x = da->Float(3);
    v->pos.y = da->Float(4);
    v->pos.z = da->Float(5);
    Sync(31);
    return 0;
}

DataNode RndMesh::OnGetVertUV(const DataArray *da) {
    Vert *v;
    s32 index = da->Int(2);
    MILO_ASSERT(index >= 0 && index < mVerts.size(), 0x886);
    v = &mVerts[index];
    *da->Var(3) = v->tex.x;
    *da->Var(4) = v->tex.y;
    return 0;
}

DataNode RndMesh::OnSetVertUV(const DataArray *da) {
    Vert *v;
    s32 index = da->Int(2);
    MILO_ASSERT(index >= 0 && index < mVerts.size(), 0x890);
    v = &mVerts[index];
    v->tex.x = da->Float(3);
    v->tex.y = da->Float(4);
    Sync(31);
    return 0;
}

DataNode RndMesh::OnGetFace(const DataArray *da) {
    Face *f;
    int index = da->Int(2);
    MILO_ASSERT(index >= 0 && index < mFaces.size(), 0x89B);
    f = &mFaces[index];
    *da->Var(3) = f->v1;
    *da->Var(4) = f->v2;
    *da->Var(5) = f->v3;
    return 0;
}

DataNode RndMesh::OnSetFace(const DataArray *da) {
    Face *f;
    int index = da->Int(2);
    MILO_ASSERT(index >= 0 && index < mFaces.size(), 0x8A6);
    f = &mFaces[index];
    f->v1 = da->Int(3);
    f->v2 = da->Int(4);
    f->v3 = da->Int(5);
    Sync(32);
    return 0;
}

DataNode RndMesh::OnUnitizeNormals(const DataArray *da) {
    for (auto it = Verts().begin(); it != Verts().end(); ++it) {
        Normalize(it->norm, it->norm);
    }
    return 0;
}

DataNode RndMesh::OnConfigureMesh(const DataArray *da) {
    static Symbol configurable_mesh("configurable_mesh");
    if (Type() != configurable_mesh)
        MILO_NOTIFY("Can't configure nonconfigurable mesh %s\n", Name());
    else {
        static Symbol left("left");
        static Symbol right("right");
        static Symbol height("height");
        float fleft = Property(left, true)->Float();
        float fright = Property(right, true)->Float();
        float fheight = Property(height, true)->Float();
        Vector3 v54(fleft, 0, fheight);
        Vector3 v60(fleft, 0, 0);
        Vector3 v6c(fright, 0, 0);
        Vector3 v78(fright, 0, fheight);
        mVerts[0].pos = v54;
        mVerts[1].pos = v60;
        mVerts[2].pos = v6c;
        mVerts[3].pos = v78;
        Sync(0x3F);
    }
    return 0;
}

void PackVector(
    unsigned int &output,
    const Vector4 &vec,
    unsigned char bitsX,
    unsigned char bitsY,
    unsigned char bitsZ,
    unsigned char bitsW,
    bool normalize
) {
    if ((u32)(bitsX + bitsY + bitsZ + bitsW) != 0x20U) {
        MILO_ASSERT(0, 0x39);
    }

    s32 offsetY = bitsX;
    s32 offsetZ = bitsX + bitsY;
    s32 offsetW = bitsX + bitsY + bitsZ;
    s32 normFactor = normalize ? 1 : 0;

    if ((u32)(bitsW + offsetW) != 0x20U) {
        MILO_ASSERT(0, 0x4E);
    }

    s32 shiftZ = bitsZ - normFactor;
    s32 shiftX = bitsX - normFactor;
    s32 shiftW = bitsW - normFactor;
    s32 shiftY = bitsY - normFactor;

    s32 maxZ = (1 << shiftZ) - 1;
    s32 maxX = (1 << shiftX) - 1;
    s32 maxW = (1 << shiftW) - 1;
    s32 maxY = (1 << shiftY) - 1;

    f64 dz = (f64)maxZ;
    f64 dx = (f64)maxX;
    f64 dw = (f64)maxW;
    f64 dy = (f64)maxY;

    f32 fz = (f32)dz;
    f32 fx = (f32)dx;
    f32 fw = (f32)dw;
    f32 fy = (f32)dy;

    u32 px = ((u32)(vec.x * fx)) & ((1U << bitsX) - 1);
    u32 py = ((u32)(vec.y * fy)) & ((1U << bitsY) - 1);
    u32 pz = ((u32)(vec.z * fz)) & ((1U << bitsZ) - 1);
    u32 pw = ((u32)(vec.w * fw)) & ((1U << bitsW) - 1);

    output = (pw << offsetW) | (pz << offsetZ) | (py << offsetY) | px;
}

static inline unsigned short FloatToHalf(float value) {
    unsigned int raw;
    memcpy(&raw, &value, sizeof(float));
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

void FillCompressedVertex(CompressedVertex_Xbox &compressed, const RndMesh::Vert &vert, bool normalize) {
    // Pack color (ARGB D3DCOLOR format)
    u32 blue = (u32)(vert.color.blue * 255.0f);
    u32 red = (u32)(vert.color.red * 255.0f);
    compressed.mColor =
        (((((u32)(vert.color.alpha * 255.0f) << 8) | (red & 0xFF)) << 8)
        | ((u32)(vert.color.green * 255.0f) & 0xFF)) << 8
        | (blue & 0xFF);

    // Pack bone weights as UDEC4N (field name is misleading — mBoneIndices stores weights)
    PackVector(
        (unsigned int &)compressed.mBoneIndices, vert.boneWeights, 10, 10, 10, 2, false
    );

    // Copy position as float bit patterns
    *(f32 *)(&compressed.mPosX) = vert.pos.x;
    *(f32 *)(&compressed.mPosY) = vert.pos.y;
    *(f32 *)(&compressed.mPosZ) = vert.pos.z;

    // Pack UV as FLOAT16_2 (field name is misleading — mNormal stores UVs)
    unsigned short halfU = FloatToHalf(vert.tex.x);
    unsigned short halfV = FloatToHalf(vert.tex.y);
    compressed.mNormal = (halfU << 16) | halfV;

    // Pack normal as DEC4N (field name is misleading — mTangent stores normals)
    Vector4 normVec(vert.norm.x, vert.norm.y, vert.norm.z, 0.0f);
    PackVector((unsigned int &)compressed.mTangent, normVec, 10, 10, 10, 2, true);

    // Pack tangent as DEC4N (field name is misleading — mBinormal stores tangents)
    PackVector((unsigned int &)compressed.mBinormal, vert.tangent, 10, 10, 10, 2, true);

    // Pack bone indices as UBYTE4 (field name is misleading — mBoneWeights stores indices)
    compressed.mBoneWeights = (((int)vert.boneIndices[3] * 0x100
        + (int)vert.boneIndices[2]) * 0x100
        + (int)vert.boneIndices[1]) * 0x100
        + (int)vert.boneIndices[0];
}

void RndMesh::LoadVertices(BinStreamRev &d) {
    int count;
    d.stream.ReadEndian(&count, 4);
    bool b58;
    if (d.rev > 0x22) {
        d >> b58;
    } else {
        b58 = false;
    }
#ifdef HX_NATIVE
    // Native: preserve Xbox compressed vertex blobs and let the native renderer
    // unpack them during GPU upload. The older eager decompression path lost
    // skinned bone data on character meshes.
    if (b58) {
        unsigned int loadedCompressedSize = 0;
        unsigned int loadedVersion = 0;
        d.stream.ReadEndian(&loadedCompressedSize, 4);
        d.stream.ReadEndian(&loadedVersion, 4);

        if (loadedCompressedSize == sizeof(CompressedVertex_Xbox) && loadedVersion == 1) {
            mNumCompressedVerts = count;
            mVerts.resize(0);
            if (mNumCompressedVerts != 0) {
                unsigned int totalSize = loadedCompressedSize * count;
                MILO_ASSERT(totalSize > 0, 0x2D4);
                MemPushTemp();
                mCompressedVerts = new unsigned char[totalSize];
                MemPopTemp();
                ReadChunks(d.stream, mCompressedVerts, totalSize, loadedCompressedSize << 9);
            }
        } else if (count > 0 && loadedCompressedSize > 0) {
            unsigned int totalSize = loadedCompressedSize * count;
            MILO_NOTIFY(
                "%s: unsupported native compressed vertex format size=%u version=%u",
                PathName(this),
                loadedCompressedSize,
                loadedVersion
            );
            MILO_ASSERT(totalSize > 0, 0x2E7);
            d.stream.Seek(totalSize, BinStream::kSeekCur);
        } else if (count > 0) {
            // loadedCompressedSize == 0 but count > 0: skip (shouldn't happen)
            mVerts.resize(0);
        }
    } else {
        mVerts.resize(count);
        int i = 0;
        for (Vert *it = mVerts.begin(); it != mVerts.end(); ++it) {
            d >> *it;
            i++;
            if (!(i & 0x1FF)) {
#ifdef HX_NATIVE
                d.stream.WaitUntilReady();
#else
                while (d.stream.Eof() == TempEof)
                    Timer::Sleep(0);
#endif
            }
        }
    }
#else
    unsigned int loadedCompressedSize = 0;
    unsigned int loadedVersion = 0;
    unsigned int compressedSize = 0;
    bool b4 = false;
    if (b58) {
        d.stream.ReadEndian(&loadedCompressedSize, 4);
        d.stream.ReadEndian(&loadedVersion, 4);
        MILO_ASSERT(IsVertexCompressionSupported(TheLoadMgr.GetPlatform()), 0x29C);
        if (TheLoadMgr.GetPlatform() != kPlatformXBox) {
            TheDebug.Fail(FormatString("Unsupported platform for vertex compression").Str(), 0);
            b4 = false;
        } else {
            compressedSize = 0x24;
            b4 = true;
        }
                unsigned int versionCheck;
        if ((TheLoadMgr.GetPlatform() == kPlatformXBox)) {
            versionCheck = 1U;
        } else {
            versionCheck = 0U;
        }
        if (compressedSize != loadedCompressedSize || versionCheck != loadedVersion) {
            b4 = false;
        }
        if (!b4) {
            TheDebug.Notify(MakeString(
                "Loaded stale compressed vertex data, resave mesh file \"%s\""
                "(loaded size = %d, current = %d; loaded ver = %d, current = %d",
                d.stream.Name(), loadedCompressedSize, compressedSize,
                loadedVersion, (unsigned int)b4
            ));
        }
    }
    if (b58) {
        if (b4) {
            mNumCompressedVerts = count;
            if (mNumCompressedVerts != 0) {
                unsigned int totalSize = compressedSize * count;
                MILO_ASSERT(totalSize > 0, 0x2D4);
                MemPushTemp();
                mCompressedVerts = new unsigned char[totalSize];
                MemPopTemp();
                ReadChunks(d.stream, mCompressedVerts, totalSize, compressedSize << 9);
            }
        } else {
            loadedCompressedSize *= count;
            MILO_ASSERT(loadedCompressedSize > 0, 0x2E7);
            d.stream.Seek(loadedCompressedSize, BinStream::kSeekCur);
        }
    } else {
        mVerts.resize(count);
        int i = 0;
        for (Vert *it = mVerts.begin(); it != mVerts.end(); ++it) {
            d >> *it;
            i++;
            if (!(i & 0x1FF)) {
#ifdef HX_NATIVE
                d.stream.WaitUntilReady();
#else
                while (d.stream.Eof() != 0)
                    Timer::Sleep(0);
#endif
            }
        }
    }
#endif
}

void RndMesh::SaveVertices(BinStream &bs) {
    VertVector *verts = &mVerts;
    unsigned int value;
    bool doCompress;
    bool cached;
    if (bs.Cached() && (bs.GetPlatform() == kPlatformPS3 || bs.GetPlatform() == kPlatformXBox)) {
        cached = true;
    } else {
        cached = false;
    }

    bool hasMeshData;
    if ((mMutable & 0x1F) > 0 || (hasMeshData = false, mKeepMeshData == true)) {
        hasMeshData = true;
    }

    if (TheLoadMgr.GetPlatform() == kPlatformXBox
        || (doCompress = false, TheLoadMgr.GetPlatform() == kPlatformPS3)) {
        doCompress = true;
    }
    if ((!doCompress) || (!cached) || (doCompress = true, hasMeshData)) {
        doCompress = false;
    }

    value = verts->mNumVerts;
    bs.WriteEndian(&value, 4);
    bool compress = doCompress;
    bs.Write(&compress, 1);
    if (compress) {
        unsigned int compressedSize = 0;
        bool isXBox;
        if (TheLoadMgr.GetPlatform() != kPlatformXBox) {
            FormatString str("Unsupported platform for vertex compression");
            int line;
            isXBox = false;
            TheDebug.Fail(str.Str(), 0);
            line = 0x339;
            TheDebug.Fail(MakeString(kAssertStr, "Mesh.cpp", line, "compressedSize > 0"), 0);
            line = 0x33A;
            TheDebug.Fail(MakeString(kAssertStr, "Mesh.cpp", line, "compressedVersion > 0"), 0);
        } else {
            compressedSize = 0x24;
            isXBox = true;
        }
        value = compressedSize;
        bs.WriteEndian(&value, 4);
        value = isXBox;
        bs.WriteEndian(&value, 4);
    }

    unsigned int i = 0;
#ifdef HX_NATIVE
    if (verts->mNumVerts != 0) {
        Vert *it = verts->mVerts;
        do {
#else
    Vert *it = verts->mVerts;
    if (it != verts->mVerts + verts->mNumVerts) {
        do {
#endif
            if (cached && compress) {
                if (TheLoadMgr.GetPlatform() != kPlatformXBox) {
                    FormatString str("Unsupported platform for vertex compression");
                    TheDebug.Fail(str.Str(), 0);
                } else {
                    static CompressedVertex_Xbox compressed;
                    FillCompressedVertex(compressed, *it, true);
                    SaveCompressedVertex(compressed, bs);
                }
            } else {
                bs << *it;
            }
            i++;
            if (bs.GetPlatform() == kPlatformWii && !(i & 0x1FF)) {
                MarkChunk(bs);
            }
            ++it;
        } while (it != verts->mVerts + verts->mNumVerts);
    }
}
