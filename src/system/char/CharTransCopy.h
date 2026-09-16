#pragma once
#include "char/CharPollable.h"
#include "obj/Object.h"
#include "rndobj/Trans.h"
#include "utl/MemMgr.h"

/** "Copies local xfm from one bone to another at poll time" */
class CharTransCopy : public CharPollable {
public:
    // Hmx::Object
    OBJ_CLASSNAME(CharTransCopy);
    OBJ_SET_TYPE_ENGINE(CharTransCopy);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // CharPollable
    virtual void Poll();
    virtual void PollDeps(std::list<Hmx::Object *> &, std::list<Hmx::Object *> &);

    OBJ_MEM_OVERLOAD_INLINE_DEL(0x2D)
    NEW_OBJ(CharTransCopy)

    // PUBLIC, not protected (which is how the sibling CharUpperTwist spells it).
    // Access is pure name-mangling -- identical machine code -- but it decides
    // whether our obj DEFINES the name the target map assigns: in this very obj
    // `$2` <-> protected and `$4` <-> public, and the deleting dtors at
    // 0x823C7948 / 0x823C8040 are `$4...` / `...UAA...`.  rb3-Wii also declares
    // both public.  Protected here yields `??_G...MAA...` / `??_E...$2...`, which
    // no target row can ever pair with => a permanent 0%.
    CharTransCopy();
    virtual ~CharTransCopy();

protected:

    // Offsets solved from RETAIL BYTES (lane W16-FT), not from header comments.
    // Every member-touching body addresses `this` as the Hmx::Object virtual-base
    // subobject at object+0x24 (see ??_G at 0x823C8040: `subi r30, r3, 0x24`), so:
    //   this-0x1c -> 0x08 = mSrc      this-0x10 -> 0x14 = mDest
    // and the ObjPtr pointer field sits at +8 inside each, which is why Copy reads
    // the source at 0x10 / 0x1c (`lwz r4, 0x10(r30)` / `0x1c(r30)`).  The vbptr is
    // at 0x04 (`lwz r11, -0x20(r31)`).  These agree with rb3-Wii's own comments.
    /** "Object to copy the local xfm from" */
    ObjPtr<RndTransformable> mSrc; // 0x8
    /** "Object to copy the local xfm to" */
    ObjPtr<RndTransformable> mDest; // 0x14
};
