#include "obj/Dir.h"
#include "Dir.h"

#include "Msg.h"
#include "Object.h"
#include "Utl.h"
#include "math/Mtx.h"
#include "math/Rot.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "obj/DataUtl.h"
#include "obj/DirLoader.h"
#include "obj/DirUnloader.h"
#include "obj/Object.h"
#include "obj/PropSync.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/System.h"
#include "utl/FilePath.h"
#include "utl/Loader.h"
#include "utl/MemMgr.h"
#include "utl/Option.h"
#include "utl/Std.h"
#include "utl/Symbol.h"
#include "utl/BinStream.h"
#ifdef HX_NATIVE
#include <cstdlib>
#endif
#include <algorithm>
#include <utility>

const char *kNotObjectMsg = "Could not find %s in dir \"%s\"";
ObjectDir *ObjectDir::sMainDir;
ObjectDir *gDir;
static std::map<std::pair<Symbol, Symbol>, bool> sSuperClassMap;

#ifdef HX_NATIVE
namespace {
    void CollectCascadeDirs(ObjectDir *dir, std::vector<ObjectDir *> &out) {
        if (!dir || std::find(out.begin(), out.end(), dir) != out.end())
            return;
        out.push_back(dir);
        for (int i = 0; i < dir->SubDirs().size(); i++) {
            CollectCascadeDirs(dir->SubDirs()[i], out);
        }
        for (ObjDirItr<Hmx::Object> it(dir, false); it != nullptr; ++it) {
            ObjectDir *asDir = dynamic_cast<ObjectDir *>(&*it);
            if (asDir && asDir != dir)
                CollectCascadeDirs(asDir, out);
        }
    }
}
// Check whether an ObjectDir has DirPtrs from OUTSIDE the cascade tree.
// Internal DirPtrs (from parent dir's mSubDirs within the cascade) don't
// count — those will be destroyed. Only external DirPtrs (from dirs not
// in the cascade) indicate the object should survive.
static bool HasExternalDirPtrs(ObjectDir *candidate, const std::vector<ObjectDir *> &cascade) {
    auto &counts = DirPtrRefCounts();
    auto it = counts.find((const void *)candidate);
    if (it == counts.end() || it->second <= 0)
        return false;
    int totalDirPtrs = it->second;
    // Count internal DirPtrs: appearances in cascade dirs' SubDirs lists
    int internalDirPtrs = 0;
    for (size_t ci = 0; ci < cascade.size(); ci++) {
        for (int si = 0; si < (int)cascade[ci]->SubDirs().size(); si++) {
            if ((ObjectDir *)cascade[ci]->SubDirs()[si] == candidate)
                internalDirPtrs++;
        }
    }
    return totalDirPtrs > internalDirPtrs;
}

// Check whether an object should be excluded from the cascade.
static bool ShouldSkipCascadeNullify(Hmx::Object *obj, const std::vector<ObjectDir *> &cascade) {
    ObjectDir *asDir = dynamic_cast<ObjectDir *>(obj);
    return asDir && HasExternalDirPtrs(asDir, cascade);
}
#endif

#pragma region Virtual Methods

ObjectDir::ObjectDir()
    : mHashTable(0, Entry(), Entry(), 0), mStringTable(0), mProxyOverride(false),
      mInlineProxyType(kInlineCached), mLoader(nullptr), mIsSubDir(false),
      mInlineSubDirType(kInlineNever), mPathName(gNullStr), mViewports(7),
      mCurViewportID((ViewportId)0), unk8c(nullptr), mCurCam(nullptr), mAlwaysInlined(0),
      mAlwaysInlineHash(gNullStr) {
    ResetViewports();
}

ObjectDir::~ObjectDir() {
#ifdef HX_NATIVE
    // Track destruction depth so ~Object, ~ObjRefConcrete, ObjDirPtr, FlowNode,
    // and Sequence can skip ring operations during cascading teardown.
    sDeleteObjectsDepth++;
    // At the outermost level, nullify ALL dir/object refs recursively before
    // mSubDirs.clear() destroys sub-dir ObjPtr storage. This keeps external
    // ObjDirPtrs and object refs consistent while the subdir vector buffers
    // are still valid.
    if (sDeleteObjectsDepth == 1 && !TheLoadMgr.AsyncUnload()) {
        std::vector<ObjectDir *> allDirs;
        CollectCascadeDirs(this, allDirs);
        for (size_t i = 0; i < allDirs.size(); i++) {
            // Skip dirs that have EXTERNAL DirPtrs — they were reparented
            // and will survive. Nullifying their refs would break external code.
            if (allDirs[i] != this && ShouldSkipCascadeNullify(allDirs[i], allDirs))
                continue;
            if (allDirs[i]->IsRefAlive())
                allDirs[i]->NullifyAllRefs();
            for (ObjDirItr<Hmx::Object> it(allDirs[i], false); it != nullptr; ++it) {
                Hmx::Object *obj = it;
                if (obj == allDirs[i])
                    continue;
                // Skip objects that have EXTERNAL DirPtrs. They will
                // survive this dir's destruction and need their refs intact.
                // Detach them from this dir so ~Object::RemoveFromDir()
                // won't access freed memory when they are eventually deleted.
                if (ShouldSkipCascadeNullify(obj, allDirs)) {
                    obj->DetachFromDir();
                    continue;
                }
                if (obj->IsRefAlive())
                    obj->NullifyAllRefs();
            }
        }
    }
#endif
    mSubDirs.clear();
    delete mLoader;
    if (TheLoadMgr.AsyncUnload()) {
#ifdef HX_NATIVE
        // Async unload still needs to nullify refs on objects that will be
        // destroyed later, so persistent ObjPtrs (TaskMgr, globals) don't
        // hold stale pointers. DirUnloader handles the actual destruction.
        for (ObjDirItr<Hmx::Object> it(this, false); it != nullptr; ++it) {
            if (it != this && ((Hmx::Object *)it)->IsRefAlive())
                ((Hmx::Object *)it)->NullifyAllRefs();
        }
#endif
        new DirUnloader(this);
    } else {
        DeleteObjects();
        DeleteSubDirs();
    }
    if (!IsProxy()) {
        SetName(nullptr, nullptr);
    }
    if (mPathName != gNullStr) {
        MemOrPoolFree(strlen(mPathName) + 1, (void *)mPathName);
    }
    if (mAlwaysInlineHash != gNullStr) {
        MemOrPoolFree(strlen(mAlwaysInlineHash) + 1, (void *)mAlwaysInlineHash);
    }
#ifdef HX_NATIVE
    sDeleteObjectsDepth--;
    if (sDeleteObjectsDepth == 0 && !sSuppressFlush)
        FlushDeferredFrees();
#endif
}

