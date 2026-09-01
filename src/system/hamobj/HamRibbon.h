#pragma once
#include "math/Key.h"
#include "math/Mtx.h"
#include "rndobj/Draw.h"
#include "rndobj/Mat.h"
#include "rndobj/Mesh.h"
#include "rndobj/Poll.h"
#include "rndobj/Trans.h"
#include "utl/MemMgr.h"

/** "Ribbon" */
class HamRibbon : public RndPollable, public RndDrawable {
public:
    // Hmx::Object
    virtual ~HamRibbon();
    OBJ_CLASSNAME(HamRibbon);
    OBJ_SET_TYPE(HamRibbon);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // RndPollable
    virtual void Poll();
    // RndDrawable
    virtual void DrawShowing();

    OBJ_MEM_OVERLOAD(0x19)
    NEW_OBJ(HamRibbon)

    void Reset();
    void ConstructMesh();
    void UpdateChase();
    void UpdateMesh();

protected:
    HamRibbon();

    void SetActive(bool);
    void ExposeMesh();

    bool mCreateTrans; // 0x2c
    float mLastTime; // 0x30
    int mNumSides; // 0x34
    RndMesh *mMesh; // 0x38
    ObjPtr<RndMat> mMat; // 0x3c
    float mWidth; // 0x48
    int mDirtyFlags; // 0x4c
    bool mActive; // 0x50
    ObjPtrList<RndTransformable> mSegTrans; // 0x54
    Keys<Transform, Transform> mChaseKeys; // 0x68
    int mNumSegments; // 0x74
    float mDecay; // 0x78
    ObjPtr<RndTransformable> mFollowA; // 0x7c
    ObjPtr<RndTransformable> mFollowB; // 0x88
    float mFollowWeight; // 0x94
    bool mTaper; // 0x98
};
