#pragma once
#include "char/CharBones.h"
#include "obj/Object.h"
#include "rndobj/Trans.h"

/** "Holds state for a set of bones, allocates own space, and sets meshes accordingly" */
class CharBonesMeshes : public CharBonesAlloc {
public:
    CharBonesMeshes();
    virtual ~CharBonesMeshes();
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);

    void AcquirePose();
    void PoseMeshes();
    void StuffMeshes(std::list<Hmx::Object *> &);

    static void Init();
    static void Terminate();

protected:
    virtual bool Replace(ObjRef *, Hmx::Object *);
    virtual void ReallocateInternal();

    /** "Transes we will change" */
    // Retail X360: ObjVector<ObjOwnerPtr<...>> (a plain std::vector subclass, no
    // leading vtable) = 0x10 bytes at 0x54 — NOT the 0x1c-byte ObjPtrVec (which
    // has a vtable + erase/list-mode fields and skewed every CharFaceServo member
    // +8). mDummyMesh is a per-instance member here (retail), not a static.
    ObjVector<ObjOwnerPtr<RndTransformable> > mMeshes; // 0x54
    RndTransformable *mDummyMesh; // 0x64
};
