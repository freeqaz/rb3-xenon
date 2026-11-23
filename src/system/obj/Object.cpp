#include "obj/Object.h"
#include "Dir.h"
#include "Msg.h"
#include "Object.h"
#include "obj/Data.h"
#ifdef HX_NATIVE
#include "obj/DirLoader.h"
#endif
#include "obj/DataFunc.h"
#include "obj/Utl.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/OSFuncs.h"
#include "os/Platform.h"
#include "os/System.h"
#include "utl/BinStream.h"
#include "utl/MemMgr.h"
#include "utl/Symbol.h"

#ifdef HX_NATIVE
#include <vector>
Hmx::Object *Hmx::Object::sDeleting;
bool Hmx::Object::sRingsDirty = false;
bool gInReplaceList = false;

// Check if an ObjRef's alive sentinel is still set. Reads potentially freed
// memory during cascading destruction — suppress ASAN for this specific check.
// Under glibc, freed memory is typically zeroed → sentinel reads as 0 → dead.
void ObjRef::ReplaceList(Hmx::Object *obj) {
    // Suppress ObjPtrVec::erase and Transitions::RemoveNodes during ring walk.
    bool wasInReplace = gInReplaceList;
    gInReplaceList = true;

    while (next != this) {
        ObjRef *cur = next;
        cur->Replace(obj);
        if (cur == next) {
            // Replace didn't advance — force-unlink to prevent infinite loop.
            cur->prev->next = cur->next;
            cur->next->prev = cur->prev;
            cur->prev = cur;
            cur->next = cur;
        }
    }

    gInReplaceList = wasInReplace;
}
#endif

bool gLoadingProxyFromDisk = false;
bool gMiloTool = false;

// The object-version stack PushRev/PopRev (obj/Object.h) work on. Retail's
// `~vector<ObjVersion>` (0x8275CDC8) and its atexit thunk come from this TU.
std::vector<ObjVersion> sRevStack;
std::map<Symbol, ObjectFunc *> Hmx::Object::sFactories;
#ifdef HX_NATIVE
// The 8-slot recursion-safe property-path pool is a DC3-ERA ADDITION that RB3
// retail does NOT have (dc3-decomp/src/system/obj/Object.cpp has it; RB3 has
// no such symbol, and retail's SetProperty(Symbol,...) body contains no pool scan
// -- see the comment on SetProperty(Symbol, const DataNode &) below).  Retail
// shares ONE static scratch array, which is a latent re-entrancy bug DC3 later
// fixed.  We reproduce retail's bug in the match build and keep the safe pool
// natively, where this code actually executes.
DataArrayPtr gPropPaths[8] = {
    DataArrayPtr(new DataArray(1)), DataArrayPtr(new DataArray(1)),
    DataArrayPtr(new DataArray(1)), DataArrayPtr(new DataArray(1)),
    DataArrayPtr(new DataArray(1)), DataArrayPtr(new DataArray(1)),
    DataArrayPtr(new DataArray(1)), DataArrayPtr(new DataArray(1))
};
#endif
MsgSinks gSinks(nullptr);

#ifndef HX_NATIVE
// RB3 retail X360: Hmx::Object is 0x28 bytes (verified from ctor lbl_82737FE8 /
// dtor fn_82738050): vtable@0, TypeProps(inline,0xc)@4, mTypeDef@10,
// mNote(const char*)@14, mName@18, mDir@1c, mRefs(std::list)@20.
// dc3-decomp uses a larger layout (0x2c, pointer TypeProps, String mNote).
static_assert(sizeof(Hmx::Object) == 0x28, "Hmx::Object must be 0x28 (RB3 retail)");
static_assert(sizeof(ObjRefList) == 8, "Hmx::Object::mRefs must be 8 bytes (RB3 retail)");

void Hmx::Object::AddRef(ObjRefOwner *ref) {
    if (ref->RefOwner() != this)
        mRefs.push_front(ref);
}

void Hmx::Object::Release(ObjRefOwner *ref) {
    if (sDeleting != this && ref->RefOwner() != this) {
        for (ObjRefList::iterator it = mRefs.begin(); it != mRefs.end(); ++it) {
            if (*it == ref) {
                mRefs.erase(it);
                return;
            }
        }
    }
}
#endif

#pragma region Virtual Methods

Hmx::Object::Object()
#ifdef HX_NATIVE
    : mTypeProps(nullptr), mTypeDef(nullptr), mName(gNullStr), mDir(nullptr), mSinks(nullptr)
#else
    // Retail X360: TypeProps is inline; mTypeProps(this) stores this as mOwner immediately.
    : mTypeProps(this), mTypeDef(nullptr), mNote(gNullStr), mName(gNullStr), mDir(nullptr)
#endif
{
#ifdef HX_NATIVE
    mRefs.DetachSelf();
#endif
}

Hmx::Object::~Object() {
    // Retail X360 (0x8275CBF0) EVALUATES this assert's condition: `bl MainThread`
    // survives with its result unused. MILO_ASSERT_FMT's sizeof() form never ran it.
    MILO_ASSERT(MainThread(), 0xA7);
    if (mTypeDef) {
        mTypeDef->Release();
        mTypeDef = nullptr;
    }
#ifdef HX_NATIVE
    ClearAllTypeProps();
#endif
    RemoveFromDir();
#ifdef HX_NATIVE
    RELEASE(mSinks);
#endif
    Hmx::Object *old = sDeleting;
    sDeleting = this;
#ifdef HX_NATIVE
    // X360 (0x8275CBF0) always walks mRefs here. Natively, inside a cascade
    // (ObjectDir::DeleteObjects), the walk is NullifyAllRefs instead of
    // ReplaceRefs: it runs every owner-control holder's Replace(nullptr) from a
    // drained local ring, nulls every other ref, and skips nodes whose
    // mAliveSentinel ~ObjRef has cleared (the freed-ObjPtrVec-buffer hazard).
    // For an object the dir names this walks an empty ring (Phase 0 already
    // emptied it). It used to be skipped outright (`if (!InDeleteObjects())
    // ReplaceRefs(nullptr);`), which left every holder of an object destroyed
    // AS COLLATERAL of a cascade -- a Task an owner step deletes, an object a
    // Phase-1 destructor deletes -- aimed at a block about to be freed, its
    // owner never told. dc3-decomp made the same change (9d3fc20f8).
    if (ObjectDir::InDeleteObjects())
        NullifyAllRefs();
    else
        ReplaceRefs(nullptr);
    sDeleting = old;
#else
    // Retail X360 (0x8275CBF0, lane W5-A): no explicit ClearAll -- ~TypeProps
    // (fn_82274048 == { ClearAll(); }) runs as the implicit member destructor
    // AFTER this body. The ref walk advances BEFORE dispatching Replace,
    // since Replace(this, 0) may erase the entry it is called on. The note is
    // an owned pool string (see SetNote) and is freed here; mRefs' own nodes
    // are freed by its implicit member dtor (`_List_base::clear`, folded onto
    // a SynthPollable* instantiation).
    for (ObjRefList::iterator it = mRefs.begin(); it != mRefs.end();) {
        ObjRefList::iterator cur = it;
        ++it;
        (*cur)->Replace(reinterpret_cast<ObjRef *>(this), nullptr);
    }
    sDeleting = old;
    if (mNote != gNullStr) {
        MemOrPoolFreeSTL(strlen(mNote) + 1, (void *)mNote);
    }
#endif
    if (gDataThis == this) {
        gDataThis = nullptr;
    }
}

