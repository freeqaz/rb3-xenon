#include "utl/NetCacheMgr.h"
#include "utl/NetCacheMgr_Xbox.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/FileCache.h"
#include "os/PlatformMgr.h"
#include "os/System.h"
#include "utl/Loader.h"
#include "utl/MemMgr.h"
#include "utl/NetCacheLoader.h"
#include "utl/NetLoader.h"
#include "utl/Symbol.h"

NetCacheMgr *TheNetCacheMgr;

NetCacheMgr::NetCacheMgr()
    : mState(-1), mHasFailed(0), mFailType(kNCMFT_Unknown), mServiceId(0), mServiceIDObtained(0),
      mLoadCacheSize(0), mCache(0), mLoadCount(0) {
    SetName("net_cache_mgr", ObjectDir::Main());
}

NetCacheMgr::~NetCacheMgr() {}

BEGIN_HANDLERS(NetCacheMgr)
    HANDLE_ACTION(init, OnInit(_msg->Array(2)))
    HANDLE_ACTION(debug_clear_cache, DebugClearCache())
    HANDLE_EXPR(cheat_next_server, CheatNextServer())
    HANDLE_EXPR(server_type, mServerType)
    HANDLE_EXPR(is_local, IsLocalFile(_msg->Str(2)))
END_HANDLERS

void NetCacheMgr::Poll() {
    if (!mServiceIDObtained) {
        mServiceIDObtained = ThePlatformMgr.GetServiceID("store", mServiceId);
    }
    PollLoaders();
    switch (mState) {
    case 0:
        if (IsDoneLoading()) {
            SetState((NetCacheMgrState)1);
        }
        break;
    case 2:
        if (IsUnloadStateDone()) {
            SetState((NetCacheMgrState)-1);
        }
        break;
    default:
        break;
    }
}

unsigned int NetCacheMgr::GetServiceId() const { return mServiceId; }
NetCacheMgrFailType NetCacheMgr::GetFailType() const { return mFailType; }
const char *NetCacheMgr::GetXLSPFilter() const { return mXLSPFilter.c_str(); }

void NetCacheMgr::DebugClearCache() {
    if (IsReady()) {
        mCache->Clear();
    }
}

bool NetCacheMgr::IsUnloaded() const { return mState != 2; }
bool NetCacheMgr::IsReady() const { return (mState == 1 && !mHasFailed && mLoadCount == 1); }

bool NetCacheMgr::IsLocalFile(const char *file) const {
    if (!IsReady()) {
        return false;
    } else
        return mCache->FileCached(file);
}

void NetCacheMgr::SetFail(NetCacheMgrFailType n) {
    mHasFailed = true;
    mFailType = n;
}

void NetCacheMgr::EnterLoadState() {
    mHasFailed = false;
    LoadInit();
    if (!mHasFailed) {
        MILO_ASSERT(!mCache, 0x2ab);
        MILO_ASSERT(mLoadCacheSize, 0x2ac);
        mCache = new FileCache(mLoadCacheSize, kLoadStayBack, true, false);
        mLoadCacheSize = 0;
    }
}

void NetCacheMgr::EnterUnloadState() {
    UnloadInit();
    FOREACH (it, mNetLoaderRefs) {
        NetLoaderRef &cur = *it;
        if (cur.mRefCount > 0) {
            MILO_NOTIFY(
                "Loader for %s has %d reference(s) left unaccounted for!",
                cur.mName,
                cur.mRefCount
            );
            cur.mRefCount = 0;
        }
    }
    RELEASE(mCache);
}

bool NetCacheMgr::IsUnloadStateDone() const {
    return IsDoneUnloading() && mNetLoaderRefs.empty();
}

void NetCacheMgr::DeleteNetLoader(NetLoader *nl) {
    if (nl) {
        FOREACH (it, mNetLoaderRefs) {
            if (it->mNetLoader == nl) {
                it->mRefCount--;
                return;
            }
        }
    }
}

