#pragma once
#include "math/Color.h"
#include "math/Geo.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/System.h"
#include "rndobj/Draw.h"
#include "rndobj/Mat.h"
#include "rndobj/Trans.h"
#include "utl/MemMgr.h"

class RndMultiMesh;
class RndMesh;

class MotionBlurCache {
public:
    MotionBlurCache() {
        mCacheKey[0] = 0;
        mCacheKey[1] = 0;
        mShouldCache = false;
    }
    unsigned int mCacheKey[2]; // 0x0, 0x4
    bool mShouldCache; // 0x8
};

/** A bone to associate with a Mesh. */
class RndBone {
public:
    RndBone(Hmx::Object *o) : mBone(o) {
#ifdef HX_NATIVE
        mOffset.Reset();
#endif
    }
    void Load(BinStream &);

    /** "Trans of the bone" */
    ObjPtr<RndTransformable> mBone; // 0x0
    Transform mOffset;
};

/** Callback interface for syncing/posing mesh vertex data (CharMeshCacheMgr,
 *  head/outfit deform). Ported from rb3-Wii bandobj/char subsystem. */
class SyncMeshCB {
public:
    class Vert {
    public:
        Vert() {}

        Vector3 pos; // 0x0
        Vector3 norm; // 0xc
    };

    SyncMeshCB() {}
    virtual ~SyncMeshCB() {}
    virtual void SyncMesh(RndMesh *, int) = 0;
    virtual bool HasMesh(RndMesh *) = 0;
    virtual const std::vector<SyncMeshCB::Vert> &GetVerts(RndMesh *) const = 0;
};

/**
 * @brief A mesh object, used to make models.
 * Original _objects description:
 * "A Mesh object is composed of triangle faces."
 */
class RndMesh : public RndDrawable, public RndTransformable {
public:
    enum Volume {
        kVolumeEmpty,
        kVolumeTriangles,
        kVolumeBSP,
        kVolumeBox
    };

    class Vert {
    public:
        Vert()
            : pos(0, 0, 0), norm(0, 1, 0), boneWeights(0, 0, 0, 0), color(1, 1, 1, 1),
              tex(0, 0) {
            boneIndices[0] = 0;
            boneIndices[1] = 1;
            boneIndices[2] = 2;
            boneIndices[3] = 3;
            tangent.Set(1, 0, 0, 1);
        }

        // Retail allocates Vert storage from the PERSISTENT heap (MemAlloc), not the
        // temp heap: ?resize@VertVector@RndMesh@@ and ??4VertVector@RndMesh@@ both
        // `bl ?MemAlloc@@YAPAXHH@Z` in the retail objs. Calling _MemAllocTemp here was
        // a real behavioural divergence (vertex buffers outlive the temp heap), hidden
        // while 0x827bcd38 was an unnamed placeholder the ruler forgave. Lane W0-ALLOC.
        static void *operator new(size_t s) {
            return MemAlloc(s, __FILE__, 0x78, "Vert", 0);
        }
        static void *operator new(size_t s, void *place) { return place; }
        static void *operator new[](size_t s) {
            return MemAlloc(s, __FILE__, 0x78, "Vert", 0);
        }
        static void operator delete(void *v) { MemFree(v, __FILE__, 0x78, "Vert"); }
        static void operator delete[](void *v) { MemFree(v, __FILE__, 0x78, "Vert"); }

        Vector3 pos; // 0x0
        Vector3 norm; // 0x10
        Vector4 boneWeights; // 0x20
        Hmx::Color color; // 0x30
        Vector2 tex; // 0x40
        short boneIndices[4]; // 0x48
        Vector4 tangent; // 0x50
    };

    /** A triangle mesh face. */
    class Face {
    public:
        Face() : v1(0), v2(0), v3(0) {}
        unsigned short &operator[](int i) { return *(&v1 + i); }
        void Set(int i0, int i1, int i2) {
            v1 = i0;
            v2 = i1;
            v3 = i2;
        }

        /** The three points that make up the face. */
        unsigned short v1, v2, v3;
    };

