#pragma once
#include "math/Mtx.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "rndobj/Anim.h"
#include "rndobj/Draw.h"
#include "rndobj/Mesh.h"
#include "rndobj/MultiMesh.h"
#include "rndobj/Part.h"
#include "rndobj/Trans.h"
#include "rndobj/TransAnim.h"
#include "utl/MemMgr.h"

/** "A Generator object flies out object instances along a path." */
class RndGenerator : public RndAnimatable, public RndTransformable, public RndDrawable {
public:
    class Instance {
    public:
        Instance() {}
        Instance(const Transform &t) : xfm(t) {}
        float startFrame; // 0x0
        Transform xfm; // 0x4
        Vector3 scale; // 0x34
    };
    // Hmx::Object
    virtual ~RndGenerator();
    OBJ_CLASSNAME(Generator);
    OBJ_SET_TYPE(Generator);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void Print();
    // RndAnimatable
    virtual void SetFrame(float, float);
    virtual float StartFrame();
    virtual float EndFrame();
    virtual void ListAnimChildren(std::list<RndAnimatable *> &) const;
    // RndDrawable
    virtual void UpdateSphere();
    virtual bool MakeWorldSphere(Sphere &, bool);
    virtual void DrawShowing();
    virtual void ListDrawChildren(std::list<RndDrawable *> &);
    // RndHighlightable
    virtual void Highlight() { RndDrawable::Highlight(); }

    OBJ_MEM_OVERLOAD(0x1F)
    NEW_OBJ(RndGenerator)
    static void Init() { REGISTER_OBJ_FACTORY(RndGenerator) }

    void Generate(float);
    void GetRateVar(float &lo, float &hi) {
        lo = mRateGenLow;
        hi = mRateGenHigh;
    }
    void SetRateVar(float lo, float hi) {
        mRateGenLow = lo;
        mRateGenHigh = hi;
    }
    void SetScaleVar(float lo, float hi) {
        mScaleGenLow = lo;
        mScaleGenHigh = hi;
    }
    void SetPathVar(float x, float y, float z) {
        mPathVarMaxX = x;
        mPathVarMaxY = y;
        mPathVarMaxZ = z;
    }
    void SetPath(RndTransAnim *, float, float);

protected:
    RndGenerator();

    void ResetInstances();
    void DrawMesh(Transform &, float);
    void DrawMultiMesh(Transform &, float);
    void DrawParticleSys(Transform &, float);

    DataNode OnSetPath(const DataArray *);
    DataNode OnSetRateVar(const DataArray *);
    DataNode OnSetScaleVar(const DataArray *);
    DataNode OnSetPathVar(const DataArray *);
    DataNode OnGenerate(const DataArray *);

    std::list<Instance> mInstances; // 0xe8
    ObjPtr<RndTransAnim> mPath; // 0xf0
    float mPathStartFrame; // 0xfc
    float mPathEndFrame; // 0x100
    ObjPtr<RndMesh> mMesh; // 0x104
    ObjPtr<RndMultiMesh> mMultiMesh; // 0x110
    ObjPtr<RndParticleSys> mParticleSys; // 0x11c
    float mNextFrameGen; // 0x128
    float mRateGenLow; // 0x12c
    float mRateGenHigh; // 0x130
    float mScaleGenLow; // 0x134
    float mScaleGenHigh; // 0x138
    float mPathVarMaxX; // 0x13c
    float mPathVarMaxY; // 0x140
    float mPathVarMaxZ; // 0x144
    RndParticle *mCurParticle; // 0x148
    RndMultiMesh::InstanceList::iterator mCurMultiMesh; // 0x14c
};
