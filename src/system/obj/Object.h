#pragma once
#include "obj/Data.h" /* IWYU pragma: keep */
#include "obj/DataUtl.h"
#include "obj/MessageTimer.h" /* IWYU pragma: keep */
#include "os/Debug.h"
#include "utl/BinStream.h" /* IWYU pragma: keep */
#include "utl/MemMgr.h" /* IWYU pragma: keep */
#include "utl/Symbol.h" /* IWYU pragma: keep */
#include <list> /* IWYU pragma: keep */
#include <map> /* IWYU pragma: keep */

// forward declarations
class MsgSinks;
namespace Hmx {
    class Object;
}
class ObjRef;
class ObjRefOwner;
class ObjectDir;

class MergeFilter;
void MergeObjectsRecurse(ObjectDir *, ObjectDir *, MergeFilter &, bool);

#ifdef HX_NATIVE
// Set during ReplaceList to prevent ObjPtrVec::erase from shifting vector
// elements (CopyRef during shift corrupts ring prev/next pointers).
// Checked in ObjPtrVec::ReplaceNode and Transitions::Replace to suppress
// structural mutations during ring walks.
extern bool gInReplaceList;

#endif

#pragma region ObjRef

// Object Ref/Ptr declarations
// ObjRefOwner size: 0x4
class ObjRefOwner {
public:
    ObjRefOwner() {}
    virtual ~ObjRefOwner() {}
    virtual Hmx::Object *RefOwner() const = 0;
    virtual void Replace(ObjRef *from, Hmx::Object *to) = 0;
#ifndef HX_NATIVE
    // Vtable slot +c. Present on every ring-ref so ring-walks (HasDirPtrs) can
    // dispatch uniformly. Only ObjDirPtr overrides to true.
    virtual bool IsDirPtr() { return false; }
#endif
};

void ObjRefRelinkRing(ObjRef *ref);

#ifndef HX_NATIVE
// ----------------------------------------------------------------------------
// ObjRefBase (RB3 retail X360 — the standalone smart-pointer abstract base)
// ----------------------------------------------------------------------------
// In retail, the standalone smart pointers (ObjPtr/ObjOwnerPtr) are NOT ring
// nodes. They are polymorphic objects {vtable@0, mOwner@4, mObject@8} = 0xc.
// The ring is a separate pool of {next@0, prev@4, refPtr@8} nodes owned by
// Hmx::Object; each node's refPtr@8 points back at the ring-ref. Ring-walks
// (dtor, HasDirPtrs, ...) dispatch through node->refPtr's vtable.
//
// The ring-ref is the SAME shape as ObjRefOwner (slot +4 RefOwner, slot +8
// Replace(from,to)) — the retail AddRef/dtor treat refPtr as an ObjRefOwner.
// For ObjOwnerPtr the ring-ref is mOwner (an ObjRefOwner); for ObjPtr the
// ring-ref is `this`. So the smart-pointer base derives ObjRefOwner and adds
// IsDirPtr@c, giving the 4-slot vtable retail emits (verified fn_82489268 /
// fn_82738050 / fn_82737168):
//   +0  scalar-deleting dtor
//   +4  RefOwner() const   (returns raw mOwner@4)
//   +8  Replace(ObjRef *from, Hmx::Object *to)
//   +c  IsDirPtr()
// ObjRefBase is just an alias for ObjRefOwner now — the smart-pointer base.
// (IsDirPtr@c lives in ObjRefOwner so the whole ring shares the slot.)
typedef ObjRefOwner ObjRefBase;
#endif

// ObjRef size: 0x8 (retail X360) / 0xc (HX_NATIVE: extra sentinel + vtable).
// RB3 retail's ObjRef is NON-polymorphic: next@0x0, prev@0x4, no vtable. The
// ring's Replace dispatch goes through the *referenced* object's vtable, not
// the node's (verified: Hmx::Object dtor fn_82738050 ring-walk + ring-free
// fn_82451A48, which read next@0x0 and free 0xc-byte ObjRefConcrete nodes).
// dc3-decomp models ObjRef as polymorphic (0xc, vptr@0x0) — a genuine
// RB3-vs-DC3 divergence. We keep dc3's polymorphic ObjRef for the HX_NATIVE
// engine (its ring machinery + ASAN sentinel rely on it) but drop the vtable
// for the X360 match build so Hmx::Object lays out at 0x28.
#ifdef HX_NATIVE
#define OBJREF_VIRTUAL virtual
#else
#define OBJREF_VIRTUAL
#endif
/** A circular doubly linked list to track an Object's refs. */
class ObjRef {
    friend class Hmx::Object;
    friend void ::MergeObjectsRecurse(ObjectDir *, ObjectDir *, MergeFilter &, bool);
    friend void ::ObjRefRelinkRing(ObjRef *);
#ifndef HX_NATIVE
    // X360 ring machinery (Object.cpp) needs to splice/free pool nodes.
    friend void ObjRingInsert(ObjRef *, ObjRefOwner *);
    friend void ObjRingFree(ObjRef *);
#endif

protected:
    ObjRef *next; // 0x0 (retail) / 0x4 (native, after vptr)
    ObjRef *prev; // 0x4 (retail) / 0x8 (native)
#ifdef HX_NATIVE
    // Sentinel to detect freed ObjRefs during snapshot-based ReplaceRefs.
    // Set to kAliveSentinel in constructor, cleared to 0 in destructor.
    // Checked before calling Replace on snapshot entries — if the sentinel
    // doesn't match, the ObjRef was freed by a cascading destruction during
    // an earlier Replace callback. No ABI impact (native-only field).
    static constexpr uint32_t kAliveSentinel = 0xCAFEBABE;
    uint32_t mAliveSentinel = kAliveSentinel;
#endif

    // i *think* this is good?
    void AddRef(ObjRef *ref) {
        next = ref;
        prev = ref->prev;
        ref->prev = this;
        prev->next = this;
    }

    void Release(ObjRef *ref) {
        prev->next = next;
        next->prev = prev;
    }

#ifdef HX_NATIVE
    /** Unlink from ring, suppressing ASAN for writes to potentially-freed
     *  neighbors. After cascade, ring neighbors may be in quarantined
     *  (freed) memory — the writes are harmless but ASAN would flag them. */
    __attribute__((no_sanitize("address")))
    static void SafeReleaseFromRing(ObjRef *ref) {
        ref->prev->next = ref->next;
        ref->next->prev = ref->prev;
        ref->next = ref;
        ref->prev = ref;
    }
#endif

public:
    ObjRef() {
#ifdef HX_NATIVE
        // Self-loop so AddRef can safely read next/prev on first insertion.
        // On PPC, MemAlloc zeros memory so this isn't needed.
        next = this;
        prev = this;
#endif
    }
    OBJREF_VIRTUAL ~ObjRef() {
#ifdef HX_NATIVE
        mAliveSentinel = 0;
#endif
    }
#ifdef HX_NATIVE
    bool IsAlive() const { return mAliveSentinel == kAliveSentinel; }
#endif
    OBJREF_VIRTUAL Hmx::Object *RefOwner() const { return nullptr; }
    OBJREF_VIRTUAL bool IsDirPtr() { return false; }
    OBJREF_VIRTUAL Hmx::Object *GetObj() const {
        MILO_FAIL("calling get obj on abstract ObjRef");
        return nullptr;
    }
    OBJREF_VIRTUAL void Replace(Hmx::Object *) {
        MILO_FAIL("calling get obj on abstract ObjRef");
    }
    OBJREF_VIRTUAL ObjRefOwner *Parent() const { return nullptr; }
#ifdef HX_NATIVE
    /** Null the ref's target pointer and self-loop ring pointers.
     *  No Replace callback fires — purely mechanical. Used by
     *  NullifyAllRefs during cascade Phase 0 to avoid delete-this. */
    virtual void NullifyObj() { next = this; prev = this; }
#endif

    class iterator {
    private:
        ObjRef *curRef;

    public:
        iterator() : curRef(nullptr) {}
        iterator(ObjRef *ref) : curRef(ref) {}
        operator ObjRef *() const { return curRef; }
        ObjRef *operator->() const { return curRef; }

        iterator operator++() {
            curRef = curRef->next;
            return *this;
        }

        iterator operator++(int) {
            iterator tmp = *this;
            ++*this;
            return tmp;
        }

        iterator operator--() {
            curRef = curRef->prev;
            return *this;
        }

        bool operator!=(iterator it) { return curRef != it.curRef; }
        bool operator==(iterator it) { return curRef == it.curRef; }
        bool operator!() { return curRef == nullptr; }
    };

    iterator begin() const { return iterator(next); }
    iterator end() const { return iterator((ObjRef *)this); }
    bool empty() const { return next == this; }

    /** Make `this` its own standalone single list node. */
    // Assignment order: next first, prev second (matches retail stw 0x20 then 0x24).
    void DetachSelf() { next = this; prev = this; }
    /** Alias for DetachSelf */
    void Clear() { next = this; prev = this; }

    // Splice this ref from its current ring and insert at end of targetRing.
    // Returns predecessor in the original ring for safe iteration.
    ObjRef *SpliceToRing(ObjRef *targetRing) {
        ObjRef *p = prev;
        prev->next = next;
        next->prev = prev;
        next = targetRing;
        prev = targetRing->prev;
        targetRing->prev = this;
        prev->next = this;
        return p;
    }

    void AddSelf() {
        prev->next = this;
        next->prev = this;
    }

    /** Reposition `this` so it's just before `ref`. */
    ObjRef *MoveBefore(ObjRef *ref) {
        ObjRef *oldPrev = prev;
        Release(nullptr);
        AddRef(ref);
        return oldPrev;
    }

    // per ObjectDir::HasDirPtrs, this is the way to iterate across refs
    // for (ObjRef *it = mRefs.next; it != &mRefs; it = it->next) {

#ifdef HX_NATIVE
    void ReplaceList(Hmx::Object *obj);
#else
    // X360 retail: ring entries are separate pool nodes (ObjRefNode), so the
    // node's refPtr@8 carries the ObjRefBase to dispatch Replace on. Used by
    // the ObjRefRelinkRing swap path (Key<ObjectStage>) which manipulates a
    // single detached ring directly.
    void ReplaceList(Hmx::Object *obj);
#endif
};

#ifndef HX_NATIVE
// RB3 retail X360 ring node: a separate 0xc PoolAlloc allocation spliced into
// Hmx::Object::mRefs. Layout-compatible with ObjRef at {next@0, prev@4}; adds
// refPtr@8 pointing back at the referencing ObjRefBase. Allocated/freed only
// inside Hmx::Object::AddRef/Release/~Object (Object.cpp). Ring-walkers in
// headers read RefPtrOf(it) to recover the ObjRefBase.
class ObjRefNode : public ObjRef {
public:
    // The ring-ref this node tracks: for ObjOwnerPtr it's the mOwner, for
    // ObjPtr/ObjDirPtr it's the smart pointer itself. Typed ObjRefOwner because
    // that is the interface the ring dispatches on (RefOwner@4, Replace@8).
    ObjRefOwner *refPtr; // 0x8
};

// Recover the ring-ref (ObjRefOwner) from a ring entry (which is an ObjRefNode).
inline ObjRefOwner *RefPtrOf(const ObjRef *node) {
    return static_cast<const ObjRefNode *>(node)->refPtr;
}
#else
// Native (DC3 model): ObjRef is itself polymorphic and carries RefOwner(), so
// the ring entry IS the ref — no ObjRefNode indirection. RefPtrOf is identity.
inline const ObjRef *RefPtrOf(const ObjRef *node) { return node; }
#endif

#pragma endregion
#pragma region ObjRefConcrete

// ObjRefConcrete size:
//   Retail X360: 0xc (non-polymorphic: next@0x0, prev@0x4, mObject@0x8 — no vtable)
//   HX_NATIVE:   0x10 (polymorphic: vtable@0x0, next@0x4, prev@0x8, mObject@0xc)
// DC3 uses polymorphic 0x10; RB3 retail verifiably uses 0xc (ring-free fn_82451A48
// allocates and frees 0xc-byte nodes).
#ifdef HX_NATIVE
template <class T1, class T2 = class ObjectDir>
class ObjRefConcrete : public ObjRef {
protected:
    T1 *mObject; // 0xc (HX_NATIVE)
public:
    ObjRefConcrete(T1 *obj);
    ObjRefConcrete(const ObjRefConcrete &o);
    virtual ~ObjRefConcrete();
    virtual Hmx::Object *GetObj() const { return mObject; }
    virtual void Replace(Hmx::Object *obj) { SetObj(obj); }
    void NullifyObj() override { mObject = nullptr; ObjRef::NullifyObj(); }

    T1 *operator->() const { return mObject; }
    operator T1 *() const { return mObject; }
    void operator=(T1 *obj) { SetObjConcrete(obj); }
    void operator=(const ObjRefConcrete &o) { SetObjConcrete(o); }

