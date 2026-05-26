#include "rndobj/AmbientOcclusion.h"
#include "math/Geo.h"
#include "math/Mtx.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Dir.h"
#include "rndobj/Group.h"
#include "rndobj/Mesh.h"
#include "rndobj/PropAnim.h"
#include "rndobj/Trans.h"
#include "rndobj/TransAnim.h"
#include "rndobj/Utl.h"
#include "world/Instance.h"
#include "os/Timer.h"
#include <float.h>
#include <set>
#include "utl/Std.h"

void BuildSphereStratified(unsigned int, std::vector<Vector3> &);

// Quality parameters: [samples_q0, samples_q1, splitPlane_q0, splitPlane_q1]
// Values are guesses; exact values in .rdata at 0x820A658C (16 bytes)
static const int kQualityLUT[] = { 256, 1024, 0, 2 };

// PPC: Edge::operator< lives in Utl.cpp (matching original link unit).
// Native: define it here since AmbientOcclusion.cpp is the natural home.
#ifdef HX_NATIVE
bool RndAmbientOcclusion::Edge::operator<(const Edge &other) const {
    short aMin = v0 < v1 ? v0 : v1;
    short aMax = v0 < v1 ? v1 : v0;
    short bMin = other.v0 < other.v1 ? other.v0 : other.v1;
    short bMax = other.v0 < other.v1 ? other.v1 : other.v0;
    unsigned int a = ((unsigned int)(unsigned short)aMax << 16) | (unsigned short)aMin;
    unsigned int b = ((unsigned int)(unsigned short)bMax << 16) | (unsigned short)bMin;
    return a < b;
}
#endif

void RndAmbientOcclusion::BlendVert(
    const RndMesh::Vert &v1, const RndMesh::Vert &v2, RndMesh::Vert &out
) {
    memcpy(&out, &v1, sizeof(RndMesh::Vert));
    out.pos.z = out.pos.z + v2.pos.z;
    out.pos.y = out.pos.y + v2.pos.y;
    out.pos.x = out.pos.x + v2.pos.x;
    out.tex.x = out.tex.x + v2.tex.x;
    out.tex.y = out.tex.y + v2.tex.y;
    out.color.green = out.color.green + v2.color.green;
    out.color.red = out.color.red + v2.color.red;
    out.color.alpha = out.color.alpha + v2.color.alpha;
    out.color.blue = out.color.blue + v2.color.blue;
    out.norm.x = out.norm.x + v2.norm.x;
    out.norm.y = out.norm.y + v2.norm.y;
    out.norm.z = out.norm.z + v2.norm.z;
    Vector3 tang;
    tang.x = v2.tangent.x + out.tangent.x;
    tang.y = v2.tangent.y + out.tangent.y;
    tang.z = v2.tangent.z + out.tangent.z;
    out.pos.x = out.pos.x * 0.5f;
    out.pos.y = out.pos.y * 0.5f;
    out.pos.z = out.pos.z * 0.5f;
    out.tex.x = out.tex.x * 0.5f;
    out.tex.y = out.tex.y * 0.5f;
    out.color.blue = out.color.blue * 0.5f;
    out.color.red = out.color.red * 0.5f;
    out.color.green = out.color.green * 0.5f;
    out.color.alpha = out.color.alpha * 0.5f;
    Normalize(out.norm, out.norm);
    Normalize(tang, tang);
    out.tangent.x = tang.x;
    out.tangent.y = tang.y;
    out.tangent.z = tang.z;
    out.color.alpha = 0.0f;
    out.color.blue = 0.0f;
    out.color.green = 0.0f;
    out.color.red = 0.0f;
}

bool IsValidObject(Hmx::Object *obj) {
    RndMesh *mesh = dynamic_cast<RndMesh *>(obj);
    RndGroup *group = dynamic_cast<RndGroup *>(obj);
    return mesh || group || dynamic_cast<WorldInstance *>(obj);
}

template <class T>
unsigned int GatherObjectsFromDir(ObjectDir *dir, std::vector<T *> &objects) {
    RndDir *rDir = dynamic_cast<RndDir *>(dir);
    bool showing = rDir ? rDir->Showing() : true;
    if (showing) {
        for (ObjDirItr<Hmx::Object> it(dir, true); it != NULL; ++it) {
            ObjectDir *curDir = dynamic_cast<ObjectDir *>(&*it);
            if (curDir && curDir != dir
                && dynamic_cast<WorldInstance *>((Hmx::Object *)curDir)) {
                GatherObjectsFromDir(curDir, objects);
            }
            T *curObj = dynamic_cast<T *>(&*it);
            if (curObj) {
                objects.push_back(curObj);
            }
        }
    }
    return objects.size();
}

template <class T>
unsigned int GatherObjectsFromGroup(RndGroup *grp, std::vector<T *> &objects) {
    if (grp->Showing()) {
        std::list<RndDrawable *> drawChildren;
        grp->ListDrawChildren(drawChildren);
        for (std::list<RndDrawable *>::iterator it = drawChildren.begin();
             it != drawChildren.end(); ++it) {
            RndGroup *subGrp = dynamic_cast<RndGroup *>(*it);
            if (subGrp && subGrp != grp) {
                GatherObjectsFromGroup(subGrp, objects);
            }
            ObjectDir *curDir = dynamic_cast<ObjectDir *>(*it);
            if (curDir && dynamic_cast<WorldInstance *>((Hmx::Object *)curDir)) {
                GatherObjectsFromDir(curDir, objects);
            }
            T *curObj = dynamic_cast<T *>(*it);
            if (curObj) {
                objects.push_back(curObj);
            }
        }
    }
    return objects.size();
}

template <class T>
unsigned int GatherObject(Hmx::Object *object, std::vector<T *> &objects) {
    MILO_ASSERT(IsValidObject(object), 0xD1);
    T *templateObj = dynamic_cast<T *>(object);
    if (templateObj) {
        objects.push_back(templateObj);
    } else {
        RndGroup *group = dynamic_cast<RndGroup *>(object);
        if (group) {
            GatherObjectsFromGroup(group, objects);
        } else {
            ObjectDir *dir = dynamic_cast<ObjectDir *>(object);
            if (dir) {
                GatherObjectsFromDir(dir, objects);
            }
        }
    }
    return objects.size();
}

RndAmbientOcclusion::RndAmbientOcclusion()
    : mDontCastAO(this), mDontReceiveAO(this), mTessellate(this),
      mIgnoreTransparent(true), mIgnorePrelit(true), mIgnoreHidden(true),
      mUseMeshNormals(true), mIntersectBackFaces(false), mTessellateTriLimit(8),
      mTessellateTriError(0.67625f), mTessellateTriLarge(gUnitsPerMeter * 2.0f),
      mTessellateTriSmall(gUnitsPerMeter * 0.5f), mTree(0), mQuality((Quality)1) {}

RndAmbientOcclusion::~RndAmbientOcclusion() { Clean(); }

