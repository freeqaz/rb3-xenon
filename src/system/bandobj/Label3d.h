#pragma once
// Label3d (bandobj/Label3d.h), MSVC X360.
//
// Recovered from retail bytes; no reference decomp carries this class.
// Retail RTTI (.?AVLabel3d@@, COL 0x821D2730) gives the bases: RndTransformable
// at +0, RndDrawable at +0xB4, virtual bases Hmx::Object at +0x128 and
// RndHighlightable at +0x154; NewObject allocates 0x15C. Member offsets come
// from the constructor (0x822F0B90), Save/PreLoad and SyncProperty, whose
// property names ("text", "localize", "resource", "all_caps", "max_width",
// "jitter_depth", "jitter_height") name the members. The class draws a string
// as a row of per-character meshes ("%02x.mesh" / "%02x_box.mesh") copied out of
// the ObjectDir loaded from `resource`.
//
// Non-virtual method names are descriptive; retail carries none.
#include "obj/ObjMacros.h"
#include "obj/Dir.h"
#include "obj/ObjPtr_p.h"
#include "rndobj/Draw.h"
#include "rndobj/Mesh.h"
#include "rndobj/Trans.h"
#include "utl/FilePath.h"
#include "utl/MemMgr.h"
#include "utl/Str.h"

class Label3d : public RndTransformable, public RndDrawable {
public:
    Label3d();
    // Hmx::Object
    virtual ~Label3d();
    OBJ_CLASSNAME(Label3d);
    OBJ_SET_TYPE(Label3d);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    // RndDrawable
    virtual float GetDistanceToPlane(const Plane &, Vector3 &);
    virtual bool MakeWorldSphere(Sphere &, bool);
    virtual void DrawShowing();
    // RndHighlightable
    virtual void Highlight();

    OBJ_NEW_OVERLOAD;
    DELETE_OVERLOAD_INLINE;
    NEW_OBJ(Label3d)
    static void Init() { REGISTER_OBJ_FACTORY(Label3d) }

    /** Rebuild the character meshes from mText (0x822F0568). */
    void SyncText();
    /** Load the `resource` dir, then rebuild (0x822F1350). */
    void LoadResource();
    /** Min/max vertex x of a box mesh (0x822F0000). */
    void MeshXExtent(float &, float &, RndMesh *);

    String mText; // 0xd8
    /** Parent of every character mesh; scaled down to honour mMaxWidth. */
    RndTransformable *mTrans; // 0xe4
    FilePath mResource; // 0xe8
    float mMaxWidth; // 0xf4
    bool mAllCaps; // 0xf8
    bool mLocalize; // 0xf9
    ObjDirPtr<ObjectDir> mDir; // 0xfc
    float mJitterDepth; // 0x108
    float mJitterHeight; // 0x10c
    ObjPtrList<RndMesh> mMeshes; // 0x110
    // 0x124 is the vtordisp of the Hmx::Object virtual base (vfptr at 0x128).
};
