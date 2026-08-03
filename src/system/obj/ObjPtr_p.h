#pragma once
#include "Object.h"
#include "obj/Dir.h" /* IWYU pragma: keep */
#include "obj/Object.h"
#include "utl/BinStream.h" /* IWYU pragma: keep */
#include "os/Debug.h" /* IWYU pragma: keep */
#include "utl/PoolAlloc.h"
#include <algorithm> /* IWYU pragma: keep */
#include <cstddef> /* IWYU pragma: keep */

// DO NOT try to include this header directly!
// include obj/Object.h instead

#pragma region ObjRefConcrete
// ------------------------------------------------
// ObjRefConcrete
// ------------------------------------------------

#ifdef HX_NATIVE
template <class T1, class T2>
ObjRefConcrete<T1, T2>::ObjRefConcrete(T1 *obj) : mObject(obj) {
    if (mObject)
        mObject->AddRef(this);
}

template <class T1, class T2>
__forceinline ObjRefConcrete<T1, T2>::ObjRefConcrete(const ObjRefConcrete &o)
    : mObject(o.mObject) {
    if (mObject)
        mObject->AddRef(this);
}

template <class T1, class T2>
ObjRefConcrete<T1, T2>::~ObjRefConcrete() {
    if (mObject) {
        if (ObjectDir::InDeleteObjects() || Hmx::Object::sRingsDirty) {
            SafeReleaseFromRing(this);
            mObject = nullptr;
            return;
        }
        mObject->Release(this);
    }
}

template <class T1, class T2>
void ObjRefConcrete<T1, T2>::SetObjConcrete(T1 *obj) {
    if (mObject) {
        if (ObjectDir::InDeleteObjects() || Hmx::Object::sRingsDirty) {
            SafeReleaseFromRing(this);
            goto skip_ring_ops;
        }
        mObject->Release(this);
    }
    mObject = obj;
    if (mObject) {
        mObject->AddRef(this);
    }
    return;
skip_ring_ops:
    mObject = obj;
    if (mObject) {
        mObject->AddRef(this);
    }
}

template <class T1, class T2>
void ObjRefConcrete<T1, T2>::CopyRef(const ObjRefConcrete &o) {
    if (&o == this) return;
    if (mObject) this->ObjRef::Release(nullptr);
    mObject = o.mObject;
    if (!mObject) return;
    this->ObjRef::AddRef(const_cast<ObjRef *>(static_cast<const ObjRef *>(&o)));
}
#else
// ----------------------------------------------------------------------------
// Retail X360: separate-pool-node ring. The ObjRefConcrete is the smart-pointer
// {vtable@0, mOwner@4, mObject@8}. The ring-ref passed to Hmx::Object::AddRef /
// Release is `this` (an ObjRefOwner).
//
// The base ctor ONLY stores mOwner/mObject — it does NOT AddRef. The AddRef
// belongs in the most-derived ctor (ObjPtr/ObjOwnerPtr/ObjDirPtr), AFTER the
// derived vtable is set (matching retail fn_8270B9A8: store fields, store the
// ObjPtr vtable, THEN AddRef). Doing AddRef in the base ctor would (a) emit it
// against the ObjRefConcrete vtable instead of the derived one and (b) make the
// base ctor the out-of-line body instead of ObjPtr's, so SampleZone/Instance
// would call ??0ObjRefConcrete and store the vtable inline (the 88.9% miss).
// ----------------------------------------------------------------------------
template <class T1, class T2>
ObjRefConcrete<T1, T2>::ObjRefConcrete(Hmx::Object *owner, T1 *obj)
    : mOwner(owner), mObject(obj) {}

template <class T1, class T2>
ObjRefConcrete<T1, T2>::ObjRefConcrete(const ObjRefConcrete &o)
    : mOwner(o.mOwner), mObject(o.mObject) {}

template <class T1, class T2>
ObjRefConcrete<T1, T2>::~ObjRefConcrete() {
    if (mObject)
        mObject->Release(this);
}

// Explicit specialization for RndParticleSys: retail's compiled dtor for
// ObjRefConcrete<RndParticleSys, ObjectDir> passes mOwner (not this) as the
// ring-ref to Release. Verified via objdiff on
// ??1?$ObjRefConcrete@VRndParticleSys@@VObjectDir@@@@UAA@XZ (default/PartAnim):
// target loads offset+4 (mOwner) into the arg register, base (generic `this`
// body) emits a plain register copy -- one-instruction replace, 97.9%->100%.
// NOT generalized to the primary template: an A/B (same worktree, clean
// report.cache, isolated to this one header line) measured applying `mOwner`
// GLOBALLY at -64 matched_functions (41415 -> 41351), so most other T1
// instantiations genuinely need `this`. This is a real per-(T1,T2) divergence,
// not a template-wide bug -- scope the fix with an explicit specialization so
// only the RndParticleSys instantiation (shared by every ObjPtr<RndParticleSys>
// / ObjOwnerPtr<RndParticleSys> / ObjDirPtr<RndParticleSys> site) is affected.
// Declared here (not defined -- RndParticleSys is only forward-declared in
// this header, and mObject->Release() needs its complete type) so every TU
// that instantiates ObjPtr<RndParticleSys>/ObjOwnerPtr<RndParticleSys>/
// ObjDirPtr<RndParticleSys> sees the specialization and emits an external
// reference instead of the primary template. Defined once, out-of-line, in
// PartAnim.cpp (which already includes rndobj/Part.h for RndParticleSys).
class RndParticleSys;
template <>
ObjRefConcrete<RndParticleSys, ObjectDir>::~ObjRefConcrete();

// Lane DR-2: the same per-(T1,T2) mOwner divergence, found by censusing EVERY
// sub-100 ObjRefConcrete dtor row in the binary for the exact RndParticleSys
// signature -- target `lwz r4, 4(r3)` (mOwner) paired against base `mr r4, r3`
// (`this`) as the row's SOLE mismatch.  Exactly four dtor rows are sub-100 and
// exactly these three carry that signature (the fourth, ObjRefConcrete<LightHue,
// ObjectDir> in default/Sequence, is a branch-distance diff_arg -- a different
// class, deliberately left alone).  Each is 116 B at fuzzy 97.931 with one
// mismatched instruction out of 29.
//
// ⚠ COLLATERAL, and the rule that follows from it: declaring the specialization
// suppresses IMPLICIT INSTANTIATION of the dtor in every other TU, and MSVC ties
// emission of the compiler-generated scalar deleting dtor `??_G` to that
// instantiation.  So a TU that used to emit `??_G?$ObjRefConcrete@T...` stops
// doing so.  That is free only when retail placed `??1` and `??_G` in the SAME
// unit -- true for RndParticleSys (both in PartAnim) and RndGroup (both in
// Spotlight), FALSE for RndCam, whose `??1` row is in default/CamAnim while its
// `??_G` row is in default/Rnd_Xbox.  Measured: the RndCam arm is +116 B (??1
// crosses) -76 B (??_G in Rnd_Xbox drops to 0) = net +40 B and 0 functions.
// ⇒ Before adding another instantiation here, check which unit owns its `??_G`
// row; if it differs from the `??1` unit, the arm buys bytes but no function.
//
// Still scoped per-instantiation rather than applied to the primary template,
// for the reason recorded above: the global change was measured at -64
// matched_functions, so most T1 genuinely pass `this`.  Definitions live in the
// TU whose pinned .text range retail put the COMDAT in -- MeshAnim.cpp,
// world/Spotlight.cpp, CamAnim.cpp respectively -- each guarded #ifndef
// HX_NATIVE for the ODR reason given in PartAnim.cpp.
class BandIKEffector;
class RndGroup;
class RndCam;
template <>
ObjRefConcrete<BandIKEffector, ObjectDir>::~ObjRefConcrete();
template <>
ObjRefConcrete<RndGroup, ObjectDir>::~ObjRefConcrete();
template <>
ObjRefConcrete<RndCam, ObjectDir>::~ObjRefConcrete();