void Hmx::Object::Replace(ObjRef *from, Hmx::Object *to) {
#ifdef HX_NATIVE
    if (mSinks)
        mSinks->Replace(from, to);
#endif
}

BEGIN_HANDLERS(Hmx::Object)
    HANDLE(get, OnGet)
    HANDLE_EXPR(get_array, PropertyArray(_msg->Sym(2)))
    HANDLE_EXPR(size, PropertySize(_msg->Array(2)))
    HANDLE(set, OnSet)
    HANDLE_ACTION(insert, InsertProperty(_msg->Array(2), _msg->Evaluate(3)))
    HANDLE_ACTION(remove, RemoveProperty(_msg->Array(2)))
    HANDLE_ACTION(clear, PropertyClear(_msg->Array(2)))
    HANDLE(append, OnPropertyAppend)
    HANDLE_EXPR(has, Property(_msg->Array(2), false) != nullptr)
    HANDLE_EXPR(prop_handle, HandleProperty(_msg->Array(2), _msg, true))
    HANDLE_ACTION(copy, Copy(_msg->Obj<Hmx::Object>(2), (CopyType)_msg->Int(3)))
    // Retail (fn_8275BD78) dispatches "replace" through the ObjRefOwner vtable
    // slot @+8 (Object.h's verified vtable-layout comment), which is the
    // existing virtual Replace(ObjRef*, Hmx::Object*) override -- NOT a new
    // 2-Object-arg overload (a differently-shaped
    // Replace(Hmx::Object*, Hmx::Object*) is wrong here: adding that signature to
    // Hmx::Object would introduce a new vtable slot and shift every derived
    // class's layout). Both message args are fetched via the same
    // Obj<Hmx::Object> accessor retail uses elsewhere in this chain; only the
    // first is cast to fit the existing virtual's declared parameter type.
    HANDLE_ACTION(
        replace, Replace((ObjRef *)_msg->Obj<Hmx::Object>(2), _msg->Obj<Hmx::Object>(3))
    )
    HANDLE_EXPR(class_name, ClassName())
    HANDLE_EXPR(name, mName)
    HANDLE_EXPR(note, mNote)
    HANDLE_ACTION(set_note, SetNote(_msg->Str(2)))
    HANDLE(iterate_refs, OnIterateRefs)
    HANDLE_EXPR(dir, mDir)
    HANDLE_ACTION(
        set_name,
        SetName(_msg->Str(2), _msg->Size() > 3 ? _msg->Obj<ObjectDir>(3) : Dir())
    )
    HANDLE_ACTION(set_type, SetType(_msg->Sym(2)))
    HANDLE_EXPR(is_a, IsASubclass(ClassName(), _msg->Sym(2)))
    HANDLE_EXPR(get_type, Type())
    HANDLE_EXPR(get_heap, AllocHeapName())
#ifdef RB3_KEEP_DC3_ONLY_HANDLERS
    // Retail fn_8275BD78 builds exactly 23 Symbols; get_types_list is not one
    // of them, and the literal is absent from every non-executable section of
    // band.exe. DC3-only.
    HANDLE(get_types_list, OnGetTypeList)
#endif
    HANDLE_ARRAY(mTypeDef)
#ifdef RB3_KEEP_DC3_ONLY_HANDLERS
    // Retail's 23-Symbol chain has neither; the literals DO exist elsewhere in
    // the image (MsgSource), so this is structural evidence only -- but
    // they are absent from THIS body.
    HANDLE(add_sink, OnAddSink)
    HANDLE(remove_sink, OnRemoveSink)
    // Third member of the same DC3-only trio. Retail fn_8275BD78 contains only
    // FOUR indirect calls (vtable slots 0x10 ClassName x2, 0x14 SetType, and the
    // shared $LN303 join) -- slot 0x38 (Export) appears NOWHERE in the body, and
    // all three HANDLE_ARRAY(mTypeDef) miss paths branch straight to the
    // warn/return block at .L_8275C7B4. Ghidra's decomp agrees.
    // The chain ends at HANDLE_ARRAY(mTypeDef)+HANDLE_CHECK with no Export.
    Export(_msg, false);
#endif
END_HANDLERS

// W16-BS 2026-09-15: RB3 retail's Hmx::Object::SyncProperty is the EMPTY
// terminal -- 72 B at 0x8235c2e0, i.e. exactly what BEGIN/END_PROPSYNCS emit
// with no SYNC_PROP entries. Our 720-byte body carried three DC3-era entries
// RB3 does not have; retail's BEGIN_PROPSYNCS(Hmx::Object) is empty,
// and the "sinks" string has exactly 2 retail xrefs, both in MsgSource.
// Kept for the native build, which relies on name/type/sinks propsync.
BEGIN_PROPSYNCS(Hmx::Object)
#ifdef HX_NATIVE
    SYNC_PROP_SET(name, mName, SetName(_val.Str(), mDir))
    SYNC_PROP_SET(type, Type(), SetType(_val.Sym()))
    SYNC_PROP(sinks, mSinks ? *mSinks : gSinks)
#endif
END_PROPSYNCS

void Hmx::Object::InitObject() {
    static DataArray *objects = SystemConfig("objects");
    static Symbol init = "init";
    DataArray *def = ObjectDef(gNullStr)->FindArray(init, false);
    if (def) {
        def->ExecuteScript(1, this, nullptr, 1);
    }
}

void Hmx::Object::Save(BinStream &bs) {
    SaveType(bs);
    SaveRest(bs);
}

void Hmx::Object::SaveType(BinStream &bs) {
    bs << 2;
    bs << Type();
}

void Hmx::Object::SaveRest(BinStream &bs) {
#ifdef HX_NATIVE
    if (!mTypeProps)
        bs << (DataArray *)nullptr;
    else
        mTypeProps->Save(bs);
    if (mNote.empty() || bs.Cached())
        bs << 0;
    else
        bs << mNote;
#else
    // Retail X360: TypeProps is inline, mNote is const char*. Retail calls
    // TypeProps::Save unconditionally and tests only mNote's pointer.
    mTypeProps.Save(bs);
    if (mNote && !bs.Cached()) {
        // retail inlines the string write: strlen, the int, then the bytes
        int len = strlen(mNote);
        bs << len;
        bs.Write(mNote, len);
    } else
        bs << 0;
#endif
}

#ifndef HX_NATIVE
// Retail X360 (0x8275A898, lane W5-A): kCopyFromMax returns; SetNote COPIES the
// note (fn_8275A500 -- Object owns its note string); same-class => SetTypeDef +
// an UNCONDITIONAL TypeProps assignment (DC3's "either side has props" guard is
// absent); else the warn's arguments are evaluated in hoisted order
// (o->ClassName(), ClassName(), o->Type()) with the emission stripped.
void Hmx::Object::Copy(const Hmx::Object *o, CopyType ty) {
    if (ty == kCopyFromMax)
        return;
    SetNote(o->Note());
    if (ClassName() == o->ClassName()) {
        SetTypeDef(o->TypeDef());
        mTypeProps = o->mTypeProps;
    } else if (o->TypeDef() || TypeDef()) {
        Symbol className = o->ClassName();
        const char *selfname = Name();
        Symbol selfclass = ClassName();
        const char *objname = o->Name();
        Symbol typ = o->Type();
        MILO_NOTIFY(
            "Can't copy type \"%s\" or type props of %s to %s, different classes %s and %s",
            typ,
            selfname,
            objname,
            selfclass,
            className
        );
    }
}
#else
void Hmx::Object::Copy(const Hmx::Object *o, CopyType ty) {
    if (ty != kCopyFromMax) {
        mNote = o->Note();
        if (ClassName() == o->ClassName()) {
            SetTypeDef(o->TypeDef());
            if (o->HasTypeProps() && !mTypeProps) {
                mTypeProps = new TypeProps(this);
            } else if (!o->HasTypeProps()) {
                if (mTypeProps) {
                    RELEASE(mTypeProps);
                }
            }
            if (mTypeProps) {
                *mTypeProps = *o->mTypeProps;
            }
        } else if (o->TypeDef() || TypeDef()) {
            MILO_NOTIFY(
                "Can't copy type \"%s\" or type props of %s to %s, different classes %s and %s",
                o->Type(),
                Name(),
                o->Name(),
                ClassName(),
                o->ClassName()
            );
        }
    }
}
#endif