BEGIN_HANDLERS(RndAmbientOcclusion)
    HANDLE(get_valid_objects, OnGetValidObjects)
    HANDLE(get_recv_meshes, OnGetRecvMeshes)
    HANDLE_ACTION(
        calculate, _msg->Size() > 2 ? OnCalculate(_msg->Int(2)) : OnCalculate(true)
    )
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(RndAmbientOcclusion)
    SYNC_PROP(dont_cast_ao, mDontCastAO)
    SYNC_PROP(dont_receive_ao, mDontReceiveAO)
    SYNC_PROP(tessellate, mTessellate)
    SYNC_PROP(ignore_transparent, mIgnoreTransparent)
    SYNC_PROP(ignore_prelit, mIgnorePrelit)
    SYNC_PROP(ignore_hidden, mIgnoreHidden)
    SYNC_PROP(use_mesh_normals, mUseMeshNormals)
    SYNC_PROP(intersect_back_faces, mIntersectBackFaces)
    SYNC_PROP(tessellate_tri_limit, mTessellateTriLimit)
    SYNC_PROP(tessellate_tri_error, mTessellateTriError)
    SYNC_PROP(tessellate_tri_large, mTessellateTriLarge)
    SYNC_PROP(tessellate_tri_small, mTessellateTriSmall)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(RndAmbientOcclusion)
    SAVE_REVS(4, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mDontReceiveAO;
    bs << mDontCastAO;
    bs << mTessellate;
    bs << mIgnoreTransparent;
    bs << mIgnorePrelit;
    bs << mIgnoreHidden;
    bs << mUseMeshNormals;
    bs << mIntersectBackFaces;
    bs << mTessellateTriLimit;
    bs << mTessellateTriError;
    bs << mTessellateTriLarge;
    bs << mTessellateTriSmall;
    bs << mQuality;
END_SAVES

BEGIN_COPYS(RndAmbientOcclusion)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(RndAmbientOcclusion)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mDontReceiveAO)
        COPY_MEMBER(mDontCastAO)
        COPY_MEMBER(mTessellate)
        COPY_MEMBER(mIgnoreTransparent)
        COPY_MEMBER(mIgnorePrelit)
        COPY_MEMBER(mIgnoreHidden)
        COPY_MEMBER(mUseMeshNormals)
        COPY_MEMBER(mIntersectBackFaces)
        COPY_MEMBER(mTessellateTriLimit)
        COPY_MEMBER(mTessellateTriError)
        COPY_MEMBER(mTessellateTriLarge)
        COPY_MEMBER(mTessellateTriSmall)
        COPY_MEMBER(mQuality)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(4, 0)

BEGIN_LOADS(RndAmbientOcclusion)
    LOAD_REVS(bs)
    ASSERT_REVS(4, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    d >> mDontReceiveAO;
    d >> mDontCastAO;
    d >> mTessellate;
    d >> mIgnoreTransparent;
    d >> mIgnorePrelit;
    d >> mIgnoreHidden;
    d >> mUseMeshNormals;
    if (d.rev > 3) {
        d >> mIntersectBackFaces;
    }
    if (d.rev > 1) {
        d >> mTessellateTriLimit;
        d >> mTessellateTriError;
        d >> mTessellateTriLarge;
        d >> mTessellateTriSmall;
    }
    if (d.rev > 2) {
        d >> (int &)mQuality;
    }
END_LOADS

void RndAmbientOcclusion::BuildTrees(Quality quality) {
    MILO_ASSERT(quality < kQuality_Max, 0x1E3);
    mQuality = quality;
    if (!mObjectsCast.empty() && !mObjectsReceive.empty()) {
        MILO_ASSERT(mTriList.empty(), 0x1E9);
        Timer timer;
        timer.Restart();
        MILO_LOG("RndAmbientOcclusion: Building kd-Tree...\n");

        int packDepth = kQualityLUT[quality + 2];
        BuildSphereStratified(kQualityLUT[quality], mSampleDirs);

        Box box(Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX), Vector3(FLT_MAX, FLT_MAX, FLT_MAX));

        {
            auto it = mObjectsCast.begin();
            if (it != mObjectsCast.end()) {
                do {
                    RndMesh *mesh = *it;
                    const Transform &xfm = mesh->WorldXfm();

                    for (unsigned int i = 0; i < mesh->Faces().size(); i++) {
                        RndMesh::Face &face = mesh->Faces(i);
                        Vector3 v1, v0, v2;
                        Multiply(mesh->Verts()[face.v1].pos, xfm, v0);
                        Multiply(mesh->Verts()[face.v2].pos, xfm, v1);
                        Multiply(mesh->Verts()[face.v3].pos, xfm, v2);

                        float d01 = Distance(v1, v0);
                        float d12 = Distance(v1, v2);
                        float d20 = Distance(v0, v2);

                        if ((d01 + d12 + d20) > 9.999999747378752e-05f && (d01 * d12 * d20) > 1.1920928955078125e-07f) {
                            box.GrowToContain(v0, false);
                            box.GrowToContain(v1, false);
                            box.GrowToContain(v2, false);

                            Triangle tri;
                            tri.Set(v0, v1, v2);
                            mTriList.push_back(tri);

                            if (mIntersectBackFaces) {
                                tri.Set(v0, v2, v1);
                                mTriList.push_back(tri);
                            }
                        }
                    }
                    ++it;
                } while (it != mObjectsCast.end());
            }
        }

        MILO_ASSERT(mTree == NULL, 0x234);
        box.Extend(0.001f);
        mTree = new kdTree<Triangle>(box);

        FOREACH (it, mTriList) {
            mTree->Add(&*it);
        }

        mTree->PackNodes((kdTree<Triangle>::SplitPlaneType)packDepth, 0);
        static const float kMsToSec = 0.001f;
        MILO_LOG(
            "RndAmbientOcclusion: Built kd-Tree in %0.2f seconds\n",
            timer.SplitMs() * kMsToSec
        );
        timer.Restart();
    }
    DumpObjList(" RndAmbientOcclusion: Cast List:\n", mObjectsCast);
    DumpObjList(" RndAmbientOcclusion: Recv List:\n", mObjectsReceive);
    DumpObjList(" RndAmbientOcclusion: Tess List:\n", mObjectsTessellate);
    mObjectsCast.clear();
}

void RndAmbientOcclusion::Clean() {
    RELEASE(mTree);
    mObjectsCast.clear();
    mObjectsReceive.clear();
    mObjectsTessellate.clear();
    mTriList.clear();
    mSampleDirs.clear();
}

void RndAmbientOcclusion::BuildSHCoeff(const Vector3 &inVector, float *fArr) const {
    float diff = 1.0f - Length(inVector);
    if (diff <= 0) diff = -diff;
    MILO_ASSERT(diff <= kSmallFloat, 0x298);
    fArr[0] = 0.2820948f;
    fArr[1] = inVector.y * 0.48860252f;
    fArr[2] = inVector.z * 0.48860252f;
    fArr[3] = inVector.x * 0.48860252f;
}

float RndAmbientOcclusion::DistanceSH(
    const Vector4 &sh1, const Vector3 &n1, const Vector4 &sh2, const Vector3 &n2
) const {
    float dw = (sh1.w * 2.0f - 1.0f) - (sh2.w * 2.0f - 1.0f);
    float dz = (sh1.z * 2.0f - 1.0f) - (sh2.z * 2.0f - 1.0f);
    float dy = (sh1.y * 2.0f - 1.0f) - (sh2.y * 2.0f - 1.0f);
    float dot = n1.x * n2.x + n1.z * n2.z + n1.y * n2.y;
    if (dot <= 0.0f) {
        dot = -dot;
    }
    float dx = sh1.x - sh2.x;
    auto dist = sqrtf(dx * dx + dy * dy + dw * dw + dz * dz);
    return dist / (dot + 1.0f);
}

template <class T>
struct VectorSort {
    VectorSort(const std::vector<T> &v) : vector(v) {}

    bool operator()(T item1, T item2);

    const std::vector<T> &vector;
};

template <>
bool VectorSort<RndMesh *>::operator()(RndMesh *item1, RndMesh *item2) {
    std::vector<RndMesh *>::const_iterator it1 =
        std::find(vector.begin(), vector.end(), item1);
    std::vector<RndMesh *>::const_iterator it2 =
        std::find(vector.begin(), vector.end(), item2);
    return (it1 - vector.begin()) < (it2 - vector.begin());
}