template <class T1, class T2>
void ObjRefConcrete<T1, T2>::SetObjConcrete(T1 *obj) {
    if (obj != mObject) {
        if (mObject)
            mObject->Release(this);
        mObject = obj;
        if (mObject)
            mObject->AddRef(this);
    }
}

template <class T1, class T2>
void ObjRefConcrete<T1, T2>::CopyRef(const ObjRefConcrete &o) {
    SetObjConcrete(o.mObject);
}
#endif

template <class T1, class T2>
Hmx::Object *ObjRefConcrete<T1, T2>::SetObj(Hmx::Object *root_obj) {
    // Retail X360 guards on the identity of the *current* referent rather than
    // null-checking the incoming pointer: target emits `lwz r11,8(r31)` /
    // `cmplw r11,r4` / `bne`, then an unconditional __RTDynamicCast. The
    // apparent SetObjConcrete<T> vs SetObjConcrete<Hmx::Object> call
    // difference is ICF folding of identical bodies, not a real divergence.
    if (mObject != root_obj) {
        SetObjConcrete(dynamic_cast<T1 *>(root_obj));
    }
    return mObject;
}

template <class T1>
BinStream &operator<<(BinStream &bs, const ObjRefConcrete<T1, class ObjectDir> &f) {
    MILO_ASSERT(f.RefOwner(), 0x4D1);
    T1 *obj = f;
    const char *objName = obj ? obj->Name() : "";
    bs << objName;
    return bs;
}

// ---------------------------------------------------------------------------
// Codegen trait: does T1 reach Hmx::Object through a VIRTUAL base?
//
// Retail's ObjRefConcrete<T1,T2>::Load() no-dir path is a single unconditional
// `SetObjConcrete(0)` call.  MSVC /O1 /Ob2 then makes a PER-INSTANTIATION
// inlining decision on it:
//
//   * T1 with only non-virtual bases -- the inlined body is
//     `if (mObject) { mObject->Release(this); mObject = 0; }`, cheap, so it is
//     INLINED and retail shows the open-coded sequence.
//   * T1 reaching Hmx::Object through a VIRTUAL base -- inlining would also have
//     to emit the vbase displacement lookup (lwz +4 / lwz +4 / add / addi +4)
//     to form the Hmx::Object subobject for the virtual Release, which under
//     /O1 (favour size) loses to a 3-instruction call, so it is NOT inlined and
//     retail shows `li r4,0 / mr r3,this / bl SetObjConcrete`.
//
// Our compiler declines to inline in BOTH cases, so neither source form alone
// reproduces retail: writing `SetObjConcrete(0)` matched the 9 virtual-base
// instantiations and broke the 18 non-virtual ones (measured 100% -> ~92%),
// and open-coding it did the reverse.  The trait below picks per T1, and the
// dead arm folds away at /O1 because the condition is a compile-time constant.
//
// The predicate is EXACT over the whole ObjRefConcrete<T,ObjectDir>::Load
// population: a transitive-virtual-base scan of all 1,457 parsed classes
// separates the two groups 27/27 (the 9 below all have a virtual base; the 18
// that retail open-codes all have none).  So to extend this correctly, add a
// type here iff it reaches Hmx::Object through `virtual` inheritance --
// directly (BandCharDesc, CharWeightable, RndTransformable) or transitively
// (RndGroup / RndMesh / RndTexBlender via RndDrawable+RndTransformable,
// UIComponent -> UIList -> BandList).
// ---------------------------------------------------------------------------
template <class T>
struct ObjRefVirtualBaseObject {
    enum { value = 0 };
};
#define RB3_OBJREF_VIRTUAL_BASE(T)                                                       \
    class T;                                                                             \
    template <>                                                                          \
    struct ObjRefVirtualBaseObject<T> {                                                  \
        enum { value = 1 };                                                              \
    };
RB3_OBJREF_VIRTUAL_BASE(BandCharDesc)
RB3_OBJREF_VIRTUAL_BASE(BandList)
RB3_OBJREF_VIRTUAL_BASE(CharWeightable)
RB3_OBJREF_VIRTUAL_BASE(RndGroup)
RB3_OBJREF_VIRTUAL_BASE(RndMesh)
RB3_OBJREF_VIRTUAL_BASE(RndTexBlender)
RB3_OBJREF_VIRTUAL_BASE(RndTransformable)
RB3_OBJREF_VIRTUAL_BASE(UIComponent)
RB3_OBJREF_VIRTUAL_BASE(UIList)

