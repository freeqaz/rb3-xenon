#include "meta/StorePreviewMgr.h"

#include "meta/StreamPlayer.h"
#include "movie/TexMovie.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/System.h"
#include "rndobj/MetaMaterial.h"
#include "synth/MoggClip.h"
#include "utl/NetCacheLoader.h"
#include "utl/NetCacheMgr.h"
#include "utl/Str.h"
#include "utl/Symbol.h"

// Retail ctor @0x827B1FC8 — virtual-base form; see StorePreviewMgr.h.
StorePreviewMgr::StorePreviewMgr() : mStreamPlayer(0), mNetCacheLoader(0) {
    mStreamPlayer = new StreamPlayer();
    MILO_ASSERT(mStreamPlayer, 0x1d);
    SetName("store_preview_mgr", ObjectDir::Main());
}

// Retail dtor @0x827B2218.
StorePreviewMgr::~StorePreviewMgr() {
    RELEASE(mStreamPlayer);
    if (mNetCacheLoader) {
        TheNetCacheMgr->DeleteNetCacheLoader(mNetCacheLoader);
        mNetCacheLoader = 0;
    }
}

bool StorePreviewMgr::IsPlaying() const {
    return (!mCurrentPreviewFile.empty()
            && TheNetCacheMgr->IsLocalFile(mCurrentPreviewFile.c_str()));
}

void StorePreviewMgr::ClearCurrentPreview() {
    if (!mCurrentPreviewFile.empty()) {
        mCurrentPreviewFile = gNullStr;
        PlayCurrentPreview();
    }
}

void StorePreviewMgr::SetCurrentPreviewFile(String const &str) {
    if (mCurrentPreviewFile == str)
        return;
    mCurrentPreviewFile = str;
    PlayCurrentPreview();
}

bool StorePreviewMgr::IsDownloadingFile(String const &str) {
    if (mNetCacheLoader) {
        if (str == mNetCacheLoader->GetRemotePath()) {
            return true;
        }
    }
    return mDownloadQueue.end()
        != std::find(mDownloadQueue.begin(), mDownloadQueue.end(), str);
}

bool StorePreviewMgr::AllowPreviewDownload(String const &str) {
    if (mNetCacheLoader) {
        if (str == mNetCacheLoader->GetRemotePath())
            return false;
    }
    if (TheNetCacheMgr->IsLocalFile(str.c_str()))
        return false;
    else
        return std::find(mDownloadQueue.begin(), mDownloadQueue.end(), str)
            == mDownloadQueue.end();
}

// Retail pools the two PlayFile float args into a single 8-byte .rdata object
// at 0x82114E48 ({-3.0f, 0.0f}), addressed as base+0 / base+4 — NOT as the
// globally-interned __real@00000000 COMDAT (which lives at 0x82000D78 and is
// referenced 2188x elsewhere). Same idiom as DC3's RndOverlay::Draw
// (dc3-decomp/src/system/rndobj/Overlay.cpp:54, sDrawFloats).
static const float sPlayFileFloats[2] = { -3.0f, 0.0f };

// Retail @0x827B19B8 — no TexMovie branch, attenuation is the literal -3.0f.
void StorePreviewMgr::PlayCurrentPreview() {
    MILO_ASSERT(mStreamPlayer, 0xd8);
    if (mCurrentPreviewFile.empty()
        || !TheNetCacheMgr->IsLocalFile(mCurrentPreviewFile.c_str())) {
        mStreamPlayer->StopPlaying();
    } else {
        String str(mCurrentPreviewFile.c_str());
        int len = str.length();
        if (str.find(".mogg", len - 5) != String::npos) {
            str.erase(len - 5);
        }
        mStreamPlayer->PlayFile(
            str.c_str(), sPlayFileFloats[0], sPlayFileFloats[1], true
        );
    }
}

void StorePreviewMgr::AddToDownloadQueue(String const &str) {
    if (mNetCacheLoader) {
        if (str == mNetCacheLoader->GetRemotePath()) {
            return;
        }
    }
    if (!TheNetCacheMgr->IsLocalFile(str.c_str())) {
        if (std::find(mDownloadQueue.begin(), mDownloadQueue.end(), str)
            == mDownloadQueue.end())
            mDownloadQueue.push_back(str);
    }
}

BEGIN_HANDLERS(StorePreviewMgr)
HANDLE_ACTION(clear_current_preview, ClearCurrentPreview())
HANDLE_ACTION(set_current_preview_file, SetCurrentPreviewFile(_msg->Str(2)))
HANDLE_ACTION(download_preview_file, AddToDownloadQueue(_msg->Str(2)))
HANDLE_EXPR(is_downloading_file, IsDownloadingFile(_msg->Str(2)))
HANDLE_EXPR(allow_preview_download, AllowPreviewDownload(_msg->Str(2)))
HANDLE_SUPERCLASS(MsgSource)
END_HANDLERS

// Retail @0x827B1D60 (pinned, was fn_827B1D60).
void StorePreviewMgr::Poll() {
    MILO_ASSERT(mStreamPlayer, 0x6f);
    mStreamPlayer->Poll();
    if (mNetCacheLoader) {
        if (mNetCacheLoader->IsLoaded()) {
            TheNetCacheMgr->DeleteNetCacheLoader(mNetCacheLoader);
            mNetCacheLoader = 0;
            PlayCurrentPreview();
            static PreviewDownloadCompleteMsg msg;
            MsgSource::Handle(msg, false);
        } else if (mNetCacheLoader->HasFailed()) {
            TheNetCacheMgr->DeleteNetCacheLoader(mNetCacheLoader);
            mNetCacheLoader = 0;
        }
    }
    while (mDownloadQueue.begin() != mDownloadQueue.end()) {
        if (!TheNetCacheMgr->IsLocalFile(mDownloadQueue.front().c_str()))
            break;
        mDownloadQueue.erase(mDownloadQueue.begin());
    }
    if (!mNetCacheLoader && !mDownloadQueue.empty()) {
        MILO_ASSERT(!TheNetCacheMgr->IsLocalFile(mDownloadQueue.front().c_str()), 0xa5);
        mNetCacheLoader = TheNetCacheMgr->AddNetCacheLoader(
            mDownloadQueue.front().c_str(), (NetLoaderPos)1
        );
        mDownloadQueue.pop_front();
    }
}
