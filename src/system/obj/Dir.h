#pragma once
#include "math/Mtx.h"
#include "obj/DirLoader.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "utl/BinStream.h"
#include "utl/FilePath.h"
#include "utl/KeylessHash.h"
#include "utl/Loader.h"
#include "utl/MemMgr.h"
#include "utl/Std.h"
#include "utl/StringTable.h"
#ifdef HX_NATIVE
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

// O(1) HasDirPtrs() — tracks how many ObjDirPtrs reference each object.
// Avoids O(n) ref ring walk that caused O(n²) destructor cascade.
inline std::unordered_map<const void *, int> &DirPtrRefCounts() {
    static std::unordered_map<const void *, int> counts;
    return counts;
}
#endif
#include <vector>

enum InlineDirType {
    kInlineNever = 0,
    kInlineCached = 1,
    kInlineAlways = 2,
    kInlineCachedShared = 3
};

#ifdef HX_NATIVE
static inline bool MiloDebugChooseModeEnabled() {
    static int enabled = -1;
    if (enabled == -1) {
        const char *env = getenv("MILO_DEBUG_CHOOSE_MODE");
        enabled = (env && env[0] && strcmp(env, "0") != 0) ? 1 : 0;
    }
    return enabled != 0;
}

static inline bool MiloDebugChooseModePath(const char *path) {
    return path
        && (strstr(path, "choose_mode") || strstr(path, "HamList")
            || strstr(path, "ham_list") || strstr(path, "hamnav")
            || strstr(path, "ui/choose_mode") || strstr(path, "ui/common"));
}
#endif

// Retail X360: ObjDirPtr is its OWN polymorphic smart pointer, NOT an
// ObjRefConcrete. Layout {vtable@0, mObject@4, mLoader@8} = 0xc — there is NO
// mOwner. The ring-ref is `this` (an ObjRefOwner); AddRef/Release dispatch on
// the *referenced object*'s ring. Verified from the retail binary:
//   - operator= fn_824D77D0: reads mObject@4, mLoader@8 (Release fn_827367D8 /
//     AddRef fn_82737168 with `this` as the ring-ref).
//   - PostLoad helper fn_824D7480: reads mLoader@8.
//   - rb3-Wii oracle (obj/Dir.h:31): `ObjDirPtr : ObjRef { T* mDir; DirLoader*
//     mLoader; }` — vtable-first, mDir first, mLoader after, NO mOwner.
// Vtable (4 slots, from ObjRefOwner): +0 dtor, +4 RefOwner()=>nullptr,
// +8 Replace(from,to), +c IsDirPtr()=>true.
#ifdef HX_NATIVE
template <class C>
class ObjDirPtr : public ObjRefConcrete<C> {
public:
    ObjDirPtr() : ObjRefConcrete(nullptr), mLoader(nullptr) {}
    ObjDirPtr(const ObjDirPtr &o) : ObjRefConcrete<C>(o.mObject), mLoader(nullptr) {
        if (o.mObject) DirPtrRefCounts()[(const void *)o.mObject]++;
    }
    ObjDirPtr(C *);
    virtual ~ObjDirPtr() { *this = nullptr; }
    virtual bool IsDirPtr() { return true; }
    virtual void Replace(Hmx::Object *o) {
        if (!ObjRefConcrete<C>::mObject) {
            // mObject is already null — operator= won't call Release, so
            // manually unlink from ring to prevent infinite ReplaceList loop.
            ObjRef::Release(this);
            return;
        }
        *this = o ? dynamic_cast<C *>(o) : nullptr;
    }
#else
template <class C>
class ObjDirPtr : public ObjRefBase {
protected:
    C *mObject; // 0x4 (immediately after vtable — NO mOwner)
public:
    ObjDirPtr() : mObject(nullptr), mLoader(nullptr) {}
    ObjDirPtr(const ObjDirPtr &o) : mObject(o.mObject), mLoader(nullptr) {
        if (mObject)
            mObject->AddRef(this);
    }
    ObjDirPtr(C *);
    virtual ~ObjDirPtr() { *this = nullptr; }
    // Vtable slot +4: RefOwner() — ObjDirPtr has no owner; returns null
    // (rb3-Wii oracle: `RefOwner() { return 0; }`).
    virtual Hmx::Object *RefOwner() const { return nullptr; }
    // Vtable slot +8: Replace(from, to). from==nullptr => unconditional.
    virtual bool Replace(ObjRef *from, Hmx::Object *o) {
        Hmx::Object *fromObj = reinterpret_cast<Hmx::Object *>(from);
        if (fromObj == nullptr || (Hmx::Object *)mObject == fromObj) {
            *this = o ? dynamic_cast<C *>(o) : nullptr;
        }
        return false;
    }
    // Vtable slot +c: IsDirPtr() => true.
    virtual bool IsDirPtr() { return true; }
#endif

