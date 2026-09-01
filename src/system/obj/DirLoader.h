#pragma once
#include "obj/Object.h"
#include "utl/BinStream.h"
#include "utl/FilePath.h"
#include "utl/Loader.h"
#include "utl/MemPoint.h"
#include "utl/PoolAlloc.h"
#include "utl/TextFileStream.h"

typedef bool PathEvalFunc(const char *);

class DirLoader : public Loader, public ObjRefOwner {
    typedef void (DirLoader::*DirLoaderStateFunc)(void);

public:
    struct ClassAndNameSort {
        bool operator()(Hmx::Object *, Hmx::Object *);

    protected:
        int ClassIndex(Hmx::Object *);
    };

    DirLoader(
        const FilePath &,
        LoaderPos,
        Loader::Callback *,
        BinStream *,
        class ObjectDir *,
        bool
#ifdef HX_NATIVE
        ,
        class ObjectDir *
#endif
    );
    virtual ~DirLoader();
    virtual Hmx::Object *RefOwner() const { return nullptr; }
    virtual void Replace(ObjRef *, Hmx::Object *);
    virtual const char *DebugText();
    virtual bool IsLoaded() const;
    virtual const char *StateName() const;

    ObjectDir *GetDir();
    void Cleanup(const char *);

    const char *ProxyName() const { return mProxyName; }
    ObjectDir *ProxyDir() const { return mProxyDir; }
    void SetDeleteSelf(bool set) { mDeleteSelf = set; }
    void SetForceFailCallback(bool b) { mForceFailCallback = b; }

#ifdef HX_NATIVE
    ObjectDir *ParentDir() const { return mParentDir; }
    void SetParentDir(ObjectDir *d) { mParentDir = d; }
#endif

    POOL_OVERLOAD(DirLoader, 0x2A);

    static bool sPrintTimes;
    static ObjectDir *TopSaveDir() { return sTopSaveDir; }
    static void SetCacheMode(bool);
    static void SetPathEvalCallback(bool (*cb)(const char *)) { sPathEval = cb; }

    static Symbol GetDirClass(const char *);
    static const char *CachedPath(const char *, bool);
    static bool ShouldBlockSubdirLoad(const FilePath &);
    // RETAIL-ADJUDICATED arity: ObjectDir::Handle's `save_objects` action calls
    // this with TWO argument registers (r3,r4) and never materialises r5, which
    // is volatile and clobbered by the immediately preceding `bl DataNode::Str`.
    // DC3 (a NEWER engine revision) grew a third `bool` parameter; the rb3-Wii
    // oracle, which is RB3-era, has the two-parameter form. Retail agrees with
    // rb3-Wii.
    static bool SaveObjects(const char *, ObjectDir *);
    static void SaveObjects(BinStream &, ObjectDir *);
    static void WriteTypeMemDump(TextFileStream *);
    static Loader *New(const FilePath &, LoaderPos);
    static DirLoader *Find(const FilePath &);
    static DirLoader *FindLast(const FilePath &);
    static ObjectDir *LoadObjects(const FilePath &, Callback *, BinStream *);
    static void SetPathEvalFunc(PathEvalFunc *func) { sPathEval = func; }

private:
    static ObjectDir *sTopSaveDir;

    virtual void PollLoading() { (this->*mState)(); }

    Symbol FixClassName(Symbol);
    bool SetupDir(Symbol);
    void DumpObjectMemDelta(const Hmx::Object *, const MemPointDelta &) const;
    void AddTypeObjectMemDelta(const Hmx::Object *, const MemPointDelta &) const;

    void LoadObjs();
    void LoadDir();
    void LoadResources();
    void CreateObjects();
    void LoadHeader();
    void OpenFile();
    void DoneLoading() {}

    DirLoaderStateFunc mState; // 0x20
    class String mRoot; // 0x28
    bool mOwnStream; // 0x34
    BinStream *mStream; // 0x38
    int mRev; // 0x3c
    int mCounter; // 0x40
    ObjPtrList<Hmx::Object> mObjects; // 0x44
    Callback *mCallback; // 0x58
    class ObjectDir *mDir; // 0x5c
    bool mPostLoad; // 0x60
    bool mLoadDir; // 0x61
    bool mDeleteSelf; // 0x62
    const char *mProxyName; // 0x64 (compiler-verified)
#ifndef HX_NATIVE
    // 0x68 (compiler-verified). RETAIL-ADJUDICATED, not inferred from the header
    // comments (which are a known lie class): ObjectDir::Handle's `proxy_dir`
    // expression reads `mLoader->mProxyDir` as `lwz r11, 0x68(r11)`. This slot
    // used to be `int mPad64 // unused padding (dead code from RB2)`; it is not
    // padding, it is mProxyDir, exactly as the rb3-Wii oracle has it
    // (mProxyName @0x60 / mProxyDir @0x64 / mTimer @0x68 in Wii's own base
    // layout). sizeof(DirLoader) stays 168 (0xa8) = retail's PoolAlloc size.
    class ObjectDir *mProxyDir; // 0x68
#else
    int mPad64; // 0x68 - native keeps the ObjOwnerPtr mProxyDir below instead
#endif
    Timer mTimer; // 0x70 (compiler-verified)
    bool mAccessed;
    bool mForceFailCallback;
    bool mHasEditorDir; // 0xa2 - gates ReadEditorDirDead in LoadObjs
    bool mSubDir;
#ifdef HX_NATIVE
    // mParentDir + the ObjOwnerPtr-owning mProxyDir below are a native-port-only
    // addition (used by the ObjPtr fallback / FileMerger parent-dir walk under
    // HX_NATIVE — see Dir.cpp/ObjPtr_p.h ParentDir() call sites, all HX_NATIVE-
    // gated). RB3 retail X360 has neither: PoolAlloc's compiled size argument for
    // `new DirLoader(...)` is exactly 0xa8 (168) bytes, which only reconciles once
    // both mParentDir is dropped and mProxyDir reverts to the rb3-Wii oracle's raw
    // (non-owning) `ObjectDir *mProxyDir` — see docs/decomp/research (DirLoader
    // size-probe investigation).
    class ObjectDir *mParentDir; // 0x9c
    ObjOwnerPtr<ObjectDir> mProxyDir; // 0xa0
#endif

    static bool sCacheMode;
    static PathEvalFunc *sPathEval;
    static TextFileStream *sObjectMemDumpFile;
    static TextFileStream *sTypeMemDumpFile;
    static std::map<String, MemPointDelta> sMemPointMap;
};
