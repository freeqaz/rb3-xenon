#include "obj/Object.h"
#include "Dir.h"
#include "Msg.h"
#include "Object.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "obj/Utl.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/OSFuncs.h"
#include "os/Platform.h"
#include "os/System.h"
#include "utl/BinStream.h"
#include "utl/Symbol.h"

#ifdef HX_NATIVE
#include <vector>
Hmx::Object *Hmx::Object::sDeleting;
bool Hmx::Object::sRingsDirty = false;
bool gInReplaceList = false;

// Check if an ObjRef's alive sentinel is still set. Reads potentially freed
// memory during cascading destruction — suppress ASAN for this specific check.
// Under glibc, freed memory is typically zeroed → sentinel reads as 0 → dead.
// Snapshot ring entries into a vector, skipping freed nodes.
// During cascading ObjectDir destruction, some ObjRefs may be freed but still
// linked in the ring (~ObjRefConcrete skipped Release). Their mAliveSentinel
// was cleared by ~ObjRef(). We read the sentinel and next pointer from these
// freed nodes — suppress ASAN since the memory is quarantined but readable.
#if defined(__SANITIZE_ADDRESS__) || (defined(__has_feature) && __has_feature(address_sanitizer))
__attribute__((noinline, no_sanitize("address")))
#endif
static void SnapshotRing(ObjRef *sentinel, std::vector<ObjRef *> &out) {
    constexpr size_t kSentinelOffset = 3 * sizeof(void *);
    constexpr size_t kNextOffset = sizeof(void *);
    constexpr size_t kMaxRingSize = 100000; // safety limit
    ObjRef *first = *(ObjRef **)((const char *)sentinel + kNextOffset);
    size_t count = 0;
    for (ObjRef *it = first; it != sentinel; ) {
        if ((uintptr_t)it < 0x10000)
            break;
        if (++count > kMaxRingSize) {
            MILO_LOG("SnapshotRing: RING CORRUPTION — walked %zu nodes without returning to sentinel %p (first=%p, cur=%p)\n",
                count, (void*)sentinel, (void*)first, (void*)it);
            break;
        }
        // Read mAliveSentinel to check if node was freed
        uint32_t alive = *(const uint32_t *)((const char *)it + kSentinelOffset);
        ObjRef *nextNode = *(ObjRef **)((const char *)it + kNextOffset);
        if (alive == 0xCAFEBABE)
            out.push_back(it);
        it = nextNode;
    }
}

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
std::map<Symbol, ObjectFunc *> Hmx::Object::sFactories;
DataArrayPtr gPropPaths[8] = {
    DataArrayPtr(new DataArray(1)), DataArrayPtr(new DataArray(1)),
    DataArrayPtr(new DataArray(1)), DataArrayPtr(new DataArray(1)),
    DataArrayPtr(new DataArray(1)), DataArrayPtr(new DataArray(1)),
    DataArrayPtr(new DataArray(1)), DataArrayPtr(new DataArray(1))
};
MsgSinks gSinks(nullptr);

#ifndef HX_NATIVE
// RB3 retail X360: Hmx::Object is 0x28 bytes (verified from ctor lbl_82737FE8 /
// dtor fn_82738050): vtable@0, TypeProps(inline,0xc)@4, mTypeDef@10,
// mNote(const char*)@14, mName@18, mDir@1c, mRefs(8B ring)@20.
// dc3-decomp uses a larger layout (0x2c, pointer TypeProps, String mNote).
static_assert(sizeof(Hmx::Object) == 0x28, "Hmx::Object must be 0x28 (RB3 retail)");
static_assert(sizeof(ObjRefNode) == 0xc, "ObjRefNode must be 0xc (RB3 retail)");

#include "utl/PoolAlloc.h"

// Allocate a 0xc ring node {next,prev,refPtr} and splice it just after `head`,
// pointing back at `ref`. Mirrors retail fn_8271EAE0 / fn_82262360 / fn_82737168.
void ObjRingInsert(ObjRef *head, ObjRefOwner *ref) {
    ObjRefNode *node =
        (ObjRefNode *)PoolAlloc(sizeof(ObjRefNode), sizeof(ObjRefNode));
    node->refPtr = ref;
    node->next = head->next;
    node->prev = head;
    head->next->prev = node;
    head->next = node;
}