    bool IsLoaded() const;

    ObjDirPtr &operator=(const ObjDirPtr &oPtr) {
        *this = (C *)oPtr;
        return *this;
    }

    ObjDirPtr &operator=(C *dir) {
        if (mLoader && mLoader->IsLoaded())
            PostLoad(nullptr);
        if ((dir != mObject) || !dir) {
            RELEASE(mLoader);
            if (mObject) {
#ifdef HX_NATIVE
                    DirPtrRefCounts()[(const void *)mObject]--;
                    // During cascading ~ObjectDir destruction, ring neighbors
                    // may be freed. Use ASAN-suppressed unlink to properly
                    // remove this node from the ring before mSubDirs storage
                    // disappears.
                    if (ObjectDir::InDeleteObjects())
                        SafeReleaseFromRing(this);
                    else
#endif
                    mObject->Release(this);
                    if (!mObject->HasDirPtrs()) {
#ifdef HX_NATIVE
                            // Virtual inheritance makes Hmx::Object* point to
                            // a subobject offset within the malloc'd block.
                            // Using explicit destructor + free(dynamic_cast<void*>)
                            // avoids ambiguous operator delete in multi-inheritance.
                            void *block = dynamic_cast<void *>(
                                static_cast<Hmx::Object *>(mObject));
                            mObject->~C();
                            // During cascade, defer the free — sibling destructors
                            // may still read from this object's memory.
                            if (ObjectDir::InDeleteObjects())
                                ObjectDir::DeferFree(block);
                            else
                                free(block);
#else
                            delete mObject;
#endif
                    }
            }
            mObject = dir;
            if (mObject) {
                    dir->AddRef(this);
#ifdef HX_NATIVE
                    DirPtrRefCounts()[(const void *)dir]++;
#endif
            }
        }
        return *this;
    }

    operator C *() const { return mObject; }
    C *Ptr() const { return mObject; }
    C *operator->() const {
        MILO_ASSERT(mObject, 0x5F);
        return mObject;
    }
    void PostLoad(Loader *loader) {
        if (mLoader) {
#ifdef HX_NATIVE
            const char *file = mLoader->LoaderFile().c_str();
            bool debug = MiloDebugChooseModeEnabled() && MiloDebugChooseModePath(file);
            if (debug) {
                printf(
                    "DC3 CHOOSE ObjDirPtr::PostLoad begin file='%s' loader=%p current=%p\n",
                    file ? file : "<null>",
                    (void *)mLoader,
                    (void *)mObject
                );
            }
#endif
            TheLoadMgr.PollUntilLoaded(mLoader, loader);
            C *gotten = dynamic_cast<C *>(mLoader->GetDir());
#ifdef HX_NATIVE
            if (debug) {
                Hmx::Object *obj = dynamic_cast<Hmx::Object *>(gotten);
                printf(
                    "DC3 CHOOSE ObjDirPtr::PostLoad end file='%s' resolved=%p class=%s name=%s\n",
                    file ? file : "<null>",
                    (void *)gotten,
                    obj ? obj->ClassName().Str() : "<null>",
                    obj ? obj->Name() : "<null>"
                );
            }
#endif
            mLoader = nullptr;
            *this = gotten;
        }
    }