void Hmx::Object::Load(BinStream &bs) {
    LoadType(bs);
    LoadRest(bs);
}

INIT_REVS(2, 0)

#ifdef HX_NATIVE
void Hmx::Object::LoadType(BinStream &bs) {
    LOAD_REVS(bs)
    ASSERT_REVS(2, 0)
    Symbol s;
    bs >> s;
    SetType(s);
    PushRev(packRevs(d.altRev, d.rev), this);
}
#else
// Retail keeps Object's load revisions in a file-static pair (alt at +0, rev at
// +4, one halfword each) rather than a BinStreamRev local: LoadType stores both
// halves of the packed word there and pushes them back for LoadRest.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gObjectRevs;

void Hmx::Object::LoadType(BinStream &bs) {
    int revs;
    bs >> revs;
    gObjectRevs.rev = getHmxRev(revs);
    gObjectRevs.altRev = getAltRev(revs);
    Symbol s;
    bs >> s;
    SetType(s);
    PushRev(packRevs(gObjectRevs.altRev, gObjectRevs.rev), this);
}
#endif

void Hmx::Object::LoadRest(BinStream &bs) {
#ifdef HX_NATIVE
    BinStreamRev d(bs, PopRev(this));
    if (!mTypeProps) {
        mTypeProps = new TypeProps(this);
    }
    mTypeProps->Load(d.stream, d.rev < 2);
    if (!mTypeProps->HasProps()) {
        RELEASE(mTypeProps);
    }
    if (d.rev > 0) {
        d >> mNote;
    }
#else
    // Retail 0x8275CE78: the popped rev goes back into LoadType's file static,
    // TypeProps gets the raw stream, and the note is read in place -- the old
    // pool copy is freed, then a length-prefixed string is pool-allocated and
    // read straight into it (an empty note stays gNullStr).
    int revs = PopRev(this);
    gObjectRevs.rev = getHmxRev(revs);
    gObjectRevs.altRev = getAltRev(revs);
    mTypeProps.Load(bs, gObjectRevs.rev < 2);
    if (gObjectRevs.rev != 0) {
        if (mNote != gNullStr) {
            MemOrPoolFreeSTL(strlen(mNote) + 1, (void *)mNote);
        }
        mNote = gNullStr;
        int len;
        bs >> len;
        if (len != 0) {
            mNote = (const char *)MemOrPoolAlloc(len + 1);
            bs.Read((void *)mNote, len);
            ((char *)mNote)[len] = '\0';
        }
    }
#endif
}

#ifdef HX_NATIVE
// DC3's body. RB3 retail's Export is empty (see Object.h).
void Hmx::Object::Export(DataArray *a, bool b) {
    if (b)
        HandleType(a);
    if (mSinks)
        mSinks->Export(a);
}
#endif

void Hmx::Object::SetTypeDef(DataArray *def) {
    if (mTypeDef != def) {
        if (mTypeDef) {
            mTypeDef->Release();
            mTypeDef = nullptr;
        }
        ClearAllTypeProps();
        mTypeDef = def;
        if (mTypeDef) {
            mTypeDef->AddRef();
        }
    }
}

DataArray *Hmx::Object::ObjectDef(Symbol s) {
    if (s == gNullStr) {
        return SystemConfig("objects", ClassName());
    } else {
        return SystemConfig("objects", s);
    }
}

void Hmx::Object::SetName(const char *name, ObjectDir *dir) {
    RemoveFromDir();
    if (!name || *name == '\0') {
        mName = gNullStr;
        mDir = nullptr;
    } else {
        MILO_ASSERT(dir, 0xE7);
        mDir = dir;
        ObjectDir::Entry *entry = dir->FindEntry(name, true);
        if (entry->obj) {
            MILO_FAIL("%s already exists", name);
        }
        entry->obj = this;
        mName = entry->name;
        dir->AddedObject(this);
    }
}

ObjectDir *Hmx::Object::DataDir() {
    return mDir ? mDir : ObjectDir::Main();
}

__declspec(noinline) static const char *FindPathNameHelper(const char *name, const char *path) {
    return MakeString("%s (%s)", name, FileLocalize(path, nullptr));
}

const char *Hmx::Object::FindPathName() {
    const char *name = (mName && *mName) ? mName : ClassName().Str();

    ObjectDir *dataDir = DataDir();
    if (dataDir) {
        DirLoader *loader = dataDir->Loader();
        if ((int)loader) {
            return FindPathNameHelper(name, loader->LoaderFile().c_str());
        } else if (!dataDir->ProxyFile().empty()) {
            return FindPathNameHelper(name, dataDir->ProxyFile().c_str());
        } else if (*dataDir->GetPathName() != '\0') {
            return FindPathNameHelper(name, dataDir->GetPathName());
        } else if (dataDir != this && dataDir->Name() && *dataDir->Name()) {
            return MakeString("%s/%s", dataDir->Name(), name);
        } else if (mDir && *mDir->GetPathName()) {
            return FindPathNameHelper(name, mDir->GetPathName());
        }
    }
    return name;
}

#pragma region Ref Methods