BEGIN_HANDLERS(ObjectDir)
    HANDLE_ACTION(iterate, Iterate(_msg, true))
    HANDLE_ACTION(iterate_self, Iterate(_msg, false))
    HANDLE_ACTION(save_objects, DirLoader::SaveObjects(_msg->Str(2), this, false))
    HANDLE(find, OnFind)
    HANDLE_EXPR(exists, FindObject(_msg->Str(2), false, true) != nullptr)
    HANDLE_ACTION(sync_objects, SyncObjects())
    HANDLE_EXPR(is_proxy, IsProxy())
    HANDLE_EXPR(proxy_dir, ProxyDir())
    HANDLE_EXPR(proxy_name, ProxyName())
    HANDLE_ACTION(
        add_names,
        Reserve(
            mHashTable.Size() + _msg->Int(2) * 2,
            mStringTable.Size() + _msg->Int(2) * 0x14
        )
    )
    HANDLE_ACTION(override_proxy, SetProxyFile(_msg->Str(2), true))
    HANDLE_ACTION(delete_loader, RELEASE(mLoader))
    HANDLE_ACTION(reset_editor_state, ResetEditorState())
    HANDLE_EXPR(get_path_name, mPathName)
    HANDLE_EXPR(get_file_name, FileGetName(mPathName))
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

ObjectDir *ObjectDir::ProxyDir() const {
    return Loader() ? Loader()->ProxyDir() : nullptr;
}

const char *ObjectDir::ProxyName() const {
    return Loader() ? (Loader()->ProxyName() ? Loader()->ProxyName() : "") : "";
}

ObjectDir *SyncSubDir(const FilePath &fp, ObjectDir *dir) {
    Loader *loader = TheLoadMgr.GetLoader(fp);
    DirLoader *dirLoader = dir->IsProxy()
        ? dynamic_cast<DirLoader *>(loader)
        : dynamic_cast<DirLoader *>(TheLoadMgr.ForceGetLoader(fp));
    if (!dirLoader)
        return nullptr;
    ObjectDir *retDir = dirLoader->GetDir();
    if (retDir) {
        for (ObjDirItr<Hmx::Object> it(dir, false); it != nullptr; ++it) {
            Hmx::Object *found = retDir->FindObject(it->Name(), false, true);
            if (found && found != retDir && &*it != dir) {
                MILO_NOTIFY(
                    "%s exists in dir and subdir, so replacing %s with %s",
                    it->Name(),
                    PathName(it),
                    PathName(found)
                );
                it->ReplaceRefs(found);
                delete it;
            }
        }
    }
    return retDir;
}

bool PropSyncSubDirs(
    std::vector<ObjDirPtr<ObjectDir> > &subdirs,
    DataNode &val,
    DataArray *prop,
    int i,
    PropOp op
) {
    ObjectDir *theGDir = gDir;
    if (op == kPropSize) {
        MILO_ASSERT(i == prop->Size(), 0x947);
        val = (int)subdirs.size();
        return true;
    } else {
        MILO_ASSERT(i == prop->Size() - 1, 0x94D);
        std::vector<ObjDirPtr<ObjectDir> >::iterator subdirIt =
            subdirs.begin() + prop->Int(i);
        ObjDirPtr<ObjectDir> &ptr = *subdirIt;
        if (op == kPropSet || op == kPropInsert) {
            FilePath valPath = val.Str();
            FilePath relative =
                FileRelativePath(FilePath::Root().c_str(), valPath.c_str());
            FOREACH (it, subdirs) {
                if (it != subdirIt) {
                    const char *curRelative =
                        FileRelativePath(FilePath::Root().c_str(), it->GetFile().c_str());
                    if (streq(relative.c_str(), curRelative)) {
                        MILO_NOTIFY(
                            "Subdir '%s' can't be added to '%s' more than once!",
                            relative,
                            PathName(theGDir)
                        );
                        return true;
                    }
                }
            }
        }
        switch (op) {
        case kPropGet:
            val = FileRelativePath(FilePath::Root().c_str(), ptr.GetFile().c_str());
            break;
        case kPropSet:
            theGDir->RemovingSubDir(ptr);
            ptr = SyncSubDir(val.Str(), theGDir);
            theGDir->AddedSubDir(ptr);
            break;
        case kPropRemove:
            theGDir->RemovingSubDir(ptr);
            subdirs.erase(subdirIt);
            break;
        case kPropInsert:
            subdirIt = subdirs.insert(subdirIt, SyncSubDir(val.Str(), theGDir));
            theGDir->AddedSubDir(*subdirIt);
            break;
        default:
            return false;
        }
        return true;
    }
}

BEGIN_PROPSYNCS(ObjectDir)
    gDir = this;
    {
        static Symbol _s("subdirs");
        if (sym == _s) {
            PropSyncSubDirs(mSubDirs, _val, _prop, _i + 1, _op);
            return true;
        }
    }
    SYNC_PROP_SET(
        proxy_file,
        FileRelativePath(FilePath::Root().c_str(), ProxyFile().c_str()),
        SetProxyFile(_val.Str(), false)
    )
    SYNC_PROP(inline_proxy, (int &)mInlineProxyType)
    SYNC_PROP_SET(path_name, mPathName, )
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

inline BinStream &operator<<(BinStream &bs, const ObjectDir::Viewport &v) {
    bs << v.mXfm;
    return bs;
}

