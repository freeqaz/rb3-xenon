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
    static bool SaveObjects(const char *, ObjectDir *, bool);
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
    bool mOwnStream; // 0x30
    BinStream *mStream; // 0x34
    int mRev; // 0x38
    int mCounter; // 0x3c
    ObjPtrList<Hmx::Object> mObjects; // 0x40
    Callback *mCallback; // 0x54
    class ObjectDir *mDir; // 0x58
    bool mPostLoad; // 0x5c
    bool mLoadDir; // 0x5d
    bool mDeleteSelf; // 0x5e
    const char *mProxyName; // 0x60
    int mPad64; // 0x64 - unused padding (dead code from RB2)
    Timer mTimer; // 0x68
    bool mAccessed;
    bool mForceFailCallback;
    bool mHasEditorDir; // 0x9a - gates ReadEditorDirDead in LoadObjs
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
#else
    class ObjectDir *mProxyDir;
#endif

    static bool sCacheMode;
    static PathEvalFunc *sPathEval;
    static TextFileStream *sObjectMemDumpFile;
    static TextFileStream *sTypeMemDumpFile;
    static std::map<String, MemPointDelta> sMemPointMap;
};