    /** A specialized vector for RndMesh vertices. */
    class VertVector { // more custom STL! woohoo!!!! i crave death
        friend bool PropSync(
            RndMesh ::VertVector &o, DataNode &_val, DataArray *_prop, int _i, PropOp _op
        );

    public:
        VertVector() : mVerts(nullptr), mNumVerts(0), mCapacity(0) {}
        ~VertVector() {
            mCapacity = 0;
            clear();
        }
        int size() const { return mNumVerts; }
        bool empty() const { return mNumVerts == 0; }
        Vert &operator[](int i) { return mVerts[i]; }
        const Vert &operator[](int i) const { return mVerts[i]; }
        void clear() { resize(0); }
        void resize(int);
        Vert *begin() { return &mVerts[0]; }
        Vert *end() { return &mVerts[mNumVerts]; }
        void operator=(const VertVector &);

        Vert *mVerts; // 0x0
        int mNumVerts; // 0x4
        // Signed `int`, and the LAST member -- retail VertVector is exactly 0xc.
        // Proven from two independent target sites:
        //   VertVector::resize   -> `lwz r11, 0x8, r3` + `cmpwi cr6, r11, 0x0`
        //                           + `ble`  (4-byte SIGNED load, `mCapacity > 0`)
        //   ~GemRepTemplate      -> `stw r29, 0x60/0x6c, r30` (4-byte zero store
        //                           at mTailVerts+8 / mCapVerts+8)
        // DC3 (same engine, same MSVC X360 flags) also has `int mCapacity`; only
        // rb3-Wii's MWCC build uses `unsigned short`. The former `unkc` at 0xa
        // cannot exist -- 3 x 4 bytes already fills the 0xc stride (confirmed by
        // GemRepTemplate's mTailVerts 0x58 / mCapVerts 0x64).
        int mCapacity; // 0x8
    };

    virtual ~RndMesh();
    virtual void Replace(ObjRef *, Hmx::Object *);
    OBJ_CLASSNAME(Mesh);
    OBJ_SET_TYPE(Mesh);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void Print();

    virtual void UpdateSphere();
    virtual float GetDistanceToPlane(const Plane &, Vector3 &);
    virtual bool MakeWorldSphere(Sphere &, bool);
    virtual void Mats(std::list<class RndMat *> &, bool);
    virtual RndDrawable *CollideShowing(const Segment &, float &, Plane &);
    virtual int CollidePlane(const Plane &);
    virtual void Highlight() { RndDrawable::Highlight(); }
    virtual void LoadVertices(BinStreamRev &);
    virtual void SaveVertices(BinStream &);
    virtual void DrawFacesInRange(int, int) {}
    // Retail X360 RB3 keeps NumFaces()/NumVerts() NON-VIRTUAL; they are DC3-only
    // vtable slots. Proven by three independent machine-code anchors on RndMesh's
    // own-vfptr slice (the vptr at this+0, i.e. the RndDrawable primary vtable):
    //   * SaveVertices is a vcall at slot 0x34 in RndMesh::Save (that function is
    //     a 100% match, target and base agree on `lwz r11, 0x34(r11)`), so slots
    //     0x30/0x34 = LoadVertices/SaveVertices and the slice is NOT shorter there.
    //   * RndTexBlender::DrawShowing vcalls slot 0x38 on `mesh->mGeomOwner`
    //     (`lwz r3, 0x110(r26)` = ObjOwnerPtr<RndMesh>::mObject at 0x108+8), i.e.
    //     the DrawFaces slot, so 0x38 is still occupied.
    //   * OnSync is a vcall at target slot 0x3c vs our 0x44 in seven functions
    //     (SetNumFaces/SetNumVerts/OnSetFace/OnSetVertXYZ/OnSetVertNorm/
    //     OnSetVertUV/Copy) -- a uniform -8 = exactly two dropped slots.
    // 0x30,0x34,0x38 are accounted for and OnSync is 0x3c, so the ONLY two slots
    // that can be missing are these accessors. Nothing in the tree overrides
    // them, so dropping `virtual` is behaviour-preserving (they simply inline).
    // Same idiom as DRAW_DC3_VIRTUAL in rndobj/Draw.h; the native engine keeps
    // the keyword so host-side subclasses can still specialise them.
#ifdef HX_NATIVE
#define MESH_DC3_VIRTUAL virtual
#else
#define MESH_DC3_VIRTUAL
#endif
    /** "Number of faces in the mesh" */
    MESH_DC3_VIRTUAL int NumFaces() const { return mFaces.size(); }
    /** "Number of verts in the mesh" */
    MESH_DC3_VIRTUAL int NumVerts() const { return mVerts.size(); }
#ifdef HX_NATIVE
    virtual void DrawShowing();

