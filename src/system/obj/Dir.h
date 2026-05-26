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

template <class C>
class ObjDirPtr : public ObjRefConcrete<C> {
public:
    ObjDirPtr() : ObjRefConcrete(nullptr), mLoader(nullptr) {}
    ObjDirPtr(C *);
    ObjDirPtr(const ObjDirPtr &o) : ObjRefConcrete<C>(o.mObject), mLoader(nullptr) {
#ifdef HX_NATIVE
        if (o.mObject) DirPtrRefCounts()[(const void *)o.mObject]++;
#endif
    }
    virtual ~ObjDirPtr() { *this = nullptr; }
    virtual bool IsDirPtr() { return true; }
    virtual void Replace(Hmx::Object *o) {
#ifdef HX_NATIVE
        if (!ObjRefConcrete<C>::mObject) {
            // mObject is already null — operator= won't call Release, so
            // manually unlink from ring to prevent infinite ReplaceList loop.
            ObjRef::Release(this);
            return;
        }
#else
        MILO_ASSERT(ObjRefConcrete<C>::mObject, 0x70);
#endif
        *this = o ? dynamic_cast<C *>(o) : nullptr;
    }

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
    C *operator->() const {
        MILO_ASSERT(ObjRefConcrete<C>::mObject, 0x5F);
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
    class DirLoader *mLoader; // 0x10
};

template <class C>
ObjDirPtr<C>::ObjDirPtr(C *dir) : ObjRefConcrete<C>(dir), mLoader(nullptr) {
#ifdef HX_NATIVE
    if (dir) {
        DirPtrRefCounts()[(const void *)dir]++;
    }
#endif
}

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
    FilePath mProxyFile; // 0x3c
    bool mProxyOverride; // 0x44
    /** "How is this Proxy inlined?  Note that when you change this,
        you must resave everything subdiring this file for it to take effect"
        kInlineNever: "Never inline this, this is the default value"
        kInlineCached: "Inline it during cached saves"
        kInlineAlways: "Always inline it, even during non cached saves" */
    InlineDirType mInlineProxyType; // 0x48
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
    FilePath mStoredFile; // 0x68
    std::vector<InlinedDir> mInlinedDirs; // 0x70
    std::vector<Viewport> mViewports; // 0x7c
    ViewportId mCurViewportID; // 0x88
    Hmx::Object *unk8c; // 0x8c
    Hmx::Object *mCurCam; // 0x90
    int mAlwaysInlined; // 0x94 / -0xC
    const char *mAlwaysInlineHash; // 0x98

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
    virtual DataArrayPtr GetExposedProperties() { return nullptr; }
    virtual void SyncObjects();
    virtual void ResetEditorState();
    virtual InlineDirType InlineSubDirType();

    /** Find an Object of type T in this ObjectDir.
     * @param [in] name The name of the Object to search for.
     * @param [in] fail If true, fail the system if no Object was found.
     */
    template <class T>
    T *Find(const char *name, bool fail = true) {
        T *castedObj = dynamic_cast<T *>(FindObject(name, false, true));
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
    const char *GetPathName() const { return mPathName; }
    const std::vector<ObjDirPtr<ObjectDir> > &SubDirs() const { return mSubDirs; }
    InlineDirType InlineProxyType() const { return mInlineProxyType; }
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
    Hmx::Object *FindObject(const char *name, bool parentDirs, bool subDirs);
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

/** Iterates through each Object in an ObjectDir that is of type T. */
template <class T>
class ObjDirItr {
private:
    void Advance() {
        for (; mEntry != nullptr; mEntry = mSubDirs.front()->HashTable().Next(mEntry)) {
#ifdef HX_NATIVE
            // During DeleteObjects, ~Object() nulls entry->obj via RemoveFromDir().
            // Skip null entries so we never touch freed memory.
            if (!mEntry->obj)
                continue;
#endif
            mObj = dynamic_cast<T *>(mEntry->obj);
            if (mObj)
                return;
        }
        if (mSubDirs.size() != 0) {
            mSubDirs.pop_front();
            if (mSubDirs.size() != 0) {
                mEntry = mSubDirs.front()->HashTable().Begin();
                Advance();
                return;
            }
        }
        mObj = nullptr;
    }
    void RecurseSubdirs(ObjectDir *dir) {
        if (dir && std::find(mSubDirs.begin(), mSubDirs.end(), dir) == mSubDirs.end()) {
            mSubDirs.push_back(dir);
            for (int i = 0; i < dir->SubDirs().size(); i++) {
                RecurseSubdirs(dir->SubDirs()[i]);
            }
        }
    }

    /** The current ObjectDir::Entry in the iterator. */
    ObjectDir::Entry *mEntry; // 0x0
    /** The current object in the iterator. */
    T *mObj; // 0x4
    /** All the subdirs we need to iterate through. */
    std::list<ObjectDir *> mSubDirs; // 0x8

public:
    /** Create an ObjDirItr (ObjectDir iterator).
     @param [in] dir The ObjectDir we're iterating inside.
     @param [in] recurse If true, we want to iterate through the ObjectDir's subdirs too.
     */
    ObjDirItr(ObjectDir *dir, bool recurse) {
        if (dir) {
            if (recurse) {
                RecurseSubdirs(dir);
            } else {
                mSubDirs.push_back(dir);
            }
            mEntry = mSubDirs.front()->HashTable().Begin();
            Advance();
        } else {
            mObj = nullptr;
            mEntry = nullptr;
        }
    }
    ObjDirItr &operator++() {
        if (mEntry) {
            mEntry = mSubDirs.front()->HashTable().Next(mEntry);
            Advance();
        }
        return *this;
    }

    operator T *() { return mObj; }
    T *operator->() { return mObj; }
};

void PreloadSharedSubdirs(Symbol s);