void NetCacheMgr::DeleteNetCacheLoader(NetCacheLoader *ncl) {
    if (ncl) {
        FOREACH (it, mNetLoaderRefs) {
            if (it->mCacheLoader == ncl) {
                it->mRefCount--;
                return;
            }
        }
    }
}

const NetCacheMgr::ServerData &NetCacheMgr::Server() const {
    auto s = mServers.begin();
    for (; s != mServers.end() && s->type != mServerType; s++)
        ;
    MILO_ASSERT(s != mServers.end(), 0x2D7);
    return *s;
}

unsigned short NetCacheMgr::GetPort() const { return Server().port; }
const char *NetCacheMgr::GetServerRoot() const { return Server().root; }
bool NetCacheMgr::IsServerLocal() const { return Server().local; }
bool NetCacheMgr::IsDebug() const { return Server().debug; }

Symbol NetCacheMgr::CheatNextServer() {
    auto s = mServers.begin();
    for (; s != mServers.end() && s->type != mServerType; s++)
        ;
    MILO_ASSERT(s != mServers.end(), 0x22B);
    ++s;
    if (s == mServers.end()) {
        s = mServers.begin();
    }
    mServerType = s->type;
    static Symbol local("local");
    if (UsingCD() && mServerType == local) {
        CheatNextServer();
    }
    return mServerType;
}

void NetCacheMgr::Load(NetCacheMgr::CacheSize cs) {
    if (mLoadCount == 0) {
        while (mState == 2) {
            NetCacheMgr::Poll();
        }
    }
    mLoadCount++;
    MILO_ASSERT(mLoadCount <= 2, 0x120);
    if (mState == 0 && !mHasFailed) {
        MILO_NOTIFY("NetCcaheMgr::Load() called before previous load had finished.");
    }
    mLoadCacheSize = cs == 0 ? 0x100000 : 0x500000;
    if (mLoadCount == 1) {
        SetState((NetCacheMgrState)0);
    }
}

void NetCacheMgr::Unload() {
    mLoadCount--;
    if (mLoadCount < 0) {
        MILO_NOTIFY("NetCacheMgr::Unload() called more times than NetCacheMgr::Load()!\n"
        );
        mLoadCount = 0;
    } else {
        SetState((NetCacheMgrState)2);
    }
}

NetLoader *NetCacheMgr::AddNetLoader(const char *cc, NetLoaderPos pos) {
    NetLoaderRef *pNetLoaderRef = AddLoaderRef(cc, (RefType)1, pos);
    if (!pNetLoaderRef)
        return nullptr;
    else {
        MILO_ASSERT(pNetLoaderRef && pNetLoaderRef->mNetLoader, 0x160);
        return pNetLoaderRef->mNetLoader;
    }
}

NetCacheLoader *NetCacheMgr::AddNetCacheLoader(const char *cc, NetLoaderPos pos) {
    NetLoaderRef *pNetLoaderRef = AddLoaderRef(cc, (RefType)0, pos);
    if (!pNetLoaderRef)
        return nullptr;
    else {
        MILO_ASSERT(pNetLoaderRef && pNetLoaderRef->mCacheLoader, 0x14F);
        return pNetLoaderRef->mCacheLoader;
    }
}

void NetCacheMgr::SetState(NetCacheMgrState state) {
    if (mState != state) {
        while (true) {
            if (mState == kNCMS_UnloadWaitForWrite) {
                mHasFailed = false;
            }
            if (mState == kNCMS_Nil && state == kNCMS_UnloadWaitForWrite) {
                TheDebug.Fail(
                    MakeString("NetCacheMgr attempted to move straight from kNCMS_Nil to kNCMS_Unload!\n"),
                    0
                );
            }
            mState = state;
            if (state != kNCMS_Nil)
                break;
            MILO_ASSERT(mNetLoaderRefs.empty(), 0x28B);
            if (mLoadCount <= 0)
                return;
            state = kNCMS_Load;
            if (mState == kNCMS_Load)
                return;
        }
        switch (state) {
        case kNCMS_Load:
            EnterLoadState();
            break;
        case kNCMS_Ready:
            ReadyInit();
            break;
        case kNCMS_UnloadWaitForWrite:
            EnterUnloadState();
            break;
        default:
            break;
        }
    }
}

