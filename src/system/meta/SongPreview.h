#pragma once
#include "meta/SongMgr.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/ContentMgr.h"
#include "movie/TexMovie.h"
#include "synth/Faders.h"
#include "utl/Symbol.h"
#include "synth/Stream.h"

class SongPreview : public ContentMgr::Callback, public Hmx::Object {
public:
    enum State {
        kIdle = 0,
        kMountingSong = 1,
        kPreparingSong = 2,
        kDeletingSong = 3,
        kPlayingSong = 4,
        kFadingOutSong = 5,
    };

    SongPreview(const SongMgr &);
    // ContentMgr::Callback
    virtual ~SongPreview();
    virtual void ContentMounted(const char *, const char *);
    virtual void ContentFailed(const char *);

    // Hmx::Object
    virtual DataNode Handle(DataArray *, bool);

    bool IsWaitingToDelete() const;
    bool IsFadingOut() const;
    void SetMusicVol(float);
    void Init();
    void Terminate();
    void Start(Symbol, TexMovie *);
    // Retail RB3-360 has a single-arg Start(Symbol) overload (fn_827808B0);
    // the two-arg TexMovie form is a DC3-newer addition. MusicLibrary's
    // Clear/CheckSongPreview call the single-arg form. rb3-Wii oracle agrees.
    void Start(Symbol);
    void PreparePreview();
    void Poll();
    DataNode OnStart(DataArray *);
    void SetCrowdSingVol(float);

    bool HasMovie() const { return mTexMovie && !mTexMovie->IsEmpty(); }
    float PreviewDb() const { return mPreviewDb; }

    static const float kSilenceVal;

private:
    const SongMgr &mSongMgr; // 0x30
    Stream *mStream; // 0x34
    // NOTE: retail RB3-360 lacks the DC3-added TexMovie preview member here;
    // its absence shifts every member below by -0x14 (20 bytes). We keep the
    // member (the .cpp/native build use it) but relocate it to the END of the
    // class so the front members match retail's compiled offsets. Verified via
    // objdiff: PreparePreview's 9 this-relative accesses all shift by exactly
    // +20 above this offset. (RELAYOUT b14, 2026-07)
    Fader *mFader;
    Fader *mMusicFader;
    Fader *mCrowdSingFader;
    int mNumChannels;
    float mAttenuation;
    float mFadeTime;
    bool mRestart;
    bool mLoopForever;
    bool unk6e;
    bool unk6f;
    State mState;
    Symbol mSong;
    Symbol mSongContent;
    float mStartMs;
    float mEndMs;
    float mStartPreviewMs;
    float mEndPreviewMs;
    bool mRegisteredWithCM;
    bool mSameSongRequested;
    bool mSecurePreview;
    // The following three members are DC3-newer additions absent in retail
    // RB3-360. Relocated to the END of the class so the members above keep
    // retail's compiled offsets (verified: PreparePreview accesses shift by
    // exactly the removed size). They remain usable by the .cpp / native.
    bool mInitted;
    float mPreviewDb;
    ObjPtr<TexMovie> mTexMovie;

    void DetachFader(Fader *);
    void PrepareFaders(const SongInfo *);
    void PrepareSong(Symbol);
};