void ObjectDir::Save(BinStream &bs) {
    SAVE_REVS(0x1C, 0)
    SaveType(bs);
    bs << mAlwaysInlined;
    if (mAlwaysInlineHash && !bs.Cached()) {
        int len = strlen(mAlwaysInlineHash);
        bs << len;
        bs.Write(mAlwaysInlineHash, len);
    } else {
        bs << 0;
    }
    bs << mViewports;
    bs << mCurViewportID;
    bs << (unsigned char)mInlineProxyType;
    bs << mProxyFile;
    std::vector<ObjDirPtr<ObjectDir> > inlinedSubDirs;
    std::vector<ObjDirPtr<ObjectDir> > notInlinedSubDirs;
    if (SaveSubdirs()) {
        for (int i = 0; i < mSubDirs.size(); i++) {
            if (mSubDirs[i]) {
                ObjDirPtr<ObjectDir> &curSubDir = mSubDirs[i];
                if (curSubDir->InlineSubDirType() != kInlineNever) {
                    inlinedSubDirs.push_back(curSubDir);
                } else {
                    notInlinedSubDirs.push_back(curSubDir);
                }
            }
        }
    }
    bs << notInlinedSubDirs;
    bs << (unsigned char)mInlineSubDirType;
    bs << inlinedSubDirs;

    for (int i = 0; i < inlinedSubDirs.size(); i++) {
        InlineDirType iType = ((ObjectDir *)inlinedSubDirs[i])->InlineSubDirType();
        bs << (unsigned char)iType;
        SaveInlined(inlinedSubDirs[i].GetFile(), false, iType);
    }

    std::vector<bool> boolVec;
    boolVec.resize(mInlinedDirs.size(), false);
    for (int i = 0; i < mInlinedDirs.size(); i++) {
        InlinedDir &id = mInlinedDirs[i];
        switch (id.mType) {
        case kInlineCachedShared:
            id.shared = true;
        case kInlineCached: {
            bool old = gLoadingProxyFromDisk;
            if (bs.Cached()) {
                gLoadingProxyFromDisk = false;
                DirLoader::SetCacheMode(false);
                id.dir.LoadFile(id.file, false, false, kLoadFront, true);
                DirLoader::SetCacheMode(true);
                gLoadingProxyFromDisk = old;
            } else {
                id.dir = nullptr;
            }
            break;
        }
        default: {
            MILO_ASSERT(id.mType == kInlineAlways, 0x211);
            int gg = 0;
            for (; gg != mSubDirs.size(); gg++) {
                if (mSubDirs[gg].GetFile() == id.file)
                    break;
            }
            MILO_ASSERT(gg < mSubDirs.size(), 0x21A);
            id.dir = (ObjectDir *)mSubDirs[gg];
            if (id.shared) {
                id.shared = false;
                MILO_NOTIFY("Can't share kInlineAlways dirs");
            }
            break;
        }
        }
        if (id.dir) {
            if (id.shared && !bs.AddSharedInlined(id.file)) {
                boolVec[i] = true;
            }
        } else {
            boolVec[i] = true;
        }
        bs << boolVec[i];
    }

    bool oldProxy = gLoadingProxyFromDisk;
    gLoadingProxyFromDisk = false;
    for (int i = mInlinedDirs.size() - 1; i >= 0; i--) {
        InlinedDir &id = mInlinedDirs[i];
        if (!boolVec[i]) {
            bool isSubDir = id.dir->IsSubDir();
            if (isSubDir) {
                RemovingSubDir(id.dir);
            }
            {
                String dirName = id.dir->Name();
                ObjectDir *dirDir = id.dir->Dir();
                if (!id.shared) {
                    ObjectDir *dirToSet = id.dir;
                    if (dirToSet->Dir()) {
                        int uniqIdx = 0;
                        const char *uniqStr;
                        while (true) {
                            uniqStr = MakeString("uniq%x", uniqIdx);
                            if (!dirToSet->FindContainingDir(uniqStr)
                                && !FindContainingDir(uniqStr))
                                break;
                            uniqIdx++;
                        }
                        dirToSet->SetName(uniqStr, dirToSet);
                    }
                }
                FilePathTracker tracker(FileGetPath(id.file.c_str()));
                DirLoader::SaveObjects(bs, id.dir);
                if (!id.shared) {
                    id.dir->SetName(dirName.c_str(), dirDir);
                }
            }
            if (isSubDir) {
                AddedSubDir(id.dir);
            }
        }
    }
    std::vector<InlinedDir> unused;
    mInlinedDirs.swap(unused);
    gLoadingProxyFromDisk = oldProxy;
    mCurViewportID = (ViewportId)0;
    const char *nextname = unk8c ? unk8c->Name() : "";
    bs << nextname;
    const char *camName = mCurCam ? mCurCam->Name() : "";
    bs << camName;
    SaveRest(bs);
    gLoadingProxyFromDisk = false;
}

BEGIN_COPYS(ObjectDir)
    COPY_SUPERCLASS(Hmx::Object)
    if (ty != kCopyFromMax) {
        CREATE_COPY(ObjectDir)
        BEGIN_COPYING_MEMBERS
            if (!IsProxy()) {
                COPY_MEMBER(mViewports)
                COPY_MEMBER(mCurViewportID)
#ifdef HX_NATIVE
                // During MergeDirs, skip mSubDirs — MergeObjectsRecurse handles
                // subdirs separately. Copying here triggers cascading deletion of
                // old subdirs whose objects have live ring refs in the parent dir,
                // corrupting ref rings and causing OOM in SnapshotRing.
                if (!InMergeDirs()) {
#endif
                for (int i = 0; i < mSubDirs.size(); i++) {
                    RemovingSubDir(mSubDirs[i]);
                }
                COPY_MEMBER(mSubDirs)
                for (int i = 0; i < mSubDirs.size(); i++) {
                    AddedSubDir(mSubDirs[i]);
                }
#ifdef HX_NATIVE
                }
#endif
            }
            COPY_MEMBER(mInlineProxyType)
            COPY_MEMBER(mInlineSubDirType)
        END_COPYING_MEMBERS
    }
END_COPYS

void ObjectDir::Load(BinStream &bs) {
    PreLoad(bs);
    PostLoad(bs);
    if (IsProxy() && mLoader && !mLoader->IsLoaded()) {
        TheLoadMgr.PollUntilLoaded(mLoader, nullptr);
    }
}

void ObjectDir::PostSave(BinStream &) { SyncObjects(); }

void ObjectDir::SetProxyFile(const FilePath &file, bool override) {
    if (!IsProxy()) {
        MILO_NOTIFY("Can't set proxy file if own dir");
    } else {
        mProxyFile = file;
        mProxyOverride = override;
        if (!override) {
            DeleteObjects();
            DeleteSubDirs();
            if (!mProxyFile.empty()) {
                DirLoader *dl = new DirLoader(
                    mProxyFile, kLoadFront, nullptr, nullptr, this, false, nullptr
                );
                TheLoadMgr.PollUntilLoaded(dl, nullptr);
            }
        }
    }
}

void ObjectDir::SetSubDir(bool isSubdir) {
    if (isSubdir) {
        mIsSubDir = true;
        SetName(nullptr, nullptr);
        SetTypeDef(nullptr);
    }
}

InlineDirType ObjectDir::InlineSubDirType() { return mInlineSubDirType; }

void ObjectDir::SyncObjects() {
    static Message msg("sync_objects");
    HandleType(msg);
}

void ObjectDir::ResetEditorState() {
    mViewports.resize(kNumViewports);
    mCurViewportID = (ViewportId)0;
    unk8c = nullptr;
    mCurCam = nullptr;
    ResetViewports();
}

void ObjectDir::AddedObject(Hmx::Object *) {}

void ObjectDir::RemovingObject(Hmx::Object *obj) {
    if (obj == unk8c) {
        unk8c = nullptr;
    }
    if (obj == mCurCam) {
        mCurCam = nullptr;
        if (mCurViewportID == 7) {
            mCurViewportID = (ViewportId)0;
        }
    }
}

void ObjectDir::OldLoadProxies(BinStream &bs, int i) {
    int x;
    bs >> x;
    if (x != 0)
        MILO_FAIL("Proxies not allowed here");
}

#pragma endregion

