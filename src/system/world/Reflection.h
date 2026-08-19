#pragma once
#include "char/Character.h"
#include "obj/Object.h"
#include "rndobj/Cam.h"
#include "rndobj/Draw.h"
#include "rndobj/Trans.h"
#include "utl/MemMgr.h"

/** "Reflects all drawables in draws." */
class WorldReflection : public RndDrawable, public RndTransformable {
public:
    // Hmx::Object
    virtual ~WorldReflection();
    OBJ_CLASSNAME(WorldReflection);
    OBJ_SET_TYPE(WorldReflection);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // RndDrawable
    virtual void DrawShowing();
    // RndHighlightable
    virtual void Highlight();

    OBJ_MEM_OVERLOAD(0x16)
    NEW_OBJ(WorldReflection)

protected:
    WorldReflection();

    void DoHide();
    void UnHide();
    void DoLOD(int);

    /** "things to draw in the reflection, in this order" */
    ObjPtrList<RndDrawable> mDraws; // 0xd8
    /** "Set LOD to 1 on these reflected characters" */
    ObjPtrList<Character> mLodChars; // 0xec
    /** "How far to stretch vertically" */
    float mVerticalStretch; // 0x100
    std::list<RndMat *> unk12c; // 0x104
    RndCam *mReflectionCamera; // 0x10c
    bool mInDrawShowing; // 0x110
    /** "List of objects to hide in the reflection,
        shows them when reflection has finished drawing." */
    ObjPtrList<RndDrawable> mHideList; // 0x114
    /** "List of objects to show in the reflection,
        hides them when reflection has finished drawing." */
    ObjPtrList<RndDrawable> mShowList; // 0x128
    ObjPtrList<RndDrawable> mPreviouslyShownDrawables; // 0x13c
    ObjPtrList<RndDrawable> mPreviouslyHiddenDrawables; // 0x150
};