    void SetObjConcrete(T1 *obj);
    void CopyRef(const ObjRefConcrete &);
    Hmx::Object *SetObj(Hmx::Object *root_obj);
    bool Load(BinStream &, bool, ObjectDir *);
};
#else
// Retail X360: the standalone smart-pointer base. Polymorphic, vtable-first:
// {vtable@0, mOwner@4, mObject@8} = 0xc. The ring is separate (pool nodes), so
// there is NO inline next/prev here. RefOwner() returns mOwner@4 (vtable slot
// +4). Replace(from,to) is left pure (overridden by ObjPtr/ObjOwnerPtr). The
// ctor/SetObj/CopyRef/Load are non-trivial and use mOwner so MSVC /O1 /Ob2
// emits the ctor/Load out-of-line (matching fn_8270B9A8 / fn_8270BAD0).
template <class T1, class T2 = class ObjectDir>
class ObjRefConcrete : public ObjRefBase {
protected:
    Hmx::Object *mOwner; // 0x4
    T1 *mObject; // 0x8
public:
    ObjRefConcrete(Hmx::Object *owner, T1 *obj);
#ifdef RB3_TU_OBJPTR_OWNER_CTOR_DEFER_OBJECT
    // TU-gated (lane NCCC f70): owner-only base ctor leaving mObject to the
    // DERIVED ctor body, so the mObject store lands AFTER the derived vptr
    // store instead of before it. Only the gated ObjPtr owner-only ctor uses
    // it; the two-arg ctor above is untouched, so out-of-line ObjPtr ctors in
    // an opted-in TU keep initializing mObject exactly as before.
    ObjRefConcrete(Hmx::Object *owner) : mOwner(owner) {}
#endif
    ObjRefConcrete(const ObjRefConcrete &o);
    ~ObjRefConcrete();
    virtual Hmx::Object *RefOwner() const { return mOwner; }
    Hmx::Object *GetObj() const { return mObject; }

    T1 *operator->() const { return mObject; }
    operator T1 *() const { return mObject; }
    void operator=(T1 *obj) { SetObjConcrete(obj); }
    void operator=(const ObjRefConcrete &o) { SetObjConcrete(o); }

    void SetObjConcrete(T1 *obj);
    void CopyRef(const ObjRefConcrete &);
    Hmx::Object *SetObj(Hmx::Object *root_obj);
    bool Load(BinStream &, bool, ObjectDir *);
};
#endif

template <class T1>
BinStream &operator<<(BinStream &bs, const ObjRefConcrete<T1, class ObjectDir> &f);

#pragma endregion
#pragma region ObjPtr

#ifdef HX_NATIVE
// ObjPtr size (HX_NATIVE): 0x14 (adds mOwner@0x10 for explicit owner tracking)
template <class T>
class ObjPtr : public ObjRefConcrete<T> {
protected:
    struct DeferOwner {};
    ObjPtr(DeferOwner, T *ptr) : ObjRefConcrete<T>(ptr) {}
public:
    ObjPtr(Hmx::Object *owner, T *ptr = nullptr);
    ObjPtr(const ObjPtr &p);
    ~ObjPtr();
    Hmx::Object *mOwner; // 0x10 (HX_NATIVE only)
    virtual Hmx::Object *RefOwner() const { return mOwner; }
    Hmx::Object *Owner() const { return mOwner; }

