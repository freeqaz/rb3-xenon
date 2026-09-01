#pragma once
#include "rndobj/Draw.h"
#include "rndobj/Mat.h"
#include "math/Color.h"
#include "math/Geo.h"

/** "Draws full screen quad with material and color." */
class RndScreenMask : public RndDrawable {
public:
    OBJ_CLASSNAME(ScreenMask);
    OBJ_SET_TYPE(ScreenMask);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void DrawShowing();

    OBJ_MEM_OVERLOAD(0x17);
    NEW_OBJ(RndScreenMask)
    static void Init() { REGISTER_OBJ_FACTORY(RndScreenMask) }

protected:
    RndScreenMask();

    /** "Material to draw on full screen quad" */
    ObjPtr<RndMat> mMat; // 0x24
    /** "Color of full screen quad" */
    Hmx::Color mColor; // 0x30
    /** "The area of the screen in normalized coordinates (0 to 1) to draw into." */
    Hmx::Rect mRect; // 0x40
    /** "Use current camera screen_rect instead of the full screen" */
    bool mUseCamRect; // 0x50
    // NOTE(laneBQ2): a 148-byte `mDroppedTrailingState_[0x94]` pad used to sit here
    // (added by 0149637d to make ??_GRndScreenMask's `this`-adjust read -236).
    // REMOVED: that adjustor evidence was a MAP MISPAIR -- the retail body at
    // 0x824816a8 mapped to ??_GRndScreenMask calls ??_DRndMultiMeshProxy@@, so it is
    // RndMultiMeshProxy's scalar deleting dtor and only scored 100% because the
    // differing `bl` target is a relocation (functionRelocDiffs=none masks it).
    // Ground truth instead comes from ?SetType@RndScreenMask@@ at 0x82481ad8 --
    // content-corroborated (it `bl`s ?StaticClassName@RndScreenMask@@) -- whose
    // vbase-displacement immediate is 84 (0x54). Dropping the pad puts the Object
    // vtordisp at mUseCamRect(80)+1 padded = 84, matching retail exactly.
};