template <class T1, class T2>
bool ObjRefConcrete<T1, T2>::Load(BinStream &bs, bool print, ObjectDir *dir) {
    char buf[128];
    bs.ReadString(buf, 128);
#ifdef HX_NATIVE
    Hmx::Object *refOwner = RefOwner();
    if (!dir && refOwner) {
        dir = refOwner->Dir();
    }
    // On native, allow dir-only lookup (no refOwner needed) so ObjPtrs
    // with null owners (e.g. Font3d CharInfo::mMesh) can resolve when
    // the caller passes an explicit dir.
    if (dir) {
        SetObj(dir->FindObject(buf, false));
        // Native fallback: walk up the parent dir chain when not found locally.
        // On Xbox, FileMerger flattens all objects into the same scope.
        // On native, the merge pipeline is incomplete so objects may live in
        // a parent dir that isn't reachable with parentDirs=false.
        if (!mObject && buf[0] != '\0') {
            ObjectDir *searchDir = dir;
            while (!mObject && searchDir) {
                ObjectDir *nextDir = nullptr;
                if (searchDir->Dir() && searchDir->Dir() != searchDir) {
                    nextDir = searchDir->Dir();
                } else if (searchDir->Loader() && searchDir->Loader()->ParentDir()) {
                    nextDir = searchDir->Loader()->ParentDir();
                }
                if (!nextDir || nextDir == searchDir) break;
                SetObj(nextDir->FindObject(buf, false));
                searchDir = nextDir;
            }
            if (!mObject) {
                SetObj(ObjectDir::Main()->FindObject(buf, false));
            }
        }
        if (!mObject && buf[0] != '\0') {
            if (print) {
                MILO_NOTIFY(
                    "%s couldn't find %s in %s", PathName(refOwner), buf, PathName(dir)
                );
            }
            return false;
        }
    } else {
#else
    // Retail X360: reads mOwner directly rather than through the virtual
    // RefOwner() accessor and does not cache it in a local -- Load() is
    // itself a member of ObjRefConcrete (RefOwner() is just `return
    // mOwner;`), and the retail codegen re-materializes mOwner@0x4 at each
    // use point instead of keeping a cached value live across the whole
    // function (confirmed via objdiff: no vtable call at entry, and three
    // separate field reads of mOwner rather than one cached register).
    if (!dir && mOwner) {
        dir = mOwner->Dir();
    }
    if (mOwner && dir) {
        // Retail X360: SetObj()'s dyncast+SetObjConcrete body is inlined
        // directly at this call site rather than calling the out-of-line
        // SetObj() (confirmed via objdiff: raw __RTDynamicCast + a direct
        // SetObjConcrete call, no call to the SetObj symbol).
        SetObjConcrete(dynamic_cast<T1 *>(dir->FindObject(buf, false)));
        if (!mObject && buf[0] != '\0') {
            if (print) {
                // Retail X360: PathName(dir) is evaluated before
                // PathName(mOwner) (confirmed via objdiff instruction
                // order) -- unspecified argument evaluation order, pin it
                // explicitly to match.
                const char *dirPath = PathName(dir);
                MILO_NOTIFY("%s couldn't find %s in %s", PathName(mOwner), buf, dirPath);
            }
            return false;
        }
    } else {
#endif
        // Retail X360: the no-dir path calls SetObjConcrete(nullptr) rather than
        // open-coding `if (mObject) { mObject->Release(this); mObject = 0; }`.
        // Semantically identical (SetObjConcrete's `obj != mObject` guard folds to
        // the same null test, and its trailing AddRef branch folds away for a null
        // argument), but the two forms are NOT codegen-identical for every T1:
        //
        //   - T1 with SINGLE inheritance from Hmx::Object: MSVC /O1 /Ob2 inlines
        //     SetObjConcrete(0) back into exactly the open-coded sequence, so the
        //     19 such instantiations were -- and stay -- byte-identical either way.
        //   - T1 reaching Hmx::Object through a VIRTUAL BASE (RndTransformable,
        //     RndGroup, RndMesh, UIComponent, UIList, BandList, CharWeightable,
        //     BandCharDesc, RndTexBlender): inlining would have to emit the vbase
        //     displacement lookup (lwz +4 / lwz +4 / add / addi +4) before the
        //     virtual Release, so /Ob2's cost heuristic DECLINES and emits
        //     `bl SetObjConcrete` instead. Retail shows exactly that single call
        //     with r3=this, r4=0; open-coding it costs 8 surplus instructions
        //     (+32 bytes) on each of those 9 instantiations.
        //
        // Verified on ?Load@?$ObjRefConcrete@VUIComponent@@... : target idx 59-61
        // is `li r4,0 / mr r3,this / bl SetObjConcrete`, ours was the vbase-adjusted
        // `bl Hmx::Object::Release` + a separate `stw 0, 8(this)`.
        //
        // Both arms are the SAME operation; only which one MSVC emits differs.
        // See ObjRefVirtualBaseObject above for why the choice is per-T1.
        if (ObjRefVirtualBaseObject<T1>::value) {
            SetObjConcrete(nullptr);
        } else {
            if (mObject) {
                mObject->Release(this);
                mObject = nullptr;
            }
        }
        if (buf[0] != '\0') {
            if (print)
                MILO_NOTIFY("No dir to find %s", buf);
        }
    }
    return true;
}

#pragma endregion
#pragma region ObjPtr
// ------------------------------------------------
// ObjPtr
// ------------------------------------------------

#ifdef HX_NATIVE
template <class T>
ObjPtr<T>::ObjPtr(Hmx::Object *owner, T *ptr) : ObjRefConcrete<T>(ptr), mOwner(owner) {}

template <class T>
ObjPtr<T>::ObjPtr(const ObjPtr &p) : ObjRefConcrete<T>(p), mOwner(p.mOwner) {}

template <class T>
ObjPtr<T>::~ObjPtr() {}
#else
// Retail X360 (fn_8270B9A8): the base ObjRefConcrete ctor stores mOwner/mObject
// (and the vtable is set to ObjPtr's during this derived ctor); then AddRef(this)
// runs HERE so the ring-ref recorded is `this` with the ObjPtr vtable already in
// place. Keeping AddRef in the derived ctor makes ObjPtr's ctor the out-of-line
// body (callers do `bl fn_8270B9A8`, not an inline vtable store + base-ctor call).
// A SECOND, distinct inline lever, not a variant of RB3_OBJPTR_INLINE_OWNER_CTOR
// in Object.h -- and on some sites the two point OPPOSITE ways. Define
// RB3_OBJPTR_FORCEINLINE_CTOR at the top of a .cpp (before any include) to force
// THIS body inline; at an `mFoo(this)` site the default `ptr = nullptr` folds the
// AddRef branch away, leaving the same three stores.
//
// WHY IT IS NOT THE SAME AS THE OWNER-ONLY LEVER: that one binds a *branch-free*
// delegating ctor, so the ??_7ObjPtr@...@6B@ address materialization floats free
// and MSVC hoists the lis/addi to the top of the scheduling region, parking it in
// a long-lived register across ~17 instructions. Force-inlining a body that
// CONTAINS a (statically folded) branch keeps the vptr materialization pinned
// after the member stores -- which is what retail does.
// Measured on ??0NgSpotlightDrawer@@QAA@XZ (world/SpotlightDrawer_NG.cpp):
//   plain out-of-line `bl`            92.37%
//   RB3_OBJPTR_INLINE_OWNER_CTOR      84.50%   (vtable hoisted -- REGRESSES)
//   RB3_OBJPTR_FORCEINLINE_CTOR       98.38%
// It also repairs the ctor's EH unwind funclet: retail spills &mFoo to a frame
// temp and the funclet reloads it (`lwz r3,0x54(r31); bl ~ObjPtr`), whereas the
// non-inlined form recomputes `this+off` from the `this$` home slot. That funclet
// (fn_824D2928, 40 B) goes 99.9 -> 100.0, so this lever is worth +1 honest match
// on top of the ctor's fuzzy gain. Whole-binary A/B (define on vs off, header
// edit present in BOTH legs): 41667 vs 41666 matched, exactly 2 rows changed,
// both in the opting-in unit -- the gate is inert for every other TU.
// Under RB3_OBJPTR_INLINE_TWOARG_CTOR the ctor is defined in-class in Object.h
// (see the #elif chain there) -- this out-of-line body must be compiled out for
// that TU or the two definitions collide.
#ifndef RB3_OBJPTR_INLINE_TWOARG_CTOR
template <class T>
// Must repeat __forceinline on the definition: MSVC honours it here, and the
// declaration in Object.h carries it only under the RB3_TU_OBJPTR_FORCEINLINE_CTOR
// gate (lanes NCCC f187 + f332 — two spellings of the same per-TU lever).
#if defined(RB3_OBJPTR_FORCEINLINE_CTOR) || defined(RB3_TU_OBJPTR_FORCEINLINE_CTOR)
__forceinline
#endif
#ifdef RB3_TU_OBJPTR_DEFER_OWNER
ObjPtr<T>::ObjPtr(Hmx::Object *owner, T *ptr) : ObjRefConcrete<T>() {
    // DEFER-BOTH (lane DS-4/C). Same reasoning as the redundant `mObject`
    // re-assignment below, applied to mOwner as well: a member store emitted
    // from the BASE mem-init list sits in the base ctor's scheduling region and
    // is free to float ABOVE the derived vptr materialization, which is what
    // makes our stream {mOwner, lis, mObject, addi, vptr} instead of retail's
    // {lis, mOwner, mObject, addi, vptr}. Writing both members in the DERIVED
    // body over a base ctor that initializes nothing pins both stores after the
    // vptr store, and the lis->addi gap is then filled by them -- retail's exact
    // order. See the gate comment on ObjRefConcrete() in obj/Object.h.
    this->mOwner = owner;
    this->mObject = ptr;
    if (this->mObject)
        this->mObject->AddRef(this);
}
#else
ObjPtr<T>::ObjPtr(Hmx::Object *owner, T *ptr) : ObjRefConcrete<T>(owner, ptr) {
    // The redundant re-assignment is LOAD-BEARING, exactly as documented for
    // RB3_OBJPTR_INLINE_OWNER_CTOR_EH at the gate in obj/Object.h. Initialising
    // mObject in the BASE mem-init emits its store BEFORE the derived vptr
    // store, leaving a constant store free to float; MSVC then fills the
    // addi->stw gap with it instead of the lis->addi gap -- a 3-instruction
    // rotation costing ~1.1-2.1pp on every TU that inlines this ctor. Assigning
    // it again here makes the base's store dead, so the surviving store lands
    // after the vtable materialization and the schedule matches retail exactly.
    // Retail's order {mOwner, vptr-lis, mObject, vptr-addi, vptr-store} is
    // UNIVERSAL in the target: a shape scan over all non-stale target asm found
    // 124 sites in retail's order and ZERO in ours.
    this->mObject = ptr;
    if (this->mObject)
        this->mObject->AddRef(this);
}
#endif // RB3_TU_OBJPTR_DEFER_OWNER
#endif

#ifdef RB3_TU_OBJPTR_DEFER_OWNER
// PER-TU (lane NCCC f264): defer BOTH members to the derived ctor body, same
// mechanism -- and reusing the SAME gate + the SAME already-declared empty
// ObjRefConcrete() base ctor -- as RB3_TU_OBJPTR_DEFER_OWNER uses for the
// two-arg ctor above (see obj/Object.h; no new declaration needed there).
// Our unfixed compile of the copy ctor emits ObjRefConcrete's OWN vtable
// materialization (lis/addi + store) from the base copy-ctor's inlined
// mem-init, immediately followed by the derived ObjPtr vtable materialization
// overwriting the same word -- a double vtable store retail does not have
// (retail shows exactly one vtable write, per the out-of-line two-arg ctor
// evidence in Object.h). Routing through the EMPTY base ctor (no mem-init at
// all) and assigning mOwner/mObject explicitly in the derived body -- after
// the point where the derived vptr store would land -- removes the base's
// provably-dead stores.
template <class T>
ObjPtr<T>::ObjPtr(const ObjPtr &p) : ObjRefConcrete<T>() {
    this->mOwner = p.mOwner;
    this->mObject = p.mObject;
    if (this->mObject)
        this->mObject->AddRef(this);
}
#else
template <class T>
ObjPtr<T>::ObjPtr(const ObjPtr &p) : ObjRefConcrete<T>(p) {
    if (this->mObject)
        this->mObject->AddRef(this);
}
#endif

// ~ObjPtr: intentionally NOT user-declared on the retail path — retail's dtor
// is implicit, which elides the ??_7ObjPtr vtable store at inlined dtor entry
// (implicit-destructor vtable-store elision pattern).
#endif

template <class T>
BinStream &operator>>(BinStream &bs, ObjPtr<T> &ptr) {
    ptr.Load(bs, true, nullptr);
    return bs;
}

#pragma endregion
#pragma region ObjOwnerPtr
// ------------------------------------------------
// ObjOwnerPtr
// ------------------------------------------------

#ifdef HX_NATIVE
template <class T>
ObjOwnerPtr<T>::ObjOwnerPtr(ObjRefOwner *owner, T *ptr)
    : ObjRefConcrete<T>(ptr), mOwner(owner), mSelfSeed(ptr) {
    MILO_ASSERT(owner, 0xC8);
}

template <class T>
ObjOwnerPtr<T>::ObjOwnerPtr(const ObjOwnerPtr &o)
    : ObjRefConcrete<T>(o.mObject), mOwner(o.mOwner), mSelfSeed(nullptr) {
    // NOT o.mSelfSeed: a copy belongs to a DIFFERENT owner, so inheriting the
    // source's seed would restore this pointer to the *original* object rather
    // than to its own owner -- a wrong value dressed as an invariant. Null is
    // the conservative choice: a copied ObjOwnerPtr behaves exactly as it does
    // today (bare null on teardown), so this repair can only improve the 14
    // directly-seeded sites and can never regress a copy.
    MILO_ASSERT(mOwner, 0xCE);
}

template <class T>
ObjOwnerPtr<T>::~ObjOwnerPtr() {}

template <class T>
Hmx::Object *ObjOwnerPtr<T>::RefOwner() const {
    return mOwner->RefOwner();
}
#else
// Retail X360: the ring-ref is mOwner (an ObjRefOwner), NOT this. We pass a null
// object to the base ctor (so it does not AddRef(this)), then AddRef(mOwner).
// The base ctor stores mOwner (as Hmx::Object*, reinterpreted) and mObject.
#ifdef RB3_TU_OBJPTR_DEFER_OWNER
// DEFER-BOTH (lane DS-4/C): base ctor initializes nothing, so BOTH the mOwner
// and mObject stores land after the derived vptr store, matching retail's
// {lis, mOwner, mObject, cmplwi, addi, vptr-store}. See obj/Object.h.
template <class T>
ObjOwnerPtr<T>::ObjOwnerPtr(ObjRefOwner *owner, T *ptr) : ObjRefConcrete<T>() {
    mOwner = reinterpret_cast<Hmx::Object *>(owner);
    mObject = ptr;
    if (mObject)
        mObject->AddRef(owner);
}
#else
template <class T>
ObjOwnerPtr<T>::ObjOwnerPtr(ObjRefOwner *owner, T *ptr)
    : ObjRefConcrete<T>(reinterpret_cast<Hmx::Object *>(owner), nullptr) {
    mObject = ptr;
    if (mObject)
        mObject->AddRef(owner);
}
#endif

template <class T>
ObjOwnerPtr<T>::ObjOwnerPtr(const ObjOwnerPtr &o)
    : ObjRefConcrete<T>(o.mOwner, nullptr) {
    mObject = o.mObject;
    if (mObject)
        mObject->AddRef(OwnerRef());
}

template <class T>
ObjOwnerPtr<T>::~ObjOwnerPtr() {
    if (mObject)
        mObject->Release(OwnerRef());
    // Prevent the base dtor from releasing again with the wrong (this) ring-ref.
    mObject = nullptr;
}

template <class T>
void ObjOwnerPtr<T>::SetOwnerObj(T *obj) {
    if (obj != mObject) {
        if (mObject)
            mObject->Release(OwnerRef());
        mObject = obj;
        if (mObject)
            mObject->AddRef(OwnerRef());
    }
}
#endif

// template <class T1>
// BinStream &operator<<(BinStream &bs, const ObjOwnerPtr<T1> &ptr);

template <class T1>
BinStream &operator>>(BinStream &bs, ObjOwnerPtr<T1> &ptr) {
    ptr.Load(bs, true, nullptr);
    return bs;
}

#pragma endregion
#pragma region ObjPtrVec
// ------------------------------------------------
// ObjPtrVec
// ------------------------------------------------

template <class T1, class T2>
ObjPtrVec<T1, T2>::ObjPtrVec(Hmx::Object *owner, EraseMode e, ObjListMode o)
    : mOwner(owner), mEraseMode(e), mListMode(o) {
    MILO_ASSERT(owner, 0x321);
}

template <class T1, class T2>
ObjPtrVec<T1, T2>::ObjPtrVec(const ObjPtrVec &other)
    : mOwner(other.mOwner), mEraseMode(other.mEraseMode), mListMode(other.mListMode) {
    *this = other;
}

template <class T1, class T2>
ObjPtrVec<T1, T2>::Node::Node(const Node &n)
    : ObjRefConcrete<T1, T2>(n), mVecOwner(n.mVecOwner) {
#ifndef HX_NATIVE
    // X360: the base copy ctor only stores fields (AddRef moved to derived ctors),
    // so register this Node as a ring-ref here to preserve ring membership.
    if (this->mObject)
        this->mObject->AddRef(this);
#endif
}

template <class T1, class T2>
ObjPtrVec<T1, T2>::~ObjPtrVec() {
    mNodes.clear();
}

template <class T1, class T2>
void ObjPtrVec<T1, T2>::ReplaceNode(Node *n, Hmx::Object *obj) {
    if (mListMode == kObjListOwnerControl) {
#ifdef HX_NATIVE
        mOwner->Replace(n, obj);
#else
        mOwner->Replace(reinterpret_cast<ObjRef *>(n), obj);
#endif
    } else {
        Hmx::Object *oldObj = n->SetObj(obj);
        if (!oldObj && mListMode == kObjListNoNull) {
#ifdef HX_NATIVE
            // During ReplaceList, erasing from the vector shifts subsequent
            // nodes via CopyRef, which modifies ring prev/next pointers and
            // corrupts the ring walk. Suppress the erase; the null entry is
            // cleaned up when the vector is destroyed or iterated.
            if (!gInReplaceList) {
                erase(iterator(mNodes.begin() + (n - mNodes.data())));
            } else {
                MILO_WARN("ReplaceNode: suppressed erase during ReplaceList (owner=%s)",
                    mOwner ? PathName(mOwner) : "<null>");
            }
#else
            erase(n);
#endif
        }
#ifdef HX_NATIVE
        else if (obj && mListMode == kObjListNoNull) {
            // MergeObject redirects refs from o1→o2. If vec had refs to both,
            // both Nodes now point to o2 (duplicate). Erase the duplicate to
            // prevent stale-pointer dereference during later destruction.
            T1 *typed = dynamic_cast<T1 *>(obj);
            if (typed) {
                for (size_t i = 0; i < mNodes.size(); i++) {
                    if (&mNodes[i] != n && mNodes[i].Obj() == typed) {
                        n->SetObj(nullptr);
                        if (!gInReplaceList)
                            erase(iterator(mNodes.begin() + (n - mNodes.data())));
                        break;
                    }
                }
            }
        }
#endif
    }
}

template <class T1, class T2>
void ObjPtrVec<T1, T2>::Set(iterator it, T1 *obj) {
    if (!obj && mListMode == 0) {
        erase(it);
    } else
        it->SetObjConcrete(obj);
}

// see Draw.cpp for this
template <class T1, class T2>
void ObjPtrVec<T1, T2>::operator=(const ObjPtrVec &other) {
    if (this == &other) return;
    mNodes.clear();
    mNodes.reserve(other.mNodes.size());
    for (const_iterator it = other.begin(); it != other.end(); ++it) {
        mNodes.push_back(Node(this));
        Set(begin() + (mNodes.size() - 1), *it);
    }
}

template <class T1, class T2>
void ObjPtrVec<T1, T2>::push_back(T1 *obj) {
    insert(end(), obj);
}

template <class T1, class T2>
typename ObjPtrVec<T1, T2>::iterator
ObjPtrVec<T1, T2>::insert(typename ObjPtrVec<T1, T2>::const_iterator it, T1 *obj) {
    if (obj != 0 || mListMode != kObjListNoNull) {
#ifdef HX_NATIVE
        int idx = it.it - mNodes.begin();
#else
        // MSVC iterators can be null (zero-initialized); GCC iterators cannot.
        int idx = it.it ? (it.it - mNodes.begin()) : 0;
#endif
        Node newNode(this);
        typename std::vector<Node>::iterator pos = mNodes.begin() + idx;
        mNodes.insert(pos, 1, newNode);
        Set(begin() + idx, obj);
    }
#ifdef HX_NATIVE
    return iterator(mNodes.begin() + (it.it - mNodes.cbegin()));
#else
    return iterator(const_cast<typename std::vector<Node>::iterator>(it.it));
#endif
}

template <class T1, class T2>
bool ObjPtrVec<T1, T2>::Load(BinStream &bs, bool print, ObjectDir *dir) {
    bool ret = true;
    mNodes.clear();
    int count;
    bs >> count;
    mNodes.reserve(count);
    if (!dir && mOwner) {
        dir = mOwner->Dir();
    }
    if (print) {
        MILO_ASSERT(dir, 0x488);
    }
    while (count != 0U) {
        char buf[0x80];
        bs.ReadString(buf, 0x80);
        if (dir) {
            T1 *casted = dynamic_cast<T1 *>(dir->FindObject(buf, false));
#ifdef HX_NATIVE
            // Native fallback: walk up the parent dir chain when not found locally.
            // On Xbox, FileMerger flattens all objects into the same scope.
            // On native, the merge pipeline is incomplete so objects may live in
            // a parent dir that isn't reachable with parentDirs=false.
            if (!casted && buf[0] != '\0') {
                ObjectDir *searchDir = dir;
                while (!casted && searchDir) {
                    ObjectDir *nextDir = nullptr;
                    if (searchDir->Dir() && searchDir->Dir() != searchDir) {
                        nextDir = searchDir->Dir();
                    } else if (searchDir->Loader() && searchDir->Loader()->ParentDir()) {
                        nextDir = searchDir->Loader()->ParentDir();
                    }
                    if (!nextDir || nextDir == searchDir) break;
                    casted = dynamic_cast<T1 *>(nextDir->FindObject(buf, false));
                    searchDir = nextDir;
                }
                if (!casted) {
                    casted = dynamic_cast<T1 *>(
                        ObjectDir::Main()->FindObject(buf, false));
                }
            }
#endif
            if (!casted && buf[0] != '\0') {
                if (print)
                    MILO_NOTIFY(
                        "%s couldn't find %s in %s", PathName(mOwner), buf, PathName(dir)
                    );
                ret = false;
            } else if (casted || mListMode != kObjListNoNull) {
                push_back(casted);
            }
        }
        count--;
    }
    return ret;
}

template <class T1>
BinStream &operator<<(BinStream &bs, const ObjPtrVec<T1, ObjectDir> &c) {
    bs << c.size();
    MILO_ASSERT(c.Owner(), 0x525);
    for (auto it = c.begin(); it != c.end(); ++it) {
        if (*it) {
            bs << (*it)->Name();
        } else {
            bs << "";
        }
    }
    return bs;
}

template <class T1>
BinStream &operator>>(BinStream &bs, ObjPtrVec<T1, ObjectDir> &vec) {
    vec.Load(bs, true, nullptr);
    return bs;
}

// ObjPtrVec::find, ::swap, ::sort bodies are in ObjPtrVec_impl.h
// to keep them out of the PCH (inlining budget pollution).
// Include ObjPtrVec_impl.h in .cpp files that call these methods.
#ifdef HX_NATIVE
#include "obj/ObjPtrVec_impl.h"
#endif

#pragma endregion
#pragma region ObjPtrList
// ------------------------------------------------
// ObjPtrList
// ------------------------------------------------

template <class T1, class T2>
ObjPtrList<T1, T2>::ObjPtrList(ObjRefOwner *owner, ObjListMode mode)
    : mSize(0), mNodes(nullptr), mOwner(owner), mListMode(mode) {
    if (mode == kObjListOwnerControl) {
        MILO_ASSERT(owner, 0x103);
    }
}

template <class T1, class T2>
ObjPtrList<T1, T2>::ObjPtrList(const ObjPtrList &other)
    : mSize(0), mNodes(nullptr), mOwner(other.mOwner), mListMode(other.mListMode) {
#ifdef HX_NATIVE
    for (iterator it = other.begin(); it != other.end(); ++it) {
        push_back(*it);
    }
#else
    // Retail X360: the copy ctor does NOT delegate through push_back()/
    // insert() (target asm inlines PoolAlloc + a raw mObject store + a direct
    // Link(end(), node) call, with none of insert()'s mListMode/assert
    // machinery surviving) — write the node alloc + link inline to match.
    for (iterator it = other.begin(); it != other.end(); ++it) {
        // Evaluate *it before the alloc (matches retail's instruction
        // schedule: the dereference is loaded before the PoolAlloc call).
        T1 *obj = *it;
        // No parens: default-init (Node is POD; value-init via "new Node()"
        // makes MSVC emit a null-check + zero-store guard that retail's asm
        // does not have). next/prev are set by Link(); mObject is set below.
        Node *node = new Node;
        node->mObject = obj;
        Link(end(), node);
    }
#endif
}

#ifdef HX_NATIVE
template <class T1, class T2>
void *ObjPtrList<T1, T2>::Node::operator new(size_t s) {
    return PoolAlloc(s, s, __FILE__, 0x122, "ObjPtrList_node");
}

template <class T1, class T2>
void ObjPtrList<T1, T2>::Node::operator delete(void *v) {
    PoolFree(sizeof(Node), v, __FILE__, 0x122, "ObjPtrList_node");
}
#else
// X360 retail: 2-arg pool form (no debug info), matching POOL_OVERLOAD's retail
// X360 spelling and the AddObject evidence (fn_824410F0: PoolAlloc(0xc,0xc)).
template <class T1, class T2>
void *ObjPtrList<T1, T2>::Node::operator new(unsigned int s) {
    return PoolAlloc(s, s);
}

template <class T1, class T2>
void ObjPtrList<T1, T2>::Node::operator delete(void *v) {
    PoolFree(sizeof(Node), v);
}
#endif

#ifdef HX_NATIVE
template <class T1, class T2>
void ObjPtrList<T1, T2>::ReplaceNode(struct ObjPtrList::Node *node, Hmx::Object *obj) {
    if (mListMode == kObjListOwnerControl) {
        mOwner->Replace(static_cast<ObjRef *>(static_cast<ObjRefConcrete<T1, T2> *>(node)), obj);
    } else {
        Hmx::Object *old = static_cast<ObjRefConcrete<T1, T2> *>(node)->SetObj(obj);
        if (!old && mListMode == kObjListNoNull) {
            // Erase unconditionally, exactly as rb3-Wii does
            // (rb3/src/system/obj/ObjPtr_p.h:266-268, `if (mMode ==
            // kObjListNoNull && !to) { it = erase(it).mNode; continue; }`).
            //
            // ⚠ THE `gInReplaceList` SUPPRESSION THAT USED TO BE HERE WAS A
            // FALSE ANALOGY, and its own last line said where it came from
            // ("Matches the guard in ObjPtrVec::ReplaceNode"). In ObjPtrVec the
            // guard is REAL: that container's Nodes live INLINE in a
            // std::vector, so erase() memmoves surviving nodes via CopyRef,
            // which rewrites live ring prev/next pointers underneath an
            // in-progress ring walk. ObjPtrList's Nodes are individually
            // heap-allocated and are never moved, so that mechanism cannot
            // occur here.
            //
            // Nor can the "frees a node other ObjRefs still point to" hazard the
            // old comment described: `old` is SetObj's return, which is the NEW
            // referent (ObjPtr_p.h:143-153 `return mObject;`). Reaching this arm
            // therefore means SetObjConcrete(nullptr) ALREADY ran
            // `mObject->Release(this)` and unlinked this node from the ring. By
            // the time erase() runs the node holds no ring membership at all, so
            // `delete node` reaches ~ObjRefConcrete with mObject == nullptr and
            // performs NO ring operation (ObjPtr_p.h:34-43).
            //
            // Suppressing the erase instead left a NULL entry in a
            // kObjListNoNull list — which the shipped
            // BandCharacter::SyncObjects loop dereferences — and the entry never
            // left, so even a null-skip would spin on !empty().
            erase(node);
        }
    }
}
#else
// X360 retail: the thin node has no SetObj/vtable; the LIST is the ring-ref.
// Do the ring ops directly on `this` (the list), mirroring rb3-Wii's Replace.
template <class T1, class T2>
void ObjPtrList<T1, T2>::ReplaceNode(struct ObjPtrList::Node *node, Hmx::Object *obj) {
    if (mListMode == kObjListOwnerControl) {
        // List-as-ref: the owner's Replace expects `from` = the dying
        // Hmx::Object* (same convention as ObjPtr::Replace), not the node.
        // Pass the currently-held object reinterpreted into the `from` slot.
        mOwner->Replace(reinterpret_cast<ObjRef *>(node->mObject), obj);
    } else if (!obj && mListMode == kObjListNoNull) {
        erase(node);
    } else {
        if (node->mObject)
            node->mObject->Release(this);
        node->mObject = dynamic_cast<T1 *>(obj);
        if (node->mObject)
            node->mObject->AddRef(this);
    }
}
#endif

template <class T1, class T2>
void ObjPtrList<T1, T2>::operator=(const ObjPtrList &other) {
    if (this == &other)
        return;
    while (mSize > other.mSize)
        pop_back();
    Node *otherNodes = other.mNodes;
    for (Node *n = mNodes; n != nullptr; n = n->next, otherNodes = otherNodes->next) {
#ifdef HX_NATIVE
        *n = *otherNodes;
#else
        // Thin X360 node has no operator=; replace the held object in place,
        // keeping the existing links, with list-as-ref Release/AddRef on `this`
        // (mirrors rb3-Wii operator= calling Set()).
        if (n->mObject)
            n->mObject->Release(this);
        n->mObject = otherNodes->mObject;
        if (n->mObject)
            n->mObject->AddRef(this);
#endif
    }
    for (; otherNodes != nullptr; otherNodes = otherNodes->next) {
        push_back(otherNodes->Obj());
    }
}

#ifndef HX_NATIVE
template <class T1, class T2>
T1 *ObjPtrList<T1, T2>::front() const {
    MILO_ASSERT(mNodes != NULL, 0x189);
    return mNodes->Obj();
}

template <class T1, class T2>
T1 *ObjPtrList<T1, T2>::back() const {
    MILO_ASSERT(mNodes != NULL, 0x18A);
    return mNodes->prev->Obj();
}
#endif

template <class T1, class T2>
void ObjPtrList<T1, T2>::pop_back() {
    MILO_ASSERT(mNodes != NULL, 0x18B);
    erase(mNodes->prev);
}

template <class T1, class T2>
void ObjPtrList<T1, T2>::pop_front() {
    MILO_ASSERT(mNodes != NULL, 0x18C);
    erase(mNodes);
}

template <class T1, class T2>
__forceinline void ObjPtrList<T1, T2>::push_back(T1 *obj) {
    // Deliberately NOT `insert(end(), obj)`: insert() returns `iterator` by
    // value, and since iterator has user-declared constructors MSVC's X360
    // ABI passes it via a caller-allocated hidden-return-object temp (visible
    // as a stack materialization at the call site). Retail's push_back skips
    // insert()'s no-null validation and calls Link() directly with a
    // freshly-allocated Node, avoiding that temp entirely.
    iterator it = end();
#ifdef HX_NATIVE
    Node *node = new Node();
    node->SetObjConcrete(obj);
#else
    // Default-init (no parens): Node has no user-declared constructor on the
    // retail X360 path, so `new Node()` (value-init) would zero-initialize
    // next/prev before Link() unconditionally overwrites them — dead code the
    // real compiler doesn't emit since it can't see across Link()'s call
    // boundary either; matching retail means avoiding the zero-init trigger
    // entirely by using default-init instead.
    Node *node = new Node;
    node->mObject = obj;
#endif
    Link(it, node);
}

template <class T1, class T2>
typename ObjPtrList<T1, T2>::iterator
ObjPtrList<T1, T2>::insert(typename ObjPtrList<T1, T2>::iterator it, T1 *obj) {
    if (mListMode == kObjListNoNull) {
#ifdef HX_NATIVE
        // NullifyAllRefs can nullify ObjPtrList entries during async unload
        // or cascade Phase 0. Skip inserting null into no-null lists.
        if (!obj)
            return end();
#endif
        MILO_ASSERT(obj, 0x177);
    }
    Node *node = new Node();
#ifdef HX_NATIVE
    // Native: Node derives ObjRefConcrete; use SetObjConcrete so AddRef fires
    // on the node (the node IS the ring-ref in native mode).
    node->SetObjConcrete(obj);
#else
    // Thin X360 node has no SetObjConcrete; just store the raw pointer. Link()
    // performs the ring AddRef(this) (binary fn_826E8098 / rb3-Wii link()).
    node->mObject = obj;
#endif
    Link(it, node);
    return node;
}

template <class T1, class T2>
void ObjPtrList<T1, T2>::Set(iterator it, T1 *obj) {
#ifdef HX_NATIVE
    // Native: Node derives ObjRefConcrete which owns the ring-ref; use
    // SetObjConcrete so AddRef/Release fire on the node (not the list).
    it.mNode->SetObjConcrete(obj);
#else
    // Retail X360: thin pool node has no ring machinery; the LIST is the
    // ring-ref. Release/AddRef `this` (ObjRefOwner) directly.
    // Mirrors rb3-Wii ObjPtrList::Set (fn_80453DC4).
    Node *n = it.mNode;
    if (n->mObject)
        n->mObject->Release(this);
    n->mObject = obj;
    if (n->mObject)
        n->mObject->AddRef(this);
#endif
}

template <class T1, class T2>
inline typename ObjPtrList<T1, T2>::iterator
ObjPtrList<T1, T2>::find(const Hmx::Object *target) const {
    for (iterator it = begin(); it != end(); ++it) {
        if (*it == target)
            return it;
    }
    return end();
}

// TODO: not 100%, work on this
// addr: 0x825C6868
template <class T1, class T2>
bool ObjPtrList<T1, T2>::remove(T1 *target) {
    for (iterator it = begin(); it != end();) {
        auto old = it++;
        if (*old == target) {
            erase(old);
            return true;
        }
    }
    return false;
}

// remove a particular item inside iterator otherIt, from list otherList,
// and insert it into this list at the position indicated by thisIt
template <class T1, class T2>
void ObjPtrList<T1, T2>::MoveItem(
    iterator thisIt, ObjPtrList<T1, T2> &otherList, iterator otherIt
) {
    if (otherIt != thisIt) {
        otherList.Unlink(otherIt.mNode);
        Link(thisIt, otherIt.mNode);
    }
}

template <class T1, class T2>
bool ObjPtrList<T1, T2>::Load(BinStream &bs, bool print) {
    bool ret = true;
    clear();
    int count;
    bs >> count;
    Hmx::Object *refOwner = mOwner ? mOwner->RefOwner() : nullptr;
    ObjectDir *dir = nullptr;
    if (refOwner)
        dir = refOwner->Dir();
    if (print) {
        MILO_ASSERT(dir, 0x210);
    }
    while (count != 0U) {
        char buf[0x80];
        bs.ReadString(buf, 0x80);
        if (dir) {
            T1 *casted = dynamic_cast<T1 *>(dir->FindObject(buf, false));
#ifdef HX_NATIVE
            if (!casted && buf[0] != '\0') {
                ObjectDir *searchDir = dir;
                while (!casted && searchDir) {
                    ObjectDir *nextDir = nullptr;
                    if (searchDir->Dir() && searchDir->Dir() != searchDir) {
                        nextDir = searchDir->Dir();
                    } else if (searchDir->Loader() && searchDir->Loader()->ParentDir()) {
                        nextDir = searchDir->Loader()->ParentDir();
                    }
                    if (!nextDir || nextDir == searchDir) break;
                    casted = dynamic_cast<T1 *>(nextDir->FindObject(buf, false));
                    searchDir = nextDir;
                }
                if (!casted) {
                    casted = dynamic_cast<T1 *>(
                        ObjectDir::Main()->FindObject(buf, false));
                }
            }
#endif
            if (!casted && buf[0] != '\0') {
                if (print)
                    MILO_NOTIFY(
                        "%s couldn't find %s in %s", PathName(refOwner), buf, PathName(dir)
                    );
                ret = false;
            } else if (casted) {
                push_back(casted);
            }
        }
        count--;
    }
    return ret;
}

template <class T1>
BinStream &operator<<(BinStream &bs, const ObjPtrList<T1, ObjectDir> &c) {
    bs << c.size();
    MILO_ASSERT(c.Owner(), 0x4E1);
    FOREACH (it, c) {
        if (*it) {
            bs << (*it)->Name();
        } else {
            bs << "";
        }
    }
    return bs;
}

template <class T1>
BinStream &operator>>(BinStream &bs, ObjPtrList<T1, ObjectDir> &list) {
    list.Load(bs, true);
    return bs;
}

template <class T1, class T2>
template <class S>
void ObjPtrList<T1, T2>::sort(const S &cmp) {
    if (mNodes && mNodes->next) {
#ifdef HX_NATIVE
        // Native Link() creates NULL-terminated forward links (last->next = nullptr).
        // PPC Link() creates circular links (last->next = sentinel).
        Node *sentinel = mNodes;
        for (Node *outer = sentinel->next; outer != nullptr; outer = outer->next) {
            for (Node *inner = outer; inner != sentinel; inner = inner->prev) {
                Node *prev = inner->prev;
                if (cmp(inner->Obj(), prev->Obj())) {
                    // Both nodes stay in the list; set membership is unchanged,
                    // so just swap the held objects — no ring Release/AddRef.
                    // Mirrors rb3-Wii ObjPtrList::sort.
                    T1 *tmp = inner->mObject;
                    inner->mObject = prev->mObject;
                    prev->mObject = tmp;
                } else {
                    break;
                }
            }
        }
#else
        // PPC ring: mNodes is the head sentinel, mNodes->prev is the tail.
        // Retail walks the *other* direction from the native form: outer runs
        // backward from (tail->prev) up to (excluding) the tail, inner walks
        // forward via next comparing (next, cur) until cmp fails. Matches
        // dc3-decomp's #else branch (ObjPtr_p.h) byte-for-byte.
        Node *last = mNodes->prev;
        for (Node *n = last->prev; n != last; n = n->prev) {
            for (Node *x = n; x != last; x = x->next) {
                Node *nextX = x->next;
                if (cmp(nextX->Obj(), x->Obj())) {
                    T1 *tmp = x->mObject;
                    x->mObject = nextX->mObject;
                    nextX->mObject = tmp;
                } else {
                    break;
                }
            }
        }
#endif
    }
}

#ifndef HX_NATIVE
// Xbox template implementations.
// These were originally explicit specializations in link_glue.cpp for each type.
// Providing generic template versions so the compiler can instantiate them.

template <class T1, class T2>
void ObjPtrList<T1, T2>::Link(iterator it, Node *node) {
    // List-as-ref: the LIST is the ring-ref, so AddRef `this` (not the thin
    // node) up front, before splicing. Matches binary fn_826E8098 and
    // rb3-Wii ObjPtrList::link().
    if (node->mObject)
        node->mObject->AddRef(this);
    node->next = it.mNode;
    if (it.mNode == mNodes) {
        if (mNodes) {
            node->prev = mNodes->prev;
            mNodes->prev = node;
        } else {
            node->prev = node;
        }
        mNodes = node;
    } else if (it.mNode == nullptr) {
        // Insert at end
        node->prev = mNodes->prev;
        mNodes->prev->next = node;
        mNodes->prev = node;
    } else {
        // Insert before a middle node
        node->prev = it.mNode->prev;
        it.mNode->prev->next = node;
        it.mNode->prev = node;
    }
    mSize++;
}

template <class T1, class T2>
typename ObjPtrList<T1, T2>::Node *ObjPtrList<T1, T2>::Unlink(Node *node) {
    MILO_ASSERT(node != NULL && mNodes != NULL, 0x26B);
    // List-as-ref: Release `this` (the ring-ref), not the thin node. The thin
    // node has no dtor, so erase()'s `delete node` only pool-frees — no double
    // release. Matches binary fn_823A2538 and rb3-Wii ObjPtrList::unlink().
    if (node->mObject)
        node->mObject->Release(this);
    if (node == mNodes) {
        if (mNodes->next != nullptr) {
            mNodes->next->prev = mNodes->prev;
            mNodes = mNodes->next;
        } else {
            mNodes = nullptr;
            mSize--;
            return nullptr;
        }
    } else if (node == mNodes->prev) {
        // Removing tail
        mNodes->prev = node->prev;
        mNodes->prev->next = nullptr;
        mSize--;
        return mNodes->prev;
    } else {
        // Middle node
        node->prev->next = node->next;
        node->next->prev = node->prev;
        mSize--;
        return node->next;
    }
    mSize--;
    return mNodes;
}

template <class T1, class T2>
typename ObjPtrList<T1, T2>::iterator ObjPtrList<T1, T2>::erase(iterator it) {
    Node *node = it.mNode;
    Node *next = Unlink(node);
    delete node;
    return iterator(next);
}

template <class T1, class T2>
Hmx::Object *ObjPtrList<T1, T2>::RefOwner() const {
    return mOwner ? mOwner->RefOwner() : 0;
}

template <class T1, class T2>
void ObjPtrList<T1, T2>::Replace(ObjRef *from, Hmx::Object *obj) {
    // Retail shape (fn_82272490 etc., 11 RTTI-verified instantiations in
    // BandCharacter): mirrors rb3-Wii ObjPtrList::Replace exactly —
    //  1. kObjListOwnerControl delegates via the owner's VTABLE (slot 8,
    //     `lwz r11,0xc(this); lwz r11,0(r11); lwz r11,8(r11); bctrl`);
    //  2. otherwise walk the ring inline (no ReplaceNode call): match the node
    //     whose held object == `from` (reinterpreted dying Hmx::Object*, same
    //     convention as ObjPtr::Replace), then either erase (NoNull + null obj)
    //     or Release/dynamic_cast/AddRef in place.
    if (mListMode == kObjListOwnerControl) {
        mOwner->Replace(from, obj);
    } else {
        Hmx::Object *fromObj = reinterpret_cast<Hmx::Object *>(from);
        for (Node *node = mNodes; node != nullptr;) {
            if ((Hmx::Object *)node->mObject == fromObj) {
                if (mListMode == kObjListNoNull && !obj) {
                    // Erase: unlink and free the node, continue from successor.
                    Node *next = Unlink(node);
                    delete node;
                    node = next;
                    continue;
                } else {
                    fromObj->Release(this);
                    node->mObject = dynamic_cast<T1 *>(obj);
                    if (obj)
                        obj->AddRef(this);
                }
            }
            node = node->next;
        }
    }
}

// The thin X360 ObjPtrList::Node is non-polymorphic and declares no RefOwner();
// the LIST is the ring-ref. (No ObjPtrList::Node::RefOwner definition here.)

// -- ObjPtrVec Xbox template implementations --

template <class T1, class T2>
typename ObjPtrVec<T1, T2>::iterator
ObjPtrVec<T1, T2>::erase(typename ObjPtrVec<T1, T2>::iterator it) {
    int idx = it.it ? (it.it - mNodes.begin()) : 0;
    if (mEraseMode == kEraseSwapLast) {
        unsigned int last = mNodes.size() - 1;
        if ((unsigned int)idx != last) {
            T1 *lastObj = mNodes.back().Obj();
            mNodes.pop_back();
            Set(begin() + idx, lastObj);
            return it;
        }
    }
    mNodes.erase(mNodes.begin() + idx);
    return it;
}

template <class T1, class T2>
typename ObjPtrVec<T1, T2>::iterator ObjPtrVec<T1, T2>::FindRef(ObjRef *ref) {
    for (iterator it = begin(); it != end(); ++it) {
        if (reinterpret_cast<ObjRef *>(&(*it)) == ref) return it;
    }
    return end();
}

#endif // !HX_NATIVE

#ifdef HX_NATIVE
// Generic template implementations for native port.
// On Xbox, these were explicit specializations in link_glue.cpp for each type.
// On native, we provide generic versions so all ObjPtrList types work.

// ObjPtrList uses a semi-circular doubly-linked list:
//   - mNodes->prev always points to the TAIL node (O(1) pop_back)
//   - Forward links (next) are NULL-terminated (last->next = nullptr)
//   - Non-head nodes have regular prev links to their predecessor
//   - Single node: prev = self (it IS the tail)

template <class T1, class T2>
void ObjPtrList<T1, T2>::Link(iterator it, Node *node) {
    node->mListOwner = this;
    Node *pos = it.mNode;
    if (mNodes == nullptr) {
        // First node: self-referencing prev (head->prev = tail = self)
        node->next = nullptr;
        node->prev = node;
        mNodes = node;
    } else if (pos == nullptr) {
        // Insert at end (push_back): append after current tail
        Node *tail = mNodes->prev;
        tail->next = node;
        node->prev = tail;
        node->next = nullptr;
        mNodes->prev = node; // update head's tail pointer
    } else if (pos == mNodes) {
        // Insert before head: new node becomes head
        node->next = mNodes;
        node->prev = mNodes->prev; // inherit tail pointer
        mNodes->prev = node; // old head's prev = new predecessor
        mNodes = node;
    } else {
        // Insert before a middle/tail node
        node->next = pos;
        node->prev = pos->prev;
        pos->prev->next = node;
        pos->prev = node;
    }
    mSize++;
}

template <class T1, class T2>
typename ObjPtrList<T1, T2>::Node *ObjPtrList<T1, T2>::Unlink(Node *node) {
    Node *next = node->next;
    Node *prev = node->prev;
    if (mNodes == node) {
        // Removing head
        mNodes = next;
        if (mNodes) {
            // New head inherits the tail pointer
            // (prev = old head's prev = tail, which is still valid)
            mNodes->prev = prev;
        }
    } else {
        // Removing non-head node
        prev->next = next;
        if (next) {
            next->prev = prev;
        }
        // If removing the tail, update head's tail pointer
        if (mNodes->prev == node) {
            mNodes->prev = prev;
        }
    }
    mSize--;
    return next;
}

template <class T1, class T2>
typename ObjPtrList<T1, T2>::iterator ObjPtrList<T1, T2>::erase(iterator it) {
    Node *node = it.mNode;
    Node *next = Unlink(node);
    delete node;
    return iterator(next);
}

template <class T1, class T2>
Hmx::Object *ObjPtrList<T1, T2>::RefOwner() const {
    return mOwner ? mOwner->RefOwner() : 0;
}

template <class T1, class T2>
void ObjPtrList<T1, T2>::Replace(ObjRef *ref, Hmx::Object *obj) {
    for (iterator it = begin(); it != end(); ++it) {
        if (it.mNode == ref) {
            ReplaceNode(it.mNode, obj);
            return;
        }
    }
}

template <class T1, class T2>
Hmx::Object *ObjPtrList<T1, T2>::Node::RefOwner() const {
    ObjPtrList<T1, T2> *list = static_cast<ObjPtrList<T1, T2> *>(mListOwner);
    return list->Owner();
}

template <class T1, class T2>
T1 *ObjPtrList<T1, T2>::front() const {
    return mNodes ? mNodes->Obj() : 0;
}

template <class T1, class T2>
T1 *ObjPtrList<T1, T2>::back() const {
    return mNodes ? mNodes->prev->Obj() : 0;
}

// -- ObjPtrVec generic template implementations --
// NOTE: Node::RefOwner is inline-defined in Object.h (line 318) for both
// Xbox and native; the BinStream operator<<'s for ObjPtrList/ObjPtrVec are
// defined unconditionally earlier in this file. Do NOT redefine them here
// or native builds will fail with redefinition errors.

template <class T1, class T2>
typename ObjPtrVec<T1, T2>::iterator
ObjPtrVec<T1, T2>::erase(ObjPtrVec<T1, T2>::iterator it) {
    return iterator(mNodes.erase(it.it));
}

template <class T1, class T2>
typename ObjPtrVec<T1, T2>::iterator ObjPtrVec<T1, T2>::FindRef(ObjRef *ref) {
    for (iterator it = begin(); it != end(); ++it) {
        if (&(*it) == ref) return it;
    }
    return end();
}

// ObjPtrVec::merge, ::unique, ::remove bodies are in ObjPtrVec_impl.h
#include "obj/ObjPtrVec_impl.h"

#endif
