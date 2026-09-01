#pragma once
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "rndobj/Mesh.h"
#include "rndobj/Trans.h"
#include "math/Color32.h"
#include <set>

class ChordShapeGenerator : public Hmx::Object {
public:
    class Edge {
    public:
        unsigned short mV0; // 0x0
        unsigned short mV1; // 0x2
    };

    class CrossSec {
    public:
        // The rb3-Wii MWCC STLport accepted `vector<Edge, unsigned short>` (2nd
        // param treated as a size hint), but our dc3-derived STLport treats the
        // 2nd param as a real allocator and rejects `unsigned short`, which
        // triggers an _Alloc_traits::rebind cascade. The element type and on-disc
        // layout don't depend on the allocator, and STLport vector is the same
        // size regardless, so drop the 2nd arg on every target.
        std::vector<Edge> mEdges; // 0x0
        std::set<unsigned short> mVerts; // 0xc
        float mXOffset; // 0x24
    };

    ChordShapeGenerator();
    // NOTE: no user-declared destructor (implicit dtor omits the vptr store).
    OBJ_CLASSNAME(ChordShapeGenerator);
    OBJ_SET_TYPE(ChordShapeGenerator);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);

    const Transform &SlotXfm(int) const;
    bool CheckParams() const;
    void DumpChordGenData();
    void NameMesh(RndMesh *, bool);
    void InterpolateXfm(const Transform &, const Transform &, float, Transform &);
    void
    TransformVert(RndMesh::Vert &, float, float, float, const Transform &, Hmx::Color32);
    void AddVertProfile(
        RndMesh *,
        const Transform &,
        float,
        const CrossSec &,
        std::map<unsigned short, unsigned short> &,
        Hmx::Color32
    );
    void BuildContourCap(
        RndMesh *,
        std::map<unsigned short, unsigned short> &,
        int,
        const Transform &,
        const Transform &,
        Symbol,
        Hmx::Color32,
        Hmx::Color32
    );
    void BuildEndCap(
        RndMesh *,
        std::map<unsigned short, unsigned short> &,
        int,
        const Transform &,
        Symbol,
        Hmx::Color32
    );
    void GetCrossSection(float, CrossSec &);
    void ExtendProfile(
        RndMesh *,
        std::map<unsigned short, unsigned short> &,
        const Transform &,
        const Transform &,
        float,
        float,
        const CrossSec &,
        Hmx::Color32,
        Hmx::Color32
    );
    void BuildSpan(
        RndMesh *,
        std::map<unsigned short, unsigned short> &,
        int,
        int,
        const Transform &,
        const Transform &,
        Hmx::Color32,
        Hmx::Color32
    );
    void ConnectVertProfiles(
        RndMesh *,
        const std::map<unsigned short, unsigned short> &,
        const std::map<unsigned short, unsigned short> &,
        const CrossSec &
    );
    RndMesh *BuildChordMesh(unsigned int, int);
    RndMesh *BuildChordMesh();
    RndMesh *MakeInvertedMesh(const RndMesh *);

    DataNode OnGenerate(const DataArray *);
    DataNode OnInvert(const DataArray *);
    DataNode OnSetStringFret(const DataArray *);
    DataNode OnGetStringTrans(const DataArray *);

    // NOTE: NOT DECLARE_REVS (two separate gRev/gAltRev statics) -- MSVC does not
    // lay out .bss in declaration order (measured on CrowdAudio, see
    // docs/decomp/patterns/ objmacros dialect notes), so retail's Load() addresses
    // them as one 4-byte aggregate (altRev at +0, rev at +4, sharing a single
    // `lis`). Force that layout with an explicit struct.
    struct RevsT {
        __declspec(align(4)) unsigned short altRev, rev;
    };
    static RevsT gRevs;
    NEW_OVERLOAD;
    DELETE_OVERLOAD;
    NEW_OBJ(ChordShapeGenerator)
    static void Init() { Register(); }
    REGISTER_OBJ_FACTORY_FUNC(ChordShapeGenerator)

    ObjPtr<RndMesh> mFingerSrcMesh; // 0x28
    ObjPtr<RndMesh> mChordSrcMesh; // 0x34
    ObjPtr<RndTransformable> mBaseXSection; // 0x40
    ObjPtr<RndTransformable> mContourXSection; // 0x4c
    ObjPtr<RndTransformable> mBaseHeight; // 0x58
    int mNumSlots; // 0x64
    std::vector<int> mStringFrets; // 0x68
    std::vector<bool> unk64; // 0x74
    ObjPtr<RndTransformable> mString0; // 0x88
    ObjPtr<RndTransformable> mString1; // 0x94
    ObjPtr<RndTransformable> mString2; // 0xa0
    ObjPtr<RndTransformable> mString3; // 0xac
    ObjPtr<RndTransformable> mString4; // 0xb8
    ObjPtr<RndTransformable> mString5; // 0xc4
    std::vector<float> mFretHeights; // 0xd0
    std::vector<float> mGradeDistances; // 0xdc
    RndMesh *mSource; // 0xe8
    float mBaseXVal; // 0xec
    float mContourXVal; // 0xf0
    float mBaseHeightVal; // 0xf4
    CrossSec sec1; // 0xf8
    CrossSec sec2; // 0x120
};