void Hmx::Object::ReplaceRefs(Hmx::Object *obj) {
    if (mRefs.begin() != mRefs.end()) {
#ifdef HX_NATIVE
        // Drain approach (lane A31): the entries are moved onto `pending`, a
        // consistent local ring, and Replace(obj) is called on pending.next
        // until it is empty, as NullifyAllRefs drains its `pending` and
        // ReplaceList its `other`. A Replace callback that frees an entry
        // (its ~ObjRef unlinks it from `pending`) or retargets one (SetObj
        // moves it onto its new object's ring) takes it off `pending`, so it
        // is never stepped as a stale node.
        //
        // The vector snapshot this replaces held entry ADDRESSES. An owner
        // that keeps its refs by value in a std::vector erases by shifting:
        // CharClipGroup::Replace (retail fn_8238F270, erase at +0xD8) does
        // mClips.erase, and each later ObjOwnerPtr<CharClip> is assigned from
        // its successor (SetOwnerObj: Release, then AddRef onto the clip). With
        // the dying clip D in a group twice, the step for the first entry
        // shifted D's second entry down a slot -- AddRef'ing it back onto D's
        // cleared ring -- and destroyed the last slot. The snapshot then
        // stepped that last slot's address (a destroyed ObjOwnerPtr: "calling
        // get obj on abstract ObjRef") or a shifted slot that now held another
        // clip (which it dropped from the group), and D was freed with the
        // group still holding it: ASan heap-use-after-free in AddRef on the
        // next erase that shifted it, then RING CORRUPTION and a SIGSEGV in
        // ~CharClip <- FileMerger::Merger::Clear on coop_endgame ->
        // meta_loading_continue (lanes A28 core h2, A31 asan1/w1).
        //
        // Retail X360 (below) walks the live std::list and erases each entry
        // before its Replace, so an entry AddRef'd back onto the dying object
        // is appended and reached by the same walk. Natively an entry that
        // comes back onto mRefs during a drain is stepped by another pass.
        // Entries this object owns itself are left on mRefs after the first
        // pass, as the snapshot left them, so a self-seeded member that
        // re-seeds onto its owner cannot loop.
        //
        // takeInto moves the live entries of mRefs onto `pending` in ring
        // order and leaves mRefs empty; an entry whose RefOwner() is
        // `keepOwner` (when non-null) stays on mRefs. It skips freed nodes:
        // during cascading ObjectDir destruction some ObjRefs may be freed but
        // still linked (~ObjRefConcrete skipped Release), their mAliveSentinel
        // cleared by ~ObjRef(). It reads the sentinel and next pointer from
        // those nodes, so ASan is suppressed: the memory is quarantined but
        // readable. Returns the number of entries moved onto `pending`.
        auto takeInto = [this](ObjRef *pending, Hmx::Object *keepOwner)
            __attribute__((noinline, no_sanitize("address"))) -> size_t {
            constexpr size_t kSentinelOffset = 3 * sizeof(void *);
            constexpr size_t kNextOffset = sizeof(void *);
            constexpr size_t kMaxRingSize = 100000; // safety limit
            ObjRef *sentinel = &mRefs;
            ObjRef *first = *(ObjRef **)((const char *)sentinel + kNextOffset);
            ObjRef kept;
            size_t count = 0, moved = 0;
            for (ObjRef *it = first; it != sentinel;) {
                if ((uintptr_t)it < 0x10000)
                    break;
                if (++count > kMaxRingSize) {
                    MILO_LOG("SnapshotRing: RING CORRUPTION — walked %zu nodes without returning to sentinel %p (first=%p, cur=%p)\n",
                             count, (void *)sentinel, (void *)first, (void *)it);
                    break;
                }
                uint32_t alive = *(const uint32_t *)((const char *)it + kSentinelOffset);
                ObjRef *nextNode = *(ObjRef **)((const char *)it + kNextOffset);
                if (alive == ObjRef::kAliveSentinel) {
                    it->DetachSelf();
                    if (keepOwner && it->RefOwner() == keepOwner) {
                        it->AddRef(&kept);
                    } else {
                        it->AddRef(pending);
                        moved++;
                    }
                }
                it = nextNode;
            }
            sentinel->DetachSelf();
            while (!kept.empty()) {
                ObjRef *ref = kept.next;
                ObjRef::SafeReleaseFromRing(ref);
                ref->AddRef(sentinel);
            }
            return moved;
        };
        bool wasInReplace = gInReplaceList;
        gInReplaceList = true;
        constexpr int kMaxPasses = 8;
        int pass = 0;
        for (; pass < kMaxPasses && !mRefs.empty(); pass++) {
            ObjRef pending;
            if (takeInto(&pending, pass ? this : nullptr) == 0)
                break;
            while (!pending.empty()) {
                ObjRef *ref = pending.next;
                ref->Replace(obj);
                if (pending.next == ref) {
                    // Replace left it in place: unlink it, as the snapshot's
                    // self-loop did, so the drain advances.
                    ObjRef::SafeReleaseFromRing(ref);
                }
            }
        }
        if (pass == kMaxPasses && !mRefs.empty()) {
            MILO_LOG("ReplaceRefs: %s keeps gaining refs after %d passes\n",
                     Name() ? Name() : "", kMaxPasses);
        }
        gInReplaceList = wasInReplace;
#else
        // Retail X360: erase each entry, then dispatch Replace(this, obj) on
        // its ring-ref (vtable slot +8).
        for (ObjRefList::iterator it = mRefs.begin(); it != mRefs.end();) {
            ObjRefOwner *ref = *it;
            it = mRefs.erase(it);
            ref->Replace(reinterpret_cast<ObjRef *>(this), obj);
        }
#endif
    }
}

#ifdef HX_NATIVE
/** X16. Default ON. `RB3_NO_OWNERPTR_SEED=1` restores pre-X16 behaviour (a bare
 *  null on teardown) so the two arms can be compared in one binary. Defined
 *  inside the HX_NATIVE guard so the X360 object is untouched. */
bool ObjRefSeedRestoreEnabled() {
    static const bool on = (getenv("RB3_NO_OWNERPTR_SEED") == nullptr);
    return on;
}

// Owner-control holders in the native ~ObjectDir cascade.
//
// X360 frees a dir's objects with `delete` (ObjectDir::DeleteObjects), and
// ~Object (0x8275CBF0) walks mRefs dispatching Replace(this, 0) on every
// entry. For an OWNER-CONTROL holder -- an ObjOwnerPtr, or a node of an
// ObjPtrList/ObjPtrVec in kObjListOwnerControl mode (ObjRef::ControlOwner) --
// that call forwards to the owner's Replace(ref, 0), which is the owner's only
// notice that the target died. Native DeleteObjects pre-nullifies every ring
// first (Phase 0, Dir.cpp), and this function used to null every ref with
// NullifyObj and nothing else, so for those holders the ref went NULL and the
// owner's state did not follow. Measured on rb3-app's first App::Run frame:
// Splash::EndSplasher's ~RndDir nulled the splash AnimTask's mAnim, retail's
// AnimTask::Replace (which deletes the task) never ran, and TaskMgr::Poll's
// AnimTask::Poll dereferenced the null anim.
//
// Why Phase 0 used NullifyObj alone (dc3-decomp 07bad0ab, 2026-03-20, carried
// here by c5c1650fb): MessageTask, ScriptTask, PropertyTask and DirLoader
// `delete this` from Replace, and running Replace INSIDE the half-walked ring
// unlinks through neighbours this walk never repairs. So, as dc3-decomp did
// (73642d516, 2026-09-30), this does what X360's ReplaceRefs does instead:
// the walk takes each ref out of the ring, nulls ordinary refs with NullifyObj
// as before, and moves owner-control refs onto a local head `pending` (the
// image's `ObjRef other`). Once this object's own ring is empty, `pending` is
// drained the way ReplaceList drains `other`: Replace(nullptr) on
// pending.next until it is empty, so every unlink happens on a consistent
// ring, and a ref an owner copies (ObjPtrVec::erase shifts nodes with
// CopyRef) joins `pending` beside its source and is stepped too. A
// `delete this` there frees a Task or a DirLoader, not an entry of
// DeleteObjects' todo list, and every Hmx::Object free inside a cascade is
// deferred to FlushDeferredFrees (NativeObjMemFree, Dir.cpp).
//
// Not stepped (NullifyObj, as before):
//   * a dying dir's OWN DirLoader (its mProxyDir): DirLoader::Replace deletes
//     the loader, and ~ObjectDir deletes mLoader again. X360 deletes mLoader in
//     ~ObjectDir's body, before ~Object's ReplaceRefs; the native pre-nullify
//     runs before that body.
//   * a ref whose owning object is already destroyed (its sentinel is dead).
// An owner that IS the dying object is stepped: it is alive here (X360 would
// have destroyed that member first, taking the ref with it), and a member left
// nulled in place breaks containers that relink their refs by ring neighbour
// (dc3-decomp measured this on CharClip::Transitions::RemoveNodes).
namespace {
    bool TakesOwnerStep(ObjRef *ref, Hmx::Object *dying) {
        // Classify by the REF first (the caller checked its sentinel) and only
        // then ask for its owner: an ordinary holder's owner can already be
        // gone, while an owner-control holder is a member of its owner.
        ObjRefOwner *owner = ref->ControlOwner();
        if (!owner)
            return false;
        if (DirLoader *loader = dynamic_cast<DirLoader *>(owner)) {
            ObjectDir *dir = dynamic_cast<ObjectDir *>(dying);
            if (dir && dir->Loader() == loader)
                return false;
        }
        Hmx::Object *obj = owner->RefOwner();
        return !obj || obj->IsRefAlive();
    }
}