    void operator=(T *obj) { SetObjConcrete(obj); }
    void operator=(const ObjPtr &p) { CopyRef(p); }
    T *Ptr() const { return mObject; }
};
#else
// ObjPtr size (Retail X360): 0xc {vtable@0, mOwner@4, mObject@8}. mOwner lives
// in the ObjRefConcrete base; the ring-ref is `this`.
//
// The two-arg ctor has an out-of-line body that stores all three fields and
// conditionally AddRef(this). Verified example (lane BY-1, TU5 image) --
// retail fn_8229D9C8, the ObjPtr<RndMat> instantiation:
//     stw r4,0x4(r3)   mOwner = owner
//     stw r5,0x8(r3)   mObject = ptr
//     stw r11,0x0(r3)  vtable  (per-T ??_7?$ObjPtr@VT@@@@6B@)
//     cmplwi r5,0 ; beq ; bl <AddRef>
// NOTE it is a TEMPLATE: every T gets its OWN out-of-line instantiation (each
// stores its own per-T vtable, so ICF cannot fold them). There is therefore no
// single "the" out-of-line ObjPtr ctor address -- scripts/target_symbol_map.json
// carries 6 distinct ones (0x8229ef00 0x822b9590 0x82311910 0x823bd988
// 0x823d6a10 0x823daab8). Do not cite one address pair for the family.
//
// ⛔ This comment previously cited "fn_8270B9A8 / fn_8270BAD0" as the
// out-of-line ctor/Load. Those were TU0-era addresses (written 2026-05-30,
// before the 2026-07-15 TU5 flip) and are INVALID: in the TU5 image 0x8270B9A8
// is 0x48 bytes INSIDE ??1FaderTask@@QAA@XZ and 0x8270BAD0 is 0x90 bytes inside
// ?Replace@?$ObjPtrList@VNoteVoiceInst@@VObjectDir@@@@ -- neither is a function
// entry, let alone an ObjPtr ctor. The prose description was right; only the
// addresses were stale.
template <class T>
class ObjPtr : public ObjRefConcrete<T> {
protected:
    struct DeferOwner {};
    ObjPtr(DeferOwner, T *ptr) : ObjRefConcrete<T>(nullptr, ptr) {}
public:
    // ---- PER-SITE: inline owner-only ctor ----------------------------------
    // ★★★ Retail's policy is PER-SITE, not per-TU. Proven inside a SINGLE
    // function (lane BY-1): retail ??0RndParticleSys@@ constructs FOUR
    // owner-only ObjPtrs and splits them 3 inlined / 1 called --
    //     +0x1c8 mMeshEmitter  INLINE (three stores)
    //     +0x1d4 mMat          OUT-OF-LINE (bl fn_8229D9C8 = ObjPtr<RndMat>)
    //     +0x268 mMotionParent INLINE
    //     +0x274 mBounce       INLINE
    // (offsets from cl.exe /d1reportSingleClassLayout, and the callee's T
    // independently corroborates the member: RndMat at the RndMat slot.)
    // No per-TU switch can express that.
    //
    // THE PER-SITE LEVER, no new machinery required: with the define on,
    //     mFoo(this)           -> binds the inline one-arg ctor  (three stores)
    //     mFoo(this, nullptr)  -> binds the two-arg overload, whose body is
    //                             out-of-class in obj/ObjPtr_p.h and too big
    //                             for /Ob2 -> a real `bl`.
    // So a TU picks its MAJORITY policy with the define and each minority site
    // opts back out by spelling the two-arg overload. Live users: rndobj/Part.cpp
    // (mMat) and ui/UITrigger.cpp (mCallbackObject) -- the latter is what lets
    // ?Load@UITrigger@@ AND ??0UITrigger@@ both sit at 100% at the same time,
    // which the per-TU switch alone could not do (it fixed Load 96.3->100 while
    // breaking the ctor 100->86.37).
    //
    // Retail's `mFoo(this)` sites expand two ways: (a) an out-of-line
    // `bl ??0ObjPtr...` -- the default here -- or (b) INLINE to exactly three
    // stores (mOwner@4, mObject@8 = 0, vtable@0) with no AddRef. Case (b) is
    // visible in the retail CharNeckTwist / CharEyes / RndParticleSys ctors.
    //
    // Splitting the owner-only case into its own body-in-class ctor is what
    // lets MSVC /Ob2 inline it: the two-arg ctor's `if (mObject) AddRef(this)`
    // makes that one too big to inline under /O1.
    //
    // Measured whole-binary A/B: applying this GLOBALLY is net -121 (retail
    // mostly does NOT inline), so it is gated per-TU. Define
    // RB3_OBJPTR_INLINE_OWNER_CTOR at the top of a .cpp (before any include)
    // to opt that TU in.
    //
    // ★ AUDIT (lane BY-1, 2026-07-30): toggle-off A/B of all opt-ins, same
    // split (total_functions 69367 both legs), whole binary:
    //   ON  41168 matched / 39658 honest / 34.820637% / 3684036 B
    //   OFF 41167 matched / 39657 honest / 34.810730% / 3682988 B
    // i.e. the define is worth exactly +1 fn / +1048 B = ?Load@RndPartLauncher@@,
    // and a per-UNIT diff shows exactly ONE unit moves. CharEyes and Part are
    // metric-zero -- but NOT inert (their objs change by 6164/2662 B), so each
    // was checked against retail's instruction stream rather than the metric:
    //   CharEyes.cpp   CORRECT -- retail ??0CharEyes@@ has 8 `bl`, none an
    //                  ObjPtr ctor, while the ctor builds 8 ObjPtr(this) members.
    //   Part.cpp       CORRECT for 3 of 4 sites; the 4th (mMat) is retail
    //                  out-of-line and now uses the two-arg spelling above.
    //   PartLauncher   CORRECT -- reaches 100%, i.e. byte identity with retail.
    // This is the metric-fitted-build-config check (cf. W9 MILO_MESSAGE_TIMERS):
    // the define survives it on binary evidence, the granularity did not.
#ifdef RB3_OBJPTR_INLINE_OWNER_CTOR
#ifdef RB3_TU_OBJPTR_OWNER_CTOR_DEFER_OBJECT
    // ---- PER-TU: retail's real owner-only ctor SHAPE (lane NCCC f70) -------
    // Semantically identical to the empty-bodied form below -- same three
    // stores, same values, and the guard always folds because mObject is the
    // literal nullptr -- but it emits them in retail's ORDER. Reconstructs what
    // Harmonix's general two-arg ctor must have looked like:
    //     ObjPtr(Hmx::Object *owner, T *ptr) : ObjRefConcrete<T>(owner) {
    //         mObject = ptr;
    //         if (mObject) mObject->AddRef(this);
    //     }
    // i.e. the BASE ctor takes only the owner, and the DERIVED body assigns
    // mObject. That puts the mObject store AFTER the derived vptr store, so
    // retail's stream is {mOwner, vptr-lis, mObject, vptr-addi, vptr-store}
    // with ZERO scheduler motion. Our old spelling put mObject in the base's
    // mem-init list (before the vptr store), leaving the constant store free to
    // float; MSVC then hoisted it 4 slots up into the lwz->add load-use stall.
    //
    // BOTH halves are load-bearing -- measured on ??0CharClipSet@@IAA@XZ:
    //   base mem-init + empty body        96.6%   (mObject hoisted 4 slots)
    //   base mem-init + folded AddRef     97.3%   (fixes the 3 leading insns)
    //   owner-only base + body assign,
    //     WITHOUT the folded AddRef       96.6%   (regresses again)
    //   owner-only base + body assign
    //     + folded AddRef                100.0%   (all 110 insns equal)
    // Dropping either half costs the match, so the guard is not decoration.
    //
    // Two things that do NOT work, so nobody re-hunts them:
    //   * Reordering the mem-initializer LIST is VACUOUS -- C++ initializes in
    //     DECLARATION order regardless of list order; MSVC emits byte-identical
    //     code (measured, with a live-gate control to prove the edit compiled).
    //   * Splitting the two stores across a statement boundary inside the base
    //     ctor (`: mOwner(owner) { mObject = obj; }`) is also inert -- the
    //     scheduler hoists the constant store across it anyway. Only moving the
    //     store past the derived VPTR store pins it.
    ObjPtr(Hmx::Object *owner) : ObjRefConcrete<T>(owner) {
        this->mObject = nullptr;
        if (this->mObject)
            this->mObject->AddRef(this);
    }
#else
#ifdef RB3_OBJPTR_INLINE_OWNER_CTOR_EH
    // ---- PER-TU: inline owner-only ctor that KEEPS the AddRef arm ----------
    // Same inline policy as below, but retains the two-arg body's
    // `if (mObject) AddRef(this)`. With ptr == nullptr the arm is dead and the
    // optimizer folds it to the same three stores -- but the FRONT END still
    // opens an EH region for it, so MSVC registers a cleanup for the
    // partially-constructed base subobject. That cleanup is what BOUNDS the
    // live range of the inlined ctor's `this` temp (&mFoo), letting a later
    // temp re-use the same frame slot. Without it the &mFoo temp stays live to
    // function end, a second slot is allocated, and the whole tail reschedules.
    // Evidence (lane NCCC f278, ??0RndMultiMesh@@IAA@XZ): retail 0x82469AD0 has
    // THREE unwind funclets -- the third (0x82469C8C) is `lwz r3,0x50(r31); bl
    // ??1ObjRef@@QAA@XZ`, i.e. it destroys the base subobject via a SAVED
    // POINTER rather than a this-relative offset, which is the signature of an
    // inlined ctor's EH state. Our AddRef-less inline emitted only two, so the
    // &mMesh temp never died, the std::list temp was given a SECOND frame slot,
    // and the whole tail rescheduled (92.47% -> 76.5%). Restoring the AddRef arm
    // alone took it to 98.4%; all three temps then colour to frame slot 0x50.
    //
    // The redundant `mObject = nullptr` is LOAD-BEARING, not a typo. Retail's
    // store order is mOwner(0x4) -> vtable(0x0) -> mObject(0x8); initialising
    // mObject in the BASE mem-init emits it before the vtable store instead,
    // and the scheduler then fills the addi->stw gap with it rather than the
    // lis->addi gap -- a 3-instruction rotation, the last 1.6%. Assigning it
    // again in the body makes the base's store dead, so the surviving store
    // lands after the vtable and the schedule matches exactly. 98.4% -> 100.0%.
    ObjPtr(Hmx::Object *owner) : ObjRefConcrete<T>(owner, nullptr) {
        this->mObject = nullptr;
        if (this->mObject)
            this->mObject->AddRef(this);
    }
#else
    ObjPtr(Hmx::Object *owner) : ObjRefConcrete<T>(owner, nullptr) {}
#endif
#endif
    ObjPtr(Hmx::Object *owner, T *ptr);
#elif defined(RB3_OBJPTR_INLINE_OWNER_CTOR_EH)
    // ---- PER-TU: FORCE-INLINE THE **TWO-ARG** CTOR (EH region preserved) ---
    // A third policy, distinct from RB3_OBJPTR_INLINE_OWNER_CTOR above. That
    // one inlines an owner-only ctor with an EMPTY body; this one inlines the
    // two-arg ctor *with* its `if (mObject) AddRef(this)` body still present.
    // With a literal nullptr the branch constant-folds away, so the emitted
    // code is the same three stores -- but the front end has already opened an
    // EH cleanup region for the ObjRefConcrete base around that (potentially
    // throwing) AddRef call, and that region SURVIVES the fold.
    //
    // Binary evidence (??0RndMultiMeshProxy@@, retail 0x82481100): retail emits
    // FOUR unwind funclets, one more than the empty-body form produces --
    // fn_824812FC does `lwz r3,0x50(r31); bl fn_82270298` (an ObjRef-family
    // dtor: `*p = vtbl; return`), i.e. a cleanup whose target is read from the
    // $T temp slot. That funclet is what makes the `stw &mMultiMesh,$T(r31)`
    // store LIVE. With the empty-body form the store is dead, and MSVC's
    // prescheduler is then free to hoist it -- together with the ObjPtr vtable
    // lis/addi -- ~25 instructions up into the vbase-fixup block's stall slots,
    // burning r7/r8 and reordering the three member stores (measured 84.8% vs
    // 92.7% for the plain out-of-line call).
    //
    // Measured (worktree A/B, same split, total_functions 69366 both legs):
    //   ??0RndMultiMeshProxy@@ 92.74% -> 98.46%, and our build now emits the
    //   missing funclet, which lands byte-identical to retail's 0x28-byte
    //   fn_824812FC (99.8% -> 100.0%). Whole binary +1 fn / +40 B, and EXACTLY
    //   those two functions change -- zero collateral, nothing regressed.
    // Residual 3 instructions are a scheduler tie-break in the tail (we emit
    // mObject/mOwner then lis+addi; retail emits mOwner, lis, mObject, addi).
    // NOT source-steerable: reversing the two stores in the base ctor is
    // normalized straight back by the scheduler (measured: byte-identical
    // output), and #pragma optimize("t") only churns regalloc (93.6%).
    __forceinline ObjPtr(Hmx::Object *owner, T *ptr = nullptr);
#elif defined(RB3_TU_OBJPTR_FORCEINLINE_CTOR)
    // ---- PER-TU: force the TWO-arg ctor inline (a SECOND route to case (b)) --
    // Reaches retail's inlined three-store form WITHOUT the one-arg ctor above.
    // `ptr` is nullptr at owner-only sites, so /O1 constant-folds the
    // `if (mObject) AddRef` branch away after inlining, leaving exactly the same
    // three stores (mOwner@4, mObject@8 = 0, vtable@0) -- but it SCHEDULES them
    // differently, and for some TUs that is the difference between matching and
    // not. Measured on ??0CharServoBone@@IAA@XZ (lane NCCC-0731 f332):
    //     baseline (out-of-line `bl ??0?$ObjPtr@VWaypoint@@@@`)  91.1%, base 316 B
    //     RB3_OBJPTR_INLINE_OWNER_CTOR (one-arg ctor)            79.2%, base 332 B
    //     RB3_TU_OBJPTR_FORCEINLINE_CTOR (this)                  98.5%, base 332 B
    // Target is 332 B, so BOTH inline routes are size-exact and the 91.1%
    // baseline is the semantically WRONG one (it carries a `bl` retail does not
    // have) that merely aligns better -- do not read the baseline % as "closer".
    // The one-arg route loses because MSVC hoists the ObjPtr vtable-address
    // lis/addi up into the class's own vtable-install block and keeps it live in
    // an extra register (r7); forcing the two-arg ctor instead leaves the
    // constant materialized late, right where retail puts it.
    // Residue at 98.5% is a 2-slot scheduler tie-break at the tail (retail fills
    // the gap after `addi` with `lis`, we fill it with the mObject zero-store);
    // the instruction multiset, all six address constants and every store offset
    // are already identical.
    __forceinline ObjPtr(Hmx::Object *owner, T *ptr = nullptr);
    //
    // NOTE: only valid for TUs outside the PCH-eligible
    // dirs (char/, rndobj/, world/, ui/ are PCH-excluded), otherwise the
    // /FI decomp_pch.h include of this header precedes the .cpp's #define.
    // No layout/ABI change either way -- purely an inline-policy switch.
#elif defined(RB3_OBJPTR_INLINE_TWOARG_CTOR)
    // ---- PER-TU: inline the TWO-ARG ctor (keeping its AddRef branch) ------
    // Distinct from RB3_OBJPTR_INLINE_OWNER_CTOR above, which inlines a
    // branchless one-arg ctor. Both produce retail's three stores
    // (mOwner@4, mObject@8, vtable@0), but they schedule differently at the
    // call site: with a literal-null `ptr` the `if (mObject)` test folds away
    // only AFTER inlining, so while the scheduler runs the region still
    // contains a branch and cannot hoist later constant materializations
    // across it. The branchless one-arg form lets MSVC hoist the ObjPtr
    // vtable and the member float constants ~30 instructions early, which is
    // what wrecks ??0ScrollbarDisplay@@ (92.2% -> 70.3%) even though its
    // instruction multiset is correct.
    ObjPtr(Hmx::Object *owner, T *ptr = nullptr) : ObjRefConcrete<T>(owner, ptr) {
        // Redundant re-assignment is LOAD-BEARING -- see the identical note on
        // the primary template body in obj/ObjPtr_p.h. Without it the base
        // mem-init's mObject store floats up into the addi->stw load-use stall
        // and fills the wrong gap, a 3-instruction rotation vs retail.
        this->mObject = ptr;
        if (this->mObject)
            this->mObject->AddRef(this);
    }
#else
    ObjPtr(Hmx::Object *owner, T *ptr = nullptr);
#endif
    ObjPtr(const ObjPtr &p);
    // ---- PER-TU: user-DECLARED, never-defined destructor ------------------
    // Purely a codegen lever, gated so it is inert for every TU that does not
    // opt in (one TU today: bandobj/BandScoreboard.cpp). No layout/ABI change:
    // ~ObjPtr is already virtual via ObjRefOwner, so the vtable is unchanged.
    //
    // WHY: cl 10224 packs two sequential destructible temporaries into ONE
    // stack slot only when the temp's destructor is USER-DECLARED and its body
    // is NOT visible in the TU. Measured directly on the compiler with /FAs --
    // one function, two structurally identical loops, only ~Ptr varying:
    //     ~Ptr();      declared, no body here  ->  $T1=80  $T2=80   PACKED
    //     (implicit -- none declared)          ->  $T1=80  $T2=96
    //     ~Ptr() {}    inline body             ->  $T1=80  $T2=96
    // `#pragma auto_inline(off)` does NOT do it; `__declspec(noinline)` does.
    // So the trigger is inline *candidacy* of the dtor, and an implicit dtor is
    // always a candidate.
    //
    // The one consumer is BandScoreboard::SetupScore, whose two sequential
    // push_back(ObjPtr<RndMesh>(...)) loops retail packs into a single frame
    // slot 0x58 (giving a 0xc0 frame, and two byte-identical unwind funclets
    // 0x822CF254 / 0x822CF27C that BOTH do `addi r3,r31,0x58`). Without this we
    // emitted 0x58 + 0x68 and a 0xd0 frame -- 5 diff_arg mismatches, no source
    // spelling of the function could reach it. Whole-binary A/B of this gate:
    // matched 41267 -> 41268, masked_equal 1508 -> 1507 (honest +2), code
    // 35.506874% -> 35.512127%, and EXACTLY one function changed.
    //
    // Deliberately never defined: the opted-in TU emits an external reference
    // for ~ObjPtr (and its vtable slot). That is fine here -- this build
    // compiles and objdiffs .obj files and never links the game -- but a TU
    // that must LINK cannot use this macro without providing a definition.
    //
    // Do NOT un-gate this. A globally user-declared ~ObjPtr makes MSVC store
    // ??_7ObjPtr@@6B@ before the inlined base ??1ObjRefConcrete call in every
    // containing destructor (see the "NO user dtor" note below).
#ifdef RB3_TU_OBJPTR_OUTOFLINE_DTOR
    ~ObjPtr();
#endif
    // NO user dtor: retail's ~ObjPtr is compiler-generated (implicit). A
    // user-declared empty dtor makes MSVC store ??_7ObjPtr@@6B@ before the
    // inlined base ??1ObjRefConcrete call in every containing dtor
    // (implicit-destructor vtable-store elision,
    // docs/decomp/patterns/fixable-declarations.md).
    // Vtable slot +8: Replace(from, to). The ring dispatches with the *dying
    // Hmx::Object* * in `from` (the declared ObjRef* is a modelling artefact of
    // this tree — rb3-Wii's ObjRef declares `Replace(Hmx::Object*, Hmx::Object*)`,
    // src/system/obj/Object.h:185 there — so `from` is reinterpreted, not cast).
    //
    // Retail body (verified byte-for-byte against 0x82314B70 / 0x823BDAB0, both
    // 0x60 = 96 B): NO `from == nullptr` short-circuit, and the dynamic_cast +
    // SetObjConcrete are emitted inline rather than delegating to SetObj:
    //   lwz r11,8(r31); cmplw r11,r4; bne out
    //   __RTDynamicCast(to,0,&Object,&T,0); SetObjConcrete(this, result)
    // The earlier `from==nullptr || ...; SetObj(to)` shape compiled to a 36 B
    // tail-call (22.7 % vs the retail body) and matched nothing.
    virtual void Replace(ObjRef *from, Hmx::Object *to) {
        if (mObject == reinterpret_cast<Hmx::Object *>(from))
            SetObjConcrete(dynamic_cast<T *>(to));
    }
    Hmx::Object *Owner() const { return mOwner; }

    void operator=(T *obj) { SetObjConcrete(obj); }
    void operator=(const ObjPtr &p) { CopyRef(p); }
    T *Ptr() const { return mObject; }
};
#endif

// template <class T1>
// BinStream &operator<<(BinStream &bs, const ObjPtr<T1> &ptr);

template <class T1>
BinStream &operator>>(BinStream &bs, ObjPtr<T1> &ptr);

#pragma endregion
#pragma region ObjOwnerPtr

#ifdef HX_NATIVE
// ObjOwnerPtr size (HX_NATIVE): 0x14 (adds mOwner@0x10)
/** X16 kill-switch for the ObjOwnerPtr seed restore below. Default ON; set
 *  `RB3_NO_OWNERPTR_SEED=1` to get the pre-X16 bare-null teardown in the SAME
 *  binary. One binary means the A/B has no cross-tree confound (embedded debug
 *  paths) and no `--revert` trap. Cached: NullifyObj is a teardown hot path. */
bool ObjRefSeedRestoreEnabled();

template <class T>
class ObjOwnerPtr : public ObjRefConcrete<T> {
public:
    ObjOwnerPtr(ObjRefOwner *owner, T *ptr = nullptr);
    ObjOwnerPtr(const ObjOwnerPtr &o);
    ~ObjOwnerPtr();
    ObjRefOwner *mOwner; // 0x10 (HX_NATIVE only)
    virtual Hmx::Object *RefOwner() const;
    virtual void Replace(Hmx::Object *obj) { mOwner->Replace(this, obj); }
    void operator=(T *obj) { SetObjConcrete(obj); }
    T *Ptr() const { return mObject; }

    /** X16. The ctor's seed pointer, retained so the native ring-teardown
     *  shortcut can restore this pointer's "null means me" invariant.
     *
     *  THE DEFECT (X15 §3.4, enumerated by tools/x16_ownerptr_census.py):
     *  14 members across src/ are seeded `mX(this, this)` and carry the
     *  invariant "mX is NEVER null; a null owner means *me*". The ONLY place
     *  that invariant is re-established is the consumer's `Replace()`. Native
     *  `~ObjectDir` Phase 0 tears the ring down with `NullifyAllRefs()` ->
     *  `NullifyObj()`, which deliberately does NOT fire `Replace()` -- so all
     *  14 are left NULL and the class dereferences a pointer it was entitled
     *  to assume non-null. Retail X360's teardown DOES fire Replace.
     *
     *  WHY NOT JUST FIRE Replace()? Because the skip is load-bearing, and the
     *  reason is documented upstream (dc3-decomp `07bad0ab`, 2026-03-20):
     *  MessageTask::mObj, ScriptTask::mThis, PropertyTask::mTarget and
     *  DirLoader::mProxyDir all `delete this` from `Replace()`, which would
     *  free entries out from under Phase 0's `todo` iteration. All four are
     *  ObjOwnerPtrs, so a blanket "fire Replace" would reintroduce exactly the
     *  hazard the shortcut exists to dodge.
     *
     *  WHY THE SEED IS SAFE. All four of those are seeded with the default
     *  `ptr = nullptr`, so their seed is null and NullifyObj's behaviour is
     *  BIT-IDENTICAL to today -- no Replace fires, nothing is deleted. And no
     *  ObjOwnerPtr anywhere in src/ is seeded with a *foreign* object (checked:
     *  every `(this, <non-null>)` hit in the tree is an ObjPtrList/ObjPtrVec
     *  `(owner, mode)` ctor, not an ObjOwnerPtr), so the seed can only ever be
     *  null or self. That makes this repair provably scoped to the 14 sites
     *  and a no-op everywhere else -- upstream, but not blanket. */
    T *mSelfSeed;

