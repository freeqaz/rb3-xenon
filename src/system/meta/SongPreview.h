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
    // Retail RB3-360 has exactly ONE Start, and it takes a single Symbol:
    // fn_827A5790, whose prologue captures only r3 (this) and r4 (the Symbol)
    // and never reads r5, and whose 4 callers all stage r3/r4 only. The
    // two-arg TexMovie form is a DC3-newer addition, kept native-only below.
    // ⚠ This comment previously cited "fn_827808B0" as the single-arg form.
    // That is a TU0-era address and is INVALID since the TU5 flip — 0x827808B0
    // is mid-function branch code on TU5, not a function start.
    void Start(Symbol);
#ifdef HX_NATIVE
    void Start(Symbol, TexMovie *);
#endif
    void PreparePreview();
    void Poll();
    DataNode OnStart(DataArray *);
    void SetCrowdSingVol(float);

#ifdef HX_NATIVE
    bool HasMovie() const { return mTexMovie && !mTexMovie->IsEmpty(); }
    float PreviewDb() const { return mPreviewDb; }
#else
    // Retail RB3-360 has no TexMovie song preview at all (see the member note
    // below) -- the movie branches fold away entirely.
    bool HasMovie() const { return false; }
#endif

    static const float kSilenceVal;

private:
    const SongMgr &mSongMgr; // 0x2c
    Stream *mStream; // 0x30
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
    // ^ mSecurePreview is the LAST member in retail RB3-360: sizeof(SongPreview)
    // is 112 (0x70) there, proved by MetaPanel (mMusic @0x60 matches, and the
    // bool right after the embedded `SongPreview mSongPreview` @0x64 sits at
    // 0xD4 in the target vs 0xE4 with the DC3 tail present -- a 16-byte delta).
    // The retail ctor (??0SongPreview@@QAA@ABVSongMgr@@@Z) corroborates: it
    // stores exactly 0x2c/0x30/0x34/0x38/0x3c/0x40/0x44/0x50/0x54/0x58/0x5c/
    // 0x60/0x64/0x68/0x6c/0x6d and nothing at 0x6e..0x74, and it makes no
    // ObjPtr ctor call. So mPreviewDb (0x70) and ObjPtr<TexMovie> (0x74) cannot
    // exist -- either one would push sizeof past 0x70. They are DC3-newer
    // additions; keep them for the native build only.
    //
    // mInitted is DC3-only too: retail's ?Terminate@SongPreview@@QAAXXZ has no
    // `if (mInitted)` guard and no `mInitted = 0` store at all, and the ctor
    // stores nothing at 0x6e/0x6f. The rb3-Wii oracle agrees (no such member,
    // no guards in Init/Terminate/SetMusicVol/SetCrowdSingVol/Start).
#ifdef HX_NATIVE
    bool mInitted;
    float mPreviewDb;
    ObjPtr<TexMovie> mTexMovie;
#endif

    void DetachFader(Fader *);
    void PrepareFaders(const SongInfo *);
    void PrepareSong(Symbol);
};