BinStream &operator>>(BinStream &bs, InlineDirType &ty) {
    unsigned char uc;
    bs >> uc;
    ty = (InlineDirType)uc;
    return bs;
}

bool ObjectDir::SaveSubdirs() {
    return !IsProxy() || mProxyFile.empty() || gLoadingProxyFromDisk;
}

bool ObjectDir::ShouldSaveProxy(BinStream &bs) {
    return IsProxy() && (!mProxyFile.empty() || InlineProxy(bs));
}

void ObjectDir::SetInlineProxyType(InlineDirType t) {
    MILO_ASSERT(t != kInlineCachedShared, 0x198);
    mInlineProxyType = t;
}

BinStreamRev &operator>>(BinStreamRev &bs, ObjectDir::Viewport &v) {
    bs >> v.mXfm;
    if (bs.rev < 0x12) {
        int x;
        bs >> x;
    }
    return bs;
}

BinStream &operator>>(BinStream &bs, ObjectDir::Viewport &v) {
    bs >> v.mXfm;
    return bs;
}

void ObjectDir::TransferLoaderState(ObjectDir *dir) {
    mProxyFile = dir->mProxyFile;
    mProxyOverride = dir->mProxyOverride;
    mLoader = dir->mLoader;
    dir->mLoader = nullptr;
}

bool ObjectDir::HasDirPtrs() const {
#ifdef HX_NATIVE
    // O(1) counter check instead of O(n) ring walk.
    // sDeleting guard prevents double-delete during Object::~Object().
    if (sDeleting == this) return true;
    auto &counts = DirPtrRefCounts();
    auto it = counts.find((const void *)this);
    return it != counts.end() && it->second > 0;
#else
    if (sDeleting == this) {
        return true;
    } else {
        FOREACH (it, mRefs) {
            if (it->IsDirPtr())
                return true;
        }
        return false;
    }
#endif
}

namespace {
    int gPreloadIdx = 0;
    ObjDirPtr<ObjectDir> gPreloaded[128];

    void DeleteShared() {
        for (; gPreloadIdx > 0; gPreloadIdx--) {
            gPreloaded[gPreloadIdx - 1] = 0;
        }
    }
}

ObjectDir::Viewport &ObjectDir::CurViewport() {
    if (mCurViewportID >= kNumViewports) {
        MILO_FAIL("%s mCurView = %d, >= kNumViewports", PathName(this), mCurViewportID);
    }
    return mViewports[mCurViewportID];
}

bool ObjectDir::HasSubDir(ObjectDir *dir) {
    if (this == dir)
        return true;
    else {
        for (int i = 0; i < mSubDirs.size(); i++) {
            if (mSubDirs[i] && mSubDirs[i]->HasSubDir(dir)) {
                return true;
            }
        }
    }
    return false;
}

void ObjectDir::SaveProxy(BinStream &bs) {
    if (ShouldSaveProxy(bs) && InlineProxy(bs)) {
        gLoadingProxyFromDisk = true;
        const char *path = mProxyFile.empty() ? FilePath::Root().c_str()
                                              : FileGetPath(mProxyFile.c_str());
        FilePathTracker tracker(path);
        DirLoader::SaveObjects(bs, this);
    }
}

void ObjectDir::ResetViewports() {
    Viewport *vp = &mViewports[0];
    vp[1].mXfm.m.Set(0, -1, 0, 1, 0, 0, 0, 0, 1);
    vp[1].mXfm.v.Set(-768, 0, 0);
    vp[2].mXfm.m.Set(0, 1, 0, -1, 0, 0, 0, 0, 1);
    vp[2].mXfm.v.Set(768, 0, 0);
    vp[3].mXfm.m.Set(1, 0, 0, 0, 0, 1, 0, 1, 0);
    vp[3].mXfm.v.Set(0, 0, 768);
    vp[4].mXfm.m.Set(1, 0, 0, 0, 0, 1, 0, -1, 0);
    vp[4].mXfm.v.Set(0, 0, -768);
    vp[5].mXfm.m.Set(1, 0, 0, 0, 1, 0, 0, 0, 1);
    vp[5].mXfm.v.Set(0, -768, 0);
    vp[6].mXfm.m.Set(-1, 0, 0, 0, -1, 0, 0, 0, 1);
    vp[6].mXfm.v.Set(0, 768, 0);
    MakeRotMatrix(Vector3(1, 1, -1), Vector3(0, 0, 1), vp[0].mXfm.m);
    Vector3 v(0, -768.0f, 0);
    Hmx::Matrix3 &m = vp[0].mXfm.m;
    vp[0].mXfm.v.Set(
        m.x.x * v.x + (m.z.x * v.z + m.y.x * v.y),
        m.x.y * v.x + (m.z.y * v.z + m.y.y * v.y),
        m.x.z * v.x + (m.z.z * v.z + m.y.z * v.y)
    );
}

DataNode OnLoadObjects(DataArray *a) {
    return DirLoader::LoadObjects(a->Str(1), nullptr, nullptr);
}

DataNode OnPathName(DataArray *a) { return PathName(a->Obj<Hmx::Object>(1)); }

DataNode OnReserveToFit(DataArray *a) {
    ReserveToFit(a->Obj<ObjectDir>(1), a->Obj<ObjectDir>(2), a->Int(3));
    return 0;
}

DataNode OnInitObject(DataArray *a) {
    a->Obj<Hmx::Object>(1)->InitObject();
    return 0;
}

void ObjectDir::Reserve(int hashSize, int stringSize) {
    MemTemp tmp;
    if (mHashTable.Size() < hashSize) {
        mHashTable.Resize(hashSize, 0);
    }
    mStringTable.Reserve(stringSize);
}

ObjectDir::InlinedDir::InlinedDir() : dir(), file() {}
ObjectDir::InlinedDir::~InlinedDir() {}

void ObjectDir::LoadSubDir(int i, const FilePath &fp, BinStream &bs, bool b) {
    if (IsProxy() && !mProxyFile.empty()) {
        mSubDirs[i] = 0;
    } else {
        FilePath subdirpath = GetSubDirPath(fp, bs);
        if (streq(mPathName, subdirpath.c_str())) {
            MILO_NOTIFY(
                "%s trying to subdir self in slot %d, setting NULL", PathName(this), i
            );
            mSubDirs[i] = 0;
        } else {
            mSubDirs[i].LoadFile(subdirpath, true, b, kLoadFront, true);
#ifdef HX_NATIVE
            // Propagate parent dir so ObjPtr fallback can walk up to this dir
            // when the subdir's Dir() is self-referential during loading.
            DirLoader *dl = mSubDirs[i].GetLoader();
            if (dl) dl->SetParentDir(this);
#endif
        }
    }
}