    void LoadFile(const FilePath &p, bool async, bool share, LoaderPos pos, bool b3) {
        *this = nullptr;
        DirLoader *d = nullptr;
#ifdef HX_NATIVE
        bool debug = MiloDebugChooseModeEnabled() && MiloDebugChooseModePath(p.c_str());
        if (debug) {
            printf(
                "DC3 CHOOSE ObjDirPtr::LoadFile request file='%s' async=%d share=%d pos=%d b3=%d\n",
                p.c_str(),
                async,
                share,
                (int)pos,
                b3
            );
        }
#endif
        if (share) {
            d = DirLoader::Find(p);
            if (d && !d->IsLoaded()) {
                MILO_NOTIFY("Can't share unloaded dir %s", p.c_str());
                d = nullptr;
            }
        }
        if (!d) {
            if (TheLoadMgr.GetLoaderPos() == kLoadStayBack
                || TheLoadMgr.GetLoaderPos() == kLoadFrontStayBack) {
                pos = kLoadFrontStayBack;
            }
            if (!p.empty())
                d = new DirLoader(p, pos, nullptr, nullptr, nullptr, b3, nullptr);
        }
        mLoader = d;
#ifdef HX_NATIVE
        if (debug) {
            printf(
                "DC3 CHOOSE ObjDirPtr::LoadFile loader=%p loaded=%d file='%s'\n",
                (void *)d,
                d ? d->IsLoaded() : 0,
                p.c_str()
            );
        }
#endif
        if (d) {
            if (!async || mLoader->IsLoaded())
                PostLoad(nullptr);
        } else if (!p.empty())
            MILO_NOTIFY("Couldn't load %s", p);
    }

    FilePath &GetFile() const {
        if (mObject && mObject->Loader()) {
            return mObject->Loader()->LoaderFile();
        }
        if (mLoader)
            return mLoader->LoaderFile();
        if (mObject)
            return mObject->StoredFile();
        return FilePath::Null();
    }

    void LoadInlinedFile(const FilePath &fp, BinStream &bs) {
        *this = nullptr;
        LoaderPos pos;
        if (TheLoadMgr.GetLoaderPos() == kLoadStayBack
            || TheLoadMgr.GetLoaderPos() == kLoadFrontStayBack) {
            pos = kLoadFrontStayBack;
        } else {
            pos = kLoadFront;
        }
        mLoader = new DirLoader(fp, pos, nullptr, &bs, nullptr, false, nullptr);
    }

#ifdef HX_NATIVE
    class DirLoader *GetLoader() const { return mLoader; }
#endif

protected:
    class DirLoader *mLoader; // 0x8 (X360) / 0x10 (native)
};

#ifdef HX_NATIVE
template <class C>
ObjDirPtr<C>::ObjDirPtr(C *dir) : ObjRefConcrete<C>(dir), mLoader(nullptr) {
    if (dir) {
        DirPtrRefCounts()[(const void *)dir]++;
    }
}
#else
template <class C>
ObjDirPtr<C>::ObjDirPtr(C *dir) : mObject(dir), mLoader(nullptr) {
    if (mObject)
        mObject->AddRef(this);
}
#endif

template <class C>
bool ObjDirPtr<C>::IsLoaded() const {
    return mObject != nullptr || (mLoader != nullptr && mLoader->IsLoaded());
}

#ifdef HX_NATIVE
template <class C>
BinStream &operator<<(BinStream &bs, const ObjDirPtr<C> &ptr) {
    C *dir = ptr;
    const char *name = dir ? dir->Name() : "";
    bs << name;
    return bs;
}
#else
template <class C>
BinStream &operator<<(BinStream &bs, const ObjDirPtr<C> &ptr);
#endif

template <class T>
BinStream &operator>>(BinStream &bs, ObjDirPtr<T> &ptr) {
    FilePath path;
    bs >> path;
    ptr.LoadFile(path, true, true, kLoadFront, false);
    return bs;
}

class ObjectDir;
#ifdef HX_NATIVE
class MergeFilter;
#endif

// GetExposedProperties is a DC3-only virtual: retail RB3 (and the rb3-Wii dev
// decomp, src/system/obj/Dir.h) has NO such slot in ObjectDir's vbase vtable —
// its presence here pushed SyncObjects/ResetEditorState/InlineSubDirType (and
// every ObjectDir-vbase virtual of every descendant) down one slot. Verified
// against the retail vtable @0x82029D64 (slot 3 = SyncObjects, directly after
// SetSubDir; no GetExposedProperties), which made VocalTrackDir::TrackReset's
// SyncObjects() vcall load +0xc retail vs our +0x10. Gate the `virtual` keyword
// behind HX_NATIVE (same idiom as DRAW_DC3_VIRTUAL); the method stays a normal
// member so the TypeProps.cpp call site still compiles (nothing overrides it, so
// non-virtual dispatch is behavior-identical). See
// docs/decomp/research/2026-06-11-vtable-walls.md.
#ifdef HX_NATIVE
#define DIR_DC3_VIRTUAL virtual
#else
#define DIR_DC3_VIRTUAL
#endif

/**
 * @brief: A directory of Objects.
 * Original _objects description:
 * "An ObjectDir keeps track of a set of Objects.
 * It can subdir or proxy in other ObjectDirs.
 * To rename subdir or proxy files search for remap_objectdirs in
 * system/run/config/objects.dta"
 */
class ObjectDir : public virtual Hmx::Object {
    friend class Hmx::Object;
    friend void MergeObjectsRecurse(ObjectDir *, ObjectDir *, MergeFilter &, bool);
    friend bool PropSyncSubDirs(
        std::vector<ObjDirPtr<ObjectDir> > &subdirs,
        DataNode &val,
        DataArray *prop,
        int i,
        PropOp op
    );

public:
    enum ViewportId {
        kNumViewports = 7
    };