void NetCacheMgr::OnInit(DataArray *pData) {
    MILO_ASSERT(pData, 0x46);
    static Symbol xlsp_service_id("xlsp_service_id");
    static Symbol xlsp_filter("xlsp_filter");
    mServiceId = 0;
    mXLSPFilter = pData->FindStr(xlsp_filter);
    static Symbol servers("servers");
    DataArray *serverArr = pData->FindArray(servers);
    static Symbol server("server");
    static Symbol port("port");
    static Symbol root("root");
    static Symbol local("local");
    MILO_ASSERT(mServers.empty(), 0x56);
    for (int i = 1; i < serverArr->Size(); i++) {
        ServerData serverData;
        DataArray *curArr = serverArr->Array(i);
        serverData.type = curArr->Sym(0);
        serverData.port = 0;
        serverData.server = gNullStr;
        static Symbol debug("debug");
        serverData.debug = false;
        curArr->FindData(debug, serverData.debug, false);
        static Symbol verify_ssl("verify_ssl");
        bool vSSL = true;
        curArr->FindData(verify_ssl, vSSL, false);
        bool isLocal = false;
        serverData.verifySSL = vSSL;
        curArr->FindData(local, isLocal, false);
        const char *serverStr = nullptr;
        serverData.local = isLocal;
        curArr->FindData(server, serverStr, false);
        serverData.server = serverStr;
        int serverPort = 0;
        curArr->FindData(port, serverPort, false);
        serverData.port = serverPort;
        serverData.root = curArr->FindStr(root);
        mServers.push_back(serverData);
    }
    static Symbol default_server("default_server");
    mServerType = pData->FindSym(default_server);
    FOREACH (it, mServers) {
        // ok then
    }
}

void NetLoaderRef::Poll() {
    MILO_ASSERT(IsValid(), 0x315);
    if (mCacheLoader) {
        mCacheLoader->PollLoading();
    } else {
        mNetLoader->PollLoading();
    }
}

bool NetLoaderRef::IsSafeToDelete() {
    MILO_ASSERT(IsValid(), 0x334);
    if (mCacheLoader) {
        return mCacheLoader->IsSafeToDelete();
    } else {
        return mNetLoader->IsSafeToDelete();
    }
}

void NetLoaderRef::DeleteLoader() {
    MILO_ASSERT(IsSafeToDelete(), 0x33A);
    RELEASE(mCacheLoader);
    RELEASE(mNetLoader);
}

bool NetLoaderRef::IsValid() const {
    // mCacheLoader XOR mNetLoader
    return (!mCacheLoader || !mNetLoader) && (mCacheLoader || mNetLoader);
}

bool NetLoaderRef::NeedsToDownload() {
    MILO_ASSERT(IsValid(), 0x31B);
    if (mCacheLoader) {
        int state = (int)mCacheLoader->mState;
        return state == 1 || state == 2;
    }
    return true;
}

bool NetLoaderRef::IsDownloading() {
    MILO_ASSERT(IsValid(), 0x321);
        return !mCacheLoader || (int)mCacheLoader->mState == 2;
}

bool NetLoaderRef::IsLoadedOrFailed() {
    MILO_ASSERT(IsValid(), 0x327);

    if (!mCacheLoader) {
        if (mNetLoader) {
            return true;
        }
        return false;
    }

    if (!mNetLoader) {
        return true;
    }

    bool loaded = mCacheLoader->IsLoaded();
    if (loaded) {
        return true;
    }

    char failed = mCacheLoader->HasFailed();
    return failed != '\0';
}