void PreloadArray(DataArray *arr, int idx) {
    for (int i = idx; i < arr->Size(); i++) {
        DataArray *curArr = arr->Array(i);
        if (curArr->Size() != 0) {
            if (curArr->Type(0) == kDataArray) {
                PreloadArray(curArr, 0);
            } else {
                const char *str = curArr->Str(0);
                bool shouldPop = false;
                if (curArr->Size() > 1) {
                    MemPushHeap(MemFindHeap(curArr->Sym(1).Str()));
                    shouldPop = true;
                }
                MILO_ASSERT(gPreloadIdx < DIM(gPreloaded), 0xA35);
                gPreloaded[gPreloadIdx++].LoadFile(str, false, true, kLoadFront, false);
                if (shouldPop) {
                    MemPopHeap();
                }
            }
        }
    }
}

void PreloadSharedSubdirs(Symbol s) {
    DataArray *arr = SystemConfig("preload_subdirs")->FindArray(s, false);
    if (arr) {
        PreloadArray(arr, 1);
    }
}

void ObjectDir::Terminate() {
    DeleteShared();
    sSuperClassMap.clear();
}

void ObjectDir::AddedSubDir(ObjDirPtr<ObjectDir> &subdir) {
    ObjectDir *dir = subdir;
    if (dir) {
        dir->InlineSubDirType();
        dir->SetSubDir(true);
        for (ObjDirItr<Hmx::Object> it(dir, true); it != nullptr; ++it) {
            AddedObject(it);
        }
    }
}

void ObjectDir::RemovingSubDir(ObjDirPtr<ObjectDir> &subdir) {
    ObjectDir *dir = subdir;
    if (dir) {
        dir->SetSubDir(false);
        for (ObjDirItr<Hmx::Object> it(dir, true); it != nullptr; ++it) {
            RemovingObject(it);
        }
    }
}


#ifdef HX_NATIVE
int ObjectDir::sDeleteObjectsDepth = 0;
bool ObjectDir::sInMergeDirs = false;
bool ObjectDir::sSuppressFlush = false;
#endif

void ObjectDir::DeleteObjects() {
#ifdef HX_NATIVE
    // Three-phase deletion: nullify refs, destroy, defer-free.
    //
    // Can't use `delete obj` because virtual inheritance makes
    // Hmx::Object* point to a subobject offset within the malloc'd block,
    // and multiple base classes define operator delete (ambiguous).
    std::vector<std::pair<void *, Hmx::Object *>> todo;
    for (ObjDirItr<Hmx::Object> it(this, false); it != nullptr; ++it) {
        if (it != this)
            todo.push_back({dynamic_cast<void *>((Hmx::Object *)it), it});
    }
    // Phase 0: nullify ref rings via NullifyObj while memory is valid.
    // Inner DeleteObjects skips this because the outermost destructor already
    // walked all reachable ObjectDirs before any mSubDirs buffers were freed.
    if (sDeleteObjectsDepth <= 1) {
        for (ObjDirItr<Hmx::Object> it(this, true); it != nullptr; ++it) {
            if (it != this && ((Hmx::Object *)it)->IsRefAlive())
                ((Hmx::Object *)it)->NullifyAllRefs();
        }
    }
    // Phase 1: destroy all (memory stays valid for sibling destructors).
    for (size_t i = 0; i < todo.size(); i++) {
        auto &[block, obj] = todo[i];
        if (!block || !obj || !obj->IsRefAlive()) {
            obj = nullptr;
            continue;
        }
        obj->~Object();
    }
    // Phase 2: defer frees until outermost ~ObjectDir completes.
    for (auto &[block, obj] : todo) {
        if (!obj)
            continue;
        DirPtrRefCounts().erase((const void *)obj);
        DeferFree(block);
    }
#else
    for (ObjDirItr<Hmx::Object> it(this, false); it != nullptr; ++it) {
        if (it != this) {
            delete it;
        }
    }
#endif
}

