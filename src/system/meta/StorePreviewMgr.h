#pragma once

#include "obj/Msg.h"
#include "meta/StreamPlayer.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "utl/NetCacheLoader.h"
#include "utl/NetCacheMgr.h"
#include "utl/Str.h"

/** Retail RB3-360 (TU5) layout, ground-truthed from the XEX.
 *
 *  The base is `MsgSource`, which is `public virtual Hmx::Object` — NOT the
 *  plain `Hmx::Object` DC3 (newer) refactored it down to. Evidence:
 *
 *  - ctor @0x827B1FC8 is textbook MSVC virtual-base codegen:
 *        if (mostDerived & 1) { *this = &vbtable_82114FC8;
 *                               Hmx::Object::Object(this + 0x38); }
 *        MsgSource::MsgSource(this, 0);                  // 0x82767FB0, flag 0
 *        *(this + vbt[1])     = &vtbl_82114F74;          // INDIRECT vbase off
 *        *(this + vbt[1] - 4) = vbt[1] - 0x38;           // vtordisp init
 *        String::String(this + 0x18); ... new(0x38) StreamPlayer ...
 *        SetName("store_preview_mgr", ObjectDir::Main());
 *  - dtor @0x827B2218 mirrors it and ends with
 *        ~String(this + 0x18); ~MsgSource(this + 0x1c);
 *    (`this + 0x1c` is MsgSource's own static vbase offset — the adjusted-this
 *    convention ??1MsgSource@@UAA@XZ @0x827680D0 itself decodes with.)
 *  - Poll @0x827B1D60 reaches MsgSource::Handle via `this + 0x1c` likewise.
 *  - rb3-Wii oracle agrees: `class StorePreviewMgr : public MsgSource`.
 *
 *  Offsets:
 *      0x00  MsgSource base   vbptr 0x00, mSinks 0x04, mEventSinks 0x0c,
 *                             mExporting 0x14; nvsize 0x18
 *      0x18  String           mCurrentPreviewFile   (ctor/dtor at this+0x18;
 *                             PlayCurrentPreview reads c_str at this+0x20)
 *      0x24  StreamPlayer*    mStreamPlayer         (Poll: StreamPlayer::Poll)
 *      0x28  NetCacheLoader*  mNetCacheLoader
 *      0x2c  list<String>     mDownloadQueue        (self-ref node 0x2c/0x30)
 *      0x34  <vtordisp>       compiler-generated
 *      0x38  Hmx::Object      VIRTUAL base (0x28 bytes)
 *      ----  sizeof == 0x60
 *
 *  Retail has none of DC3's mAttenuation / mLoopForever / mLastFailType /
 *  mHasFailure / mTexMovie, and none of the rb3-Wii DEV build's
 *  mRequestedPreview / mPreviewRequestedSeconds / mIsPreviewPlaying — either
 *  set would overflow 0x60.
 */
class StorePreviewMgr : public MsgSource {
public:
    StorePreviewMgr();
    // Hmx::Object
    virtual DataNode Handle(DataArray *, bool);
    virtual ~StorePreviewMgr();

    bool IsPlaying() const;
    void ClearCurrentPreview();
    void SetCurrentPreviewFile(String const &);
    bool IsDownloadingFile(String const &);
    bool AllowPreviewDownload(String const &);
    void Poll();

    String mCurrentPreviewFile; // 0x18
    StreamPlayer *mStreamPlayer; // 0x24
    NetCacheLoader *mNetCacheLoader; // 0x28
    std::list<String> mDownloadQueue; // 0x2c
    // 0x34 vtordisp, 0x38 Hmx::Object virtual base, sizeof 0x60

protected:
    void PlayCurrentPreview();
    void AddToDownloadQueue(String const &);
};

DECLARE_MESSAGE(PreviewDownloadCompleteMsg, "preview_download_complete_msg")
PreviewDownloadCompleteMsg() : Message(Type()){};
PreviewDownloadCompleteMsg(bool b1, bool b2) : Message(Type(), b1, b2){};
END_MESSAGE
