#pragma once

#include "obj/Msg.h"
#include "meta/StreamPlayer.h"
#include "movie/TexMovie.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "utl/NetCacheLoader.h"
#include "utl/NetCacheMgr.h"
#include "utl/Str.h"

class StorePreviewMgr : public Hmx::Object {
public:
    // Hmx::Object
    virtual ~StorePreviewMgr();
    virtual DataNode Handle(DataArray *, bool);

    StorePreviewMgr();
    bool GetLastFailure(NetCacheMgrFailType &);
    bool IsPlaying() const;
    void ClearCurrentPreview();
    void SetCurrentPreviewFile(String const &, TexMovie *);
    bool IsDownloadingFile(String const &);
    bool AllowPreviewDownload(String const &);
    void Poll();

    NetCacheLoader *mNetCacheLoader; // 0x28
    std::list<String> mDownloadQueue; // 0x2c
    float mAttenuation;
    bool mLoopForever;
    String mCurrentPreviewFile;
    StreamPlayer *mStreamPlayer;
    NetCacheMgrFailType mLastFailType;
    bool mHasFailure;
    TexMovie *mTexMovie;

protected:
    void PlayCurrentPreview();
    void AddToDownloadQueue(String const &);
};

DECLARE_MESSAGE(PreviewDownloadCompleteMsg, "preview_download_complete_msg")
PreviewDownloadCompleteMsg(bool b1, bool b2) : Message(Type(), b1, b2){};
END_MESSAGE