void ObjectDir::RemoveSubDir(const ObjDirPtr<ObjectDir> &dPtr) {
    std::vector<ObjDirPtr<ObjectDir> >::iterator it = mSubDirs.begin();
    while (it != mSubDirs.end()) {
#ifdef HX_NATIVE
        if ((ObjectDir *)(*it) == (ObjectDir *)dPtr) {
#else
        if (*(u32 *)((u8 *)&(*it) + 0xc) == *(u32 *)((u8 *)&dPtr + 0xc)) {
#endif
            RemovingSubDir(*it);
            it = mSubDirs.erase(it);
            if (it == mSubDirs.end())
                break;
        }
        ++it;
    }
}

void ObjectDir::DeleteSubDirs() {
    for (int i = 0; i < mSubDirs.size(); i++) {
        RemovingSubDir(mSubDirs[i]);
    }
    mSubDirs.clear();
}

void CheckForDuplicates() {
    DataArray *cfg = SystemConfig("objects");
    std::list<Symbol> syms;
    for (int i = 1; i < cfg->Size(); i++) {
        syms.push_back(cfg->Array(i)->Sym(0));
    }
    syms.sort();
    Symbol previous;
    bool fail = false;
    for (std::list<Symbol>::iterator it = syms.begin(); it != syms.end();
         previous = *it, ++it) {
        const Symbol& cur = *it;
        if (cur == previous) {
            MILO_NOTIFY("Duplicate object %s in config", cur);
            fail = true;
        }
    }
    if (fail)
        MILO_FAIL("duplicate objects found in configs, bailing");
    syms.unique();
}

void ObjectDir::Init() {
    MessageTimer::Init();
    TheLoadMgr.RegisterFactory("milo", DirLoader::New);
    TheLoadMgr.RegisterFactory("milo_xbox", DirLoader::New);
    TheLoadMgr.RegisterFactory("milo_ps3", DirLoader::New);
    TheLoadMgr.RegisterFactory("milo_pc", DirLoader::New);
    TheLoadMgr.RegisterFactory("milo_ps2", DirLoader::New);
    TheLoadMgr.RegisterFactory("milo_wii", DirLoader::New);
    TheLoadMgr.RegisterFactory("rnd", DirLoader::New);
    TheLoadMgr.RegisterFactory("m2", DirLoader::New);
    TheLoadMgr.RegisterFactory("gh", DirLoader::New);
    TheLoadMgr.RegisterFactory("kr", DirLoader::New);
    CheckForDuplicates();
    DataRegisterFunc("load_objects", OnLoadObjects);
    DataRegisterFunc("init_object", OnInitObject);
    DataRegisterFunc("path_name", OnPathName);
    DataRegisterFunc("reserve_to_fit", OnReserveToFit);
    DirLoader::sPrintTimes = OptionBool("loader_times", false);
}

void ObjectDir::Iterate(DataArray *arr, bool b) {
    const DataNode &n = arr->Evaluate(2);
    Symbol s2;
    Symbol s8;
    if (n.Type() == kDataSymbol) {
        const char *str = n.UncheckedStr();
        s2 = STR_TO_SYM(str);
    } else {
        DataArray *a2 = n.UncheckedArray();
        s2 = a2->Sym(0);
        s8 = a2->Sym(1);
    }
    static DataArray *objects = SystemConfig("objects");
    objects->FindArray(s2);
    DataNode *var = arr->Var(3);
    DataNode varNode(*var);
    Symbol first;
    for (ObjDirItr<Hmx::Object> it(this, b); it != nullptr; ++it) {
        bool bbb;
                first = it->ClassName();
        std::pair<Symbol, Symbol> key = std::make_pair(first, s2);
        std::map<std::pair<Symbol, Symbol>, bool>::iterator superclassIt =
            sSuperClassMap.find(key);
        if (superclassIt == sSuperClassMap.end()) {
            bbb = IsASubclass(first, s2);
            sSuperClassMap[key] = bbb;
        } else
            bbb = superclassIt->second;
        if (bbb && (s2.Null() || it->Type() == s2)) {
            *var = &*it;
            for (int i = 4; i < arr->Size(); i++) {
                arr->Command(i)->Execute(true);
            }
        }
    }
    *var = varNode;
}

ObjDirPtr<ObjectDir> ObjectDir::PostLoadInlined() {
    MILO_ASSERT(mInlinedDirs.size() > 0, 0x296);
    InlinedDir iDir = mInlinedDirs.back();
    mInlinedDirs.pop_back();
    if (mInlinedDirs.size() == 0) {
        std::vector<InlinedDir> empty;
        mInlinedDirs.swap(empty);
    }
    if (iDir.shared && iDir.file.length() > 0 && !iDir.dir) {
        MILO_NOTIFY("Couldn't load shared inlined file %s\n", iDir.file);
    }
    return iDir.dir;
}

ObjectDir::Entry *ObjectDir::FindEntry(const char *name, bool add) {
    if (name == nullptr || *name == '\0')
        return nullptr;
    else {
        Entry *entry = mHashTable.Find(name);
        if (!entry && add) {
            Entry newEntry;
            newEntry.name = mStringTable.Add(name);
            entry = mHashTable.Insert(newEntry);
        }
        return entry;
    }
}

Hmx::Object *ObjectDir::FindObject(const char *name, bool parentDirs, bool subDirs) {
    Entry *entry = FindEntry(name, false);
    if (entry && entry->obj)
        return entry->obj;
    if (subDirs) {
        for (int i = 0; i < mSubDirs.size(); i++) {
            if (mSubDirs[i]) {
                Hmx::Object *found = mSubDirs[i]->FindObject(name, false, true);
                if (found)
                    return found;
            }
        }
    }
    if (strlen(name) != 0) {
        if (strcmp(name, Name()) == 0) {
            return this;
        }
    }
    if (parentDirs) {
        if (Dir() && Dir() != this) {
            return Dir()->FindObject(name, parentDirs, true);
        }
        if (this != sMainDir) {
            return sMainDir->FindObject(name, false, true);
        }
    }
#ifdef HX_NATIVE
    // Native fallback: when Dir() is self-referential during loading,
    // search via the DirLoader's ProxyDir chain. On Xbox, MergeDirs
    // flattens all objects into the same scope so this isn't needed.
    // ProxyDir points to the parent dir that loaded this proxy object
    // (same fallback FlowPtr uses via FlowPtrGetLoadingDir).
    if (!parentDirs && subDirs && Dir() == this && mLoader) {
        ObjectDir *proxyDir = mLoader->ProxyDir();
        if (proxyDir && proxyDir != this) {
            Hmx::Object *found = proxyDir->FindObject(name, false, true);
            if (found) return found;
        }
        DirLoader *loader = mLoader;
        if (loader->ParentDir() && loader->ParentDir() != this) {
            Hmx::Object *found = loader->ParentDir()->FindObject(name, false, true);
            if (found) return found;
        }
    }
#endif
    return nullptr;
}

ObjectDir *ObjectDir::FindContainingDir(const char *name) {
    if (FindEntry(name, false))
        return this;
    for (int i = 0; i < mSubDirs.size(); i++) {
        if (mSubDirs[i]) {
            ObjectDir *subdir = mSubDirs[i]->FindContainingDir(name);
            if (subdir)
                return subdir;
        }
    }
    return nullptr;
}

void ObjectDir::AppendSubDir(const ObjDirPtr<ObjectDir> &subdir) {
    mSubDirs.push_back(subdir);
    AddedSubDir(mSubDirs.back());
}

DataNode ObjectDir::OnFind(DataArray *da) {
    Hmx::Object *found = FindObject(da->Str(2), false, true);
    if (da->Size() > 3) {
        if (da->Int(3) != 0 && !found) {
            MILO_FAIL("Couldn't find %s in %s", da->Str(2), Name());
        }
    }
    return found;
}

void ObjectDir::PreInit(int hashSize, int stringSize) {
    REGISTER_OBJ_FACTORY(Hmx::Object);
    REGISTER_OBJ_FACTORY(ObjectDir);
    sMainDir = new ObjectDir();
    sMainDir->Reserve(hashSize, stringSize);
    sMainDir->SetName("main", sMainDir);
    DataSetThis(sMainDir);
    sSuperClassMap.clear();
    if (UsingCD()) {
        DirLoader::SetCacheMode(true);
    }
}

void ObjectDir::SaveInlined(const FilePath &fp, bool share, InlineDirType type) {
    MILO_ASSERT(type != kInlineNever, 0x26A);
    if (type == kInlineAlways && share) {
        MILO_NOTIFY("Can't share kInlineAlways Dirs");
        share = false;
    }
    InlinedDir dir;
    dir.file = fp;
    dir.shared = share;
    dir.mType = type;
    mInlinedDirs.push_back(dir);
}

void ObjectDir::PreLoadInlined(const FilePath &fp, bool share, InlineDirType type) {
    MILO_ASSERT(type != kInlineNever, 0x27C);
    if (type == kInlineAlways && share) {
        MILO_NOTIFY("Can't share kInlineAlways Dirs");
        share = false;
    }
    InlinedDir dir;
    dir.file = fp;
    dir.shared = share;
    dir.mType = type;
    mInlinedDirs.push_back(dir);
}

void ObjectDir::SetCurViewport(ViewportId id, Hmx::Object *o) {
    mCurViewportID = id;
    mCurCam = o;
}

void ObjectDir::SetSubDirFlag(bool flag) { mIsSubDir = flag; }

bool ObjectDir::InlineProxy(BinStream &bs) {
    return (mInlineProxyType == kInlineCached && bs.Cached())
        || mInlineProxyType == kInlineAlways;
}

void ObjectDir::SetPathName(const char *path) {
    if (mPathName != gNullStr) {
        MemOrPoolFree(strlen(mPathName) + 1, (void *)mPathName);
    }
    if (path != 0 && *path != '\0') {
#ifdef HX_NATIVE
        mPathName =
            (char *)MemOrPoolAlloc(strlen(path) + 1, __FILE__, 0x996, "path name");
#else
        mPathName = (char *)MemOrPoolAlloc(strlen(path) + 1);
#endif
        strcpy((char *)mPathName, path);
        mStoredFile.Set(FilePath::Root().c_str(), mPathName);
    } else
        mPathName = gNullStr;
}

FilePath ObjectDir::GetSubDirPath(const FilePath &fp, const BinStream &bs) {
    static Message msg("change_subdir", gNullStr);
    msg[0] = fp.c_str();
    DataNode handled = HandleType(msg);
    FilePath ret;
    if (handled.Type() == kDataUnhandled) {
        ret = fp;
    } else if (streq(handled.Str(), "stream_cache")) {
        bool cached = bs.Cached();
        ret = FilePath(".", DirLoader::CachedPath(fp.c_str(), cached));
    } else {
        ret = FilePath(FileRoot(), handled.Str());
    }
    return ret;
}

INIT_REVS(0x1C, 0)

void ObjectDir::PreLoad(BinStream &bs) {
    LOAD_REVS(bs)
    ASSERT_REVS(0x1C, 0)

    if (d.rev > 0x15) {
        LoadType(bs);
    } else if (d.rev > 1 && d.rev < 17) {
        Hmx::Object::Load(bs);
    }

    if (d.rev < 3) {
        int hashSize, strSize;
        bs >> hashSize >> strSize;
        Reserve(hashSize, strSize);
    }

    if (d.rev > 0x19) {
        if (d.rev < 0x1B) {
            bool b;
            d >> b;
            mAlwaysInlined = b != 0;
        } else {
            d >> mAlwaysInlined;
        }
        int hashLen;
        d >> hashLen;
        if (hashLen) {
#ifdef HX_NATIVE
            char *hash = (char *)MemOrPoolAlloc(hashLen + 1, __FILE__, 0x30A, "Always Inline CDB");
#else
            char *hash = (char *)MemOrPoolAlloc(hashLen + 1);
#endif
            mAlwaysInlineHash = hash;
            bs.Read(hash, hashLen);
            char *ptr = (char *)mAlwaysInlineHash;
            ptr[hashLen] = '\0';
        }
    }

    if (d.rev > 1) {
        d >> mViewports;
        d >> (int &)mCurViewportID;
        if (d.rev == 3 && mCurViewportID > 6) {
            mCurViewportID = (ViewportId)6;
        }
    }

    if (d.rev > 0xC) {
        if (d.rev > 0x13) {
            InlineDirType proxyType;
            if (d.rev > 0x1B) {
                d >> proxyType;
            } else {
                bool b;
                d >> b;
                proxyType = (InlineDirType)(b != 0);
            }
            if (!gLoadingProxyFromDisk) {
                mInlineProxyType = proxyType;
            }
        }
        if (gLoadingProxyFromDisk || mProxyOverride) {
            if (mProxyOverride && mInlineProxyType != kInlineNever) {
                MILO_FAIL("You cannot override an inlined proxy!");
            }
            FilePath fp;
            bs >> fp;
            mProxyOverride = false;
        } else {
            FilePath fp;
            bs >> fp;
            if (!fp.empty() && fp == mProxyFile) {
                mProxyOverride = true;
            } else {
                if (!DirLoader::ShouldBlockSubdirLoad(fp)) {
                    mProxyFile = fp;
                }
                mProxyOverride = false;
            }
        }
    }

    if (d.rev > 1 && d.rev < 11) {
        char buf[0x80];
        bs.ReadString(buf, 0x80);
        unk8c = FindObject(buf, false, true);
    }
    if (d.rev > 3 && d.rev < 11) {
        char buf[0x80];
        bs.ReadString(buf, 0x80);
        mCurCam = FindObject(buf, false, true);
        if (mCurCam == nullptr && (int)mCurViewportID == 7) {
            mCurViewportID = (ViewportId)0;
        }
    }
    if (d.rev == 5) {
        char buf[0x80];
        bs.ReadString(buf, 0x80);
    }

    static std::vector<FilePath> inlinedSubDirs;
    static std::vector<FilePath> notInlinedSubDirs;

    if (d.rev > 2) {
        d >> notInlinedSubDirs;
        {
            std::vector<FilePath>::iterator endIter = notInlinedSubDirs.end();
            std::vector<FilePath>::iterator it = std::remove_if(
                notInlinedSubDirs.begin(), endIter,
                DirLoader::ShouldBlockSubdirLoad);
            if (it != endIter) {
                notInlinedSubDirs.erase(it, endIter);
            }
        }
        std::vector<int> intVec;
        if (d.rev == 0x17) {
            d >> intVec;
        }
        if (d.rev > 0x14) {
            d >> mInlineSubDirType;
            d >> inlinedSubDirs;
            {
                std::vector<FilePath>::iterator endIter = inlinedSubDirs.end();
                std::vector<FilePath>::iterator it = std::remove_if(
                    inlinedSubDirs.begin(), endIter,
                    DirLoader::ShouldBlockSubdirLoad);
                if (it != endIter) {
                    inlinedSubDirs.erase(it, endIter);
                }
            }
        } else {
            inlinedSubDirs.clear();
        }

        int i20 = 0;

        if (SaveSubdirs() || inlinedSubDirs.size() != 0 || notInlinedSubDirs.size() != 0) {
            for (int i = 0; i < mSubDirs.size(); i++) {
                RemovingSubDir(mSubDirs[i]);
            }
            if (!bs.Cached()
                && notInlinedSubDirs.size() + inlinedSubDirs.size() == mSubDirs.size()) {
                i20 = 1;
            } else {
                mSubDirs.reserve(notInlinedSubDirs.size() + inlinedSubDirs.size());
                mSubDirs.resize(notInlinedSubDirs.size() + inlinedSubDirs.size());
            }
        } else {
            i20 = 2;
        }

        for (int i = 0; i != notInlinedSubDirs.size(); i++) {
            bool filesneq = mSubDirs[i].GetFile() != notInlinedSubDirs[i];
            if (i20 == 0 || filesneq) {
                bool b17 = false;
                if (intVec.size() != 0) {
                    b17 = intVec[i] != 0;
                }
                LoadSubDir(i, notInlinedSubDirs[i], bs, !b17);
            }
        }

        if (d.rev > 0x17) {
            int numNotInlined = notInlinedSubDirs.size();
            for (int i = 0; i < inlinedSubDirs.size(); i++) {
                bool getfileres = mSubDirs[i + numNotInlined].GetFile() != inlinedSubDirs[i];
                InlineDirType dType;
                if (d.rev > 0x18) {
                    unsigned char b;
                    d >> b;
                    MILO_ASSERT_RANGE_EQ(b, kInlineCached, kInlineCachedShared, 0x3BE);
                    dType = (InlineDirType)b;
                } else {
                    dType = kInlineCached;
                }
                inlinedSubDirs[i] = GetSubDirPath(inlinedSubDirs[i], bs);
                PreLoadInlined(inlinedSubDirs[i], false, dType);
                if (i20 == 1) {
                    bs.PushRev(getfileres, this);
                }
            }
            bs.PushRev(numNotInlined, this);
            if (!bs.Cached()) {
                bs.PushRev(i20, this);
            }
        }
    }

    if (d.rev > 11 && d.rev < 14) {
        OldLoadProxies(bs, d.rev);
    }

    if (d.rev < 0x13) {
        if (d.rev > 0xF) {
            int inlineProxy;
            d >> inlineProxy;
            MILO_ASSERT(inlineProxy != 1, 0x3DC);
        } else if (d.rev > 0xE) {
            bool inlineProxy;
            d >> inlineProxy;
            MILO_ASSERT(!inlineProxy, 0x3E1);
        }
    }

    std::vector<bool> boolVec;
    boolVec.resize(mInlinedDirs.size());
    for (int i = 0; i < mInlinedDirs.size(); i++) {
        if (d.rev < 0x19 && !bs.Cached()) {
            boolVec[i] = true;
        } else {
            bool b;
            d >> b;
            boolVec[i] = b;
        }
    }

    for (int i = 0; i < mInlinedDirs.size(); i++) {
        InlinedDir &curIDir = mInlinedDirs[i];
        FilePath fpath(curIDir.file);
        if (!bs.Cached() || !boolVec[i]) {
            if (!boolVec[i] && (curIDir.mType == kInlineAlways || bs.Cached())) {
                curIDir.dir.LoadInlinedFile(fpath, bs);
            } else if (IsProxy() && !mProxyFile.empty()) {
                curIDir.dir = nullptr;
            } else {
                curIDir.dir.LoadFile(fpath, true, curIDir.shared, kLoadFront, true);
            }
#ifdef HX_NATIVE
            DirLoader *dl = curIDir.dir.GetLoader();
            if (dl) dl->SetParentDir(this);
#endif
        }
    }

    if (d.rev > 20 && d.rev < 24) {
        int offset = notInlinedSubDirs.size();
        MILO_ASSERT(mSubDirs.capacity() >= offset + inlinedSubDirs.size(), 0x415);
        for (int i = 0; i < inlinedSubDirs.size(); i++) {
            mSubDirs[i + offset].LoadInlinedFile(inlinedSubDirs[i], bs);
#ifdef HX_NATIVE
            DirLoader *dl = mSubDirs[i + offset].GetLoader();
            if (dl) dl->SetParentDir(this);
#endif
        }
    }

    mIsSubDir = false;
    bs.PushRev(packRevs(d.altRev, d.rev), this);
}

void ObjectDir::PostLoad(BinStream &bs) {
    BinStreamRev d(bs, bs.PopRev(this));

    for (int i = mInlinedDirs.size() - 1; i >= 0; i--) {
        InlinedDir &iDir = mInlinedDirs[i];
        int tempRev = d.rev;
        iDir.dir.PostLoad(mLoader);
        d.rev = tempRev;
        if (iDir.mType == kInlineCachedShared) {
            iDir.shared = true;
        }
        if (iDir.shared) {
            FilePath &fp = iDir.file;
            DirLoader *last = DirLoader::FindLast(fp);
            if (last) {
                if (last->IsLoaded()) {
                    iDir.dir = last->GetDir();
                } else {
                    MILO_NOTIFY("Can't share unloaded dir %s", fp);
                }
            }
        } else {
            if (iDir.dir.IsLoaded()) {
                delete iDir.dir->mLoader;
                iDir.dir->mLoader = nullptr;
            }
        }
    }

    if (d.rev > 0x17) {
        int revs2 = d.stream.Cached() ? 0 : bs.PopRev(this);
        int offset = bs.PopRev(this);
        MILO_ASSERT_RANGE_EQ(offset, 0, mSubDirs.size(), 0x466);
        if (revs2 != 2) {
            for (int i = mSubDirs.size() - offset - 1; i >= 0; i--) {
                bool bbb = false;
                if (revs2 == 1) {
                    bbb = bs.PopRev(this) != 0;
                }
                ObjDirPtr<ObjectDir> inlinedDirPtr = PostLoadInlined();
                ObjDirPtr<ObjectDir> &curDirPtr = mSubDirs[i + offset];
                if (revs2 == 0 || bbb) {
                    curDirPtr = inlinedDirPtr;
                }
                AddedSubDir(curDirPtr);
            }
            for (offset = offset - 1; offset >= 0; offset--) {
                ObjDirPtr<ObjectDir> &offsetPtr = mSubDirs[offset];
                offsetPtr.PostLoad(mLoader);
                AddedSubDir(offsetPtr);
            }
        }
    } else {
        for (int i = 0; i < mSubDirs.size(); i++) {
            ObjDirPtr<ObjectDir> &curDirPtr = mSubDirs[i];
            curDirPtr.PostLoad(mLoader);
            AddedSubDir(curDirPtr);
            if (curDirPtr.IsLoaded()) {
                if (curDirPtr->InlineSubDirType() != kInlineNever) {
                    delete curDirPtr->mLoader;
                    curDirPtr->mLoader = nullptr;
                }
            }
        }
    }

    if (d.rev > 10) {
        char buf[0x80];
        bs.ReadString(buf, 0x80);
        unk8c = FindObject(buf, false, true);
        bs.ReadString(buf, 0x80);
        mCurCam = FindObject(buf, true, true);
        if (mCurCam == nullptr && (int)mCurViewportID == 7) {
            mCurViewportID = (ViewportId)0;
        }
    }

    if (d.rev > 0x15) {
        LoadRest(bs);
    } else if (d.rev > 0x10) {
        Hmx::Object::Load(bs);
    }

    static Message change_proxies("change_proxies");
    HandleType(change_proxies);

    if (mProxyOverride) {
        mProxyOverride = false;
        if (!TheLoadMgr.EditMode()
            && (!IsProxy() || mInlineProxyType != kInlineNever)) {
            MILO_FAIL("You cannot override an inlined proxy!");
        }
    } else if (ShouldSaveProxy(bs)) {
        DeleteObjects();
        DeleteSubDirs();
        DirLoader *dl = new DirLoader(
            mProxyFile,
            kLoadFront,
            nullptr,
            InlineProxy(bs) ? &bs : nullptr,
            this,
            false,
            nullptr
        );
    }
}
