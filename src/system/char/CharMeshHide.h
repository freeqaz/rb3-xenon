#pragma once
#include "obj/Object.h"
#include "rndobj/Draw.h"
#include "utl/MemMgr.h"

/** "Hides meshes based on flags in other CharMeshHide." */
class CharMeshHide : public Hmx::Object {
public:
    class Hide {
    public:
        Hide(Hmx::Object *);
        Hide(const Hide &);
        Hide &operator=(const Hide &);

        ObjPtr<RndDrawable> mDraw; // 0x0
        int mFlags; // 0xc
        bool mShow; // 0x10
    };

    virtual ~CharMeshHide();
    OBJ_CLASSNAME(CharMeshHide);
    OBJ_SET_TYPE_ENGINE(CharMeshHide);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);

    OBJ_MEM_OVERLOAD(0x15)
    static void Init();
    static void HideAll(const ObjPtrList<CharMeshHide> &, int);
    NEW_OBJ(CharMeshHide)

protected:
    CharMeshHide();

    ObjVector<Hide> mHides; // 0x28
    int mFlags; // 0x38
};
