#pragma once
#include "char/CharPollable.h"
#include "obj/Object.h"
#include "rndobj/Trans.h"
#include "utl/MemMgr.h"

/** "Does all interpolation for the upperarm, assuming
        upperArm, upperTwist1 and 2 are under clavicle. Rotation about x is
        evenly distributed from clavicle->twist1->twist2->upperarm
    Feeds the bones when executed." */
class CharUpperTwist : public CharPollable {
public:
    // Hmx::Object
    OBJ_CLASSNAME(CharUpperTwist);
    OBJ_SET_TYPE_ENGINE(CharUpperTwist);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // CharPollable
    virtual void Poll();
    virtual void PollDeps(std::list<Hmx::Object *> &, std::list<Hmx::Object *> &);

    OBJ_MEM_OVERLOAD(0x1B)
    NEW_OBJ(CharUpperTwist)

protected:
    CharUpperTwist();
    virtual ~CharUpperTwist();

    // Retail RB3-360 layout, solved from retail bytes across all five member-
    // touching functions (lane DR-3).  Our port had these three ObjPtr names
    // ROTATED BY ONE relative to retail, which made the code read as "twist2
    // drives upperArm and twist1" -- semantic nonsense.  The correct reading is
    // the upper arm bone driving two twist bones (see Poll: upper_arm supplies
    // the parent/world frame, twist1 gets Interp 0.333, twist2 gets 0.666).
    //
    // The rotation was self-concealing: Save/Copy matched at 100% BEFORE this
    // fix because the rotated names cancelled against the rotated layout.  Only
    // SyncProperty and PollDeps -- where the property-name binding pins a member
    // independently -- exposed it.  Fixing either half alone REGRESSES the other
    // three functions; the layout and the naming must move together.
    //
    // Verified offsets (this + 0x38 == the r26/r31 base the compiler uses):
    //   mTwist1 +0x10, mTwist2 +0x1c, mUpperArm +0x28   (ObjPtr<> stride 0xc)
    // NB the previous "// 0x14 / 0x28 / 0x3c" comments were wrong on both the
    // order and the stride -- do not trust header offset comments here.
    /** "The upper arm twist1 bone" */
    ObjPtr<RndTransformable> mTwist1; // 0x10
    /** "The upper arm twist2 bone" */
    ObjPtr<RndTransformable> mTwist2; // 0x1c
    /** "The upper arm bone" */
    ObjPtr<RndTransformable> mUpperArm; // 0x28
};
