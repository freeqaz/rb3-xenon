#include "meta/PreloadPanel.h"
#include "meta/SongMgr.h"
#include "obj/Data.h"
#include "obj/DirLoader.h"
#include "obj/Object.h"
#include "os/ContentMgr.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/FileCache.h"
#include "ui/UIPanel.h"
#include "utl/Loader.h"
#include "utl/Std.h"
#include "utl/Symbol.h"

#ifdef HX_NATIVE
FileCache *PreloadPanel::sCache;
#endif

// NOTE(laneBQ2): was the member `mMaxCacheSize`; retail RB3 has no such member
// (see PreloadPanel.h). File-scope static keeps FindData's lvalue requirement.
static int gMaxCacheSize = 0x500000;

#pragma region Hmx::Object

PreloadPanel::PreloadPanel()
    : mPreloadResult(kPreloadInProgress), mMounted(0), mAppReadFailureHandler(), mContentCorrupt(0),
      mSongDoesNotExist(0) {
    if (!sCache) {
        // NOTE(NCCC-0731-ab7e/f268): two RB3-vs-DC3 divergences here.
        // (1) size is the literal 0x500000, not a load of the mutable
        //     gMaxCacheSize global (retail emits `lis r4, 0x50`); any
        //     SetTypeDef override is reapplied later by StartCache()'s
        //     sCache->SetSize(gMaxCacheSize).
        // (2) THREE args, not four -- DC3 added a trailing bool to the
        //     FileCache ctor. Retail materializes only r4/r5/r6 for this call.
        // rb3-Wii's PreloadPanel is `new FileCache(0x500000, kLoadBack, true)`,
        // i.e. it agrees on both points.
        sCache = new FileCache(0x500000, kLoadBack, true);
    }
}

PreloadPanel::~PreloadPanel() {}

BEGIN_HANDLERS(PreloadPanel)
    HANDLE_MESSAGE(ContentReadFailureMsg)
    HANDLE_MESSAGE(UITransitionCompleteMsg)
    HANDLE_SUPERCLASS(UIPanel)
END_HANDLERS

void PreloadPanel::SetTypeDef(DataArray *d) {
    UIPanel::SetTypeDef(d);
    d->FindData("max_cache_size", gMaxCacheSize, false);
    CheckTypeDef("song_mgr");
    CheckTypeDef("current_song");
    CheckTypeDef("on_preload_ok");
    CheckTypeDef("preload_files");
}

#pragma endregion
#pragma region UIPanel

void PreloadPanel::Load() {
    TheContentMgr.RegisterCallback(this, false);
    UIPanel::Load();
    TheLoadMgr.SetLoaderPeriod(14.0f);
    mPreloadResult = kPreloadInProgress;
    mAppReadFailureHandler = TheContentMgr.SetReadFailureHandler(this);
    MILO_ASSERT(mAppReadFailureHandler, 0x50);
    mContentCorrupt = false;
    mCorruptContentName = gNullStr;
    Symbol cur = CurrentSong();
    if (cur.Null()) {
        MILO_NOTIFY("Trying to preload null song");
    }
    SongMgr *song_mgr = FindSongMgr();
    MILO_ASSERT(song_mgr, 0x5E);
#ifdef HX_NATIVE
    // Native: no ark song content to mount/cache — skip directly to success
    fprintf(stderr, "DC3 Native: PreloadPanel::Load — skipping content mount/cache for '%s'\n", cur.Str());
    mMounted = true;
    mPreloadResult = kPreloadSuccess;
#else
    if (!(!song_mgr->HasSong(cur, false))) {
        song_mgr->GetContentNames(cur, mContentNames);
        for (auto it = mContentNames.begin(); it != mContentNames.end();) {
            if (!TheContentMgr.MountContent(*it)) {
                ++it;
                mMounted = false;
            } else {
                it = mContentNames.erase(it);
            }
        }
    } else {
        mSongDoesNotExist = true;
    }
    if (mContentNames.empty()) {
        StartCache();
    }
#endif
    mContentNames.clear();
    mSongDoesNotExist = false;
}

bool PreloadPanel::IsLoaded() const {
    if (!UIPanel::IsLoaded())
        return false;
    else
        return mPreloadResult != kPreloadInProgress;
}

void PreloadPanel::Unload() {
    mContentNames.clear();
    UIPanel::Unload();
}

void PreloadPanel::PollForLoading() {
    UIPanel::PollForLoading();
    if (UIPanel::IsLoaded()) {
        if (!mMounted && mContentNames.empty()) {
            StartCache();
        }
        if (mPreloadResult == kPreloadInProgress && mMounted && sCache->DoneCaching()) {
            if (mSongDoesNotExist) {
                mPreloadResult = kPreloadFailure;
            } else {
                FileCache::PollAll();
                FOREACH (it, mPreloadedFiles) {
                    if (!CheckFileCached(it->c_str())) {
                        mPreloadResult = kPreloadFailure;
                    }
                }
            }
            if (mPreloadResult != kPreloadFailure) {
                mPreloadResult = kPreloadSuccess;
            }
        }
    }
}