#if defined(__SANITIZE_ADDRESS__) || (defined(__has_feature) && __has_feature(address_sanitizer))
__attribute__((no_sanitize("address")))
#endif
void Hmx::Object::NullifyAllRefs() {
    ObjRef *sentinel = &mRefs;
    constexpr size_t kNextOffset = sizeof(void *); // ObjRef layout: vtable, next, prev, sentinel
    constexpr size_t kSentinelOffset = 3 * sizeof(void *);
    constexpr size_t kMaxRingSize = 100000;
    ObjRef *cur = sentinel->next;
    size_t count = 0;
    ObjRef pending; // the image's `other`: owner-control refs, a consistent ring
    while (cur != sentinel) {
        if ((uintptr_t)cur < 0x10000 || ++count > kMaxRingSize)
            break;
        // Read next pointer before potentially dead memory is reused.
        // Dead ObjRefs (from freed ObjPtrVec buffers during cascade) are
        // still linked in the ring — skip them, but keep walking via their
        // next pointer (same technique as ReplaceRefs' takeInto).
        ObjRef *nxt = *(ObjRef **)((const char *)cur + kNextOffset);
        uint32_t alive = *(const uint32_t *)((const char *)cur + kSentinelOffset);
        if (alive == ObjRef::kAliveSentinel) {
            // Out of the ring first: this walk never repairs neighbours, so
            // the next ref's `prev` still names this one.
            cur->DetachSelf();
            if (TakesOwnerStep(cur, this))
                cur->AddRef(&pending); // still aimed at us, for its owner
            else
                cur->NullifyObj();
        }
        cur = nxt;
    }
    sentinel->next = sentinel;
    sentinel->prev = sentinel;
    // Drain `pending` as ReplaceList drains `other`: each owner step unlinks
    // its ref (retargets it, or destroys it with its node or its owner).
    while (!pending.empty()) {
        ObjRef *ref = pending.next;
        if (TakesOwnerStep(ref, this))
            ref->Replace(nullptr);
        if (pending.next == ref) {
            // Declined (the default Hmx::Object::Replace changes nothing), or
            // its owner died in an earlier step. Do not leave it aimed at a
            // block that is about to be freed. For a self-seeded ObjOwnerPtr
            // this is the X16 seed restore, unchanged.
            ObjRef::SafeReleaseFromRing(ref);
            ref->NullifyObj();
        }
    }
}
#endif

void Hmx::Object::ReplaceRefsFrom(Hmx::Object *from, Hmx::Object *to) {
    MILO_ASSERT(from, 0xA6);
#ifdef HX_NATIVE
    ObjRef other;
    other.DetachSelf();
    FOREACH (it, mRefs) {
        // Virtual base offsets can make RefOwner() != from even for the same
        // object (Itanium ABI vbase adjustment). Use dynamic_cast<void*> to
        // compare most-derived addresses.
        bool match = (it->RefOwner() == from);
        if (!match && it->RefOwner() && from) {
            match = dynamic_cast<const void *>(it->RefOwner())
                 == dynamic_cast<const void *>(from);
        }
        if (match) {
            it->Release(&other);
            other.AddRef(it);
        }
    }
    other.ReplaceList(to);
#else
    // Retail X360: the ring-ref's RefOwner() identifies `from`. Each matching
    // entry is erased and Replace(from, to) dispatched on its ring-ref (the ref
    // re-AddRefs `to` via SetObj).
    for (ObjRefList::iterator it = mRefs.begin(); it != mRefs.end();) {
        ObjRefOwner *ref = *it;
        if (ref->RefOwner() == from) {
            it = mRefs.erase(it);
            ref->Replace(reinterpret_cast<ObjRef *>(from), to);
        } else {
            ++it;
        }
    }
#endif
}

int Hmx::Object::RefCount() const {
    int size = 0;
    FOREACH (it, mRefs) {
        size++;
    }
    return size;
}

#pragma endregion
#pragma region Sink Methods

void Hmx::Object::RemovePropertySink(Hmx::Object *o, DataArray *a) {
#ifdef HX_NATIVE
    if (mSinks)
        mSinks->RemovePropertySink(o, a);
#endif
}

bool Hmx::Object::HasPropertySink(Hmx::Object *o, DataArray *a) {
#ifdef HX_NATIVE
    if (mSinks)
        return mSinks->HasPropertySink(o, a);
#endif
    return false;
}

void Hmx::Object::RemoveSink(Hmx::Object *o, Symbol s) {
#ifdef HX_NATIVE
    if (mSinks)
        mSinks->RemoveSink(o, s);
#endif
}

MsgSinks *Hmx::Object::GetOrAddSinks() {
#ifdef HX_NATIVE
    if (!mSinks) {
        mSinks = new MsgSinks(this);
    }
    return mSinks;
#else
    return nullptr;
#endif
}

void Hmx::Object::AddSink(Hmx::Object *o, Symbol s1, Symbol s2, SinkMode sm) {
#ifdef HX_NATIVE
    GetOrAddSinks()->AddSink(o, s1, s2, sm, true);
#endif
}

void Hmx::Object::AddPropertySink(Hmx::Object *o, DataArray *a, Symbol s) {
#ifdef HX_NATIVE
    GetOrAddSinks()->AddPropertySink(o, a, s);
#endif
}

void Hmx::Object::MergeSinks(Hmx::Object *o) {
#ifdef HX_NATIVE
    if (o && o->mSinks) {
        GetOrAddSinks()->MergeSinks(o);
    }
#endif
}

void Hmx::Object::ChainSource(Hmx::Object *source, Hmx::Object *o2) {
    MILO_ASSERT(source, 0x29D);
    if (!o2)
        o2 = this;
#ifdef HX_NATIVE
    if (mSinks && !mSinks->Sinks().empty()) {
        source->GetOrAddSinks()->AddSink(this, Symbol());
    } else if (o2->mSinks) {
        o2->mSinks->ChainEventSinks(source, this);
    }
#else
    (void)o2;
    (void)source;
#endif
}

void Hmx::Object::ExportPropertyChange(DataArray *a, Symbol s) {
    if (!s.Null()) {
#ifdef HX_NATIVE
        MILO_ASSERT(mSinks, 0x17F);
#endif
        static Message msg("blah", 0);
        msg.SetType(s);
        msg[0] = a;
        Export(msg, true);
    }
}

void Hmx::Object::BroadcastPropertyChange(DataArray *a) {
#ifdef HX_NATIVE
    ExportPropertyChange(a, mSinks ? mSinks->GetPropSyncHandler(a) : Symbol());
#else
    ExportPropertyChange(a, Symbol());
#endif
}

#pragma endregion
#pragma region Property Methods

#ifdef HX_NATIVE
DataArray *GetNextPropPath() {
    for (int i = 0; i < DIM(gPropPaths); i++) {
        if (gPropPaths[i]->RefCount() == 1) {
            return gPropPaths[i];
        }
    }
    MILO_FAIL("Recursive SetProperty call count greater than %d!", 8);
    return nullptr;
}
#endif