    /** X7, native-only. Latch: this mesh's bone slots have already been
     *  repointed from the shared char/main/skeleton magnet onto a band
     *  member's OWN animated skeleton by
     *  BandCharacter::RebindOutfitBonesToOwnSkeleton.
     *
     *  It has to live ON THE MESH and not in the caller. Outfit meshes are
     *  MERGED SHARED RESOURCES: one mesh is reachable from more than one
     *  BandCharacter, so a per-character latch would let the second member
     *  rebind bones the first already moved. A file-static set<RndMesh*> would
     *  have the right scope but the wrong lifetime -- it is never pruned when
     *  a mesh is freed, so a later allocation at the same address would be
     *  skipped silently.
     *
     *  ⚠ DISCLOSED: rb3-Wii's renderer also READS this flag (to skip its
     *  rebake + fling clamp). NO CONSUMER EXISTS IN THIS TREE -- the pinned
     *  milo-native-engine (138e1606) compiles against these same xenon headers
     *  and knows nothing about it. Here it is purely the idempotency latch.
     *  Left as a documented seam rather than an engine change request.
     *
     *  HX_NATIVE-gated, so the X360 RndMesh layout is untouched. */
    bool mNativeBonesRebound = false;
#endif

    OBJ_MEM_OVERLOAD(0x2E);
    NEW_OBJ(RndMesh)
    static void Init() { REGISTER_OBJ_FACTORY(RndMesh) }

    int EstimatedSizeKb() const;
    void SetMat(RndMat *);
    void SetGeomOwner(RndMesh *);
    void SetKeepMeshData(bool);
    void SetNumBones(int);
    void SetBone(int, RndTransformable *, bool);
    VertVector &Verts() { return mGeomOwner->mVerts; }
    std::vector<Face> &Faces() { return mGeomOwner->mFaces; }
    Vert &Verts(int idx) { return mGeomOwner->mVerts[idx]; }
    Face &Faces(int idx) { return mGeomOwner->mFaces[idx]; }
    Volume GetVolume() const { return mGeomOwner->mVolume; }
    BSPNode *GetBSPTree() const { return mGeomOwner->mBSPTree; }
    bool GetKeepMeshData() const { return mKeepMeshData; }
    RndMat *Mat() const { return mMat; }
    bool IsSkinned() const { return !mBones.empty(); }
    int MaxBones() const { return GetGfxMode() != kOldGfx ? 40 : 4; }
    int NumBones() const { return mBones.size(); }
    RndTransformable *BoneTransAt(int idx) { return mBones[idx].mBone; }
    const Transform& BoneOffsetAt(int idx) const { return mBones[idx].mOffset; }
    void SetMutable(int m) { mGeomOwner->mMutable = m; }
    int Mutable() const { return mGeomOwner->mMutable; }
    // rb3-Wii (and retail-360, per GetDefaultMatShaderOpts asm: lbz this+0x134
    // direct, no owner indirection) reads mHasAOCalc on this; DC3 later added
    // the mGeomOwner-> indirection. Keep the RB3 form.
    bool HasAOCalc() const { return mHasAOCalc; }
    void SetHasAOCalc(bool calc) { mHasAOCalc = calc; }
    RndMesh *GetGeomOwner() const { return mGeomOwner; }
    MotionBlurCache &GetBlurCache() { return mMotionCache; }
    unsigned int NumCompressedVerts() const { return mGeomOwner->mNumCompressedVerts; }
    unsigned char *CompressedVerts() const { return mGeomOwner->mCompressedVerts; }
    void InstanceGeomOwnerBones();
    void DeleteBones(bool);
    void BurnXfm();
    void ResetNormals();
    void Tessellate();
    void ClearAO();
    void CopyGeometryFromOwner();
    void Sync(int);
    void SetVolume(Volume);
    void CopyBones(const RndMesh *);
    void CopyGeometry(const RndMesh *, bool);
    void SetZeroWeightBones();
    int CollidePlane(const RndMesh::Face &, const Plane &);
    Vector3 SkinVertex(const RndMesh::Vert &, Vector3 *);
    void ScaleBones(float);
    int GetBoneIndex(const RndTransformable *);
    RndMultiMesh *CreateMultiMesh();