void NetCacheMgr::PollLoaders() {
    bool firstDownload = true;
    std::list<NetLoaderRef>::iterator it = mNetLoaderRefs.begin();
    while (it != mNetLoaderRefs.end()) {
        MILO_ASSERT(it->IsValid(), 0xE9);
        if (!it->NeedsToDownload() || it->IsLoadedOrFailed()) {
            it->Poll();
        } else if (firstDownload) {
            it->Poll();
            firstDownload = false;
        }
        if (it->mRefCount < 1 && it->IsSafeToDelete()) {
            it->DeleteLoader();
            it = mNetLoaderRefs.erase(it);
        } else {
            ++it;
        }
    }
}

NetLoaderRef &NetLoaderRef::operator=(const NetLoaderRef &other) {
    mName = other.mName;
    mRefCount = other.mRefCount;
    mNetLoader = other.mNetLoader;
    mCacheLoader = other.mCacheLoader;
    return *this;
}

NetLoaderRef *NetCacheMgr::AddLoaderRef(const char *name, RefType type, NetLoaderPos pos) {
    NetLoaderRef *pNetLoaderRef = NULL;
    if (*name == '\0') {
        return NULL;
    }
    if (!IsReady()) {
        return NULL;
    }
    std::list<NetLoaderRef>::iterator it = mNetLoaderRefs.begin();
    for (; it != mNetLoaderRefs.end(); ++it) {
        NetLoaderRef &ref = *it;
        if (stricmp(ref.mName.c_str(), name) == 0) {
            if ((RefType)0 == type && ref.mCacheLoader) {
                MILO_ASSERT(ref.mNetLoader == NULL, 0x17A);
            } else if ((RefType)1 == type && ref.mNetLoader) {
                MILO_ASSERT(ref.mCacheLoader == NULL, 0x180);
            } else {
                TheDebug << MakeString("Found loader for %s, but it was not type %d.\n", ref.mName.c_str(), (int)type);
                continue;
            }
            pNetLoaderRef = &ref;
            break;
        }
    }

    NetLoaderRef newRef;
    newRef.mRefCount = 0;
    newRef.mNetLoader = NULL;
    newRef.mCacheLoader = NULL;

    if (!pNetLoaderRef) {
        if ((unsigned int)type == 1) {
            NetLoader *nl = NetLoader::Create(String(name));
            String s(name);
            NetLoaderRef tmp = { String(s), 0, nl, NULL };
            newRef = tmp;
        } else if ((unsigned int)type == 0) {
            NetCacheLoader *ncl = new NetCacheLoader(mCache, String(name));
            String s(name);
            NetLoaderRef tmp = { String(s), 0, NULL, ncl };
            newRef = tmp;
        } else {
            MILO_FAIL("Unknown ref type %d.\n", type);
        }

        if ((unsigned int)pos == 1) {
            mNetLoaderRefs.insert(mNetLoaderRefs.end(), newRef);
            pNetLoaderRef = &mNetLoaderRefs.back();
        } else if ((unsigned int)pos == 0) {
            std::list<NetLoaderRef>::iterator insertIt;
            for (insertIt = mNetLoaderRefs.begin(); insertIt != mNetLoaderRefs.end(); ++insertIt) {
                if (!insertIt->IsDownloading() && !insertIt->IsLoadedOrFailed()) {
                    break;
                }
            }
            std::list<NetLoaderRef>::iterator inserted = mNetLoaderRefs.insert(insertIt, newRef);
            pNetLoaderRef = &*inserted;
        } else {
            MILO_FAIL("Unknown net loader pos %d.\n", pos);
        }

        MILO_ASSERT(pNetLoaderRef, 0x1C2);
    }
    pNetLoaderRef->mRefCount++;
    return pNetLoaderRef;
}

void NetCacheMgrInit() {
    MILO_ASSERT(TheNetCacheMgr == NULL, 0x1f);
    TheNetCacheMgr = new NetCacheMgrXbox();
}

void NetCacheMgrTerminate() { RELEASE(TheNetCacheMgr); }