    class Viewport {
    public:
        Transform mXfm;
    };

    /** An Entry of an Object in an ObjectDir, noted by the Object's name and pointer. */
    struct Entry {
        Entry() : name(0), obj(0) {}
        bool operator==(const Entry &e) const { return name == e.name; }
        bool operator!=(const Entry &e) const { return name != e.name; }
        operator const char *() const { return name; }

        const char *name;
        Hmx::Object *obj;
    };

protected:
    struct InlinedDir {
        InlinedDir();
        ~InlinedDir();
        ObjDirPtr<ObjectDir> dir; // 0x0
        FilePath file; // 0x14
        bool shared; // 0x1c
        InlineDirType mType; // 0x20
    };

    KeylessHash<const char *, Entry> mHashTable; // 0x8
    StringTable mStringTable; // 0x28
    FilePath mProxyFile; // 0x3c (X360 String=0xC -> spans 0x3c..0x48)
    bool mProxyOverride; // 0x48 (X360) — String is 0xC so this lands at 0x48
    /** "How is this Proxy inlined?".
        RB3 retail stores this as a single bool (rb3-Wii oracle:
        `bool mInlineProxy; // 0x45`, `AllowsInlineProxy() { return mInlineProxy; }`),
        NOT a 4-byte InlineDirType enum. The enum form is a DC3-era divergence
        that over-sizes ObjectDir by +4. Verified from the retail binary:
        TransferLoaderState (fn_82729200) copies String mProxyFile@0x3c, a single
        byte @0x48, then mLoader @0x4c with NO 4-byte field between them; GetFile
        (fn_82729700) reads mLoader@0x4c and mStoredFile@0x68. Keeping DC3's enum
        for HX_NATIVE because native Dir.cpp uses its multi-value semantics. */
#ifdef HX_NATIVE
    InlineDirType mInlineProxyType; // native only
#else
    bool mInlineProxy; // 0x49 (X360) — packs directly after mProxyOverride@0x48
#endif
    DirLoader *mLoader; // 0x4c
    /** "Subdirectories of objects" */
    std::vector<ObjDirPtr<ObjectDir> > mSubDirs; // 0x50
    /** Is this dir a subdir? */
    bool mIsSubDir; // 0x5c
    /** "How is this inlined as a subdir?  Note that when you change this,
        you must resave everything subdiring this file for it to take effect"
        kInlineNever: "Always share this subdir,
            good for textures and other things you want to share"
        kInlineCached: "Never share this, each dir subdiring this will get its own copy,
            good for layering proxy or venue files for authoring"
        kInlineAlways: "Always inline it, even during non cached saves,
            this is only used for AO computations"
        kInlineCachedShared: "Always inline it, but share it like a normal subdir
            if another one has been loaded" */
    InlineDirType mInlineSubDirType; // 0x60
    /** "where this came from". aka: the path this ObjectDir was loaded from. */
    const char *mPathName; // 0x64
    FilePath mStoredFile; // 0x68 (String=0xC -> 0x68..0x74)
    std::vector<InlinedDir> mInlinedDirs; // 0x74
    std::vector<Viewport> mViewports; // 0x80
    ViewportId mCurViewportID; // 0x8c
    // Secondary object pointer between mCurViewportID and mCurCam, set from
    // FindObject during load (Dir.cpp 1203/1437). PRESENT in RB3 retail — verified
    // from SetCurViewport (fn_82728D00): it writes mCurViewportID@0x8c and
    // mCurCam@0x94 with a 4-byte GAP at 0x90, i.e. this field. (A prior note
    // wrongly excluded it; that, combined with the oversized mInlineProxyType,
    // happened to keep the 0xa0 total but mis-placed every member from 0x48 on.
    // With mInlineProxyType shrunk to a bool AND unk8c restored, the total stays
    // 0xa0 and the internal offsets match: mLoader@0x4c, mStoredFile@0x68,
    // mIsSubDir@0x5c, mCurViewportID@0x8c, mCurCam@0x94.)
    Hmx::Object *unk8c; // 0x90
    Hmx::Object *mCurCam; // 0x94
    int mAlwaysInlined; // 0x90 (X360) / 0x94 (native)
    const char *mAlwaysInlineHash; // 0x94 (X360) / 0x98 (native)