void PreloadPanel::FinishLoad() {
    UIPanel::FinishLoad();
    // RB3 retail does NOT reset the loader period here -- this call is a
    // DC3-era addition (our engine source is a verbatim DC3 copy, and DC3 is
    // NEWER than RB3).  Confirmed absent in the rb3-Wii oracle, and retail is
    // exactly 24 bytes / 6 instructions shorter: the two 10.0f stores into
    // TheLoadMgr+0x10/+0x14 plus their address/constant materialisation.
    TheContentMgr.UnregisterCallback(this, true);
    ClearAndShrink(mPreloadedFiles);
    TheContentMgr.SetReadFailureHandler(mAppReadFailureHandler);
}

#pragma endregion
#pragma region ContentMgr::Callback

void PreloadPanel::ContentMounted(const char *c1, const char *c2) {
    OnContentMountedOrFailed(c1);
}

void PreloadPanel::ContentFailed(char const *c) {
    const char *cc20 = gNullStr;
    if (TheContentMgr.IsCorrupt(c, cc20)) {
        mContentCorrupt = true;
        mCorruptContentName = cc20;
    }
    OnContentMountedOrFailed(c);
}

#pragma endregion
#pragma region PreloadPanel

Symbol PreloadPanel::CurrentSong() const {
    static Symbol current_song("current_song");
    return TypeDef()->FindSym(current_song);
}

void PreloadPanel::CheckTypeDef(Symbol s) {
    if (!TypeDef()->FindArray(s, false))
        MILO_NOTIFY(
            "PreloadPanel %s missing %s handler (%s)", Name(), s, TypeDef()->File()
        );
}

bool PreloadPanel::CheckFileCached(const char *cc) {
    if (!*cc || sCache->FileCached(cc)) {
        return true;
    } else {
        MILO_NOTIFY("Could not cache %s", cc);
        return false;
    }
}

SongMgr *PreloadPanel::FindSongMgr() const {
    static Symbol song_mgr("song_mgr");
    return TypeDef()->FindArray(song_mgr)->Obj<SongMgr>(1);
}

DataNode PreloadPanel::OnMsg(const ContentReadFailureMsg &msg) {
    mContentCorrupt = msg->Int(2);
    mCorruptContentName = msg->Str(3);
    return 1;
}

DataNode PreloadPanel::OnMsg(const UITransitionCompleteMsg &msg) {
    MILO_ASSERT(mPreloadResult != kPreloadInProgress, 0x153);
    if (mPreloadResult == kPreloadSuccess) {
        static Message msg("on_preload_ok");
        HandleType(msg);
    } else {
        static Message msg("on_preload_failed");
        if (HandleType(msg) == DATA_UNHANDLED) {
            MILO_ASSERT(mAppReadFailureHandler, 0x15F);
            static ContentReadFailureMsg msg(false, gNullStr);
            msg[0] = mContentCorrupt;
            msg[1] = mCorruptContentName;
            mAppReadFailureHandler->Handle(msg, true);
        }
    }
    mAppReadFailureHandler = nullptr;
    return DATA_UNHANDLED;
}

void PreloadPanel::OnContentMountedOrFailed(char const *contentName) {
    if (!mContentNames.empty()) {
        MILO_ASSERT(contentName, 0x12b);
        for (std::vector<Symbol>::iterator it = mContentNames.begin();
             it != mContentNames.end();) {
            Symbol s = *it;
            if (s == contentName) {
                it = mContentNames.erase(it);
            } else {
                it++;
            }
        }
    }
}

void PreloadPanel::StartCache() {
    MILO_ASSERT(mContentNames.empty(), 0xF8);
    mMounted = true;
    MILO_ASSERT(sCache, 0xFB);
    sCache->Clear();
    sCache->SetSize(gMaxCacheSize);
    sCache->StartSet(0);
    if (!mSongDoesNotExist) {
        static Symbol preload_files("preload_files");
        DataArray *files = TypeDef()->FindArray(preload_files);
        for (int i = 1; i < files->Size(); i++) {
            DataArray *arr = files->Array(i);
            const char *path = arr->Str(0);
            MILO_ASSERT(path, 0x109);
            bool b1 = arr->Int(1);
            if (!b1 || FileExists(DirLoader::CachedPath(path, false), 0, nullptr)) {
                sCache->Add(path, 1, path);
                mPreloadedFiles.push_back(path);
            }
        }
    }
    sCache->EndSet();
}

// Lane-AE b2 scatter force-emit: retail placed UIGuide's OBJ_CLASSNAME COMDAT
// (?StaticClassName@UIGuide@@SA?AVSymbol@@XZ) inside the .text span pinned to
// default/PreloadPanel. It is defined inline in the class body, so it is only
// emitted where it is odr-used -- nothing in this TU used it.
#include "ui/UIGuide.h"
Symbol ForceEmit_UIGuide_StaticClassName() { return UIGuide::StaticClassName(); }
