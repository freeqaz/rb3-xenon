#pragma once
#include "gesture/BaseSkeleton.h"
#include "gesture/CameraInput.h"
#include "gesture/Skeleton.h"
#include "math/Color.h"
#include "math/Mtx.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "rndobj/Cam.h"
#include "rndobj/Draw.h"
#include "rndobj/Env.h"
#include "rndobj/Line.h"
#include "rndobj/Mat.h"
#include "rndobj/Mesh.h"
#include "rndobj/Poll.h"
#include "rndobj/Trans.h"
#include "utl/MemMgr.h"

/** "Visualization of one natural input skeleton" */
class SkeletonViz : public RndDrawable, public RndTransformable, public RndPollable {
public:
    // Hmx::Object
    virtual ~SkeletonViz();
    OBJ_CLASSNAME(SkeletonViz);
    OBJ_SET_TYPE(SkeletonViz);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    // RndHighlightable
    virtual void Highlight() { RndDrawable::Highlight(); }
    // RndPollable
    virtual void Poll();

    OBJ_MEM_OVERLOAD(0x19);
    NEW_OBJ(SkeletonViz)

    void Init();
    float PhysicalCamRotation() const;
    void SetUsePhysicalCam(bool);
    void SetPhysicalCamRotation(float);
    void Rotate(float);
    void SetAxesCoordSys(SkeletonCoordSys);
    void Visualize(
        const CameraInput &, const BaseSkeleton &, std::vector<SkeletonCallback *> *, bool
    );
    void
    DrawLine3D(const Vector3 &, const Vector3 &, float, const Hmx::Color &, Hmx::Color *);
    void SetPhysicalCamScreenRect(const Hmx::Rect &);
    void DrawPoint3D(const Vector3 &, float, const Hmx::Color &, float);

private:
    void LoadResource(bool);
    void UpdateResource();
    void SetCamera(const SkeletonFrame &, const Transform &, float);
    void DrawJoints(const BaseSkeleton &, Vector3 *, Vector3 *, bool);

protected:
    SkeletonViz();

    /** "Draw skeleton from Natal camera perspective?" */
    bool mUsePhysicalCam; // 0xe0
    /** "Degrees to rotation physical camera around skeleton".
        Ranges from -360 to 360. */
    float mPhysicalCamRotation; // 0xe4
    float mCurrentCamRotation; // 0xe8
    /** "Which coordinate system axes to draw" */
    SkeletonCoordSys mAxesCoordSys; // 0xec
    RndLine *mBoneLines[kNumBones]; // 0xf0
    RndLine *mUtlLine; // 0x13c
    ObjDirPtr<ObjectDir> mResource; // 0x140
    RndEnviron *mSkeletonEnv; // 0x14c
    RndMesh *mCamMesh; // 0x150
    RndMesh *mJointMesh; // 0x154
    RndMesh *mSphereMesh; // 0x158
    RndMat *mJointMat; // 0x15c
    RndCam *mPhysicalCam; // 0x160
    Transform unk194; // 0x164
    Transform unk1d4; // 0x1a4
    float mLineWidthScale; // 0x1e4
    bool unk218; // 0x1e8
};

extern SkeletonViz *TheSkeletonViz;