    ObjectDir();
    static ObjectDir *sMainDir;

public:
    // Hmx::Object
    virtual ~ObjectDir();
    OBJ_CLASSNAME(ObjectDir);
    OBJ_SET_TYPE(ObjectDir);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, CopyType);
    virtual void Load(BinStream &);
    virtual void PostSave(BinStream &);
    virtual void SetName(const char *name, ObjectDir *dir) {
        Hmx::Object::SetName(name, dir);
    }
    virtual ObjectDir *DataDir() { return this; }
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    // ObjectDir
    virtual void SetProxyFile(const FilePath &file, bool override);
    virtual const FilePath &ProxyFile() { return mProxyFile; }
    /** Set whether or not this ObjectDir is a subdir. */
    virtual void SetSubDir(bool isSubdir);
    DIR_DC3_VIRTUAL DataArrayPtr GetExposedProperties() { return nullptr; }
    virtual void SyncObjects();
    virtual void ResetEditorState();
    // AllowsInlineProxy is a virtual in retail RB3 (rb3-Wii src/system/obj/Dir.h:
    // between ResetEditorState and InlineSubDirType) — verified at the retail
    // ObjectDir-vbase vtable +0x14 slot (0x822695A8). DC3 demoted it to a plain
    // member; BandCharacter::AllowsInlineProxy already overrides it, so the base
    // must declare it virtual or that override inserts a bogus slot. See
    // docs/decomp/research/2026-06-11-vtable-walls.md.
#ifdef HX_NATIVE
    virtual bool AllowsInlineProxy() { return mInlineProxyType != kInlineNever; }
#else
    virtual bool AllowsInlineProxy() { return mInlineProxy; }
#endif
    virtual InlineDirType InlineSubDirType();

    /** Find an Object of type T in this ObjectDir.
     * @param [in] name The name of the Object to search for.
     * @param [in] fail If true, fail the system if no Object was found.
     */
    template <class T>
    T *Find(const char *name, bool fail = true) {
        T *castedObj = dynamic_cast<T *>(FindObject(name, false));
        if (!castedObj && fail) {
            MILO_FAIL(
                kNotObjectMsg, name, PathName(this) ? PathName(this) : "**no file**"
            );
        }
        return castedObj;
    }

    /** Create a new Object of type T in this ObjectDir.
     * @param [in] name The name of the Object to create.
     * @returns The newly created and named Object.
     */
    template <class T>
    T *New(const char *name) {
        T *obj = Hmx::Object::New<T>();
        if (name)
            obj->SetName(name, this);
        return obj;
    }

    void SetLoader(DirLoader *dl) { mLoader = dl; }
    DirLoader *Loader() const { return mLoader; }
    bool IsProxy() const { return this != Dir(); }
    int HashTableSize() const { return mHashTable.Size(); }
    int StrTableSize() const { return mStringTable.Size(); }
    int HashTableUsedSize() const { return mHashTable.UsedSize(); }
    int StrTableUsedSize() const { return mStringTable.UsedSize(); }
    KeylessHash<const char *, Entry> &HashTable() { return mHashTable; }
    /** Depth-first subdir walk used by ObjDirItr (retail shape, ported from
     * rb3-Wii Dir.cpp). which==0 returns this; otherwise recurses into
     * mSubDirs, decrementing which at each visited dir. */
    ObjectDir *NextSubDir(int &which) {
        if (which == 0)
            return this;
        which--;
        for (int i = 0; i < mSubDirs.size(); i++) {
            if (mSubDirs[i]) {
                ObjectDir *ret = mSubDirs[i]->NextSubDir(which);
                if (ret)
                    return ret;
            }
        }
        return nullptr;
    }
    const char *GetPathName() const { return mPathName; }
    const std::vector<ObjDirPtr<ObjectDir> > &SubDirs() const { return mSubDirs; }
#ifdef HX_NATIVE
    InlineDirType InlineProxyType() const { return mInlineProxyType; }
#else
    // RB3 retail stores a bool. Map it onto the enum API so DC3-sourced callers
    // (Flow.cpp, Utl.cpp) comparing against kInlineAlways still compile.
    InlineDirType InlineProxyType() const {
        return mInlineProxy ? kInlineAlways : kInlineNever;
    }
#endif
    FilePath &StoredFile() { return mStoredFile; }
    bool IsSubDir() const { return mIsSubDir; }
    ObjectDir *ProxyDir() const;
    const char *ProxyName() const;