    /** Restore the seeded invariant instead of leaving a bare null.
     *  Reproduces retail's post-`Replace` state (`mX == this`, re-registered in
     *  its own ring) WITHOUT invoking any consumer callback. */
    void NullifyObj() override {
        T *seed = ObjRefSeedRestoreEnabled() ? mSelfSeed : nullptr;
        ObjRefConcrete<T>::NullifyObj(); // mObject = nullptr, ring self-loops
        if (seed) {
            mObject = seed;
            seed->AddRef(this);
        }
    }
};
#else
// ObjOwnerPtr size (Retail X360): 0xc {vtable@0, mOwner@4, mObject@8}. The
// ring-ref is mOwner (an ObjRefOwner) — AddRef/Release/Replace dispatch on the
// owner, not on this. So ObjOwnerPtr::Replace is empty (retail fn_82465928).
// mOwner is stored in the base mOwner@4 slot (reinterpreted ObjRefOwner*).
template <class T>
class ObjOwnerPtr : public ObjRefConcrete<T> {
public:
    ObjOwnerPtr(ObjRefOwner *owner, T *ptr = nullptr);
    ObjOwnerPtr(const ObjOwnerPtr &o);
    ~ObjOwnerPtr();
    ObjRefOwner *OwnerRef() const {
        return reinterpret_cast<ObjRefOwner *>(mOwner);
    }
    virtual Hmx::Object *RefOwner() const { return OwnerRef()->RefOwner(); }
    // Vtable slot +8: empty — the ring dispatches Replace on mOwner, not this.
    virtual void Replace(ObjRef *, Hmx::Object *) {}
    // Ring-ref is mOwner (not this), so manage the ring directly here.
    void SetOwnerObj(T *obj);
    void operator=(T *obj) { SetOwnerObj(obj); }
    // Copy-assign MUST be user-declared and MUST route through SetOwnerObj.
    // Without it the implicit copy-assignment is used, which memberwise-assigns
    // the base and therefore calls ObjRefConcrete::operator=(const
    // ObjRefConcrete&) -> SetObjConcrete -- managing the ring on `this` instead
    // of on mOwner. Retail calls SetOwnerObj at every such site (lane DS-1:
    // 20 charged bl slots across vector/ObjVector<ObjOwnerPtr<T>> internals in
    // CharBonesMeshes, CharClipGroup, CharEyes, Waypoint, plus the member
    // copy-assigns in the Copy() bodies of the RndXxxAnim family).
    void operator=(const ObjOwnerPtr &o) { SetOwnerObj(o.mObject); }
    T *Ptr() const { return mObject; }
};
#endif

// ---------------------------------------------------------------------------
// RefIs: portable "does this ring Replace() target the given member pointer?"
// In native, the ring passes the ObjRef* member as `from`; in X360 retail the
// ring passes the dying Hmx::Object* as `from` (the owner's Replace is invoked).
// Consumer Replace() overrides use RefIs(from, member) to stay portable.
// ---------------------------------------------------------------------------
template <class P>
inline bool RefIs(ObjRef *from, const P &member) {
#ifdef HX_NATIVE
    return from == static_cast<ObjRef *>(const_cast<P *>(&member));
#else
    // X360: the ring dispatches the owner's Replace with the dying Hmx::Object*
    // as `from`. Compare against the held object's raw address. (MI/vbase cases
    // can false-negative here, but these are NonMatching consumer ring paths.)
    return reinterpret_cast<void *>(from)
        == reinterpret_cast<void *>(const_cast<P &>(member).Ptr());
#endif
}

// Overload for members that are plain (non-owning) raw pointers rather than
// ObjPtr/ObjOwnerPtr wrappers (e.g. DirLoader::mProxyDir on X360 retail, which
// has no owning-ptr wrapper — see DirLoader.h). Preferred over the primary
// template by overload resolution for pointer-typed members.
template <class P>
inline bool RefIs(ObjRef *from, P *const &member) {
    return reinterpret_cast<void *>(from) == reinterpret_cast<void *>(member);
}

template <class T1>
BinStream &operator<<(BinStream &bs, const ObjOwnerPtr<T1> &ptr);

template <class T1>
BinStream &operator>>(BinStream &bs, ObjOwnerPtr<T1> &ptr);

#pragma endregion
#pragma region ObjPtrVec

enum EraseMode {
    kEraseShift = 0,
    kEraseSwapLast = 1,
};

enum ObjListMode {
    kObjListNoNull,
    kObjListAllowNull,
    kObjListOwnerControl
};

// ObjPtrVec size: 0x1c
template <class T1, class T2 = class ObjectDir>
class ObjPtrVec : public ObjRefOwner {
private:
    // Node size: 0x10. Phase 2 shrank ObjRefConcrete from 0x10 to 0xc
    // (dropped the X360 vtable-less retail layout), so this Node auto-shrank
    // from 0x14 to 0x10. The Node stays the polymorphic ring-ref
    // {vtable@0, mOwner@4, mObject@8} + mVecOwner@0xc — see EVIDENCE_SUMMARY.
    struct Node : public ObjRefConcrete<T1, T2> {
#ifdef HX_NATIVE
        Node(ObjRefOwner *owner) : ObjRefConcrete<T1>(nullptr), mVecOwner(owner) {}
#else
        Node(ObjRefOwner *owner) : ObjRefConcrete<T1>(nullptr, nullptr), mVecOwner(owner) {}
#endif
        Node(const Node &n);
        virtual ~Node() {}
        virtual Hmx::Object *RefOwner() const {
            ObjPtrVec<T1, T2> *vec = static_cast<ObjPtrVec<T1, T2> *>(mVecOwner);
            return vec->Owner();
        }
#ifdef HX_NATIVE
        virtual void Replace(Hmx::Object *obj) {
            ObjPtrVec<T1, T2> *vec = static_cast<ObjPtrVec<T1, T2> *>(mVecOwner);
            vec->ReplaceNode(this, obj);
        }
        virtual ObjRefOwner *Parent() const { return mVecOwner; }
        void NullifyObj() override {
            ObjRefConcrete<T1, T2>::NullifyObj();
            ObjPtrVec<T1, T2> *vec = static_cast<ObjPtrVec<T1, T2> *>(mVecOwner);
            if (vec && vec->Mode() == kObjListNoNull && !gInReplaceList) {
                vec->erase(typename ObjPtrVec<T1, T2>::iterator(
                    vec->mNodes.begin() + (this - vec->mNodes.data())
                ));
            }
        }
#else
        // X360: vtable slot +8 — dispatch to the owning vec's ReplaceNode.
        virtual void Replace(ObjRef *from, Hmx::Object *obj) {
            ObjPtrVec<T1, T2> *vec = static_cast<ObjPtrVec<T1, T2> *>(mVecOwner);
            vec->ReplaceNode(this, obj);
        }
#endif

        T1 *Obj() const { return mObject; }
        Node &operator=(const Node &n) {
            CopyRef(n);
            mVecOwner = n.mVecOwner;
            return *this;
        }

        /** The ObjPtrVec this Node belongs to. */
        ObjRefOwner *mVecOwner; // 0xc
    };

protected:
    virtual Hmx::Object *RefOwner() const {
        MILO_FAIL("should never be called");
        return nullptr;
    }
    virtual void Replace(ObjRef *, Hmx::Object *) {
        MILO_FAIL("should never be called");
    }

    void ReplaceNode(Node *, Hmx::Object *);

public:
    // this derives off of std::vector<Node>::iterator in some way
    class iterator {
        friend class const_iterator;
        friend class ObjPtrVec;

    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef Node value_type;
        typedef ptrdiff_t difference_type;
        typedef Node *pointer;
        typedef Node &reference;

    private:
        typedef typename std::vector<Node>::iterator Base;
        Base it;

    public:
        iterator() : it() {}
        iterator(Base base) : it(base) {}

        Node &operator*() const { return *it; }
        Node *operator->() const { return &(*it); }

        iterator operator+(int idx) const { return iterator(it + idx); }
        int operator-(const iterator &other) const { return it - other.it; }

        iterator operator++() {
            ++it;
            return *this;
        }

        iterator operator--() {
            --it;
            return *this;
        }

        bool operator!=(const iterator &other) const { return it != other.it; }
        bool operator==(const iterator &other) const { return it == other.it; }
    };
    // ditto
    class const_iterator {
        friend class ObjPtrVec;

    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef Node value_type;
        typedef ptrdiff_t difference_type;
        typedef const Node *pointer;
        typedef const Node &reference;

    private:
        typedef typename std::vector<Node>::const_iterator Base;
        Base it;

    public:
        const_iterator(Base base) : it(base) {}
        const_iterator(iterator non_const) : it(non_const.it) {}

        const Node &operator*() const { return *it; }
        const Node *operator->() const { return &(*it); }

        const_iterator operator+(int idx) const { return const_iterator(it + idx); }

        const_iterator operator++() {
            ++it;
            return *this;
        }

        const_iterator operator--() {
            --it;
            return *this;
        }

        bool operator!=(const const_iterator &other) const { return it != other.it; }
        bool operator==(const const_iterator &other) const { return it == other.it; }
    };

    ObjPtrVec(Hmx::Object *owner, EraseMode = (EraseMode)0, ObjListMode = kObjListNoNull);
    ObjPtrVec(const ObjPtrVec &);
    virtual ~ObjPtrVec();

#ifdef HX_NATIVE
    iterator begin() { return mNodes.begin(); }
    iterator end() { return mNodes.end(); }
    const_iterator begin() const { return mNodes.begin(); }
    const_iterator end() const { return mNodes.end(); }
#else
    iterator begin() { return empty() ? nullptr : mNodes.begin(); }
    // Register swap pattern: declaring size() before begin() affects PPC register allocation
    // Variable declaration order is critical for matching FlowNode::MiloPreRun codegen
    // See STYLEGUIDE.md §Variable Declaration Order
    iterator end() { return begin() + size(); }
    const_iterator begin() const { return empty() ? nullptr : mNodes.begin(); }
    const_iterator end() const { return begin() + size(); }
#endif
    iterator FindRef(ObjRef *);

    iterator erase(iterator);
    iterator insert(const_iterator, T1 *);
    const_iterator find(const Hmx::Object *) const;
    iterator find(const Hmx::Object *);
    int size() const { return mNodes.size(); }
    bool empty() const { return mNodes.empty(); }
    T1 *front() const { return *begin(); }
    T1 *operator[](int idx) { return mNodes[idx].Obj(); }
    const T1 *operator[](int idx) const { return mNodes[idx].Obj(); }

    template <class S>
    void sort(const S &);

    bool remove(T1 *);
    void push_back(T1 *);
    void swap(int, int);
    bool Load(BinStream &, bool, ObjectDir *);
    void clear() { mNodes.clear(); }
    void reserve(unsigned int n) { mNodes.reserve(n); }
    void unique();
    __declspec(noinline) void Set(iterator it, T1 *obj);
    void merge(const ObjPtrVec &);
    Hmx::Object *Owner() const { return mOwner; }
    ObjListMode Mode() const { return mListMode; }
    // see Draw.cpp for this
    void operator=(const ObjPtrVec &other);

private:
    std::vector<Node> mNodes; // 0x4
    Hmx::Object *mOwner; // 0x10
    EraseMode mEraseMode; // 0x14
    ObjListMode mListMode; // 0x18
};

template <class T1>
BinStream &operator<<(BinStream &bs, const ObjPtrVec<T1, ObjectDir> &vec);

template <class T1>
BinStream &operator>>(BinStream &bs, ObjPtrVec<T1, ObjectDir> &vec);

#pragma endregion
#pragma region ObjPtrList