void RndAmbientOcclusion::BuildObjectLists() {
    ObjectDir *myDir = Dir();
    Clean();
    MILO_ASSERT(mObjectsCast.empty(), 0x199);
    MILO_ASSERT(mObjectsReceive.empty(), 0x19A);
    MILO_ASSERT(mObjectsTessellate.empty(), 0x19B);
    std::vector<RndMesh *> meshes;
    GatherObjectsFromDir(myDir, meshes);
    std::unique_copy(meshes.begin(), meshes.end(), meshes.begin());
    std::vector<RndMesh *> dontReceiveMeshes;
    std::vector<RndMesh *> dontCastMeshes;
    std::vector<RndMesh *> tessellateMeshes;
    FOREACH (it, mDontCastAO) {
        GatherObject(*it, dontCastMeshes);
    }
    FOREACH (it, mDontReceiveAO) {
        GatherObject(*it, dontReceiveMeshes);
    }
    FOREACH (it, mTessellate) {
        GatherObject(*it, tessellateMeshes);
    }
    std::unique_copy(dontCastMeshes.begin(), dontCastMeshes.end(), meshes.end());
    std::unique_copy(dontReceiveMeshes.begin(), dontReceiveMeshes.end(), meshes.end());
    std::unique_copy(tessellateMeshes.begin(), tessellateMeshes.end(), meshes.end());
    FOREACH (it, meshes) {
        RndMesh *cur = *it;
        if (IsValid_AOCast(cur)
            && std::find(dontCastMeshes.begin(), dontCastMeshes.end(), cur)
                == dontCastMeshes.end()) {
            mObjectsCast.push_back(cur);
        }
        if (IsValid_AOReceive(cur)
            && std::find(dontReceiveMeshes.begin(), dontReceiveMeshes.end(), cur)
                == dontReceiveMeshes.end()) {
            mObjectsReceive.push_back(cur);
        }
        if (IsValid_Tessellate(cur, myDir)
            && std::find(tessellateMeshes.begin(), tessellateMeshes.end(), cur)
                != tessellateMeshes.end()) {
            mObjectsTessellate.push_back(cur);
        }
    }
    std::sort(
        mObjectsTessellate.begin(),
        mObjectsTessellate.end(),
        VectorSort<RndMesh *>(mObjectsTessellate)
    );
}

// Transform a normal vector by applying the inverse transpose of a matrix.
// This is used to correctly transform normals under non-uniform scaling.
void RndAmbientOcclusion::TransformNormal(
    const Vector3 &vin, const Hmx::Matrix3 &min, Vector3 &vout
) const {
    Vector3 vtmp;
    Normalize(vin, vtmp);
    Hmx::Matrix3 mtmp;
    Invert(min, mtmp);
    // Compute dot products with inverted matrix rows
    float z = Dot(vtmp, mtmp.z);
    float x = Dot(vtmp, mtmp.x);
    float y = Dot(vtmp, mtmp.y);
    vout.y = y;
    vout.x = x;
    vout.z = z;
    Normalize(vout, vout);
}

void RndAmbientOcclusion::DumpObjList(
    const char *msg, const std::vector<RndMesh *> &meshes
) const {
    if (!meshes.empty()) {
        MILO_LOG(msg);
        FOREACH (it, meshes) {
            RndMesh *cur = *it;
            const char *statsMsg = MakeString(
                "   %s - %d verts, %d polys\n",
                cur->Name(),
                cur->Verts().size(),
                cur->Faces().size()
            );
            MILO_LOG(statsMsg);
        }
    }
}

bool RndAmbientOcclusion::IsSerializable(const RndMesh *mesh) const {
    if (mesh->GetGeomOwner() != mesh) {
        return false;
    }
    ObjectDir *meshDir = mesh->Dir();
    if ((int)meshDir == (int)Dir())
        return true;
    return (meshDir->IsSubDir() && meshDir->InlineSubDirType() == kInlineAlways);
}

bool RndAmbientOcclusion::IsValid_Mesh(const RndMesh *mesh) const {
    RndMesh *nonConstMesh = const_cast<RndMesh *>(mesh);
    if (nonConstMesh->Verts().size() && nonConstMesh->Faces().size()) {
        static Symbol classNames[] = { "Spotlight", "WorldCrowd" };
        FOREACH (it, mesh->Refs()) {
            Hmx::Object *owner = it->RefOwner();
            if (owner) {
                for (int i = 0; i < DIM(classNames); i++) {
                    if (owner->ClassName() == classNames[i]) {
                        return false;
                    }
                }
            }
        }
        return true;
    }
    return false;
}

bool RndAmbientOcclusion::IsValid_AOCast(const RndMesh *mesh) const {
    bool isTransparent = false;
    if (!IsValid_Mesh(mesh)) {
        return false;
    }
    RndMat *mat = mesh->Mat();
    if (mat != NULL) {
        ZMode zmode = mat->GetZMode();
        isTransparent = zmode == kZModeDisable || zmode == kZModeTransparent;
    }
    if (!mIgnoreHidden || mesh->Showing()) {
        if (!mIgnoreTransparent || !isTransparent) {
            return true;
        }
    }
    return false;
}

bool RndAmbientOcclusion::IsValid_AOReceive(const RndMesh *mesh) const {
    bool isTransparent = false;
    bool isPrelit = false;
    if (!IsSerializable(mesh)) {
        return false;
    }
    if (!IsValid_Mesh(mesh)) {
        return false;
    }
    RndMat *mat = mesh->Mat();
    if (mat != NULL) {
        ZMode zmode = mat->GetZMode();
        float alpha = mat->Alpha();
        isTransparent = (zmode == kZModeDisable || zmode == kZModeTransparent || alpha == 0.0f);
        isPrelit = mat->Prelit();
    }
    if (!mIgnoreHidden || mesh->Showing()) {
        if (!mIgnoreTransparent || !isTransparent) {
            if (!mIgnorePrelit || !isPrelit) {
                return true;
            }
        }
    }
    return false;
}

bool RndAmbientOcclusion::IsValid_Tessellate(
    const RndMesh *mesh, const ObjectDir *dir
) const {
                return !(!IsValid_AOCast(mesh) || !IsValid_AOReceive(mesh) || mesh->IsSkinned() || mesh->GetGeomOwner() != mesh || mesh->Dir() == dir);
}