    void ResetViewports();
    void SetInlineProxyType(InlineDirType);
    /** Allocate space in this ObjectDir's hashtable and stringtable respectively.
     * @param [in] hashSize The desired size of the hash table.
     * @param [in] stringSize The desired size of the string table.
     */
    void Reserve(int hashSize, int stringSize);
    /** Find an Object inside this ObjectDir.
     * @param [in] name The name of the Object to search for.
     * @param [in] parentDirs If true, search the parent ObjectDirs of this ObjectDir.
     * @param [in] subDirs If true, search through this ObjectDir's subdirs.
     * @returns The object, or NULL if it wasn't found.
     */
    Hmx::Object *FindObject(const char *name, bool parentDirs);
    bool InlineProxy(BinStream &);
    bool HasDirPtrs() const;
    void TransferLoaderState(ObjectDir *);
    Viewport &CurViewport();
    bool HasSubDir(ObjectDir *);
    void SaveProxy(BinStream &);
    FilePath GetSubDirPath(const FilePath &, const BinStream &);
    /** Delete all Objects in this ObjectDir. */
    void DeleteObjects();
#ifdef HX_NATIVE
    /** Nonzero when inside DeleteObjects() (may nest via cascading dtors). */
    static bool InDeleteObjects() { return sDeleteObjectsDepth > 0; }
    /** True during MergeDirs — ObjectDir::Copy should skip mSubDirs
     *  because MergeObjectsRecurse handles subdirs separately. */
    static bool InMergeDirs() { return sInMergeDirs; }
    static void SetInMergeDirs(bool v) { sInMergeDirs = v; }
    static void DeferFree(void *block) { sPendingFrees().push_back(block); }
    static void FlushDeferredFrees() {
        auto &v = sPendingFrees();
        if (!v.empty()) {
            for (void *p : v)
                free(p);
            v.clear();
            Hmx::Object::sRingsDirty = true;
        }
    }
    /** Suppress FlushDeferredFrees until EndBatchDelete. Use when multiple
     *  independent cascades run in sequence (e.g. UnloadPanels) so that
     *  memory freed by cascade A isn't reclaimed before cascade B's
     *  NullifyAllRefs can walk rings that reference cascade A's objects. */
    static void BeginBatchDelete() { sSuppressFlush = true; }
    static void EndBatchDelete() {
        sSuppressFlush = false;
        if (sDeleteObjectsDepth == 0)
            FlushDeferredFrees();
    }
private:
    static int sDeleteObjectsDepth;
    static bool sInMergeDirs;
    static bool sSuppressFlush;
    static std::vector<void *> &sPendingFrees() {
        static std::vector<void *> v;
        return v;
    }
public:
#endif
    /** Delete all subdirs of this ObjectDir. */
    void DeleteSubDirs();
    ObjectDir *FindContainingDir(const char *);

    /** Append a subdir to this ObjectDir's list of subdirs.
     * @param [in] subdir The subdir to append.
     */
    void AppendSubDir(const ObjDirPtr<ObjectDir> &subdir);
    /** Remove a subdir from this ObjectDir's list of subdirs.
     * @param [in] subdir The subdir to remove.
     */
    void RemoveSubDir(const ObjDirPtr<ObjectDir> &subdir);

    void SetCurViewport(ViewportId id, Hmx::Object *o);
    Hmx::Object *CurCam() { return mCurCam; }
    void SetSubDirFlag(bool flag);
    /** Set this ObjectDir's path name.
     * @param [in] path The path name to set.
     */
    void SetPathName(const char *path);