// ObjPtrList size: 0x14
template <class T1, class T2 = class ObjectDir>
class ObjPtrList : public ObjRefOwner {
    friend class RndGroup;
public:
    ObjPtrList(ObjRefOwner *, ObjListMode = kObjListNoNull);
    ObjPtrList(const ObjPtrList &);
    virtual ~ObjPtrList() { clear(); }

private:
#ifdef HX_NATIVE
    // Native: the Node is the polymorphic ring-ref (ObjRefConcrete derivative).
    // The native ring machinery (NullifyObj, Parent, cascade teardown) relies on
    // the Node carrying a vtable + mListOwner back-pointer. Size: 0x1c.
    struct Node : public ObjRefConcrete<T1, T2> {
        // Clang needs explicit using-declarations for dependent base class members
        using ObjRefConcrete<T1, T2>::mObject;
        using ObjRefConcrete<T1, T2>::SetObj;
        using ObjRefConcrete<T1, T2>::SetObjConcrete;
        Node() : ObjRefConcrete<T1, T2>(nullptr) {}
        virtual ~Node() {}
        virtual Hmx::Object *RefOwner() const;
        virtual void Replace(Hmx::Object *obj) {
            ObjPtrList<T1, T2> *list = static_cast<ObjPtrList<T1, T2> *>(mListOwner);
            list->ReplaceNode(this, obj);
        }
        virtual ObjRefOwner *Parent() const { return mListOwner; }
        void NullifyObj() override {
            ObjRefConcrete<T1, T2>::NullifyObj();
            ObjPtrList<T1, T2> *list =
                static_cast<ObjPtrList<T1, T2> *>(mListOwner);
            if (list && list->Mode() == kObjListNoNull && !gInReplaceList) {
                list->Unlink(this);
                delete this;
            }
        }
        static void *operator new(size_t);
        static void operator delete(void *);

        T1 *Obj() const { return mObject; }
        void operator=(const Node &n) { SetObjConcrete(n.mObject); }

        ObjRefOwner *mListOwner; // 0x10
        Node *next; // 0x14
        Node *prev; // 0x18
    };
#else
    // RB3 retail X360 (EVIDENCE_SUMMARY §ObjPtrList): the Node is a thin 0xc
    // pool allocation with NO vtable, NO mOwner. The LIST is the ring-ref (it
    // derives ObjRefOwner); the ring AddRefs/Releases *the list*, not the node
    // (verified Link=fn_826E8098, Unlink=fn_823A2538, AddObject=fn_824410F0:
    // `node = PoolAlloc(0xc,0xc); *node = obj; Link(list,it,node)`).
    //   Node = { mObject@0, next@4, prev@8 } = 0xc
    struct Node {
        T1 *mObject; // 0x0
        Node *next; // 0x4
        Node *prev; // 0x8

        // Thin pool node: 2-arg PoolAlloc/PoolFree (matches POOL_OVERLOAD's
        // retail X360 form — no __FILE__/__LINE__/name; see PoolAlloc.h §7).
        static void *operator new(unsigned int);
        static void operator delete(void *);

        T1 *Obj() const { return mObject; }
    };
#endif
    int mSize; // 0x4
    Node *mNodes; // 0x8
    ObjRefOwner *mOwner; // 0xc
    ObjListMode mListMode; // 0x10

    virtual Hmx::Object *RefOwner() const;
    virtual void Replace(ObjRef *, Hmx::Object *);
    void ReplaceNode(Node *, Hmx::Object *);

public:
    class iterator {
    public:
        iterator() : mNode(0) {}
        iterator(Node *node) : mNode(node) {}
        T1 *operator*() { return mNode->Obj(); }

        iterator operator++() {
            mNode = mNode->next;
            return *this;
        }

        iterator operator++(int) {
            iterator tmp = *this;
            ++*this;
            return tmp;
        }

        // iterator &operator=(T1 *obj) {
        //     mNode->SetObjConcrete(obj);
        //     return *this;
        // }

        bool operator!=(iterator it) { return mNode != it.mNode; }
        bool operator==(iterator it) { return mNode == it.mNode; }
        bool operator!() { return mNode == 0; }

        struct Node *mNode; // 0x0
    };

    ObjListMode Mode() const { return mListMode; }
    int size() const { return mSize; }
    bool empty() const { return mSize == 0; }
    Hmx::Object *Owner() const { return mOwner ? mOwner->RefOwner() : nullptr; }

    void clear() {
        while (mSize != 0)
            pop_back();
    }

    void DeleteAll() {
        while (!empty()) {
            T1 *cur = front();
            pop_front();
            delete cur;
        }
    }

    T1 *front() const;
    T1 *back() const;
    void pop_front();
    void pop_back();
    void push_back(T1 *obj);
    iterator find(const Hmx::Object *target) const;
    iterator begin() const { return iterator(mNodes); }
    iterator end() const { return iterator(0); }
    iterator erase(iterator);
    iterator insert(iterator, T1 *);
    void Set(iterator it, T1 *obj);
    void MoveItem(iterator thisIt, ObjPtrList<T1, T2> &otherList, iterator otherIt);

    typedef bool SortFunc(T1 *, T1 *);
    template <typename S>
    void sort(const S &);

    void operator=(const ObjPtrList &list);
    bool remove(T1 *);
    // RB3-360 retail signature: 2 params, matching the rb3-Wii oracle
    // (ObjPtr_p.h:517 `bool Load(BinStream&, bool)`).  Retail's call site in
    // CharIKScale::Load passes exactly `r3=this, r4=bs, r5=1` -- the DC3-era
    // trailing `ObjectDir*, bool` pair made every `bs >> objPtrList` emit two
    // extra `li` argument setups.  The 4th param was never read in the body and
    // the only caller (operator>>) always passed `nullptr, true`.
    bool Load(BinStream &bs, bool);

private:
    void Link(iterator, Node *);
    Node *Unlink(Node *);
};

template <class T1>
BinStream &operator<<(BinStream &bs, const ObjPtrList<T1, ObjectDir> &list);

template <class T1>
BinStream &operator>>(BinStream &bs, ObjPtrList<T1, ObjectDir> &list);

#pragma endregion
#pragma region DataNodeObjTrack

// DataNodeObjTrack
class DataNodeObjTrack {
public:
    DataNodeObjTrack(const DataNode &node) : mObj(nullptr, nullptr) {
        mNode = node.Evaluate();
        if (mNode.Type() == kDataObject) {
            mObj = mNode.GetObj();
        }
    }
    DataNode Node() const {
        if (mNode.Type() == kDataObject) {
            return mObj.Ptr();
        } else
            return mNode;
    }
    DataNodeObjTrack &operator=(const DataNode &node) {
        mNode = node.Evaluate();
        if (mNode.Type() == kDataObject) {
            mObj = mNode.GetObj();
        }
        return *this;
    }
    DataNodeObjTrack &operator=(const DataNodeObjTrack &other) {
        mNode = other.Node().Evaluate();
        if (mNode.Type() == kDataObject) {
            mObj = mNode.GetObj();
        }
        return *this;
    }

protected:
    ObjPtr<Hmx::Object> mObj; // 0x0
    DataNode mNode; // 0x14
};

#pragma endregion
#pragma region Object Macros

// Hmx::Object-centric macros
/** Get this Object's path name.
 * @param [in] obj The Object.
 * @returns The Object's path name, or "NULL Object" if it doesn't exist.
 */
const char *PathName(const class Hmx::Object *obj);

#define NULL_OBJ (Hmx::Object *)nullptr

// BEGIN CLASSNAME MACRO -----------------------------------------------------------------
#define OBJ_CLASSNAME(classname)                                                         \
    virtual Symbol ClassName() const { return StaticClassName(); }                       \
    static Symbol StaticClassName() {                                                    \
        static Symbol name(#classname);                                                  \
        return name;                                                                     \
    }
// END CLASSNAME MACRO -------------------------------------------------------------------

// BEGIN SET TYPE MACRO ------------------------------------------------------------------
extern DataArray *SystemConfig(Symbol, Symbol, Symbol);
// Retail shape (measured 2026-07-29, lane BO6): the guarded static `types`
// initialization runs FIRST, BEFORE the `classname.Null()` test, and the stripped
// MILO_NOTIFY residue evaluates `PathName(this)` BEFORE the `ClassName()` vcall.
// This header previously carried a second, DIFFERENT spelling that sank the static
// init into the non-null branch and inverted the notify args; it produced a body
// retail never emits and was worth a uniform 62.92% (316 B, vbase-adjusted `this`)
// / 64.95% (252 B, direct `this`) on every class that saw it.  Decisive census over
// all 448 paired `?SetType@*@@UAAXVSymbol@@@Z` bodies: ZERO classes reached 100%
// through that spelling, while every class that reached OBJ_SET_TYPE_ENGINE (or
// obj/ObjMacros.h's equivalently-shaped OBJ_SET_TYPE) did.  The two spellings are
// therefore collapsed: OBJ_SET_TYPE is now OBJ_SET_TYPE_ENGINE.
// (obj/ObjMacros.h keeps its own same-shaped redefinition; TUs that include it were
// already matching and are unaffected.)
#define OBJ_SET_TYPE(classname) OBJ_SET_TYPE_ENGINE(classname)
// Engine-era variant (synth_xbox FxSend*360 et al.): the static `types` config is
// initialized BEFORE the null check, the null branch tail-calls SetTypeDef(0), and
// SetTypeDef(found) runs even when the lookup failed (after the notify).
#define OBJ_SET_TYPE_ENGINE(classname)                                                    \
    virtual void SetType(Symbol classname) {                                              \
        static DataArray *types =                                                         \
            SystemConfig("objects", StaticClassName(), "types");                          \
        if (classname.Null()) {                                                           \
            SetTypeDef(nullptr);                                                          \
        } else {                                                                          \
            DataArray *found = types->FindArray(classname, false);                        \
            if (found) {                                                                  \
                SetTypeDef(found);                                                        \
            } else {                                                                      \
                MILO_NOTIFY(                                                              \
                    "%s:%s couldn't find type %s", PathName(this), ClassName(), classname \
                );                                                                        \
                SetTypeDef(nullptr);                                                      \
            }                                                                             \
        }                                                                                 \
    }
// END SET TYPE MACRO --------------------------------------------------------------------

// BEGIN HANDLE MACROS -------------------------------------------------------------------
// Retail RB3 drops the per-handler MessageTimer profiling object entirely — every
// ::Handle is just `Symbol sym = _msg->Sym(1);` then the handler body (no Timer
// ctor / Restart / SplitMs / AddTime). Retail keeps MILO_ASSERT strings (so the
// build is not a clean MILO_DEBUG-off build); only message-timer profiling is off.
// Gate the timer on a dedicated MILO_MESSAGE_TIMERS macro (undefined by default =
// retail shape); define it to restore handler profiling without disturbing the
// rest of MILO_DEBUG.
#ifdef MILO_MESSAGE_TIMERS
#define BEGIN_HANDLERS(objType)                                                          \
    DataNode objType::Handle(DataArray *_msg, bool _warn) {                              \
        Symbol sym = _msg->Sym(1);                                                       \
        MessageTimer timer(                                                              \
            (MessageTimer::Active()) ? static_cast<Hmx::Object *>(this) : 0, sym         \
        );

// for handlers of objects that aren't directly Hmx::Objects (i.e. UIListProvider)
#define BEGIN_CUSTOM_HANDLERS(objType)                                                   \
    DataNode objType::Handle(DataArray *_msg, bool _warn) {                              \
        Symbol sym = _msg->Sym(1);                                          \
        MessageTimer timer(                                                              \
            (MessageTimer::Active()) ? dynamic_cast<Hmx::Object *>(this) : 0, sym        \
        );
#else
#define BEGIN_HANDLERS(objType)                                                          \
    DataNode objType::Handle(DataArray *_msg, bool _warn) {                              \
        Symbol sym = _msg->Sym(1);

// for handlers of objects that aren't directly Hmx::Objects (i.e. UIListProvider)
#define BEGIN_CUSTOM_HANDLERS(objType)                                                   \
    DataNode objType::Handle(DataArray *_msg, bool _warn) {                              \
        Symbol sym = _msg->Sym(1);
#endif

#define _NEW_STATIC_SYMBOL(str) static Symbol _s(#str);

#define _HANDLE_CHECKED(expr)                                                            \
    {                                                                                    \
        DataNode result = expr;                                                          \
        if (result.Type() != kDataUnhandled)                                             \
            return result;                                                               \
    }

#define HANDLE(s, func)                                                                  \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s)                                                                   \
            _HANDLE_CHECKED(func(_msg))                                                  \
    }

#define HANDLE_EXPR(s, expr)                                                             \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s)                                                                   \
            return expr;                                                                 \
    }

#define HANDLE_ACTION(s, action)                                                         \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s) {                                                                 \
            /* for style, require any side-actions to be performed via comma operator */ \
            (action);                                                                    \
            return 0;                                                                    \
        }                                                                                \
    }

#define HANDLE_ACTION_IF(s, cond, action)                                                \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s) {                                                                 \
            if (cond) {                                                                  \
                /* for style, require any side-actions to be performed via comma         \
                 * operator */                                                           \
                (action);                                                                \
            }                                                                            \
            return 0;                                                                    \
        }                                                                                \
    }

#define HANDLE_ACTION_IF_ELSE(s, cond, action_true, action_false)                        \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s) {                                                                 \
            if (cond) {                                                                  \
                /* for style, require any side-actions to be performed via comma         \
                 * operator */                                                           \
                (action_true);                                                           \
            } else {                                                                     \
                (action_false);                                                          \
            }                                                                            \
            return 0;                                                                    \
        }                                                                                \
    }

#define HANDLE_ARRAY(array)                                                              \
    {                                                                                    \
        /* this needs to be placed up here to match Hmx::Object::Handle */               \
        DataArray *found;                                                                \
        if (array && (found = array->FindArray(sym, false))) {                           \
            _HANDLE_CHECKED(found->ExecuteScript(1, this, _msg, 2))                      \
        }                                                                                \
    }

