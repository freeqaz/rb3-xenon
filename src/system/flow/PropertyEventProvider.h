#pragma once
#include "obj/Object.h"

// ----------------------------------------------------------------------------
// #pragma vtordisp(off) IS LOAD-BEARING -- it is worth 4 bytes of sizeof
// ----------------------------------------------------------------------------
// Retail allocates this class with `li r3, 0x44` (?NewObject@PropertyEventProvider@@,
// retail 0x8274df40).  We shipped 0x48 because MSVC's /vd1 default inserts a
// 4-byte vtordisp ahead of the Hmx::Object virtual base.  With it gone the
// layout closes exactly:
//     vbptr(4) + map(0x18) + Hmx::Object subobject(0x28) = 0x44.
//
// Measured with cl.exe /d1reportSingleClassLayout (authoritative -- NOT the
// header `// 0xHEX` comments), lane SIZE4-1:
//
//     ctor       dtor       pragma   sizeof   vtordisp
//     user       user       --       0x48     yes      <- DC3's shape, what we shipped
//     user       implicit   --       0x48     yes
//     implicit   user       --       0x48     yes
//     implicit   implicit   --       0x44     no
//     user       user       off      0x44     no       <- what we do now
//
// So /vd1 emits the vtordisp if EITHER special member is user-declared; the
// member's own non-trivial ctor/dtor (std::map) do NOT count, the rule keys on
// *user-declared*.  An empty `PropertyEventProvider() {}` was costing 4 bytes.
//
// ⛔ DO NOT "SIMPLIFY" THIS BY DELETING THE CTOR AND DTOR INSTEAD.  That reaches
// 0x44 too, and it was MEASURED AND REJECTED: it takes all three rows of
// default/PropertyEventProvider to mpn 0 (??_D 8.33->0, clear 7.14->0, and
// _M_erase 100->0, -92 B), because the in-header dtor is what makes THIS TU
// emit ??_D / _Rb_tree::clear / _Rb_tree::_M_erase at all.  Retail's TU
// demonstrably contains them -- _M_erase matches retail at 100% -- so removing
// them buys the right sizeof by emptying the object.  Whole-binary A/B of that
// variant: Δmatched +0, Δcode -92 B.
//
// ⚠ The vtordisp is NOT generally spurious -- do not go strip it elsewhere.
// CharClipGroup is the same shape (vtordisp at 0x1c, vbase at 0x20, sizeof
// 0x48) and ObjectDir and RndText also carry one; all three have NewObject rows
// matching retail at 100%.  This class is the exception, not the rule.
//
// ⚠ And this is NOT the _Rb_tree/+4 container thread: sizeof(std::map<Symbol,
// float>) is 0x18 on both sides.  RndText pins that against retail -- its
// mMeshMap spans 0x160..0x178 and ?NewObject@RndText@@ matches at 100%, so a
// 4-byte-narrow map would have made that row fail.
//
// DC3 has the identical ctor+dtor (verbatim copy) and cannot adjudicate this;
// retail contradicts it.
#pragma vtordisp(off)
class PropertyEventProvider : public virtual Hmx::Object {
public:
    // Hmx::Object
    virtual ~PropertyEventProvider() {}
    OBJ_CLASSNAME(PropertyEventProvider)
    OBJ_SET_TYPE(PropertyEventProvider)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, CopyType);
    virtual void Load(BinStream &);

    NEW_OVERLOAD;
    DELETE_OVERLOAD
    NEW_OBJ(PropertyEventProvider)
protected:
    PropertyEventProvider();

    std::map<Symbol, float> mProperties; // 0x4
};
// Restore the default (/vd1) so this header cannot change the layout of any
// class defined after it in an including TU.
#pragma vtordisp(on)

extern PropertyEventProvider *TheHamProvider;