// Free every 0xc node in the ring and reset the head to self-loop.
// Mirrors retail fn_82451A48.
void ObjRingFree(ObjRef *head) {
    ObjRef *it = head->next;
    while (it != head) {
        ObjRef *nxt = it->next;
        PoolFree(sizeof(ObjRefNode), it);
        it = nxt;
    }
    head->next = head;
    head->prev = head;
}

void Hmx::Object::AddRef(ObjRefOwner *ref) {
    if (ref->RefOwner() != this)
        ObjRingInsert(&mRefs, ref);
}

void Hmx::Object::Release(ObjRefOwner *ref) {
    if (this != sDeleting && ref->RefOwner() != this) {
        for (ObjRef *it = mRefs.next; it != &mRefs; it = it->next) {
            if (RefPtrOf(it) == ref) {
                it->prev->next = it->next;
                it->next->prev = it->prev;
                PoolFree(sizeof(ObjRefNode), it);
                return;
            }
        }
    }
}

// X360 ring helpers used by ObjRefRelinkRing/ReplaceList paths.
void ObjRef::ReplaceList(Hmx::Object *obj) {
    while (next != this) {
        ObjRef *cur = next;
        ObjRefOwner *ref = RefPtrOf(cur);
        cur->prev->next = cur->next;
        cur->next->prev = cur->prev;
        PoolFree(sizeof(ObjRefNode), cur);
        ref->Replace(nullptr, obj);
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
    mRefs.DetachSelf();
}

Hmx::Object::~Object() {
    MILO_ASSERT_FMT(MainThread(), "Can't delete objects outside of the main thread");
    if (mTypeDef) {
        mTypeDef->Release();
        mTypeDef = nullptr;
    }
    ClearAllTypeProps();
    RemoveFromDir();
#ifdef HX_NATIVE
    RELEASE(mSinks);
#endif
    Hmx::Object *old = sDeleting;
    sDeleting = this;
#ifdef HX_NATIVE
    // During cascading ObjectDir::DeleteObjects, ReplaceRefs is called
    // in a separate pre-pass while all memory is still valid. Calling it
    // here would be unsafe: derived member destructors have already freed
    // ObjPtrVec buffers, leaving stale ring entries that crash SnapshotRing.
    if (!ObjectDir::InDeleteObjects())
        ReplaceRefs(nullptr);
    sDeleting = old;
#else
    // Retail X360 (fn_82738050): walk mRefs, dispatch Replace(this, 0) on each
    // node's ring-ref (vtable slot +8), then free all 0xc pool nodes.
    for (ObjRef *it = mRefs.next; it != &mRefs; it = it->next) {
        RefPtrOf(it)->Replace(reinterpret_cast<ObjRef *>(this), nullptr);
    }
    sDeleting = old;
    ObjRingFree(&mRefs);
#endif
    if (gDataThis == this) {
        gDataThis = nullptr;
    }
}

bool Hmx::Object::Replace(ObjRef *from, Hmx::Object *to) {
#ifdef HX_NATIVE
    if (mSinks)
        return mSinks->Replace(from, to);
#endif
    return false;
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
    HANDLE(get_types_list, OnGetTypeList)
    HANDLE_ARRAY(mTypeDef)
    HANDLE(add_sink, OnAddSink)
    HANDLE(remove_sink, OnRemoveSink)
    Export(_msg, false);
END_HANDLERS

BEGIN_PROPSYNCS(Hmx::Object)
    SYNC_PROP_SET(name, mName, SetName(_val.Str(), mDir))
    SYNC_PROP_SET(type, Type(), SetType(_val.Sym()))
#ifdef HX_NATIVE
    SYNC_PROP(sinks, mSinks ? *mSinks : gSinks)
#else
    SYNC_PROP(sinks, gSinks)
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
    // Retail X360: TypeProps is inline, mNote is const char*
    if (!mTypeProps.HasProps())
        bs << (DataArray *)nullptr;
    else
        mTypeProps.Save(bs);
    if (!mNote || mNote == gNullStr || bs.Cached())
        bs << 0;
    else
        bs << mNote;
#endif
}

void Hmx::Object::Copy(const Hmx::Object *o, CopyType ty) {
    if (ty != kCopyFromMax) {
        mNote = o->Note();
        if (ClassName() == o->ClassName()) {
            SetTypeDef(o->TypeDef());
#ifdef HX_NATIVE
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
#else
            // Retail X360: TypeProps is inline, copy directly
            if (o->HasTypeProps() || mTypeProps.HasProps()) {
                mTypeProps = o->mTypeProps;
            }
#endif
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

void Hmx::Object::Load(BinStream &bs) {
    LoadType(bs);
    LoadRest(bs);
}

INIT_REVS(2, 0)

void Hmx::Object::LoadType(BinStream &bs) {
    LOAD_REVS(bs)
    ASSERT_REVS(2, 0)
    Symbol s;
    bs >> s;
    SetType(s);
    bs.PushRev(packRevs(d.altRev, d.rev), this);
}

void Hmx::Object::LoadRest(BinStream &bs) {
    BinStreamRev d(bs, bs.PopRev(this));
#ifdef HX_NATIVE
    if (!mTypeProps) {
        mTypeProps = new TypeProps(this);
    }
    mTypeProps->Load(d);
    if (!mTypeProps->HasProps()) {
        RELEASE(mTypeProps);
    }
    if (d.rev > 0) {
        d >> mNote;
    }
#else
    // Retail X360: TypeProps is inline
    mTypeProps.Load(d);
    if (d.rev > 0) {
        // Read string into a String, then allocate persistent copy.
        // Retail stores mNote as const char* into a memory pool.
        String noteStr;
        d >> noteStr;
        if (!noteStr.empty()) {
            unsigned int len = noteStr.length() + 1;
            char *buf = (char *)MemAlloc(len, __FILE__, __LINE__, "Object::mNote", 0);
            memcpy(buf, noteStr.c_str(), len);
            mNote = buf;
        }
    }
#endif
}

void Hmx::Object::Export(DataArray *a, bool b) {
    if (b)
        HandleType(a);
#ifdef HX_NATIVE
    if (mSinks)
        mSinks->Export(a);
#endif
}

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

const char *Hmx::Object::FindPathName() {
    const char *name = (mName && *mName) ? mName : ClassName().Str();

    ObjectDir *dataDir = DataDir();
    if (dataDir) {
        DirLoader *loader = dataDir->Loader();
        if (loader) {
            return MakeString(
                "%s (%s)",
                name,
                FileLocalize(loader->LoaderFile().c_str(), nullptr)
            );
        } else if (!dataDir->ProxyFile().empty()) {
            return MakeString(
                "%s (%s)", name, FileLocalize(dataDir->ProxyFile().c_str(), nullptr)
            );
        } else if (*dataDir->GetPathName() != '\0') {
            return MakeString(
                "%s (%s)", name, FileLocalize(dataDir->GetPathName(), nullptr)
            );
        } else if (dataDir != this && dataDir->Name() && *dataDir->Name()) {
            return MakeString("%s/%s", dataDir->Name(), name);
        } else if (mDir && *mDir->GetPathName()) {
            return MakeString("%s (%s)", name, FileLocalize(mDir->GetPathName(), nullptr));
        }
    }
    return name;
}

#pragma region Ref Methods

void Hmx::Object::ReplaceRefs(Hmx::Object *obj) {
    if (mRefs.begin() != mRefs.end()) {
#ifdef HX_NATIVE
        // Snapshot approach: copy ring entries to a vector, then iterate.
        // Immune to ring modifications during Replace callbacks (owners may
        // delete ObjRefs, modify other ring entries, or trigger cascading
        // destructions). The mAliveSentinel field (set in ObjRef constructor,
        // cleared in ~ObjRef) detects freed entries in the snapshot.
        bool wasInReplace = gInReplaceList;
        gInReplaceList = true;
        std::vector<ObjRef *> snapshot;
        SnapshotRing(&mRefs, snapshot);
        mRefs.Clear();
        for (ObjRef *ref : snapshot) {
            // Self-loop each ref so that Release() inside SetObj() writes
            // to itself (harmless) instead of to ring prev/next neighbors
            // that may reside in already-freed objects. The ring has been
            // cleared above, so maintaining ring structure is unnecessary.
            ref->next = ref;
            ref->prev = ref;
            ref->Replace(obj);
        }
        gInReplaceList = wasInReplace;
#else
        // Retail X360: walk the pool-node ring; dispatch Replace(this, obj) on
        // each ring-ref (vtable slot +8), then free the node.
        for (ObjRef *it = mRefs.next; it != &mRefs;) {
            ObjRef *nxt = it->next;
            ObjRefOwner *ref = RefPtrOf(it);
            it->prev->next = it->next;
            it->next->prev = it->prev;
            PoolFree(sizeof(ObjRefNode), it);
            ref->Replace(reinterpret_cast<ObjRef *>(this), obj);
            it = nxt;
        }
#endif
    }
}

#ifdef HX_NATIVE
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
    while (cur != sentinel) {
        if ((uintptr_t)cur < 0x10000 || ++count > kMaxRingSize)
            break;
        // Read next pointer before potentially dead memory is reused.
        // Dead ObjRefs (from freed ObjPtrVec buffers during cascade) are
        // still linked in the ring — skip them, but keep walking via their
        // next pointer (same technique as SnapshotRing).
        ObjRef *nxt = *(ObjRef **)((const char *)cur + kNextOffset);
        uint32_t alive = *(const uint32_t *)((const char *)cur + kSentinelOffset);
        if (alive == ObjRef::kAliveSentinel)
            cur->NullifyObj();
        cur = nxt;
    }
    sentinel->next = sentinel;
    sentinel->prev = sentinel;
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
    // Retail X360: ring entries are pool nodes; the ring-ref's RefOwner()
    // identifies `from`. For each matching node, dispatch Replace(from, to) on
    // the ring-ref and free the node (the ref re-AddRefs `to` via SetObj).
    for (ObjRef *it = mRefs.next; it != &mRefs;) {
        ObjRef *nxt = it->next;
        ObjRefOwner *ref = RefPtrOf(it);
        if (ref->RefOwner() == from) {
            it->prev->next = it->next;
            it->next->prev = it->prev;
            PoolFree(sizeof(ObjRefNode), it);
            ref->Replace(reinterpret_cast<ObjRef *>(from), to);
        }
        it = nxt;
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

void Hmx::Object::AddSink(Hmx::Object *o, Symbol s1, Symbol s2, SinkMode sm, bool b) {
#ifdef HX_NATIVE
    GetOrAddSinks()->AddSink(o, s1, s2, sm, b);
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

DataArray *GetNextPropPath() {
    for (int i = 0; i < DIM(gPropPaths); i++) {
        if (gPropPaths[i]->RefCount() == 1) {
            return gPropPaths[i];
        }
    }
    MILO_FAIL("Recursive SetProperty call count greater than %d!", 8);
    return nullptr;
}

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
        MILO_FAIL_DTA("%s: property %s not found", PathName(this), PrintPropertyPath(prop));
    }
    return nullptr;
}

const DataNode *Hmx::Object::Property(Symbol prop, bool fail) const {
    static DataArrayPtr d(new DataArray(1));
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
    static DataArrayPtr d(new DataArray(1));
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
            MILO_FAIL_DTA("%s: property %s not found", PathName(this), name);
#ifdef HX_NATIVE
            return 0; // MILO_FAIL_DTA warns on native, so we must bail before null deref
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
            mTypeProps->RemoveArrayValue(prop->Sym(0), prop->Int(1));
        }
#else
        mTypeProps.RemoveArrayValue(prop->Sym(0), prop->Int(1));
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

void Hmx::Object::SetProperty(DataArray *prop, const DataNode &val) {
    const DataNode *prop_n = nullptr;
    DataNode n;
    Symbol handler;
#ifdef HX_NATIVE
    if (mSinks) {
        handler = mSinks->GetPropSyncHandler(prop);
        if (!handler.Null()) {
            prop_n = Property(prop, false);
            if (prop_n) {
                n = *prop_n;
            }
        }
    }
#endif
    if (!SyncProperty((DataNode &)val, prop, 0, kPropSet)) {
        Symbol key = prop->Sym(0);
#ifdef HX_NATIVE
        if (!mTypeProps) {
            mTypeProps = new TypeProps(this);
        }
        if (prop->Size() == 1) {
            mTypeProps->SetKeyValue(key, val, true);
        } else {
            MILO_ASSERT(prop->Size() == 2, 0x1C4);
            mTypeProps->SetArrayValue(key, prop->Int(1), val);
        }
#else
        if (prop->Size() == 1) {
            mTypeProps.SetKeyValue(key, val, true);
        } else {
            MILO_ASSERT(prop->Size() == 2, 0x1C4);
            mTypeProps.SetArrayValue(key, prop->Int(1), val);
        }
#endif
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

void Hmx::Object::SetProperty(Symbol prop, const DataNode &val) {
    DataArray *path = GetNextPropPath();
    path->AddRef();
    path->Node(0) = prop;
    SetProperty(path, val);
    path->Release();
}

void Hmx::Object::InsertProperty(DataArray *prop, const DataNode &val) {
    if (!SyncProperty((DataNode &)val, prop, 0, kPropInsert)) {
        MILO_ASSERT(prop->Size() == 2, 0x240);
#ifdef HX_NATIVE
        if (!mTypeProps) {
            mTypeProps = new TypeProps(this);
        }
        mTypeProps->InsertArrayValue(prop->Sym(0), prop->Int(1), val);
#else
        mTypeProps.InsertArrayValue(prop->Sym(0), prop->Int(1), val);
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

void Hmx::Object::SetNote(const char *note) { mNote = note; }

void Hmx::Object::RemoveFromDir() {
    if (mDir && mDir != sDeleting) {
        mDir->RemovingObject(this);
        ObjectDir::Entry *entry = mDir->FindEntry(mName, false);
        if (!entry || entry->obj != this) {
            MILO_FAIL("No entry for %s in %s", PathName(this), PathName(mDir));
        }

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
        MessageTimer timer(this, t);
        return handler->ExecuteScript(1, this, (const DataArray *)msg, 2);
    }
    return DATA_UNHANDLED;
}

#pragma endregion
#pragma region Handlers

DataNode Hmx::Object::OnIterateRefs(const DataArray *da) {
    DataNode *var = da->Var(2);
    DataNode node(*var);
    ObjRef *end = &mRefs;
    for (ObjRef *it = mRefs.next; it != end;) {
        ObjRef *next_it = it->next;
#ifdef HX_NATIVE
        *var = it->RefOwner();
#else
        *var = RefPtrOf(it)->RefOwner();
#endif
        for (int i = 3; i < da->Size(); i++) {
            da->Command(i)->Execute();
        }
        it = next_it;
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
                    AddSink(obj, s7, s6, mode, chain);
                }
            }
        }
    } else {
        Symbol s1, s2;
        Hmx::Object *obj = a->GetObj(2);
        AddSink(obj, s1, s2, kHandle, true);
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
            node.Print(str, true, 0);
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
                n.Print(str, true, 0);
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

DataNode Hmx::Object::OnPropertyAppend(const DataArray *da) {
    DataArray *arr = da->Array(2);
    int size = PropertySize(arr);
    DataArray *cloned = arr->Clone(true, false, 1);
    cloned->Node(cloned->Size() - 1) = size;
    InsertProperty(cloned, da->Evaluate(3));
    cloned->Release();
    return size;
}