#define HANDLE_MESSAGE(msg)                                                              \
    if (sym == msg::Type())                                                              \
    _HANDLE_CHECKED(OnMsg(msg(_msg)))

#define HANDLE_METHOD(func) _HANDLE_CHECKED(func(_msg))

#define HANDLE_FORWARD(func) _HANDLE_CHECKED(func(_msg, false))

#define HANDLE_MEMBER(member) HANDLE_FORWARD(member.Handle)

#define HANDLE_MEMBER_PTR(member)                                                        \
    if (member)                                                                          \
    HANDLE_FORWARD(member->Handle)

#define HANDLE_SUPERCLASS(parent) HANDLE_FORWARD(parent::Handle)

#define HANDLE_VIRTUAL_SUPERCLASS(parent)                                                \
    if (ClassName() == StaticClassName())                                                \
    HANDLE_SUPERCLASS(parent)

// Retail (release) END_HANDLERS still evaluates PathName(this) at the unhandled
// tail — the warn message text is stripped, but the PathName(this) argument's
// side effect (a virtual FindPathName call, fn_82732F68) is RETAINED in every
// Family-B Handle. Our global release MILO_NOTIFY is ((void)sizeof(MakeString(...)))
// and sizeof is UNEVALUATED, so it drops PathName(this) entirely — leaving our
// Handle tails a superclass-forward where retail emits the bl PathName. Emit the
// comma-evaluated form directly here (same shape as MILO_FAIL's (void)(__VA_ARGS__)
// fix for Find<T>) so the PathName side effect lands without restoring the notifier
// text. HX_NATIVE keeps the real notifier for the native build.
#ifndef HX_NATIVE
#define END_HANDLERS                                                                     \
    if (_warn)                                                                           \
        (void)(PathName(this), sym);                                                     \
    return DATA_UNHANDLED;                                                               \
    }
#else
#define END_HANDLERS                                                                     \
    if (_warn)                                                                           \
        MILO_NOTIFY("%s unhandled msg: %s", PathName(this), sym);                        \
    return DATA_UNHANDLED;                                                               \
    }
#endif

// for handlers of objects that aren't directly Hmx::Objects (i.e. UIListProvider)
#ifndef HX_NATIVE
#define END_CUSTOM_HANDLERS                                                              \
    if (_warn)                                                                           \
        (void)(PathName(dynamic_cast<Hmx::Object *>(this)), sym);                        \
    return DATA_UNHANDLED;                                                               \
    }
#else
#define END_CUSTOM_HANDLERS                                                              \
    if (_warn)                                                                           \
        MILO_NOTIFY(                                                                     \
            "%s unhandled msg: %s", PathName(dynamic_cast<Hmx::Object *>(this)), sym     \
        );                                                                               \
    return DATA_UNHANDLED;                                                               \
    }
#endif
// END HANDLE MACROS ---------------------------------------------------------------------

// BEGIN SYNCPROPERTY MACROS -------------------------------------------------------------
#define BEGIN_PROPSYNCS(objType)                                                         \
    bool objType::SyncProperty(DataNode &_val, DataArray *_prop, int _i, PropOp _op) {   \
        if (_i == _prop->Size())                                                         \
            return true;                                                                 \
        else {                                                                           \
            Symbol sym = _prop->Sym(_i);

#define BEGIN_CUSTOM_PROPSYNC(objType)                                                   \
    bool PropSync(objType &o, DataNode &_val, DataArray *_prop, int _i, PropOp _op) {    \
        if (_i == _prop->Size())                                                         \
            return true;                                                                 \
        else {                                                                           \
            Symbol sym = _prop->Sym(_i);

#define SYNC_PROP(s, member)                                                             \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s)                                                                   \
            return PropSync(member, _val, _prop, _i + 1, _op);                           \
    }

// for propsyncs that do something extra if the prop op is specifically kPropSet
#define SYNC_PROP_SET(s, member, func)                                                   \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s) {                                                                 \
            if (_op == kPropSet) {                                                       \
                func;                                                                    \
            } else {                                                                     \
                if (_op == (PropOp)0x40)                                                 \
                    return false;                                                        \
                _val = member;                                                           \
            }                                                                            \
            return true;                                                                 \
        }                                                                                \
    }

// for propsyncs that do NOT use size or get - aka, any combo of set, insert, remove, and
// handle is used
#define SYNC_PROP_MODIFY(s, member, func)                                                \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s) {                                                                 \
            if (PropSync(member, _val, _prop, _i + 1, _op)) {                            \
                if (!(_op & (kPropSize | kPropGet))) {                                   \
                    func;                                                                \
                }                                                                        \
                return true;                                                             \
            } else {                                                                     \
                return false;                                                            \
            }                                                                            \
        }                                                                                \
    }

#ifdef HX_NATIVE
#define _SYNC_PROP_BITFIELD_CHECK_PREFIX(bitsym)                                          \
    MILO_ASSERT_FMT(                                                                      \
        strneq("BIT_", bitsym.Str(), 4), "%s does not begin with BIT_", bitsym.Str()      \
    );
#else
// Retail RB3-360 keeps a live strncmp() call here even though the MILO_ASSERT_FMT
// message-building branch is gone (lane NCCC-0731-ab7e/f133/sonnet,
// CamShot::SyncProperty's SYNC_PROP_BITFIELD(flags,...): target has
// `bl fn_8274AF68 / lis "BIT_" / bl strncmp` that our build was eliding entirely).
// Flipping MILO_ASSERT_FMT globally (Debug.h) to always-evaluate regressed -21
// exact matches / -13704 matched bytes elsewhere (21 of the other 51 call sites
// across 22 files apparently do NOT keep their cond call in retail), so this fix
// is scoped to just this macro's prefix check instead of the global definition.
#define _SYNC_PROP_BITFIELD_CHECK_PREFIX(bitsym) (void)(strneq("BIT_", bitsym.Str(), 4));
#endif

/* NB(rb3-xenon, lane NCCC-0731-ab7e/f133/opus): the index is written as the
   NON-MUTATING `_i + 1` rather than `_i++; ... Node(_i)`. Semantically identical
   (every path inside the arm returns, so the mutation never escaped), but it
   changes the expression tree MSVC 10224 sees: `8*(_i+1)` canonicalizes to
   `8*_i + 8`, so the `8*_i` computed for BEGIN_PROPSYNCS's `_prop->Sym(_i)` is a
   live common subexpression and gets parked in a callee-saved GPR. The `_i++`
   form instead creates a fresh value `t` and recomputes `8*t`. Retail does the
   former: CamShot::SyncProperty keeps `_i*8` in r23 (hence __savegprlr_23, not
   _24, and a 0xd0 frame not 0xc0) and re-derives &Node(_i+1) as
   `add r11,r23,r11 / addi r3,r11,8`. Worth +18 mismatches on CamShot
   (99.7 -> 100.0 normalized) and +0.6pp on CharEyes; whole-binary gated at
   41403 fns / 3783176 bytes, unchanged. */
#define _SYNC_PROP_BITFIELD(symbol, mask_member, line_num)                                \
    if (sym == symbol) {                                                                  \
        if (_i + 1 < _prop->Size()) {                                                      \
            DataNode &node = _prop->Node(_i + 1);                                          \
            int res = 0;                                                                  \
            switch (node.Type()) {                                                        \
            case kDataInt:                                                                \
                res = node.Int();                                                         \
                break;                                                                    \
            case kDataSymbol: {                                                           \
                Symbol bitsym = node.Sym();                                               \
                _SYNC_PROP_BITFIELD_CHECK_PREFIX(bitsym)                                  \
                bitsym = bitsym.Str() + 4;                                                \
                DataArray *macro = DataGetMacro(bitsym);                                  \
                MILO_ASSERT_FMT(                                                          \
                    macro, "PROPERTY_BITFIELD %s could not find macro %s", symbol, bitsym \
                );                                                                        \
                res = macro->Int(0);                                                      \
                break;                                                                    \
            }                                                                             \
            default:                                                                      \
                MILO_ASSERT(0, line_num);                                                  \
                break;                                                                    \
            }                                                                             \
            MILO_ASSERT(_op <= kPropInsert, line_num);                                       \
            if (_op == kPropGet) {                                                        \
                int final = mask_member & res;                                            \
                _val = (final > 0);                                                       \
            } else {                                                                      \
                if (_val.Int() != 0)                                                      \
                    mask_member |= res;                                                   \
                else                                                                      \
                    mask_member &= ~res;                                                  \
            }                                                                             \
            return true;                                                                  \
        } else                                                                            \
            return PropSync(mask_member, _val, _prop, _i + 1, _op);                       \
    }

#define SYNC_PROP_BITFIELD(symbol, mask_member, line_num)                                \
    { _NEW_STATIC_SYMBOL(symbol) _SYNC_PROP_BITFIELD(_s, mask_member, line_num) }

#define SYNC_MEMBER(s, member)                                                           \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s)                                                                   \
            return member.SyncProperty(_val, _prop, _i + 1, _op);                        \
    }

#define SYNC_SUPERCLASS(parent)                                                          \
    if (parent::SyncProperty(_val, _prop, _i, _op))                                      \
        return true;

#define SYNC_VIRTUAL_SUPERCLASS(parent)                                                  \
    if (ClassName() == StaticClassName())                                                \
    SYNC_SUPERCLASS(parent)

#define END_PROPSYNCS                                                                    \
    return false;                                                                        \
    }                                                                                    \
    }

#define END_CUSTOM_PROPSYNC                                                              \
    return false;                                                                        \
    }                                                                                    \
    }

// END SYNCPROPERTY MACROS ---------------------------------------------------------------

// BEGIN SAVE MACROS ---------------------------------------------------------------------
#define BEGIN_SAVES(objType) void objType::Save(BinStream &bs) {
#define SAVE_REVS(rev, alt) bs << packRevs(alt, rev);
#define SAVE_SUPERCLASS(parent) parent::Save(bs);

#define SAVE_VIRTUAL_SUPERCLASS(parent)                                                  \
    if (ClassName() == StaticClassName())                                                \
    SAVE_SUPERCLASS(parent)

#define END_SAVES }
// END SAVE MACRO ------------------------------------------------------------------------