    static ObjectDir *Main() { return sMainDir; }
    static void PreInit(int hashSize, int stringSize);
    static void Init();
    static void Terminate();
    NEW_OBJ(ObjectDir);
    OBJ_MEM_OVERLOAD(0x111);

protected:
    /** Routine to perform when an Object has been added to this ObjectDir. */
    virtual void AddedObject(Hmx::Object *);
    /** Routine to perform when an Object is being removed from this ObjectDir. */
    virtual void RemovingObject(Hmx::Object *);
    virtual void OldLoadProxies(BinStream &, int);

    /** Can we save our subdirs? */
    bool SaveSubdirs();
    bool ShouldSaveProxy(BinStream &);
    /** Find the Object Entry in this ObjectDir.
     * @param [in] name The name of the Object to search for.
     * @param [in] add If true, add a new Entry if one was not found.
     * @returns The Object Entry.
     */
    Entry *FindEntry(const char *name, bool add);
    void SaveInlined(const FilePath &, bool, InlineDirType);
    void PreLoadInlined(const FilePath &, bool, InlineDirType);
    void LoadSubDir(int idx, const FilePath &, BinStream &, bool);
    /** Routine to perform when a subdir has been added to the ObjectDir's subdir list. */
    void AddedSubDir(ObjDirPtr<ObjectDir> &subdir);
    /** Routine to perform when a subdir is being removed from the ObjectDir's subdir
     * list. */
    void RemovingSubDir(ObjDirPtr<ObjectDir> &subdir);
    void Iterate(DataArray *, bool);
    ObjDirPtr<ObjectDir> PostLoadInlined();

    /** Handler to search for an Object in this ObjectDir.
     * @param [in] arr The supplied DataArray.
     * Expected DataArray contents:
     *     Node 2: The name of the object to search for, in string form.
     *     Node 3: if true, fail if the desired object was not found.
     * @returns A DataNode housing the found Object.
     * Example usage: {$this find "your_object.ext" TRUE}
     */
    DataNode OnFind(DataArray *arr);
};

extern const char *kNotObjectMsg;

/** Iterates through each Object in an ObjectDir that is of type T.
 * Retail RB3 shape (== rb3-Wii): flat 0x14-byte iterator walking subdirs via
 * ObjectDir::NextSubDir — NOT the DC3 std::list-based recursive collector. */
template <class T>
class ObjDirItr {
public:
    /** Create an ObjDirItr (ObjectDir iterator).
     @param [in] dir The ObjectDir we're iterating inside.
     @param [in] recurse If true, we want to iterate through the ObjectDir's subdirs too.
     */
    ObjDirItr(ObjectDir *dir, bool recurse)
        : mDir(recurse ? dir : nullptr), mSubDir(dir), mWhich(0) {
        if (dir) {
            mEntry = dir->HashTable().Begin();
            Advance();
        } else {
            mObj = nullptr;
            mEntry = nullptr;
        }
    }

    ObjDirItr &operator++() {
        if (mEntry) {
            mEntry = mSubDir->HashTable().Next(mEntry);
            Advance();
        }
        return *this;
    }

    operator T *() { return mObj; }
    T *operator->() { return mObj; }

private:
    void Advance() {
        while (mEntry) {
#ifdef HX_NATIVE
            // During DeleteObjects, ~Object() nulls entry->obj via RemoveFromDir().
            // Skip null entries so we never touch freed memory.
            if (mEntry->obj)
#endif
            {
                mObj = dynamic_cast<T *>(mEntry->obj);
                if (mObj)
                    return;
            }
            mEntry = mSubDir->HashTable().Next(mEntry);
        }
        if (mDir) {
            int nextwhich = ++mWhich;
            mSubDir = mDir->NextSubDir(nextwhich);
            if (mSubDir) {
                mEntry = mSubDir->HashTable().Begin();
                Advance();
                return;
            }
        }
        mObj = nullptr;
    }

    /** Root dir when recursing, null otherwise. */
    ObjectDir *mDir; // 0x0
    /** The dir currently being iterated. */
    ObjectDir *mSubDir; // 0x4
    /** The current ObjectDir::Entry in the iterator. */
    ObjectDir::Entry *mEntry; // 0x8
    /** The current object in the iterator. */
    T *mObj; // 0xc
    /** Depth-first subdir cursor fed to NextSubDir. */
    int mWhich; // 0x10
};

void PreloadSharedSubdirs(Symbol s);
