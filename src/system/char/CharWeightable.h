#pragma once
#include "obj/Object.h"
#include "utl/MemMgr.h"

/** "Base class for any object that can have weight set on it,
 *  or have a Weightable parent from which it gets that state." */
class CharWeightable : public virtual Hmx::Object {
public:
    OBJ_CLASSNAME(CharWeightable);
    OBJ_SET_TYPE(CharWeightable);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void SetWeight(float wt) { mWeight = wt; }

#ifdef HX_NATIVE
    // ★★★ X15: restore this class's OWN invariant — "the weight owner is never
    // null; a null owner means *me*". Every other path in the class already
    // enforces it: the ctor seeds `mWeightOwner(this, this)`
    // (CharWeightable.cpp:7), `SetWeightOwner` coerces `o ? o : this` (:19
    // below), and `Replace` restores `mWeightOwner = this` the moment the
    // referent goes away (CharWeightable.cpp:10-13). `Weight()` is the one
    // reader that assumed the invariant instead of upholding it.
    //
    // ⛔ THIS IS NOT THE CAUSE X14 RECORDED, AND THAT CAUSE IS REFUTED. X14
    // wrote: "`Load` overwrites it with `d >> mWeightOwner` (:74), which
    // resolves NULL when the named owner is absent." MEASURED, and it does not:
    // instrumenting `ObjRefConcrete::Load` shows the weight-owner slot is not
    // among the 11,494 empty-name loads in a club run (no `T1=CharWeightable`
    // line appears at all), and instrumenting `CharWeightable::Load` shows the
    // slot resolving to a REAL, NAMED object every time —
    // `left_hand.weight` / `right_hand.weight`, the `CharWeightSetter`s in
    // `char/main/rigging/guitar_rh.milo` and `.../drum.milo`.
    //
    // ★ WHAT ACTUALLY NULLS IT — a NATIVE-ONLY ring-teardown shortcut.
    // Those setters are destroyed by the `~ObjectDir` cascade, whose Phase 0
    // (obj/Dir.cpp:119-135, entirely inside `#ifdef HX_NATIVE`) calls
    // `Hmx::Object::NullifyAllRefs()` on every object in the dying dirs.
    // `NullifyAllRefs` walks the ring calling `ObjRef::NullifyObj()`, which
    // stores `mObject = nullptr` directly and — by its own documented design
    // (obj/Object.h:2023-2025, "No Replace callbacks fire — avoids delete-this
    // in MessageTask/ScriptTask/PropertyTask/DirLoader") — deliberately does
    // NOT invoke the consumer's `Replace()`. Retail X360 has no such shortcut:
    // the referent's death runs `Replace(ref, nullptr)`, `CharWeightable::Replace`
    // sees `SetObj(nullptr)` return null, and sets `mWeightOwner = this`.
    //
    // ⇒ SO THIS GUARD IS NOT A FALLBACK VALUE, IT IS RETAIL'S VALUE. When the
    // owner dies, retail's post-`Replace` state is `mWeightOwner == this`, and
    // `Weight()` then returns `this->mWeight` — byte for byte what this
    // returns. Nothing is synthesized, chosen, or interpolated; the null case
    // is simply spelled out instead of being reached through a pointer the
    // native cascade already cleared.
    //
    // PROVED BY POINTER IDENTITY, not by naming: the censused null-owner
    // `strum.dmidi @0x558339c1edf8` is bit-identically the object that logged
    // `owner=right_hand.weight` at Load — the same object, not a copy, so the
    // owner was resolved and then taken away.
    //
    // The X360 arm below is the original token stream, unchanged.
    float Weight() { return mWeightOwner ? mWeightOwner->mWeight : mWeight; }
#else
    float Weight() { return mWeightOwner->mWeight; }
#endif
    void SetWeightOwner(CharWeightable *o) { mWeightOwner = o ? o : this; }
    CharWeightable *WeightOwner() { return mWeightOwner; }

    OBJ_MEM_OVERLOAD(0x12)
    NEW_OBJ(CharWeightable)

protected:
    CharWeightable();

    virtual void Replace(ObjRef *, Hmx::Object *);

    /** "Weight to blend in by" */
    float mWeight; // 0x8
    /** "object to get weight from" */
    ObjOwnerPtr<CharWeightable> mWeightOwner; // 0xc
};