// BEGIN COPY MACROS ---------------------------------------------------------------------
#define BEGIN_COPYS(objType)                                                             \
    void objType::Copy(const Hmx::Object *o, Hmx::Object::CopyType ty) {
#define COPY_SUPERCLASS(parent) parent::Copy(o, ty);

#define COPY_VIRTUAL_SUPERCLASS(parent)                                                  \
    if (ClassName() == StaticClassName())                                                \
    COPY_SUPERCLASS(Hmx::Object)

#define COPY_SUPERCLASS_FROM(parent, obj) parent::Copy(obj, ty);

#define CREATE_COPY(objType) const objType *c = dynamic_cast<const objType *>(o);

// copy macro where you specify the variable name (used in asserts in some copy methods)
#define CREATE_COPY_AS(objType, var_name)                                                \
    const objType *var_name = dynamic_cast<const objType *>(o);

#define BEGIN_COPYING_MEMBERS if (c) {
// copy macro where you specify the variable name (used in asserts in some copy methods)
#define BEGIN_COPYING_MEMBERS_FROM(copy_name) if (copy_name) {
#define COPY_MEMBER(mem) mem = c->mem;

// copy macro where you specify the variable name (used in asserts in some copy methods)
#define COPY_MEMBER_FROM(copy_name, member) member = copy_name->member;

#define END_COPYING_MEMBERS }

#define END_COPYS }
// END COPY MACROS -----------------------------------------------------------------------

// BEGIN LOAD MACROS  --------------------------------------------------------------------
#define INIT_REVS(rev, alt)                                                              \
    static const __declspec(align(4)) unsigned short gRev = rev;                         \
    static const __declspec(align(4)) unsigned short gAltRev = alt;

#define BEGIN_LOADS(objType) void objType::Load(BinStream &bs) {
#define LOAD_REVS(bs)                                                                    \
    int revs;                                                                            \
    bs >> revs;                                                                          \
    BinStreamRev d(bs, revs);

#ifdef HX_NATIVE
#define ASSERT_REVS(rev1, rev2)                                                          \
    if (d.rev > rev1 || d.altRev > rev2) {                                               \
        fprintf(                                                                         \
            stderr,                                                                      \
            "ASSERT_REVS WARNING: %s '%s' version %d > %d (or alt %d > %d)\n",           \
            ClassName(),                                                                 \
            Name(),                                                                      \
            d.rev,                                                                       \
            rev1,                                                                        \
            d.altRev,                                                                    \
            rev2                                                                         \
        );                                                                               \
    }
#else
// RETAIL-MATCH (lane CP-2, 2026-08-02): retail RB3 compiles ASSERT_REVS OUT
// entirely.  Proven from retail asm, not inferred: ?Load@RndMotionBlur@@ (target
// 148 B) goes straight from the rev halfword-stores to `bl Object::Load` with NO
// PathName/ClassName calls, while our 320 B body emitted TWO `bl ?PathName@@`
// blocks plus a `bctrl` virtual ClassName().
//
// Cause: MILO_FAIL is `((void)(__VA_ARGS__))` (os/Debug.h:188) — a COMMA
// EXPRESSION, so it does NOT compile the body out; every argument is still
// evaluated and PathName(this)/ClassName() are real calls with side effects the
// compiler must emit.  The rb3-Wii dialect (obj/ObjMacros.h) already gates this
// check behind VERSION_SZBE69_B8 (a DEV-build macro) and expands to nothing
// otherwise; only this DC3-derived copy was left ungated.  Same family as the
// MILO_DEBUG force-define documented in CLAUDE.md.
//
// The HX_NATIVE branch above is deliberately untouched, so the native port keeps
// its version-mismatch warning.
#define ASSERT_REVS(rev1, rev2)
#endif

#define LOAD_SUPERCLASS(parent) parent::Load(d.stream);

#define LOAD_VIRTUAL_SUPERCLASS(parent)                                                  \
    if (ClassName() == StaticClassName())                                                \
    LOAD_SUPERCLASS(parent)

#define LOAD_BITFIELD(type, name)                                                        \
    {                                                                                    \
        type bs_name;                                                                    \
        d >> bs_name;                                                                    \
        name = bs_name;                                                                  \
    }

#define LOAD_BITFIELD_ENUM(type, name, enum_name)                                        \
    {                                                                                    \
        type bs_name;                                                                    \
        d >> bs_name;                                                                    \
        name = (enum_name)bs_name;                                                       \
    }

#define END_LOADS }
// END LOAD MACROS
// -----------------------------------------------------------------------

#define NEW_OBJ(objType)                                                                 \
    static Hmx::Object *NewObject() { return new objType; }

#define REGISTER_OBJ_FACTORY(objType)                                                    \
    Hmx::Object::RegisterFactory(objType::StaticClassName(), objType::NewObject);

#pragma endregion
#pragma region TypeProps

// TypeProps implementation
// Retail X360: TypeProps is a 0xc-byte inline ObjRefOwner member of Hmx::Object
// (vtable@0, mMap@4, mOwner@8). No mObjects ring — object-ref tracking uses
// the simpler rb3-Wii approach (pass Hmx::Object* ref explicitly to each op).
// HX_NATIVE keeps the full dc3-style TypeProps (with mObjects) for
// correct ref-counting in the native engine.
class TypeProps : public ObjRefOwner {
    friend class Hmx::Object;
private:
    DataArray *mMap; // 0x4
    Hmx::Object *mOwner; // 0x8
#ifdef HX_NATIVE
    ObjPtrList<Hmx::Object> mObjects; // 0xc (native only: ref-counting for embedded objects)
#endif

    void ReplaceObject(DataNode &n, Hmx::Object *from, Hmx::Object *to);
    void ReleaseObjects();
    void AddRefObjects();
    void ClearAll();

public:
#ifdef HX_NATIVE
    TypeProps(Hmx::Object *o)
        : mOwner(o), mMap(nullptr), mObjects(this, kObjListOwnerControl) {}
    virtual ~TypeProps() { ClearAll(); }
#else
    // Retail X360: inline in Hmx::Object, constructed via mTypeProps(this) in Object ctor.
    TypeProps(Hmx::Object *o) : mMap(nullptr), mOwner(o) {}
    virtual ~TypeProps() { ClearAll(); }
#endif
    virtual Hmx::Object *RefOwner() const { return mOwner; }
    virtual void Replace(ObjRef *from, Hmx::Object *to);

    void SetOwner(Hmx::Object *o) { mOwner = o; }

    DataNode *KeyValue(Symbol key, bool fail = true) const;
    int Size() const;
    // RB3-era indexed key/value access over the flat (k0 v0 k1 v1 ...) map.
    // Decl-only (defined in the Object TU when ported); lets RB3 game TUs that
    // iterate mTypeProps by index (e.g. bandobj/VocalTrackDir.cpp legacy load)
    // compile against the retail RB3 TypeProps API.
    Symbol Key(int idx) const;
    DataNode &Value(int idx) const;
    void ClearKeyValue(Symbol key);
    void SetKeyValue(Symbol key, const DataNode &value, bool);
    DataArray *GetArray(Symbol prop);
    void SetArrayValue(Symbol prop, int i, const DataNode &value);
    void RemoveArrayValue(Symbol prop, int i);
    void InsertArrayValue(Symbol prop, int i, const DataNode &value);
    void Load(BinStreamRev &d);
    TypeProps &operator=(const TypeProps &);
    void Save(BinStream &d);
    DataArray *Map() const { return mMap; }
    bool HasProps() const { return mMap && mMap->Size() != 0; }

    MEM_OVERLOAD(TypeProps, 0x485);
};

typedef Hmx::Object *ObjectFunc(void);

#pragma endregion
#pragma region Hmx::Object
#include "obj/PropSync.h" /* IWYU pragma: keep */

// Hmx::Object implementation
namespace Hmx {

    /**
     * @brief: The base class from which all major Objects used in-game build upon.
     * Original _objects description:
     * "The Object class is the root of the class hierarchy. Every
     * class has Object as a superclass."
     */
    class Object : public ObjRefOwner {
#ifdef HX_NATIVE
        friend class ObjRef;
#endif
        friend void ::MergeObjectsRecurse(ObjectDir *, ObjectDir *, MergeFilter &, bool);
    private:
        /** Remove this Object from its associated ObjectDir. */
        void RemoveFromDir();

        /** Handler to execute dta for each of this Object's refs.
         * @param [in] arr The supplied DataArray.
         * Expected DataArray contents:
         *     Node 2: The variable representing the current ObjRef's owner.
         *     Node 3+: Any commands to execute.
         * Example usage: {$this iterate_refs $ref {$ref set 0}}
         */
        DataNode OnIterateRefs(const DataArray *arr);
        /** Handler to set this Object's properties.
         * @param [in] arr The supplied DataArray.
         * Expected DataArray contents:
         *     Node 2+: The property key to set. Must be either a Symbol or a DataArray.
         *     Node 3+: The corresponding property value to set.
         * Example usage: {$this set key1 val1 key2 val2 key3 val3}
         */
        DataNode OnSet(const DataArray *arr);
        DataNode OnPropertyAppend(const DataArray *);
        DataNode OnGetTypeList(const DataArray *);
        DataNode OnAddSink(DataArray *);
        DataNode OnRemoveSink(DataArray *);
        void ExportPropertyChange(DataArray *, Symbol);

        /** A collection of Object class names and their corresponding instantiators. */
        static std::map<Symbol, ObjectFunc *> sFactories;

    protected:
        // Retail X360 Object layout (0x28 bytes total, from ctor lbl_82737FE8):
        //   0x00: ObjRefOwner vtable
        //   0x04: TypeProps (ObjRefOwner-derived, 0xc bytes: vtable@4, mMap@8, mOwner@c)
        //   0x10: mTypeDef*
        //   0x14: mNote (const char*)
        //   0x18: mName (const char*)
        //   0x1c: mDir*
        //   0x20: mRefs.next (ring head, 8 bytes)
        //   0x24: mRefs.prev
        // HX_NATIVE uses a larger TypeProps (with mObjects + vtable = 0x20 bytes)
        // and replaces mNote with String, so layout diverges from retail there.
#ifdef HX_NATIVE
        /** A collection of object instances which reference this Object. */
        ObjRef mRefs; // 0x4 (native)
        /** An array of properties this Object can have. */
        TypeProps *mTypeProps; // 0xc+0x10 (native, pointer)
#else
        /** An array of properties this Object can have (inline, 0xc bytes). */
        TypeProps mTypeProps; // 0x4 (retail X360: vtable@4, mMap@8, mOwner@c)
#endif
    private: // these were marked private in RB2
        /** A collection of handler methods this Object can have.
         *  More specifically, this is an array of arrays, with each array
         *  housing a name, followed by a handler script.
         *  Formatted in the style of:
         *  ( (name1 {handler1}) (name2 {handler2}) (name3 {handler3}) )
         */
        DataArray *mTypeDef; // 0x14 (native) / 0x10 (X360)
        /** A note about this Object, useful for debugging. */
        /** "Just a note describing the object, stripped out of shipping assets,
            so don't make code rely on this" */
#ifdef HX_NATIVE
        String mNote; // 0x18 (native, String 0xc bytes)
#else
        const char *mNote; // 0x14 (retail X360, const char*)
#endif
        /** This Object's name. */
        /** "name of the object" */
        const char *mName; // 0x20 (native) / 0x18 (X360)
        /** The ObjectDir in which this Object resides. */
        ObjectDir *mDir; // 0x24 (native) / 0x1c (X360)
        /** "Sinks for messages sent to me" */
        // Retail X360: mSinks does not exist in the Object dtor (fn_82738050).
        // mSinks is absent from the retail binary layout; development/tool feature only.
#ifdef HX_NATIVE
        MsgSinks *mSinks; // native only
#endif
    protected:
#ifndef HX_NATIVE
        /** Retail X360: ring head at 0x20 (after mDir), non-polymorphic (8 bytes). */
        ObjRef mRefs; // 0x20 (X360)
#endif
        /** An Object in the process of being deleted. */
        static Object *sDeleting;
    public:
#ifdef HX_NATIVE
        /** True after FlushDeferredFrees — rings may have dead entries. */
        static bool sRingsDirty;
#endif
    protected:

        MsgSinks *GetOrAddSinks();
        /** Handler to get the value of a given Object property.
         * @param [in] arr The supplied DataArray.
         * @returns The property value.
         * Expected DataArray contents:
         *     Node 2: The property to search for, either as a Symbol or DataArray.
         *     Node 3: The fallback value if no property is found.
         * Example usage: {$this get some_value 69}
         */
        DataNode OnGet(const DataArray *arr);
        void BroadcastPropertyChange(DataArray *);
        void BroadcastPropertyChange(Symbol);

    public:
        enum CopyType {
            kCopyDeep = 0,
            kCopyShallow = 1,
            kCopyFromMax = 2
        };

        enum SinkMode {
            /** "does a Handle to the sink, this gets all c handlers, type handling,
             * and exporting." */
            kHandle = 0,
            /** "just Exports to the sink, so no c or type handling" */
            kExport = 1,
            /** "just calls HandleType, good if know that particular thing is only
             * ever type handled." */
            kType = 2,
            /** "do type handling and exporting using Export, no C handling" */
            kExportType = 3,
        };

        Object();
        Object(const Object &);
        virtual ~Object();
#ifdef HX_NATIVE
        /** True when inside ~Object() -> ReplaceRefs(NULL) chain. */
        static bool IsDeleting() { return sDeleting != nullptr; }
        static Object *GetDeleting() { return sDeleting; }
#endif
        virtual Object *RefOwner() const { return const_cast<Object *>(this); }
        virtual void Replace(ObjRef *from, Hmx::Object *to);
        /** This Object's class name. */
        OBJ_CLASSNAME(Object);
        /** Set this Object's mTypeDef array based this Object's types entry in
         * SystemConfig. */
        OBJ_SET_TYPE(Object);
        /** Returns whether this ref is an ObjDirPtr (vtable slot 3 in retail;
         * declared by ObjRefOwner, so it precedes ClassName/SetType. Retail
         * slot 5 is SetType -- the old "slot 5" note here was wrong). */
        virtual bool IsDirPtr() { return false; }
        /** Executes a message/command on the Object.
         * @param [in] _msg The received message.
         * @param [in] _warn If true, and the message goes unhandled, print to console.
         * @returns The return value of whatever code was executed.
         */
        virtual DataNode Handle(DataArray *_msg, bool _warn = true);
        /** Reads or modifies the object property at the given path.
         * @param [out] _val A DataNode to either place the property val into, or set the
         * property val with.
         * @param [in] _prop The DataArray containing the symbol representing the property
         * to sync.
         * @param [in] _i The index in _prop containing the symbol of the property to
         * sync.
         * @param [in] _op The operation to be performed with the property.
         * @returns Returns true if a property was synced, or if the desired index == the
         * prop array size.
         */
        virtual bool SyncProperty(DataNode &_val, DataArray *_prop, int _i, PropOp _op);
        void InitObject();
        /** Serializes the Object's state
         * @param [in] bs The BinStream to save into.
         */
        virtual void Save(BinStream &);
        /** Copies the state of the given Object into this one.
         * @param [in] o The other Object to copy from.
         * @param [in] ty The copy type.
         */
        virtual void Copy(const Hmx::Object *o, Hmx::Object::CopyType ty);
        /** Deserializes the Object's state.
         * @param [in] bs The BinStream to load from.
         */
        virtual void Load(BinStream &);
        /** Any routines to write relevant data to a BinStream before the main Save method
         * executes. */
        virtual void PreSave(BinStream &) {}
        /** Any routines to write relevant data to a BinStream after the main Save method
         * executes. */
        virtual void PostSave(BinStream &) {}
        /** Prints relevant info about this Object to the debug console. */
        virtual void Print() {}
        virtual void Export(DataArray *msg, bool);
        /** Set this Object's mTypeDef array.
         * @param [in] data The array to set.
         */
        virtual void SetTypeDef(DataArray *data);
        DataArray *ObjectDef(Symbol);
        /** Sets this Object's name and updates the ObjectDir this Object resides in.
         * @param [in] name The name to give this Object.
         * @param [in] dir The ObjectDir to place this Object in. If name is null, this
         * Object won't have a set ObjectDir.
         */
        virtual void SetName(const char *name, ObjectDir *dir);
        virtual ObjectDir *DataDir();
        /** Any routines to read relevant data from a BinStream before the main Load
         * method executes. */
        virtual void PreLoad(BinStream &bs) { Load(bs); }
        /** Any routines to read relevant data from a BinStream after the main Load method
         * executes. */
        virtual void PostLoad(BinStream &) {}
        /** Get this Object's path name. */
        virtual const char *FindPathName();

        /** "script type of the object" */
        Symbol Type() const {
            if (mTypeDef)
                return mTypeDef->Sym(0);
            else
                return Symbol();
        }
        const ObjRef &Refs() const { return mRefs; }
        void SetNote(const char *note);
        DataArray *TypeDef() const { return mTypeDef; }
        ObjectDir *Dir() const { return mDir; }
        const char *Name() const { return mName; }
#ifdef HX_NATIVE
        const String &Note() const { return mNote; }
#else
        const char *Note() const { return mNote; }
#endif
        const char *AllocHeapName() { return MemHeapName(MemFindAddrHeap(this)); }
#ifdef HX_NATIVE
        void AddRef(ObjRef *ref) {
            // During cascade, ~ObjRefConcrete skips Release, leaving dead
            // entries in the ring. If the last entry is dead (freed), the
            // ring is corrupted — reset to self-loop before insertion.
            // Only check after a cascade has completed (flag avoids
            // no_sanitize cache-miss reads during normal operation).
            if (sRingsDirty && mRefs.prev != &mRefs && !IsRingPrevAlive())
                mRefs.Clear();
            ref->AddRef(&mRefs);
        }
        __attribute__((no_sanitize("address")))
        bool IsRingPrevAlive() const {
            return mRefs.prev->mAliveSentinel == ObjRef::kAliveSentinel;
        }
        /** Check if this object's mRefs sentinel is still alive.
         *  False after ~Object (mRefs' ~ObjRef clears sentinel). */
        bool IsRefAlive() const { return mRefs.IsAlive(); }
        void Release(ObjRef *ref) {
            if (!ref) return;
            ref->Release(nullptr);
        }
#else
        // Retail X360: the ring holds separate pool nodes {next,prev,refPtr}.
        // AddRef allocates a node tracking `ref` and splices it into mRefs (if
        // ref->RefOwner() != this); Release finds and frees the node. Both are
        // out-of-line (fn_82737168 / fn_827367D8).
        void AddRef(ObjRefOwner *ref);
        void Release(ObjRefOwner *ref);
#endif
#ifdef HX_NATIVE
        MsgSinks *Sinks() const { return mSinks; }
#else
        MsgSinks *Sinks() const { return nullptr; }
#endif

        void ReplaceRefs(Hmx::Object *);
        void ReplaceRefsFrom(Hmx::Object *from, Hmx::Object *);
#ifdef HX_NATIVE
        /** Nullify all refs in this object's ring via NullifyObj.
         *  No Replace callbacks fire — avoids delete-this in
         *  MessageTask/ScriptTask/PropertyTask/DirLoader. */
        void NullifyAllRefs();
        /** Detach from parent dir without modifying the dir's hash table.
         *  Used during ~ObjectDir cascade to prevent surviving objects from
         *  holding a stale mDir pointer to the dying dir. Unlike
         *  SetName(nullptr, nullptr), this does NOT call RemovingObject
         *  or touch the hash table — the dir is about to be freed anyway. */
        void DetachFromDir() { mDir = nullptr; mName = gNullStr; }
#endif
        /** How many other objects reference this Object? */
        int RefCount() const;

        void RemovePropertySink(Hmx::Object *, DataArray *);
        bool HasPropertySink(Hmx::Object *, DataArray *);
        void RemoveSink(Hmx::Object *, Symbol = Symbol());

        void SaveType(BinStream &);
        void SaveRest(BinStream &);
        void ClearAllTypeProps();
        bool HasTypeProps() const;
        // RB3-retail signature: NO trailing `bool chain` param (DC3 added it
        // later). Proven by X360 call-site codegen (e.g. BandUI::Init AddSink
        // calls stop at r7=SinkMode; DC3's `= true` default would materialize
        // an extra `li r8, 0x1` at every call site). rb3-Wii Msg.h agrees.
        void AddSink(
            Hmx::Object *,
            Symbol = Symbol(),
            Symbol = Symbol(),
            SinkMode = kHandle
        );
        void AddPropertySink(Hmx::Object *, DataArray *, Symbol);
        void MergeSinks(Hmx::Object *);
        DataNode PropertyArray(Symbol);
        int PropertySize(DataArray *);
        void InsertProperty(DataArray *, const DataNode &);
        void RemoveProperty(DataArray *);
        void PropertyClear(DataArray *);

        /** Search for a key in this Object's properties, and return the corresponding
         * value.
         * @param [in] prop: The property to search for, in DataArray form. The first node
         * must be a Symbol.
         * @param [in] fail: If true, print a message to the console if no property value
         * was found.
         * @returns The corresponding property's value as a DataNode pointer.
         */
        const DataNode *Property(DataArray *prop, bool fail = true) const;

        /** Search for a key in this Object's properties, and return the corresponding
         * value.
         * @param [in] prop: The property to search for, in Symbol form.
         * @param [in] fail: If true, print a message to the console if no property value
         * was found.
         * @returns The corresponding property's value as a DataNode pointer.
         */
        const DataNode *Property(Symbol prop, bool fail = true) const;

        DataNode HandleProperty(DataArray *, DataArray *, bool);

        /** Execute script in this Object's TypeDef,
         * based on the contents of a received message.
         * @param [in] _msg The received message.
         * @returns The return value of whatever script was executed.
         */
        DataNode HandleType(DataArray *msg);

        void ChainSource(Hmx::Object *, Hmx::Object *);
        void LoadType(BinStream &);
        void LoadRest(BinStream &);

        /** Either adds or updates the key/value pair in the properties.
         * @param [in] prop The key to either add or update
         * @param [in] val The corresponding value associated with the key.
         */
        void SetProperty(Symbol prop, const DataNode &val);

        /** Either adds or updates the key/value pair in the properties.
         * @param [in] prop The key to either add or update. The first node must be a
         * Symbol.
         * @param [in] val The corresponding value associated with the key.
         */
        void SetProperty(DataArray *prop, const DataNode &val);

        NEW_OBJ(Hmx::Object);
        /** Given an Object derivative's class name, construct a new instance of the
         * Object. */
        static Object *NewObject(Symbol name);
        /** Given an Object derivative's class name, check if its ctor is in our factory
         * list. */
        static bool RegisteredFactory(Symbol name);
        /** Add a new Object derivative to the factory list via class name and factory
         * func. */
        static void RegisterFactory(Symbol name, ObjectFunc *func);

        /** Create a new Object derivative based on its entry in the factory list. */
        template <class T>
        static T *New() {
            T *obj = dynamic_cast<T *>(Hmx::Object::NewObject(T::StaticClassName()));
            if (!obj)
                MILO_FAIL("Couldn't instantiate class %s", T::StaticClassName());
            return obj;
        }
    };

}

extern bool gLoadingProxyFromDisk;
extern bool gMiloTool;

inline TextStream &operator<<(TextStream &ts, const Hmx::Object *obj) {
    if (obj)
        ts << obj->Name();
    else
        ts << "<null>";
    return ts;
}

struct ObjMatchPr {
    ObjMatchPr(Hmx::Object *o) : obj(o) {}
    bool operator()(const Hmx::Object *value) const { return obj == value; }
    Hmx::Object *obj;
};

struct ObjNameSort {
    bool operator()(Hmx::Object *c1, Hmx::Object *c2) const {
        return strcmp(c1->Name(), c2->Name()) < 0;
    }
};

#pragma endregion
#pragma region ObjVector

// ObjVector
template <class T>
class ObjVector : public std::vector<T> {
private:
    typedef typename std::vector<T> Base;
    Hmx::Object *mOwner;

public:
    ObjVector(Hmx::Object *o) : mOwner(o) {}
    Hmx::Object *Owner() { return mOwner; }

    // `this->` on the std::vector base members is REQUIRED, not style: `size`
    // and `back` are also names of `extern Symbol` globals declared at
    // namespace scope by utl/Symbols*.h. Unqualified, they are non-dependent
    // and bind to whichever is visible -- so in any TU that pulled Symbols*.h
    // ahead of this header, `size()` resolves to `Symbol size` and clang
    // reports "type 'Symbol' does not provide a call operator". Qualifying with
    // `this->` makes them dependent and forces base-class lookup. Purely a
    // name-binding fix: same members, same codegen, X360 unaffected.
    void push_back() { resize(this->size() + 1); }

    void push_back(const T &t) {
        push_back();
        this->back() = t;
    }

    void resize(unsigned int size) { Base::resize(size, T(mOwner)); }

    void operator=(const ObjVector &vec) {
        if (this != &vec) {
            resize(vec.size());
            Base::operator=((Base &)vec);
        }
    }
};

// there are symbols for both BinStreamRev >> ObjVector
// and BinStream >> ObjVector
// hmx why??? pick one and stick with it pls

template <class T>
BinStream &operator>>(BinStreamRev &bs, ObjVector<T> &vec) {
    unsigned int length;
    bs >> length;
#ifdef HX_NATIVE
    if (length > 0x10000) {
        fprintf(stderr, "ObjVector: CORRUPT length=%u (0x%x) at stream pos=%d\n", length, length, bs.stream.Tell());
        abort();
    }
#endif
    vec.resize(length);

    for (ObjVector<T>::iterator it = vec.begin(); it != vec.end(); it++) {
        bs >> *it;
    }
    // TU5 returns the rev wrapper itself (BinStreamRev base@0 upcast → `mr r3,&bs`),
    // not the inner stream member (`lwz r3,0x14`). Faithful + safe (base@0 identity).
    return bs;
}

template <class T>
BinStream &operator>>(BinStream &bs, ObjVector<T> &vec) {
    unsigned int length;
    bs >> length;
    vec.resize(length);

    for (ObjVector<T>::iterator it = vec.begin(); it != vec.end(); it++) {
        bs >> *it;
    }
    return bs;
}

#pragma endregion
#pragma region ObjList

// ObjList
template <class T>
class ObjList : public std::list<T> {
private:
    typedef typename std::list<T> Base;
    Hmx::Object *mOwner;

public:
    ObjList(Hmx::Object *o) : mOwner(o) {}
    Hmx::Object *Owner() { return mOwner; }

    void resize(unsigned int ul) { Base::resize(ul, T(mOwner)); }

    // `this->` for the same reason as ObjVector above -- `size`, `back`,
    // `front` and `begin` are all ALSO namespace-scope `extern Symbol` globals
    // from utl/Symbols*.h. Name binding only; no codegen change.
    void push_front() { this->insert(this->begin(), T(mOwner)); }

    void push_front(const T &t) {
        push_front();
        this->front() = t;
    }

    void push_back() { resize(this->size() + 1); }

    void push_back(const T &t) {
        push_back();
        this->back() = t;
    }

    void operator=(const ObjList &oList) {
        if (this != &oList) {
            resize(oList.size());
            Base::operator=((Base &)oList);
        }
    }
};

template <class T>
BinStream &operator>>(BinStreamRev &bs, ObjList<T> &oList) {
    unsigned int length;
    bs >> length;
    oList.resize(length);

    for (std::list<T>::iterator it = oList.begin(); it != oList.end(); ++it) {
        bs >> *it;
    }
    // TU5 returns the rev wrapper itself (BinStreamRev base@0 upcast → `mr r3,&bs`),
    // not the inner stream member (`lwz r3,0x14`). Faithful + safe (base@0 identity).
    return bs;
}

template <class T>
BinStream &operator>>(BinStream &bs, ObjList<T> &oList) {
    unsigned int length;
    bs >> length;
    oList.resize(length);

    for (std::list<T>::iterator it = oList.begin(); it != oList.end(); ++it) {
        bs >> *it;
    }
    return bs;
}

#pragma endregion
#pragma region ObjectStage

// ObjectStage
class ObjectStage : public ObjPtr<Hmx::Object> {
public:
    ObjectStage() : ObjPtr<Hmx::Object>(sOwner) {}
    ObjectStage(Hmx::Object *o) : ObjPtr<Hmx::Object>(sOwner, o) {}
    virtual ~ObjectStage() {}

    static Hmx::Object *sOwner;
};

inline void
Interp(const ObjectStage &stage1, const ObjectStage &stage2, float f, Hmx::Object *&obj) {
    const ObjectStage &out = f < 1 ? stage1 : stage2;
    obj = out.Ptr();
}

BinStream &operator<<(BinStream &, const ObjectStage &);
BinStreamRev &operator>>(BinStreamRev &, ObjectStage &);

#pragma endregion
#pragma region ObjVersion

// ObjVersion
struct ObjVersion {
    ObjVersion(int i, Hmx::Object *o) : revs(i), obj(nullptr, o) {}
    ~ObjVersion() {}

    ObjPtr<Hmx::Object> obj;
    int revs;
};

#pragma endregion

#include "ObjPtr_p.h"
