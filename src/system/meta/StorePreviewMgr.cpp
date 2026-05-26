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

StorePreviewMgr::StorePreviewMgr()
    : mAttenuation(0.0f), mLoopForever(1), mStreamPlayer(nullptr), mNetCacheLoader(0), mHasFailure(0) {
    mStreamPlayer = new StreamPlayer();
    MILO_ASSERT(mStreamPlayer, 0x1d);
    DataArray *d = SystemConfig("song_select", "sound");
    d->FindData("loop_forever", mLoopForever);
    d->FindData("attenuation", mAttenuation);
    SetName("store_preview_mgr", ObjectDir::Main());
}

StorePreviewMgr::~StorePreviewMgr() {
    RELEASE(mStreamPlayer);
    if (mNetCacheLoader) {
        TheNetCacheMgr->DeleteNetCacheLoader(mNetCacheLoader);
        mNetCacheLoader = 0;
    }
}

bool StorePreviewMgr::GetLastFailure(NetCacheMgrFailType &t) {
    if (mHasFailure) {
        t = mLastFailType;
        mHasFailure = false;
        return true;
    }
    return false;
}

bool StorePreviewMgr::IsPlaying() const {
    return (!mCurrentPreviewFile.empty() && TheNetCacheMgr->IsLocalFile(mCurrentPreviewFile.c_str()));
}

void StorePreviewMgr::ClearCurrentPreview() {
    if (!mCurrentPreviewFile.empty()) {
        mCurrentPreviewFile = gNullStr;
        PlayCurrentPreview();
    }
}

void StorePreviewMgr::SetCurrentPreviewFile(String const &str, TexMovie *tex) {
    if (mCurrentPreviewFile == str && mTexMovie == tex)
        return;
    mTexMovie = tex;
    mCurrentPreviewFile = str;
    PlayCurrentPreview();
}

bool StorePreviewMgr::IsDownloadingFile(String const &str) {
    if (mNetCacheLoader) {
        if (str == mNetCacheLoader->GetRemotePath()) {
            return true;
        }
    }
    return mDownloadQueue.end() != std::find(mDownloadQueue.begin(), mDownloadQueue.end(), str);
}

bool StorePreviewMgr::AllowPreviewDownload(String const &str) {
    if (mNetCacheLoader) {
        if (str == mNetCacheLoader->GetRemotePath())
            return false;
    }
    if (TheNetCacheMgr->IsLocalFile(str.c_str()))
        return false;
    else
        return std::find(mDownloadQueue.begin(), mDownloadQueue.end(), str) == mDownloadQueue.end();
}

void StorePreviewMgr::PlayCurrentPreview() {
    MILO_ASSERT(mStreamPlayer, 0xd8);
    if (mCurrentPreviewFile.empty() || !TheNetCacheMgr->IsLocalFile(mCurrentPreviewFile.c_str())) {
        mStreamPlayer->StopPlaying();
        if (mTexMovie) {
            FilePath fp(gNullStr);
            mTexMovie->SetFile(fp);
        }
    } else {
        String str(mCurrentPreviewFile.c_str());
        if (mTexMovie) {
            mStreamPlayer->StopPlaying();
            {
                FilePath fp(mCurrentPreviewFile.c_str());
                mTexMovie->SetFile(fp);
            }
            mTexMovie->SetVolume(-mAttenuation);
        } else {
            int len = str.length();
            if (str.find(".mogg", len - 5) != String::npos) {
                str.erase(len - 5);
            }
            mStreamPlayer->PlayFile(str.c_str(), -mAttenuation, 0.0f, mLoopForever);
        }
    }
}

void StorePreviewMgr::AddToDownloadQueue(String const &str) {
    if (mNetCacheLoader) {
        if (str == mNetCacheLoader->GetRemotePath()) {
            return;
        }
    }
    if (!TheNetCacheMgr->IsLocalFile(str.c_str())) {
        if (std::find(mDownloadQueue.begin(), mDownloadQueue.end(), str) == mDownloadQueue.end())
            mDownloadQueue.push_back(str);
    }
}

BEGIN_HANDLERS(StorePreviewMgr)
HANDLE_ACTION(clear_current_preview, ClearCurrentPreview())
HANDLE_ACTION(set_current_preview_file, SetCurrentPreviewFile(_msg->Str(2), nullptr))
HANDLE_ACTION(set_current_preview_movie, SetCurrentPreviewFile(_msg->Str(2), _msg->Obj<TexMovie>(3)))
HANDLE_ACTION(download_preview_file, AddToDownloadQueue(_msg->Str(2)))
HANDLE_EXPR(is_downloading_file, IsDownloadingFile(_msg->Str(2)))
HANDLE_EXPR(allow_preview_download, AllowPreviewDownload(_msg->Str(2)))
HANDLE_EXPR(is_playing, IsPlaying())
HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

void StorePreviewMgr::Poll() {
    MILO_ASSERT(mStreamPlayer, 0x6f);
    mStreamPlayer->Poll();
    if (mNetCacheLoader) {
        bool isCurrentFile = mCurrentPreviewFile == mNetCacheLoader->GetRemotePath();
        if (mNetCacheLoader->IsLoaded()) {
            TheNetCacheMgr->IsLocalFile(mNetCacheLoader->GetRemotePath());
            TheNetCacheMgr->DeleteNetCacheLoader(mNetCacheLoader);
            mNetCacheLoader = 0;
            if (isCurrentFile) {
                PlayCurrentPreview();
            }
            static PreviewDownloadCompleteMsg msg(true, false);
            msg[1] = isCurrentFile;
            Handle(msg, true);
        } else if (mNetCacheLoader->HasFailed()) {
            mHasFailure = true;
            mLastFailType = mNetCacheLoader->GetFailType();
            TheNetCacheMgr->DeleteNetCacheLoader(mNetCacheLoader);
            mNetCacheLoader = 0;
            static PreviewDownloadCompleteMsg msg(false, false);
            msg[1] = isCurrentFile;
            Handle(msg, true);
        }
    }
    std::list<String>::iterator it = mDownloadQueue.begin();
    while (it != mDownloadQueue.end() && TheNetCacheMgr->IsLocalFile(it->c_str())) {
        it = mDownloadQueue.erase(it);
    }
    if (!mNetCacheLoader && mDownloadQueue.begin() != mDownloadQueue.end()) {
        MILO_ASSERT(!TheNetCacheMgr->IsLocalFile(mDownloadQueue.front().c_str()), 0xa5);
        mNetCacheLoader = TheNetCacheMgr->AddNetCacheLoader(mDownloadQueue.front().c_str(), (NetLoaderPos)1);
        mDownloadQueue.erase(mDownloadQueue.begin());
    }
}