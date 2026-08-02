#pragma once
#include "char/CharPollable.h"
#include "obj/Object.h"
#include "rndobj/Trans.h"
#include "stl/_vector.h"
#include "utl/MemMgr.h"

class CharBlendBone : public CharPollable {
public:
    struct ConstraintSystem {
    public:
        ConstraintSystem();
        ConstraintSystem(Hmx::Object *);

        /** "object to constrain" */
        ObjPtr<RndTransformable> mTarget; // 0x0
        /** "influence value, from 0 to 1" */
        float mWeight; // 0xc
    };

    // Hmx::Object
    OBJ_CLASSNAME(CharBlendBone);
    OBJ_SET_TYPE(CharBlendBone);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);

    // RndPollable
    virtual void Poll();
    virtual void PollDeps(std::list<Hmx::Object *> &, std::list<Hmx::Object *> &);

    OBJ_MEM_OVERLOAD(0x19)
    NEW_OBJ(CharBlendBone);

    // RB3 retail's member is an ObjList (12 B), NOT DC3's ObjVector (16 B), and
    // there is NO `bool mSetLocal`.  The two arms sum to the exact -8 that
    // vbase_census measures (ours vbase Object @0x3c vs retail @0x34):
    //   ObjVector<ConstraintSystem> (0x10) -> ObjList<ConstraintSystem> (0xc)  -4
    //   bool mSetLocal (+3 tail pad it was holding open)                       -4
    // Evidence, none of it the metric:
    //  (1) rb3-Wii -- RB3's OWN engine generation, not DC3's newer one --
    //      declares `ObjList<ConstraintSystem> mTargets; // 0x8` followed by
    //      `mSrc1; // 0x14`, i.e. a 12-byte span, and stops at mRotation;
    //  (2) the exact NUL-terminated property Symbol "set_local\0" has ZERO
    //      occurrences in retail band.exe while this class's OWN sibling
    //      property "src_one\0" has one -- the probe could have fired and did
    //      not.  (A naive SUBSTRING search for "set_local" hits 4 times and
    //      would have blocked this fix: they are set_local_scale{,_index} and
    //      set_local_pos{,_index}, unrelated properties.)
    //  (3) lane CQ-3 read retail's ordered property list out of the target obj
    //      and it is exactly 7 literals ending at `rotation` -- independently
    //      agreeing that set_local is not a property of this class in retail.
    // NOTE the previous `// 0xHEX` comments here were WRONG (mSrc1 is 0x18 but
    // mSrc2 was marked 0x2c when the compiler says 0x24, and mTransX 0x40 vs a
    // real 0x30).  Offsets below are from cl.exe /d1reportSingleClassLayout.
    ObjList<ConstraintSystem> mTargets; // 0x8
    ObjPtr<RndTransformable> mSrc1; // 0x14
    ObjPtr<RndTransformable> mSrc2; // 0x20
    bool mTransX; // 0x2c
    bool mTransY; // 0x2d
    bool mTransZ; // 0x2e
    bool mRotation; // 0x2f

protected:
    CharBlendBone();
};

BinStream &operator<<(BinStream &, const CharBlendBone::ConstraintSystem &);