const DataNode *Hmx::Object::Property(DataArray *prop, bool fail) const {
    static DataNode n(0);
    // if prop was synced, return the prop node n
    if (const_cast<Hmx::Object *>(this)->SyncProperty(n, prop, 0, kPropGet))
        return &n;
    Symbol propKey = prop->Sym(0);
    const DataNode *propValue = nullptr;
#ifdef HX_NATIVE
    if (mTypeProps) {
        // retrieve property val from typeprops array
        propValue = mTypeProps->KeyValue(propKey, false);
    }
#else
    propValue = mTypeProps.KeyValue(propKey, false);
#endif
    if (!propValue && mTypeDef) {
        DataArray *found = mTypeDef->FindArray(propKey, fail);
        if (found)
            propValue = &found->Evaluate(1);
    }
    if (propValue) {
        int cnt = prop->Size();
        if (cnt == 1)
            return propValue;
        if (cnt == 2 && propValue->Type() == kDataArray) {
            DataArray *ret = propValue->UncheckedArray();
            return &ret->Node(prop->Int(1));
        }
    }

    if (fail) {
#ifdef HX_NATIVE
        // Native keeps the DTA-warn path verbatim (MILO_FAIL_DTA warns on native,
        // MILO_FAIL is fatal) -- do not "unify" these, it turns a recoverable
        // missing-property warning into a native abort.
        MILO_FAIL_DTA("%s: property %s not found", PathName(this), PrintPropertyPath(prop));
#else
        // Retail streams the path into a local String (String derives from
        // TextStream) rather than calling PrintPropertyPath.  Retail bytes here:
        // ??0String@@QAA@XZ, operator<<(TextStream &, const DataArray *), a String
        // COPY ctor, PathName(this), two ~String -- and NO MakeString and NO
        // Debug::Fail.  MILO_FAIL_DTA's non-native branch (TheDebugFailer <<
        // MakeString) emits both, which is why this read as a total body mismatch.
        //
        // RESIDUAL, deliberately not chased (87.0 -> 91.5%, still 0 bytes because
        // matched_code is all-or-nothing): retail's surviving argument setup is
        // RIGHT-TO-LEFT (copy of str, then PathName, then the format literal) --
        // the signature of a stripped VARARGS call, MakeString(fmt, PathName(this),
        // str), where str went through `...` by value.  MILO_FAIL's ((void)(args))
        // comma form evaluates LEFT-TO-RIGHT and makes no copy.  Forcing the copy
        // with an explicit String(str) reaches 93.4% but emits it in the wrong
        // order and is source-level metric-fitting, so it is not done here; the
        // real fix is to model MILO_FAIL_DTA's stripped varargs call in Debug.h,
        // which is PCH-wide (36 sites) and needs its own measured lane.
        String str;
        str << prop;
        // W16-HP: MILO_FAIL_DTA is now that stripped varargs call (MiloStripEval).
        MILO_FAIL_DTA("%s: property %s not found", PathName(this), str);
#endif
    }
    return nullptr;
}

const DataNode *Hmx::Object::Property(Symbol prop, bool fail) const {
    // Retail builds the static via DataArrayPtr(const DataNode &) -- it stores a
    // temp DataNode{1, kDataInt} on the frame and passes its address -- NOT via
    // DataArrayPtr(new DataArray(1)), which would inline PoolAlloc + DataArray(int).
    static DataArrayPtr d(1);
    d->Node(0) = prop;
    return Property(d, fail);
}

DataNode Hmx::Object::HandleProperty(DataArray *prop, DataArray *a2, bool fail) {
    static DataNode n(a2);
    if (SyncProperty(n, prop, 0, kPropHandle)) {
        return n;
    }
    if (fail) {
        MILO_FAIL_DTA(
            "%s: property %s not found", PathName(this), prop ? prop->Sym(0) : "<none>"
        );
    }
    return 0;
}

DataNode Hmx::Object::PropertyArray(Symbol sym) {
    static DataArrayPtr d(DataNode(1));
    d->Node(0) = sym;
    int size = PropertySize(d);
    DataArray *newArr = new DataArray(size);
    static DataArrayPtr path(new DataArray(2));
    path->Node(0) = sym;
    for (int i = 0; i < size; i++) {
        path->Node(1) = i;
        newArr->Node(i) = *Property(path, true);
    }
    DataNode ret = newArr;
    newArr->Release();
    return ret;
}

int Hmx::Object::PropertySize(DataArray *prop) {
    static DataNode n;
    if (SyncProperty(n, prop, 0, kPropSize)) {
        return n.Int();
    }
    MILO_ASSERT(prop->Size() == 1, 0x208);
    Symbol name = prop->Sym(0);
    const DataNode *a = nullptr;
#ifdef HX_NATIVE
    if (mTypeProps) {
        a = mTypeProps->KeyValue(name, false);
    }
#else
    a = mTypeProps.KeyValue(name, false);
#endif
    if (!a) {
        if (mTypeDef) {
            a = &mTypeDef->FindArray(name)->Evaluate(1);
        } else {
#ifdef HX_NATIVE
            MILO_FAIL_DTA("%s: property %s not found", PathName(this), name);
            return 0; // MILO_FAIL_DTA warns on native, so we must bail before null deref
#else
            // TU5 keeps only the PathName(this) argument evaluation; the
            // MakeString + Debug::Fail are gone.
            PathName(this);
#endif
        }
    }
    MILO_ASSERT(a->Type() == kDataArray, 0x21B);
    return a->UncheckedArray()->Size();
}

void Hmx::Object::RemoveProperty(DataArray *prop) {
    static DataNode n;
    if (!SyncProperty(n, prop, 0, kPropRemove)) {
        MILO_ASSERT(prop->Size() == 2, 0x235);
#ifdef HX_NATIVE
        if (mTypeProps) {
            mTypeProps->RemoveArrayValue(prop->Sym(0), prop->Int(1), mTypeDef);
        }
#else
        mTypeProps.RemoveArrayValue(prop->Sym(0), prop->Int(1), mTypeDef);
#endif
    }
}

void Hmx::Object::BroadcastPropertyChange(Symbol s) {
    static DataArray *a = new DataArray(1);
    a->Node(0) = s;
    BroadcastPropertyChange(a);
}

void Hmx::Object::PropertyClear(DataArray *propArr) {
    int size = PropertySize(propArr);
    DataArray *cloned = propArr->Clone(true, false, 1);
    while (size-- != 0) {
        cloned->Node(cloned->Size() - 1) = size;
        RemoveProperty(cloned);
    }
    cloned->Release();
}

#ifndef HX_NATIVE
// Retail X360 (0x8275A650, lane W5-A): RB3 has no property-change export and no
// sink handler here -- that is DC3 machinery. SetArrayValue takes the owner's
// TypeDef as its 4th argument (r7 = this->mTypeDef at the retail call site).
void Hmx::Object::SetProperty(DataArray *prop, const DataNode &val) {
    if (!SyncProperty((DataNode &)val, prop, 0, kPropSet)) {
        Symbol key = prop->Sym(0);
        if (prop->Size() == 1) {
            mTypeProps.SetKeyValue(key, val, true);
        } else {
            MILO_ASSERT(prop->Size() == 2, 0x1C4);
            mTypeProps.SetArrayValue(key, prop->Int(1), val, mTypeDef);
        }
    }
}
#else
void Hmx::Object::SetProperty(DataArray *prop, const DataNode &val) {
    const DataNode *prop_n = nullptr;
    DataNode n;
    Symbol handler;
    if (mSinks) {
        handler = mSinks->GetPropSyncHandler(prop);
        if (!handler.Null()) {
            prop_n = Property(prop, false);
            if (prop_n) {
                n = *prop_n;
            }
        }
    }
    if (!SyncProperty((DataNode &)val, prop, 0, kPropSet)) {
        Symbol key = prop->Sym(0);
        if (!mTypeProps) {
            mTypeProps = new TypeProps(this);
        }
        if (prop->Size() == 1) {
            mTypeProps->SetKeyValue(key, val, true);
        } else {
            MILO_ASSERT(prop->Size() == 2, 0x1C4);
            mTypeProps->SetArrayValue(key, prop->Int(1), val, mTypeDef);
        }
        if (prop_n && val.Equal(n, nullptr, false)) {
            handler = Symbol();
        }
    } else {
        if (prop_n) {
            const DataNode *synced = Property(prop, true);
            if (synced->Equal(n, nullptr, false)) {
                handler = Symbol();
            }
        }
    }
    ExportPropertyChange(prop, handler);
}
#endif