bool RndAmbientOcclusion::IsMeshAnimated(const RndMesh *mesh) const {
    static Symbol sRndTransAnim = RndTransAnim::StaticClassName();
    static Symbol sRndPropAnim = RndPropAnim::StaticClassName();
    static DataArrayPtr sPropPathScale(Symbol("scale"));
    static DataArrayPtr sPropPathRotation(Symbol("rotation"));
    FOREACH (it, mesh->Refs()) {
        Hmx::Object *owner = it->RefOwner();
        if (owner) {
            if (owner->ClassName() == sRndTransAnim) {
                return true;
            }
            if (owner->ClassName() == sRndPropAnim) {
                RndPropAnim *propAnim = dynamic_cast<RndPropAnim *>(owner);
                MILO_ASSERT(propAnim != NULL, 0x7C4);
                if (propAnim->GetKeys(mesh, sPropPathScale)) {
                    return true;
                }
                if (propAnim->GetKeys(mesh, sPropPathRotation)) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool RndAmbientOcclusion::CanBurnXfm(const RndMesh *mesh) const {
    if (IsMeshAnimated(mesh))
        return false;
    else {
        FOREACH (it, mesh->Children()) {
            RndMesh *cur = dynamic_cast<RndMesh *>(*it);
            if (cur) {
                if (!CanBurnXfm(cur)) {
                    return false;
                }
            }
        }
        return true;
    }
}

void RndAmbientOcclusion::BurnTransform(
    RndMesh *mesh, std::list<RndMesh *> &meshes
) const {
    // Find and remove mesh from the work list
    std::list<RndMesh *>::iterator found = meshes.end();
    for (std::list<RndMesh *>::iterator it = meshes.begin(); it != meshes.end(); ++it) {
        if (*it == mesh) {
            found = it;
            break;
        }
    }
    if (found == meshes.end())
        return;
    meshes.erase(found);

    float det = Det(mesh->WorldXfm().m);
    bool canBurn = false;
    if (mQuality == 0) {
        canBurn = CanBurnXfm(mesh);
    } else {
        if (Abs(1.0f - det) > 0.0001f) {
            MILO_NOTIFY_ONCE(
                "%s: Mesh has scale or mirroring applied. Re-export mesh to ensure accurate AO calculation.",
                PathName(mesh)
            );
        }
    }

    if (canBurn) {
        // Build a zero-translation copy of parent world rotation matrix
        Transform parentRot;
        memcpy(&parentRot, &mesh->WorldXfm(), 0x30);
        parentRot.v.Set(0.0f, 0.0f, 0.0f);

        const std::list<RndTransformable *> &children = mesh->Children();
        for (std::list<RndTransformable *>::const_iterator it = children.begin();
             it != children.end(); ++it) {
            RndMesh *childMesh = dynamic_cast<RndMesh *>(*it);
            if (childMesh) {
                BurnTransform(childMesh, meshes);

                Transform childXfm;
                RndTransformable::Constraint constraint = childMesh->TransConstraint();
                if (constraint == 0) {
                    Multiply(childMesh->WorldXfm(), parentRot, childXfm);
                    childMesh->SetWorldXfm(childXfm);
                } else if (constraint == 2) {
                    memcpy(&childXfm, &mesh->WorldXfm(), 0x40);
                    childMesh->SetWorldXfm(childXfm);
                    childMesh->SetTransConstraint(
                        childMesh->TransConstraint(),
                        childMesh->mTarget,
                        childMesh->mPreserveScale
                    );
                } else {
                    memcpy(&childXfm, &childMesh->WorldXfm(), 0x40);
                    childMesh->SetWorldXfm(childXfm);
                }
                if (!childMesh->mPreserveScale) {
                    childMesh->SetDirty_Force();
                }
            }
        }
        BurnXfm(mesh, true);
    }
}

void RndAmbientOcclusion::PreprocessMesh() {
    std::list<RndMesh *> meshes;
    FOREACH (it, mObjectsReceive) {
        RndMesh *cur = *it;
        cur->UpdateSphere();
        meshes.push_back(cur);
    }
    FOREACH (it, mObjectsReceive) {
        BurnTransform(*it, meshes);
    }
}

void RndAmbientOcclusion::OnCalculate(bool b1) {
    float f1 = 0;
    float f2 = 0;
    float f3 = 0;
    BuildObjectLists();
    BuildTrees((Quality)0);
    CalculateAO(&f1);
    Tessellate(&f2, &f3);
    Clean();
}

template <>
bool kdTree<Triangle>::kdTreeNode::FindSplit_SAH(
    const Box &box, const std::list<Triangle *> &items
) {
    unsigned int count = 0;
    for (auto it = items.begin(); it != items.end(); ++it)
        count++;

    float invSteps = 0.05882352963089943f; // 1.0f / 17
    float fCount = (float)count;

    float bestCost[3] = { FLT_MAX, FLT_MAX, FLT_MAX };
    float bestPos[3] = { FLT_MAX, FLT_MAX, FLT_MAX };

    unsigned char axis = 0;
    do {
        float step = (box.mMax[axis] - box.mMin[axis]) * invSteps;
        float current = box.mMin[axis];
        int splits = 16;
        do {
            current += step;
            float cost = EvaluateSplit(box, items, axis, current);
            if (cost < bestCost[axis]) {
                bestCost[axis] = cost;
                bestPos[axis] = current;
            }
            splits--;
        } while (splits != 0);
        axis = (axis + 1) & 0xff;
    } while (axis < 3);

    unsigned char bestAxis = 0;
    if (bestCost[1] < bestCost[0])
        bestAxis = 1;
    if (bestCost[2] < bestCost[bestAxis])
        bestAxis = 2;

    if (!(bestCost[bestAxis] < fCount))
        return false;
    mData.real = bestPos[bestAxis];
    mData.index = bestAxis;
    return true;
}

template <>
bool kdTree<Triangle>::Intersect(
    const Vector3 &origin, const Vector3 &direction, float maxDist, float &hitDist
) const {
    float tNear, tFar;
    bool boxHit = ::Intersect(origin, direction, mBounds, tNear, tFar);
    if (!boxHit)
        return false;

    bool found = false;
    int stackDepth = 0;
    hitDist = FLT_MAX;
    kdTreeNode *nodes = mNodes;
    if (tFar - maxDist < 0.0f) {
        maxDist = tFar;
    }
    tFar = maxDist;

    if (nodes) {
        static kdTreeNode::Stack nodeStack[128];
        kdTreeNode *node = nodes;
        do {
                if (hitDist < tNear)
                    break;
                if (!node->GetIsLeaf()) {
                    float splitVal = node->mData.real;
                    unsigned int axis = node->mData.index & 3;
                    float tSplit = (splitVal - origin[axis]) / direction[axis];

                    kdTreeNode *children[2];
                    children[0] = &nodes[(node->mFlags & 0x7FFF) * 2 + 1];
                    children[1] = &nodes[(node->mFlags & 0x7FFF) * 2 + 2];

                    bool isAbove = splitVal < origin[axis];

                    if (tSplit < 0.0f || tFar < tSplit) {
                        node = children[isAbove];
                    } else if (tNear <= tSplit) {
                        nodeStack[stackDepth].tFar = tFar;
                        nodeStack[stackDepth].tNear = tSplit;
                        tFar = tSplit;
                        stackDepth++;
                        node = children[isAbove];
                        nodeStack[stackDepth - 1].node = children[!isAbove];
                    } else {
                        node = children[!isAbove];
                    }
                } else {
                    kdTriList *triList = node->GetTriList();
                    if (triList) {
                        do {
                            float dist = FLT_MAX;
                            bool triHit
                                = ::Intersect(origin, direction, *triList->GetItem(), dist);
                            if (triHit) {
                                found = true;
                                if (hitDist - dist < 0.0f)
                                    dist = hitDist;
                                hitDist = dist;
                            }
                            triList++;
                        } while (!triList->IsEnd());
                    }
                    if (stackDepth == 0)
                        break;
                    stackDepth--;
                    tNear = nodeStack[stackDepth].tNear;
                    node = nodeStack[stackDepth].node;
                    tFar = nodeStack[stackDepth].tFar;
                }
            } while (node);
    }
    return found;
}

void RndAmbientOcclusion::CalculateAOAtPoint(
    const Vector3 &pos, const Vector3 &norm, float *result
) const {
    float maxDist = gUnitsPerMeter * 50.0f;
    Vector3 rayOrigin;
    rayOrigin.x = norm.x * 0.001f + pos.x;
    rayOrigin.y = norm.y * 0.001f + pos.y;
    rayOrigin.z = norm.z * 0.001f + pos.z;
    double shAccum[4] = { 0, 0, 0, 0 };
    float invMaxDist = 1.0f / maxDist;
    int numSamples = mSampleDirs.size();
    float shCoeffs[4];
    float occlusion = 1.0f;

    for (int i = 0; (unsigned int)i < numSamples; i++) {
        const Vector3 &sampleDir = mSampleDirs[i];
        float dot = norm.x * sampleDir.x + sampleDir.z * norm.z + sampleDir.y * norm.y;
        occlusion = 1.0f;
        if (dot > 0.0f) {
            float hitDist;
            bool hit = mTree->Intersect(rayOrigin, sampleDir, maxDist, hitDist);
            if (hit && hitDist <= maxDist) {
                float t = hitDist * invMaxDist;
                occlusion = t * t;
            }
            BuildSHCoeff(sampleDir, shCoeffs);
            for (int j = 0; j <= 3; j++) {
                shAccum[j] += (double)(shCoeffs[j] * occlusion * dot);
            }
        }
    }

    for (unsigned int k = 0; k < 4; k++) {
        shAccum[k] *= (double)(12.566371f / (float)numSamples);
        if (k == 0) {
            float val = (float)shAccum[0];
            val = val > 0.0f ? val : 0.0f;
            val = val < 1.0f ? val : 1.0f;
            shAccum[0] = val;
        } else {
            float val = (float)shAccum[k];
            val = val > -1.0f ? val : -1.0f;
            val = val < 1.0f ? val : 1.0f;
            shAccum[k] = val * 0.5f + 0.5f;
        }
    }

    result[0] = (float)shAccum[0];
    result[1] = (float)shAccum[1];
    result[2] = (float)shAccum[2];
    result[3] = (float)shAccum[3];
}

void RndAmbientOcclusion::SmoothResults(RndMesh *mesh) const {
    int numVerts = mesh->Verts().size();
    const Transform &xfm = mesh->WorldXfm();
    int numFaces = mesh->Faces().size();

    // Phase 1: Compute AO at each face center
    Hmx::Color aoResult;
    std::vector<Hmx::Color> faceAO(numFaces, aoResult);
    unsigned int f = 0;
    if (numFaces != 0) {
        float oneThird = 1.0f / 3.0f;
        do {
            RndMesh::Vert *verts = &mesh->Verts(0);
            RndMesh::Face &face = mesh->Faces(f);
            unsigned short i0 = face.v1;
            unsigned short i1 = face.v2;
            unsigned short i2 = face.v3;

            // Average position of the 3 face vertices
            Vector3 center;
            center.z = ((verts[i2].pos.z + (verts[i1].pos.z + verts[i0].pos.z))) * oneThird;
            center.y = ((verts[i2].pos.y + (verts[i1].pos.y + verts[i0].pos.y))) * oneThird;
            center.x = ((verts[i2].pos.x + (verts[i1].pos.x + verts[i0].pos.x))) * oneThird;

            // Average normal of the 3 face vertices
            Vector3 faceNorm;
            faceNorm.z = verts[i2].norm.z + verts[i1].norm.z + verts[i0].norm.z;
            faceNorm.y = verts[i2].norm.y + verts[i1].norm.y + verts[i0].norm.y;
            faceNorm.x = verts[i2].norm.x + verts[i1].norm.x + verts[i0].norm.x;
            Normalize(faceNorm, faceNorm);

            // Transform to world space and calculate AO
            Vector3 worldCenter;
            Multiply(center, xfm, worldCenter);
            Vector3 worldNorm;
            TransformNormal(faceNorm, (const Hmx::Matrix3 &)xfm, worldNorm);
            CalculateAOAtPoint(worldCenter, worldNorm, (float *)&aoResult);

            faceAO[f] = aoResult;
            f++;
        } while (f < (unsigned int)numFaces);
    }

    // Phase 2: Build vertex equivalence map (weld coincident vertices)
    std::vector<int> vertMap(numVerts);
    int v = 0;
    if (0 < numVerts) {
        do {
            int equiv = 0;
            if (0 < v) {
                RndMesh::Vert *verts = &mesh->Verts(0);
                do {
                    float dx = verts[v].pos.x - verts[equiv].pos.x;
                    float dy = verts[v].pos.y - verts[equiv].pos.y;
                    float dz = verts[v].pos.z - verts[equiv].pos.z;
                    if (dx * dx + dy * dy + dz * dz <= 0.001f)
                        break;
                    equiv++;
                } while (equiv < v);
            }
            vertMap[v] = equiv;
            v++;
        } while (v < numVerts);
    }

    // Phase 3: Smooth AO by accumulating angle-weighted face AO per vertex
    v = 0;
    if (0 < numVerts) {
        int vertOffset = 0;
        int *mapPtr = &vertMap[0];
        do {
            unsigned int fNum = 0;
            if (numFaces != 0) {
                int faceOffset = 0;
                int colorOffset = 0;
                float accR = 0.0f;
                float accG = 0.0f;
                float accB = 0.0f;
                float accA = 0.0f;
                float totalAngle = 0.0f;
                do {
                    int j = 0;
                    unsigned short *faceVerts =
                        (unsigned short *)((char *)&mesh->Faces(0) + faceOffset);
                    Hmx::Color *faceColor =
                        (Hmx::Color *)((char *)&faceAO[0] + colorOffset);
                    unsigned short *fvPtr = faceVerts;
                    do {
                        if (vertMap[*fvPtr] == *mapPtr) {
                            RndMesh::Vert *verts = &mesh->Verts(0);

                            // Get the two edges adjacent to this vertex
                            int cur = j % 3;
                            int next = (j + 1) % 3;
                            int prev = (j + 2) % 3;
                            RndMesh::Vert *vCur = &verts[faceVerts[cur]];
                            RndMesh::Vert *vNext = &verts[faceVerts[next]];
                            RndMesh::Vert *vPrev = &verts[faceVerts[prev]];

                            Vector3 edge1;
                            edge1.x = vNext->pos.x - vCur->pos.x;
                            edge1.z = vNext->pos.z - vCur->pos.z;
                            edge1.y = vNext->pos.y - vCur->pos.y;

                            Vector3 edge2;
                            edge2.x = vPrev->pos.x - vCur->pos.x;
                            edge2.z = vPrev->pos.z - vCur->pos.z;
                            edge2.y = vPrev->pos.y - vCur->pos.y;

                            Normalize(edge1, edge1);
                            Normalize(edge2, edge2);

                            // Weight by the angle subtended at this vertex
                            float dot = (float)(edge2.y * edge1.y
                                + edge2.x * edge1.x + edge2.z * edge1.z);
                            float angle = (float)acos((double)dot);
                            totalAngle = totalAngle + angle;
                            accG = accG + faceColor->green * angle;
                            accR = accR + angle * faceColor->red;
                            accA = accA + faceColor->alpha * angle;
                            accB = accB + faceColor->blue * angle;
                        }
                        j++;
                        fvPtr++;
                    } while (j < 3);
                    fNum++;
                    colorOffset += 0x10;
                    faceOffset += 6;
                } while (fNum < (unsigned int)numFaces);

                // Blend smoothed AO with existing vertex color
                if (totalAngle > 0.0f) {
                    float invAngle = 1.0f / totalAngle;
                    RndMesh::Vert *verts = &mesh->Verts(0);
                    RndMesh::Vert &vert = verts[v];
                    vert.color.alpha =
                        (float)((double)(accA * invAngle + vert.color.alpha) * 0.5);
                    vert.color.red =
                        (float)((double)(invAngle * accR + vert.color.red) * 0.5);
                    vert.color.blue =
                        (float)((double)(accB * invAngle + vert.color.blue) * 0.5);
                    vert.color.green =
                        (float)((double)(accG * invAngle + vert.color.green) * 0.5);
                }
            }
            v++;
            mapPtr++;
            vertOffset += 0x60;
        } while (v < numVerts);
    }
}

void RndAmbientOcclusion::CalculateAO(float *outTime) {
    if (mObjectsReceive.empty() || !mTree)
        return;

    unsigned int totalVerts = 0;
    auto receiveEnd = mObjectsReceive.end();
    for (std::vector<RndMesh *>::iterator it = mObjectsReceive.begin();
         receiveEnd != it; ++it) {
        RndMesh *mesh = *it;
        if (mesh->GetGeomOwner() != mesh) {
            mesh->CopyGeometry(mesh->GetGeomOwner(), true);
            mesh->Sync(0x3f);
        }
        totalVerts += mesh->GetGeomOwner()->NumVerts();
    }

    MILO_LOG("RndAmbientOcclusion: Calculating ambient occlusion...\n");
    Timer timer;
    timer.Restart();
    PreprocessMesh();

    unsigned int lastPercent = 0;
    unsigned int progress = 0;
    for (std::vector<RndMesh *>::iterator it = mObjectsReceive.begin();
         it != mObjectsReceive.end(); ++it) {
        RndMesh *mesh = *it;
        const Transform &xfm = mesh->WorldXfm();
        RndMesh *geomOwner = mesh->GetGeomOwner();
        unsigned int numVerts = geomOwner->NumVerts();
        for (unsigned int v = 0; v < numVerts; v++) {
            RndMesh::Vert &vert = geomOwner->Verts(v);
            Vector3 worldPos;
            Multiply(vert.pos, xfm, worldPos);
            Vector3 worldNorm;
            TransformNormal(vert.norm, (const Hmx::Matrix3 &)xfm, worldNorm);
            CalculateAOAtPoint(worldPos, worldNorm, (float *)&vert.color);
            unsigned int percent = progress * 100 / totalVerts;
            if (percent != lastPercent) {
                lastPercent = percent;
            }
            progress++;
        }
        SmoothResults(mesh);
        mesh->SetHasAOCalc(true);
    }

    float elapsed = timer.SplitMs() * 0.001f;
    MILO_LOG("RndAmbientOcclusion: AO calculation took %0.2f seconds\n", elapsed);
    if (outTime) {
        *outTime = elapsed;
    }
    timer.Restart();

    for (std::vector<RndMesh *>::iterator it = mObjectsReceive.begin();
         it != mObjectsReceive.end(); ++it) {
        (*it)->Sync(0x1f);
    }
}

struct FacePriority {
    unsigned int faceIndex;
    float priority;
    bool operator<(const FacePriority &o) const { return priority < o.priority; }
};

void RndAmbientOcclusion::Tessellate(float *outTessTime, float *outPatchTime) {
    bool noTessellate = mObjectsTessellate.empty();
    if (noTessellate)
        return;

    // Clamp triLarge >= triSmall (fsel pattern)
    float triLarge = mTessellateTriLarge;
    float triSmall = mTessellateTriSmall;
    mTessellateTriLarge = (triLarge - triSmall >= 0.0f) ? triLarge : triSmall;

    Timer timer;
    timer.Restart();

    // Calculate total face budget across all meshes
    int totalBudget = 0;
    for (std::vector<RndMesh *>::iterator it = mObjectsTessellate.begin();
         it != mObjectsTessellate.end(); ++it) {
        RndMesh *mesh = *it;
        totalBudget += mesh->Faces().size() * mTessellateTriLimit;
    }

    // Process each mesh
    for (std::vector<RndMesh *>::iterator meshIt = mObjectsTessellate.begin();
         meshIt != mObjectsTessellate.end(); ++meshIt) {
        RndMesh *mesh = *meshIt;
        char *name = (char *)mesh->Name();
        TheDebug << MakeString("RndAmbientOcclusion: Tessellating '%s'...\n", name);
        const Transform &xfm = mesh->WorldXfm();

        unsigned int totalNewFaces = 0;
        unsigned long passNum = 0;
        float negThree = -3.0f;
        int numFaces = mesh->Faces().size();
        unsigned int maxIter = (unsigned int)(numFaces * 5) / 100;
        unsigned int halfBudget = (unsigned int)(numFaces * mTessellateTriLimit * 5) >> 1;
        if (maxIter >= 50)
            maxIter = 50;

        bool keepGoing = true;
        do {
            unsigned int newFacesThisIter = 0;
            std::set<Edge> edgeSet;
            std::vector<RndMesh::Face> newFaces;
            std::vector<RndMesh::Vert> newVerts;

            // Reserve capacity for output
            RndMesh *geomOwner = mesh->GetGeomOwner();
            newFaces.reserve(geomOwner->Faces().size() * 3);
            newVerts.reserve(mesh->GetGeomOwner()->NumVerts() * 3);

            std::vector<FacePriority> priorities;
            unsigned int numVerts = mesh->GetGeomOwner()->NumVerts();
            geomOwner = mesh->GetGeomOwner();
            priorities.reserve(geomOwner->Faces().size());

            // Phase 1: Classify each face by error and perimeter
            unsigned int faceIdx = 0;
            RndMesh *geomOwnerClassify = mesh->GetGeomOwner();
            if ((unsigned int)geomOwnerClassify->Faces().size() != 0) {
                int faceOffset = 0;
                do {
                    geomOwnerClassify = mesh->GetGeomOwner();
                    RndMesh::Vert *vertsBase = &geomOwnerClassify->Verts(0);
                    RndMesh::Face *facesBase = &geomOwnerClassify->Faces()[0];
                    RndMesh::Face *face = (RndMesh::Face *)((char *)facesBase + faceOffset);
                    unsigned short i0 = face->v1;
                    unsigned short i1 = face->v2;
                    unsigned short i2 = face->v3;

                    // Compute SH error on each edge
                    float err01 = DistanceSH(
                        *(const Vector4 *)&vertsBase[i0].color,
                        vertsBase[i0].norm,
                        *(const Vector4 *)&vertsBase[i1].color,
                        vertsBase[i1].norm
                    );
                    float err12 = DistanceSH(
                        *(const Vector4 *)&vertsBase[i1].color,
                        vertsBase[i1].norm,
                        *(const Vector4 *)&vertsBase[i2].color,
                        vertsBase[i2].norm
                    );
                    float err20 = DistanceSH(
                        *(const Vector4 *)&vertsBase[i2].color,
                        vertsBase[i2].norm,
                        *(const Vector4 *)&vertsBase[i0].color,
                        vertsBase[i0].norm
                    );
                    float totalError = (float)((double)(float)((double)err20 + (double)err12)
                                               + (double)err01);
                    bool smallError = totalError <= mTessellateTriError;

                    // Compute world-space perimeter
                    geomOwnerClassify = mesh->GetGeomOwner();
                    RndMesh::Vert *vertsPos = &geomOwnerClassify->Verts(0);
                    Vector3 wp0, wp1, wp2;
                    Multiply(vertsPos[i0].pos, xfm, wp0);
                    Multiply(vertsPos[i1].pos, xfm, wp1);
                    Multiply(vertsPos[i2].pos, xfm, wp2);

                    float d12 = sqrtf((wp1.x - wp2.x) * (wp1.x - wp2.x) + (wp1.y - wp2.y) * (wp1.y - wp2.y) + (wp1.z - wp2.z) * (wp1.z - wp2.z));
                    float d02 = sqrtf((wp0.x - wp2.x) * (wp0.x - wp2.x) + (wp0.y - wp2.y) * (wp0.y - wp2.y) + (wp0.z - wp2.z) * (wp0.z - wp2.z));
                    float d01 = sqrtf((wp0.x - wp1.x) * (wp0.x - wp1.x) + (wp0.y - wp1.y) * (wp0.y - wp1.y) + (wp0.z - wp1.z) * (wp0.z - wp1.z));
                    float perimeter = d12 + d02 + d01;

                    FacePriority *pFP;
                    if (smallError || perimeter <= mTessellateTriSmall) {
                        if (mTessellateTriLarge < perimeter) {
                            // Large face, low error: priority based on size
                            FacePriority fp;
                            fp.priority = (float)((double)mTessellateTriError * (double)negThree
                                                  - (double)(perimeter - mTessellateTriLarge));
                            fp.faceIndex = faceIdx;
                            pFP = &fp;
                        } else {
                            // Small face, low error: keep as-is
                            newFaces.push_back(*face);
                            goto nextFace;
                        }
                    } else {
                        // High error: priority based on error
                        FacePriority fp2;
                        fp2.priority = -totalError;
                        fp2.faceIndex = faceIdx;
                        pFP = &fp2;
                    }
                    priorities.push_back(*pFP);
                nextFace:
                    geomOwnerClassify = mesh->GetGeomOwner();
                    faceIdx++;
                    faceOffset += 6;
                } while (faceIdx
                         < (unsigned int)geomOwnerClassify->Faces().size());
            }

            // Sort priorities (most negative = highest priority first)
            FacePriority *priEnd = &priorities[0] + priorities.size();
            FacePriority *priBegin = &priorities[0];
            std::sort(priEnd, priBegin);

            // Phase 2: Process priority faces — split all 3 edges
            unsigned int priCount = priEnd - priBegin;
            unsigned int numNewVerts = numVerts;
            unsigned int pi = 0;
            if (priCount != 0) {
                FacePriority *pPtr = priBegin;
                do {
                    geomOwner = mesh->GetGeomOwner();
                    RndMesh::Vert *vertBase = &geomOwner->Verts(0);
                    unsigned short *facePtr =
                        (unsigned short *)&geomOwner->Faces()[pPtr->faceIndex];
                    unsigned short fv0 = facePtr[0];
                    unsigned short fv1 = facePtr[1];
                    unsigned short fv2 = facePtr[2];

                    RndMesh::Vert *vert0 = &vertBase[fv0];
                    RndMesh::Vert *vert1 = &vertBase[fv1];
                    RndMesh::Vert *vert2 = &vertBase[fv2];

                    // Construct 3 midpoint edges
                    Edge edge01, edge12, edge20;
                    edge01.v0 = (short)fv0;
                    edge01.v1 = (short)fv1;
                    edge01.midpoint = (short)0xffff;
                    edge12.v0 = (short)fv1;
                    edge12.v1 = (short)fv2;
                    edge12.midpoint = (short)0xffff;
                    edge20.v0 = (short)fv2;
                    edge20.v1 = (short)fv0;
                    edge20.midpoint = (short)0xffff;

                    RndMesh::Vert blendVert01;
                    RndMesh::Vert blendVert12;
                    RndMesh::Vert blendVert20;
                    BlendVert(*vert0, *vert1, blendVert01);
                    BlendVert(*vert1, *vert2, blendVert12);
                    BlendVert(*vert2, *vert0, blendVert20);

                    // Edge 0-1
                    std::set<Edge>::iterator it01 = edgeSet.find(edge01);
                    if (it01 == edgeSet.end()) {
                        edge01.midpoint = (short)numNewVerts;
                        numNewVerts++;
                        edgeSet.insert(edge01);
                        Vector3 worldPos, worldNorm;
                        Multiply(blendVert01.pos, xfm, worldPos);
                        TransformNormal(
                            blendVert01.norm, (const Hmx::Matrix3 &)xfm, worldNorm
                        );
                        CalculateAOAtPoint(worldPos, worldNorm, (float *)&blendVert01.color);
                        newVerts.push_back(blendVert01);
                    } else {
                        edge01 = *it01;
                    }

                    // Edge 1-2
                    std::set<Edge>::iterator it12 = edgeSet.find(edge12);
                    if (it12 == edgeSet.end()) {
                        edge12.midpoint = (short)numNewVerts;
                        numNewVerts++;
                        edgeSet.insert(edge12);
                        Vector3 worldPos, worldNorm;
                        Multiply(blendVert12.pos, xfm, worldPos);
                        TransformNormal(
                            blendVert12.norm, (const Hmx::Matrix3 &)xfm, worldNorm
                        );
                        CalculateAOAtPoint(worldPos, worldNorm, (float *)&blendVert12.color);
                        newVerts.push_back(blendVert12);
                    } else {
                        edge12 = *it12;
                    }

                    // Edge 2-0
                    std::set<Edge>::iterator it20 = edgeSet.find(edge20);
                    if (it20 == edgeSet.end()) {
                        edge20.midpoint = (short)numNewVerts;
                        numNewVerts++;
                        edgeSet.insert(edge20);
                        Vector3 worldPos, worldNorm;
                        Multiply(blendVert20.pos, xfm, worldPos);
                        TransformNormal(
                            blendVert20.norm, (const Hmx::Matrix3 &)xfm, worldNorm
                        );
                        CalculateAOAtPoint(worldPos, worldNorm, (float *)&blendVert20.color);
                        newVerts.push_back(blendVert20);
                    } else {
                        edge20 = *it20;
                    }

                    unsigned short mid01 = edge01.midpoint;
                    unsigned short mid12 = edge12.midpoint;
                    unsigned short mid20 = edge20.midpoint;

                    // Create 4 new faces
                    RndMesh::Face f1;
                    f1.v1 = facePtr[0];
                    f1.v2 = mid01;
                    f1.v3 = mid20;
                    RndMesh::Face f2;
                    f2.v1 = mid20;
                    f2.v2 = mid01;
                    f2.v3 = mid12;
                    RndMesh::Face f3;
                    f3.v1 = mid01;
                    f3.v2 = facePtr[1];
                    f3.v3 = mid12;
                    RndMesh::Face f4;
                    f4.v1 = mid12;
                    f4.v2 = facePtr[2];
                    f4.v3 = mid20;
                    newFaces.push_back(f1);
                    newFaces.push_back(f2);
                    newFaces.push_back(f3);
                    newFaces.push_back(f4);

                    pi++;
                    newFacesThisIter += 3;
                    pPtr++;
                } while (pi < priCount);
            }

            // Assign Phase 2 faces to mesh geometry
            RndMesh::Face *savedFaces = &newFaces[0];
            RndMesh::Face *savedFacesEnd = &newFaces[0] + newFaces.size();
            geomOwner = mesh->GetGeomOwner();
            geomOwner->Faces().assign(newFaces.begin(), newFaces.end());

            RndMesh::Vert *savedVerts = &newVerts[0];
            RndMesh::Vert *savedVertsEnd = &newVerts[0] + newVerts.size();
            geomOwner = mesh->GetGeomOwner();
            geomOwner->Verts().resize(
                (savedVertsEnd - savedVerts) + geomOwner->Verts().size()
            );

            // Copy new verts into mesh
            if (numVerts < numNewVerts) {
                int offset = numVerts * 0x60;
                unsigned int count = numNewVerts - numVerts;
                RndMesh::Vert *src = savedVerts;
                do {
                    memcpy(
                        (char *)&mesh->GetGeomOwner()->Verts(0) + offset,
                        src, 0x60
                    );
                    count--;
                    offset += 0x60;
                    src++;
                } while (count != 0);
            }

            // Clear and re-reserve for Phase 3
            newFaces.erase(newFaces.begin(), newFaces.end());
            geomOwner = mesh->GetGeomOwner();
            newFaces.reserve(geomOwner->Faces().size() * 2);

            if (savedVerts != savedVertsEnd) {
                newVerts.erase(newVerts.begin(), newVerts.end());
            }
            newVerts.reserve(mesh->GetGeomOwner()->NumVerts());

            // Phase 3: Refine remaining faces based on split edges
            geomOwner = mesh->GetGeomOwner();
            unsigned int fIdx = 0;
            unsigned int numVertsPhase3 = geomOwner->NumVerts();
            unsigned int oldNumVerts3 = numVertsPhase3;
            if (geomOwner->Faces().size() != 0) {
                int fOff = 0;
                do {
                    int splitCount = 0;
                    unsigned int lastSplitEdge = 0;
                    RndMesh::Face *curFace =
                        (RndMesh::Face *)((char *)&geomOwner->Faces()[0] + fOff);

                    unsigned short cv0 = curFace->v1;
                    unsigned short cv1 = curFace->v2;
                    unsigned short cv2 = curFace->v3;

                    // Set up 3 edges and check which were split
                    Edge edges[3];
                    edges[0].v0 = cv0;
                    edges[0].v1 = cv1;
                    edges[0].midpoint = 0xffff;
                    edges[1].v0 = cv1;
                    edges[1].v1 = cv2;
                    edges[1].midpoint = 0xffff;
                    edges[2].v0 = cv2;
                    edges[2].v1 = cv0;
                    edges[2].midpoint = 0xffff;

                    unsigned short mids[3] = {0xffff, 0xffff, 0xffff};
                    unsigned short *midPtr = mids;
                    Edge *edgePtr = edges;
                    unsigned int edgeIdx = 0;
                    do {
                        std::set<Edge>::iterator eit = edgeSet.find(*edgePtr);
                        if (eit != edgeSet.end()) {
                            splitCount++;
                            *midPtr = eit->midpoint;
                            lastSplitEdge = edgeIdx;
                        }
                        edgeIdx++;
                        edgePtr++;
                        midPtr++;
                    } while (edgeIdx < 3);

                    if (splitCount == 0) {
                        newFaces.push_back(*curFace);
                    } else if (splitCount == 3) {
                        // All 3 edges split: 4 new faces
                        RndMesh::Face fa;
                        fa.v1 = cv0;
                        fa.v2 = mids[0];
                        fa.v3 = mids[2];
                        RndMesh::Face fb;
                        fb.v1 = mids[0];
                        fb.v2 = cv1;
                        fb.v3 = mids[1];
                        RndMesh::Face fc;
                        fc.v1 = mids[1];
                        fc.v2 = cv2;
                        fc.v3 = mids[2];
                        RndMesh::Face fd;
                        fd.v1 = mids[0];
                        fd.v2 = mids[1];
                        fd.v3 = mids[2];
                        newFaces.push_back(fa);
                        newFaces.push_back(fb);
                        newFaces.push_back(fc);
                        newFaces.push_back(fd);
                    } else if (splitCount == 1) {
                        // 1 edge split: 2 new faces
                        unsigned short mid = mids[lastSplitEdge];
                        RndMesh::Face fa, fb;
                        if (lastSplitEdge == 0) {
                            fa.v1 = cv2;
                            fa.v2 = cv0;
                            fa.v3 = mid;
                            fb.v1 = cv2;
                            fb.v2 = mid;
                            fb.v3 = cv1;
                        } else if (lastSplitEdge != 1) {
                            fa.v1 = cv1;
                            fa.v2 = cv2;
                            fa.v3 = mid;
                            fb.v1 = cv1;
                            fb.v2 = mid;
                            fb.v3 = cv0;
                        } else {
                            fa.v1 = cv0;
                            fa.v2 = cv1;
                            fa.v3 = mid;
                            fb.v1 = cv0;
                            fb.v2 = mid;
                            fb.v3 = cv2;
                        }
                        newFaces.push_back(fa);
                        newFaces.push_back(fb);
                    } else if (splitCount == 2) {
                        // 2 edges split: create blend vert + faces
                        RndMesh *geomSplit = mesh->GetGeomOwner();
                        unsigned short uv0 = curFace->v1;
                        unsigned short uv1 = curFace->v2;
                        unsigned short uv2 = curFace->v3;
                        RndMesh::Vert *vBase = &geomSplit->Verts(0);

                        RndMesh::Vert blendA;
                        RndMesh::Vert blendB;
                        BlendVert(vBase[uv0], vBase[uv1], blendA);
                        BlendVert(vBase[uv2], blendA, blendB);

                        Vector3 worldPos, worldNorm;
                        Multiply(blendB.pos, xfm, worldPos);
                        TransformNormal(
                            blendB.norm, (const Hmx::Matrix3 &)xfm, worldNorm
                        );
                        CalculateAOAtPoint(worldPos, worldNorm, (float *)&blendB.color);

                        unsigned short blendIdx = (unsigned short)numVertsPhase3;

                        // Edge 0 (v0-v1)
                        RndMesh::Face *pFace;
                        if (mids[0] == 0xffff) {
                            RndMesh::Face tmpA;
                            tmpA.v1 = blendIdx;
                            tmpA.v2 = curFace->v1;
                            tmpA.v3 = curFace->v2;
                            pFace = &tmpA;
                        } else {
                            RndMesh::Face tmpA;
                            tmpA.v1 = blendIdx;
                            tmpA.v2 = curFace->v1;
                            tmpA.v3 = mids[0];
                            newFaces.push_back(tmpA);
                            RndMesh::Face tmpB;
                            tmpB.v1 = blendIdx;
                            tmpB.v2 = mids[0];
                            tmpB.v3 = curFace->v2;
                            pFace = &tmpB;
                        }
                        newFaces.push_back(*pFace);

                        // Edge 1 (v1-v2)
                        if (mids[1] == 0xffff) {
                            RndMesh::Face tmpA;
                            tmpA.v1 = blendIdx;
                            tmpA.v2 = curFace->v2;
                            tmpA.v3 = curFace->v3;
                            pFace = &tmpA;
                        } else {
                            RndMesh::Face tmpA;
                            tmpA.v1 = blendIdx;
                            tmpA.v2 = curFace->v2;
                            tmpA.v3 = mids[1];
                            newFaces.push_back(tmpA);
                            RndMesh::Face tmpB;
                            tmpB.v1 = blendIdx;
                            tmpB.v2 = mids[1];
                            tmpB.v3 = curFace->v3;
                            pFace = &tmpB;
                        }
                        newFaces.push_back(*pFace);

                        // Edge 2 (v2-v0)
                        if (mids[2] == 0xffff) {
                            RndMesh::Face tmpA;
                            tmpA.v1 = blendIdx;
                            tmpA.v2 = curFace->v3;
                            tmpA.v3 = curFace->v1;
                            pFace = &tmpA;
                        } else {
                            RndMesh::Face tmpA;
                            tmpA.v1 = blendIdx;
                            tmpA.v2 = curFace->v3;
                            tmpA.v3 = mids[2];
                            newFaces.push_back(tmpA);
                            RndMesh::Face tmpB;
                            tmpB.v1 = blendIdx;
                            tmpB.v2 = mids[2];
                            tmpB.v3 = curFace->v1;
                            pFace = &tmpB;
                        }
                        newFaces.push_back(*pFace);

                        newVerts.push_back(blendB);
                        numVertsPhase3++;
                    }

                    geomOwner = mesh->GetGeomOwner();
                    fIdx++;
                    fOff += 6;
                } while (fIdx < (unsigned int)geomOwner->Faces().size());
            }

            // Assign Phase 3 faces to mesh
            savedFaces = &newFaces[0];
            geomOwner = mesh->GetGeomOwner();
            geomOwner->Faces().assign(newFaces.begin(), newFaces.end());

            savedVerts = &newVerts[0];
            geomOwner = mesh->GetGeomOwner();
            geomOwner->Verts().resize(
                (int)newVerts.size() + geomOwner->Verts().size()
            );

            // Copy Phase 3 new verts into mesh
            if (oldNumVerts3 < numVertsPhase3) {
                int offset = oldNumVerts3 * 0x60;
                unsigned int count = numVertsPhase3 - oldNumVerts3;
                RndMesh::Vert *src = savedVerts;
                do {
                    memcpy(
                        (char *)&mesh->GetGeomOwner()->Verts(0) + offset,
                        src, 0x60
                    );
                    count--;
                    offset += 0x60;
                    src++;
                } while (count != 0);
            }

            // Budget tracking and convergence check
            totalNewFaces += newFacesThisIter;
            if (newFacesThisIter <= maxIter || halfBudget < totalNewFaces
                || (unsigned int)totalBudget <= newFacesThisIter) {
                keepGoing = false;
            }
            totalBudget = (unsigned int)totalBudget < newFacesThisIter ? 0 : totalBudget - newFacesThisIter;

            // Debug output
            if (newFacesThisIter != 0) {
                passNum++;
                TheDebug << MakeString(
                    "RndAmbientOcclusion: Tessellation pass %d: %d new faces, %d total\n",
                    (unsigned long)passNum, (unsigned long)newFacesThisIter, (long)totalNewFaces
                );
            }
        } while (keepGoing);
    }

    // Print tessellation time
    float tessTime = timer.SplitMs() * 0.001f;
    TheDebug << MakeString(
        "RndAmbientOcclusion: Tessellation complete in %0.2f seconds.  Patching...\n",
        tessTime
    );
    if (outTessTime)
        *outTessTime = tessTime;
    timer.Restart();

    // Sync all meshes
    for (std::vector<RndMesh *>::iterator it = mObjectsTessellate.begin();
         it != mObjectsTessellate.end(); ++it) {
        (*it)->Sync(0x3f);
    }

    // Print patching time
    float patchTime = timer.SplitMs() * 0.001f;
    TheDebug << MakeString(
        "RndAmbientOcclusion: Patching complete in %0.2f seconds.\n", patchTime
    );
    if (outPatchTime)
        *outPatchTime = patchTime;
    timer.Restart();

    // Clear tessellate list
    mObjectsTessellate.erase(mObjectsTessellate.begin(), mObjectsTessellate.end());
}

DataNode RndAmbientOcclusion::OnGetRecvMeshes(DataArray *) {
    BuildObjectLists();
    unsigned int numReceives = mObjectsReceive.size();
    DataArrayPtr ptr(new DataArray(numReceives));
    for (int i = 0; i < numReceives; i++) {
        ptr->Node(i) = mObjectsReceive[i];
    }
    Clean();
    return ptr;
}

DataNode RndAmbientOcclusion::OnGetValidObjects(DataArray *) const {
    int numObjects = 0;
    for (ObjDirItr<Hmx::Object> it(Dir(), true); it != NULL; ++it) {
        if (IsValidObject(it) && it != Dir()) {
            numObjects++;
        }
    }
    DataArrayPtr ptr(new DataArray(numObjects));
    int idx = 0;
    for (ObjDirItr<Hmx::Object> it(Dir(), true); it != NULL; ++it) {
        if (IsValidObject(it) && it != Dir()) {
            ptr->Node(idx++) = &*it;
        }
    }
    return ptr;
}

#ifndef HX_NATIVE
namespace stlpmtx_std {

template <>
Triangle* vector<Triangle, StlNodeAlloc<Triangle>>::_M_erase(
    Triangle* __first,
    Triangle* __last,
    const __false_type&
) {
    Triangle* __pos = __first;
    Triangle* __src = __last;
    int __count = (int)((this->_M_finish - __src) / (unsigned int)sizeof(Triangle));

    for (; __count > 0; __count--) {
        memcpy(__pos, __src, sizeof(Triangle));
        __pos += 1;
        __src += 1;
    }

    this->_M_finish = __pos;
    return __first;
}

}  // namespace stlpmtx_std
#endif