    friend void FaceCenter(RndMesh *, Face *, Vector3 &);
    friend class RndVelocityBuffer;

protected:
    RndMesh();
    virtual void OnSync(int);

    void ClearCompressedVerts();
    bool PatchOkay(int, int);
    bool HasInstancedBones();
    bool HasValidBones(unsigned int *) const;
    void SetNumVerts(int verts);
    void SetNumFaces(int faces);
    void RemoveInvalidBones();

    DataNode OnCompareEdgeVerts(const DataArray *);
    DataNode OnAttachMesh(const DataArray *);
    DataNode OnGetFace(const DataArray *);
    DataNode OnSetFace(const DataArray *);
    DataNode OnGetVertXYZ(const DataArray *);
    DataNode OnSetVertXYZ(const DataArray *);
    DataNode OnGetVertNorm(const DataArray *);
    DataNode OnSetVertNorm(const DataArray *);
    DataNode OnGetVertUV(const DataArray *);
    DataNode OnSetVertUV(const DataArray *);
    DataNode OnUnitizeNormals(const DataArray *);
    DataNode OnBuildFromBSP(const DataArray *);
    DataNode OnPointCollide(const DataArray *);
    DataNode OnConfigureMesh(const DataArray *);

    static bool sRawCollide;
    static int sLastCollide;

public:
    // Public to mirror the rb3-Wii oracle, where sUpdateApproxLight and its setter
    // are public (Mesh.h:345-347) -- Character::DrawLodOrShadow and
    // NgSpotlightDrawer::DoPost both suppress it from outside RndMesh. Statics have
    // no layout impact and the non-static member run below is left in its original
    // access section, so sizeof(RndMesh) is unchanged.
    static bool sUpdateApproxLight;
    static void SetUpdateApproxLight(bool b) { sUpdateApproxLight = b; }

protected:

    /** This mesh's vertices. */
    VertVector mVerts; // 0x100
    /** This mesh's faces. */
    std::vector<Face> mFaces; // 0x110
    /** "Material used for rendering the Mesh" */
    ObjPtr<RndMat> mMat; // 0x11c
    std::vector<unsigned char> mPatches; // 0x130
    /** "Geometry owner for the mesh" */
    ObjOwnerPtr<RndMesh> mGeomOwner; // 0x13c
    /** This mesh's bones. */
    ObjVector<RndBone> mBones; // 0x150
    int mMutable; // 0x160
    /** "Volume of the Mesh" */
    Volume mVolume; // 0x164
    BSPNode *mBSPTree; // 0x168
    /** The MultiMesh that will draw this Mesh multiple times. */
    RndMultiMesh *mMultiMesh; // 0x16c
    bool mHasAOCalc; // 0x170
    bool mKeepMeshData; // 0x171
    MotionBlurCache mMotionCache; // 0x174
    unsigned char *mCompressedVerts; // 0x184
    unsigned int mNumCompressedVerts; // 0x188
};

class PatchVerts {
public:
    PatchVerts() : mCentroid(0, 0, 0) {}
    ~PatchVerts() {}

    int NumVerts() const { return mPatchVerts.size(); }

    void Add(int, RndMesh::VertVector &, Vector3 &);

    void Clear();
    bool HasVert(int) const;

protected:
    int GreaterEq(int) const;

    Vector3 mCentroid; // 0x0
    std::vector<int> mPatchVerts; // 0xc
};