void Hmx::Object::SetProperty(Symbol prop, const DataNode &val) {
#ifdef HX_NATIVE
    DataArray *path = GetNextPropPath();
    path->AddRef();
    path->Node(0) = prop;
    SetProperty(path, val);
    path->Release();
#else
    // Retail is byte-for-byte the same shape as Property(Symbol, bool) above: one
    // shared function-local static, no pool, no AddRef/Release.  Adjudicated on
    // retail bytes -- the body has no call to any pool accessor, and carries the
    // same static guard + DataArrayPtr(const DataNode &) + atexit sequence.
    static DataArrayPtr d(1);
    d->Node(0) = prop;
    SetProperty(d, val);
#endif
}

void Hmx::Object::InsertProperty(DataArray *prop, const DataNode &val) {
    if (!SyncProperty((DataNode &)val, prop, 0, kPropInsert)) {
        MILO_ASSERT(prop->Size() == 2, 0x240);
#ifdef HX_NATIVE
        if (!mTypeProps) {
            mTypeProps = new TypeProps(this);
        }
        mTypeProps->InsertArrayValue(prop->Sym(0), prop->Int(1), val, mTypeDef);
#else
        mTypeProps.InsertArrayValue(prop->Sym(0), prop->Int(1), val, mTypeDef);
#endif
    }
}

#pragma endregion
#pragma region Factory Methods

Hmx::Object *Hmx::Object::NewObject(Symbol name) {
    std::map<Symbol, ObjectFunc *>::iterator it = sFactories.find(name);
    MILO_ASSERT_FMT(it != sFactories.end(), "Unknown class %s", name);
#ifdef HX_NATIVE
    if (it == sFactories.end()) {
        return nullptr;
    }
#endif
    return (it->second)();
}

bool Hmx::Object::RegisteredFactory(Symbol name) {
    return sFactories.find(name) != sFactories.end();
}

void Hmx::Object::RegisterFactory(Symbol name, ObjectFunc *func) {
    sFactories[name] = func;
}

#pragma endregion
#pragma region Misc Methods

// Retail (fn_8275BD78, Handle's "set_note" arm) calls this as a real `bl`, but
// /Ob2 inlines this trivial one-line out-of-line member at the call site by
// default. Per the established VocalPlayer.cpp::OnMsg(ButtonUpMsg) precedent,
// `#pragma auto_inline(off)` is the working lever for this MSVC
// (16.00.10224.00) -- moving the definition doesn't help, and
// __declspec(noinline) is not a substitute (see OvershellSlot.cpp:1874).
#pragma auto_inline(off)
#ifdef HX_NATIVE
void Hmx::Object::SetNote(const char *note) { mNote = note; }
#else
// Retail X360 (fn_8275A500, lane W5-A): Object OWNS its note. Free the old pool
// copy (unless gNullStr) by strlen+1, pool-alloc strlen+1 for the new one and
// strcpy it in (MSVC inlines the byte loop), or fall back to gNullStr for a
// null/empty note. ~Object frees it the same way; Load routes through here.
void Hmx::Object::SetNote(const char *note) {
    if (mNote != gNullStr) {
        MemOrPoolFreeSTL(strlen(mNote) + 1, (void *)mNote);
    }
    if (note && *note) {
        char *buf = (char *)MemOrPoolAlloc(strlen(note) + 1);
        mNote = buf;
        // Retail copies with a two-pointer byte loop (lbzu/stbu), not the
        // strcpy intrinsic's dst-src indexed form: the byte is read into a
        // local, stored, then tested, which keeps the store pointer separate.
        char *dst = buf - 1;
        const char *src = note - 1;
        for (;;) {
            char c = *++src;
            *++dst = c;
            if (c == 0)
                break;
        }
    } else {
        mNote = gNullStr;
    }
}
#endif
#pragma auto_inline(on)

void Hmx::Object::RemoveFromDir() {
    if (mDir && mDir != sDeleting) {
        mDir->RemovingObject(this);
        ObjectDir::Entry *entry = mDir->FindEntry(mName, false);
#ifdef HX_NATIVE
        // retail (0x8275A460) has no entry check: FindEntry, then store
        if (!entry || entry->obj != this) {
            MILO_FAIL("No entry for %s in %s", PathName(this), PathName(mDir));
        }
#endif
        entry->obj = nullptr;
    }
}

bool Hmx::Object::HasTypeProps() const {
#ifdef HX_NATIVE
    return mTypeProps && mTypeProps->HasProps();
#else
    return mTypeProps.HasProps();
#endif
}

void Hmx::Object::ClearAllTypeProps() {
#ifdef HX_NATIVE
    if (mTypeProps) {
        RELEASE(mTypeProps);
    }
#else
    mTypeProps.ClearAll();
#endif
}

DataNode Hmx::Object::HandleType(DataArray *msg) {
    Symbol t = msg->Sym(1);
    DataArray *handler = nullptr;
    if (mTypeDef) {
        handler = mTypeDef->FindArray(t, false);
    }
    if (handler) {
#ifdef HX_NATIVE
        MessageTimer timer(this, t); // retail X360 (0x8275ABD8) has no timer here
#endif
        return handler->ExecuteScript(1, this, (const DataArray *)msg, 2);
    }
    return DATA_UNHANDLED;
}

#pragma endregion
#pragma region Handlers

DataNode Hmx::Object::OnIterateRefs(const DataArray *da) {
    DataNode *var = da->Var(2);
    DataNode node(*var);
#ifdef HX_NATIVE
    ObjRef *end = &mRefs;
    for (ObjRef *it = end->next; it != end;) {
        ObjRef *cur = it;
        it = it->next;
        *var = cur->RefOwner();
#else
    for (ObjRefList::iterator it = mRefs.begin(); it != mRefs.end();) {
        ObjRefList::iterator cur = it;
        ++it;
        *var = (*cur)->RefOwner();
#endif
        for (int i = 3; i < da->Size(); i++) {
            da->Command(i)->Execute();
        }
    }
    *var = node;
    return 0;
}

DataNode Hmx::Object::OnGetTypeList(const DataArray *a) {
    DataArray *def = ObjectDef(gNullStr);
    DataArrayPtr ptr;
    static Symbol allow_null_type = "allow_null_type";
    bool b6 = true;
    DataArray *nullArr = def->FindArray(allow_null_type, false);
    if (nullArr) {
        b6 = nullArr->ExecuteScript(1, this, nullptr, 1).Int();
    }
    if (b6) {
        ptr->Insert(ptr->Size(), Symbol());
    }
    DataArray *typesArr = def->FindArray("types", false);
    if (typesArr) {
        for (int i = 1; i < typesArr->Size(); i++) {
            DataArray *curArr = typesArr->Array(i);
            DataArray *helpArr = curArr->FindArray("help", false);
            if (helpArr) {
                DataArray *newArr = new DataArray(2);
                newArr->Node(0) = curArr->Sym(0);
                newArr->Node(1) = helpArr;
                ptr->Insert(ptr->Size(), newArr);
                newArr->Release();
            } else {
                ptr->Insert(ptr->Size(), curArr->Sym(0));
            }
        }
    }
    return ptr;
}

DataNode Hmx::Object::OnAddSink(DataArray *a) {
    if (a->Size() >= 4) {
        SinkMode mode = (a->Size() > 4) ? (SinkMode)a->Int(4) : kHandle;
        bool chain = (a->Size() > 5) ? a->Int(5) : true;
        DataArray *arr3 = a->Array(3);
        Hmx::Object *obj = a->GetObj(2);
        if (obj) {
            if (arr3->Size() == 0) {
                GetOrAddSinks()->AddSink(obj, Symbol(), Symbol(), mode, chain);
            } else {
                for (int i = 0; i < arr3->Size(); i++) {
                    DataNode eval = arr3->Evaluate(i);
                    Symbol s7;
                    Symbol s6;
                    if (eval.Type() == kDataArray) {
                        s6 = eval.LiteralArray()->LiteralSym(1);
                        s7 = eval.LiteralArray()->LiteralSym(0);
                    } else {
                        s6 = Symbol();
                        s7 = eval.LiteralSym();
                    }
                    GetOrAddSinks()->AddSink(obj, s7, s6, mode, chain);
                }
            }
        }
    } else {
        Symbol s1, s2;
        Hmx::Object *obj = a->GetObj(2);
        AddSink(obj, s1, s2, kHandle);
    }
    return 0;
}

DataNode Hmx::Object::OnRemoveSink(DataArray *a) {
#ifdef HX_NATIVE
    if (a->Size() > 3) {
        Hmx::Object *obj = a->GetObj(2);
        Symbol s;
        for (int i = 3; i < a->Size(); i++) {
            s = a->Sym(i);
            if (mSinks)
                mSinks->RemoveSink(obj, s);
        }
    } else {
        Symbol s = Symbol();
        Hmx::Object *obj = a->GetObj(2);
        if (mSinks)
            mSinks->RemoveSink(obj, s);
    }
#endif
    return 0;
}

#ifndef HX_NATIVE
// Retail 0x8275B798: no type check on the non-symbol key, and a missing
// property falls back to the default argument.
DataNode Hmx::Object::OnGet(const DataArray *a) {
    const DataNode &node = a->Evaluate(2);
    if (node.Type() == kDataSymbol) {
        const char *sym = node.UncheckedStr();
        const DataNode *prop = Property(STR_TO_SYM(sym), a->Size() < 4);
        if (prop)
            return *prop;
    } else {
        const DataNode *prop = Property(node.UncheckedArray(), a->Size() < 4);
        if (prop)
            return *prop;
    }
    return a->Node(3);
}
#else
DataNode Hmx::Object::OnGet(const DataArray *a) {
    const DataNode &node = a->Evaluate(2);
    if (node.Type() == kDataSymbol) {
        const char *sym = node.UncheckedStr();
        const DataNode *prop = Property(STR_TO_SYM(sym), a->Size() < 4);
        if (prop)
            return *prop;
    } else {
        if (node.Type() != kDataArray) {
            String str;
            node.Print(str, true);
            MILO_FAIL(
                "Data %s is not array or symbol (file %s, line %d)",
                str.c_str(),
                a->File(),
                a->Line()
            );
        }
        const DataNode *prop = Property(node.UncheckedArray(), a->Size() < 4);
        if (prop)
            return *prop;
    }
#ifdef HX_NATIVE
    if (a->Size() > 3)
        return a->Node(3);
    return DataNode(0);
#else
    return a->Node(3);
#endif
}
#endif

#ifdef HX_NATIVE
DataNode Hmx::Object::OnSet(const DataArray *a) {
    MILO_ASSERT_FMT(
        a->Size() % 2 == 0,
        "Uneven number of properties (file %s, line %d)",
        a->File(),
        a->Line()
    );
    for (int i = 2; i < a->Size(); i += 2) {
        const DataNode &n = a->Evaluate(i);
        if (n.Type() == kDataSymbol) {
            const DataNode &eval = a->Evaluate(i + 1);
            const char *str = n.UncheckedStr();
            SetProperty(STR_TO_SYM(str), eval);
        } else {
            if (n.Type() != kDataArray) {
                String str;
                n.Print(str, true);
                MILO_FAIL(
                    "Data %s is not array or symbol (file %s, line %d)",
                    str.c_str(),
                    a->File(),
                    a->Line()
                );
            }
            SetProperty(n.UncheckedArray(), a->Evaluate(i + 1));
        }
    }
    return 0;
}
#else
// RB3 retail (0x8275B5E8): no size-parity assert, no type test on the array
// branch -- a non-symbol key is taken as an array unchecked.
DataNode Hmx::Object::OnSet(const DataArray *a) {
    for (int i = 2; i < a->Size(); i += 2) {
        const DataNode &n = a->Evaluate(i);
        if (n.Type() == kDataSymbol) {
            // the symbol word is read after Evaluate(i + 1) (mValue is at +0)
            SetProperty(*reinterpret_cast<const Symbol *>(&n), a->Evaluate(i + 1));
        } else {
            SetProperty(n.UncheckedArray(), a->Evaluate(i + 1));
        }
    }
    return 0;
}
#endif

DataNode Hmx::Object::OnPropertyAppend(const DataArray *da) {
    DataArray *arr = da->Array(2);
    int size = PropertySize(arr);
    DataArray *cloned = arr->Clone(true, false, 1);
    cloned->Node(cloned->Size() - 1) = size;
    InsertProperty(cloned, da->Evaluate(3));
    cloned->Release();
    return size;
}

#ifdef HX_NATIVE
// Retail TU5's Hmx::Object copy constructor is the compiler's memberwise one,
// 0x822774F8: the two vtables, then words copied for TypeProps' mMap@8 and
// mOwner@0xc, mTypeDef@0x10, mNote@0x14, mName@0x18 and mDir@0x1c, then the
// mRefs list's copy constructor (bl 0x82370C80 on this+0x20, &other+0x20). Its
// caller on the song-select path is SongRecord's copy constructor 0x82598190
// (bl 0x822774F8 at 0x825981AC), reached through the std::pair<Symbol,
// SongRecord> that SongSortMgr::BuildFilteredSongList inserts; those records
// are unnamed and typeless, so the copy carries no props, type, name or dir.
// Natively the declaration has no body (the X360 build never needs one), so
// rb3-app linked an UNREACHED stub [ring a1-absent-body] and aborted as
// song_select_screen entered. This is retail's member copy for the native
// layout, with the three words retail shares made sound: the props are deep
// copied for this owner (TypeProps::operator=, where retail shares mMap and
// keeps the source as mOwner), the type gets its own reference (retail copies
// the pointer, and both destructors release it), and the ref ring starts empty
// (a native ring node is an ObjRef aimed at the source; it cannot hold the copy).
// mSinks is native-only and starts empty, as in Object().
Hmx::Object::Object(const Object &o)
    : mTypeProps(nullptr), mTypeDef(o.mTypeDef), mNote(o.mNote), mName(o.mName),
      mDir(o.mDir), mSinks(nullptr) {
    mRefs.DetachSelf();
    if (mTypeDef)
        mTypeDef->AddRef();
    if (o.mTypeProps && o.mTypeProps->HasProps()) {
        mTypeProps = new TypeProps(this);
        *mTypeProps = *o.mTypeProps;
    }
}
#endif
