#include "meta/SongPreview.h"
#include "SongMetadata.h"
#include "SongMgr.h"
#include "macros.h"
#include "math/Utl.h"
#include "meta/DataArraySongInfo.h"
#include "movie/TexMovie.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "os/ContentMgr.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/System.h"
#include "synth/Faders.h"
#include "synth/Synth.h"
#include "utl/SongInfoCopy.h"
#include "utl/Symbol.h"

const float SongPreview::kSilenceVal = -48;

#pragma region Hmx::Object

SongPreview::SongPreview(const SongMgr &mgr)
    : mSongMgr(mgr), mStream(0),
#ifdef HX_NATIVE
      mTexMovie(this), mPreviewDb(0.0f), mInitted(0), mSecurePreview(0),
#endif
      mFader(0), mMusicFader(0),
      mCrowdSingFader(0), mNumChannels(0), mAttenuation(0.0f),
      mState(kIdle), mStartMs(0.0f), mEndMs(0.0f), mStartPreviewMs(0.0f),
      // Retail's ctor stores nothing at 0x6e (mSecurePreview) or 0x6f; every
      // read of mSecurePreview is preceded by OnStart's `mSecurePreview = false`.
      mEndPreviewMs(0.0f), mRegisteredWithCM(0), mSameSongRequested(0) {}

SongPreview::~SongPreview() { Terminate(); }

BEGIN_HANDLERS(SongPreview)
    HANDLE(start, OnStart)
    HANDLE_ACTION(set_music_vol, SetMusicVol(_msg->Float(2)))
    HANDLE_ACTION(set_crowd_sing_vol, SetCrowdSingVol(_msg->Float(2)))
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

#pragma endregion
#pragma region ContentMgr::Callback

void SongPreview::ContentMounted(const char *contentName, const char *) {
    MILO_ASSERT(contentName, 0xbf);
    Symbol s = contentName;
    if (s == mSongContent) {
        mSongContent = 0;
    }
}

void SongPreview::ContentFailed(const char *contentName) {
    MILO_ASSERT(contentName, 0xcb);
    // it matches, don't question it
    Symbol sym = contentName;
    if (sym == mSongContent) {
        mSong = 0;
        Symbol zero = 0;
        mState = kIdle;
        mSongContent = zero;
    }
}

#pragma endregion
#pragma region SongPreview

bool SongPreview::IsWaitingToDelete() const { return mState == kDeletingSong; }
bool SongPreview::IsFadingOut() const { return mState == kFadingOutSong; }

void SongPreview::SetMusicVol(float f) {
#ifdef HX_NATIVE
    if (mInitted == 0) {
        return;
    }
#endif
    if (f < mMusicFader->GetTargetDb()) {
        mMusicFader->DoFade(f, 250.0f);
    } else {
        mMusicFader->DoFade(f, 1000.0f);
    }
}

void SongPreview::SetCrowdSingVol(float f) {
#ifdef HX_NATIVE
    if (!mInitted)
        return;
#endif
    mCrowdSingFader->DoFade(f, 0.0f);
}

void SongPreview::Init() {
#ifdef HX_NATIVE
    if (!mInitted) {
        mInitted = true;
#else
    {
#endif
        mSong = 0;
        mSongContent = 0;
        RELEASE(mStream);
#ifdef HX_NATIVE
        mTexMovie = nullptr;
#endif
        mState = kIdle;
        mRestart = true;
        DataArray *cfg = SystemConfig("sound", "song_select");
        cfg->FindData("loop_forever", mLoopForever, true);
        cfg->FindData("fade_time", mFadeTime, true);
        cfg->FindData("attenuation", mAttenuation, true);
#ifdef HX_NATIVE
        cfg->FindData("preview_db", mPreviewDb, true);
#endif
        mFadeTime *= 1000.0f;
        mFader = Hmx::Object::New<Fader>();
        mMusicFader = Hmx::Object::New<Fader>();
        mCrowdSingFader = Hmx::Object::New<Fader>();
        mCrowdSingFader->SetVal(kDbSilence);
    }
}

void SongPreview::Terminate() {
    // Retail has no mInitted guard here: the target starts straight at
    // `lwz r4, 0x38(r3); bl DetachFader`.
#ifdef HX_NATIVE
    if (mInitted) {
        mInitted = 0;
#else
    {
#endif
        DetachFader(mMusicFader);
        DetachFader(mCrowdSingFader);
        mSong = 0;
        mSongContent = 0;
        RELEASE(mStream);
        RELEASE(mFader);
        RELEASE(mMusicFader);
        RELEASE(mCrowdSingFader);
#ifdef HX_NATIVE
        mTexMovie = nullptr;
#endif
        if (mRegisteredWithCM) {
            TheContentMgr.UnregisterCallback(this, true);
            mRegisteredWithCM = 0;
        }
    }
}

// RB3-360 retail's Start takes ONE argument (fn_827A5790): its prologue
// captures `mr r31,r3` (this) and `mr r30,r4` (the Symbol) and never reads r5
// — r5 is only ever WRITTEN there (`li r5, 1`) to stage an onward call — and
// 0 of its 4 callers stage r5. The TexMovie parameter is DC3-newer. It is kept
// on a native-only overload below because native uses it for mTexMovie.
#ifdef HX_NATIVE
void SongPreview::Start(Symbol song, TexMovie *texMovie) {
    if (!mInitted)
        return; // Audio/synth not initialized on native
    if (mInitted || !song.Null()) {
#else
void SongPreview::Start(Symbol song) {
    {
#endif
        MILO_ASSERT(mFader && mMusicFader && mCrowdSingFader,0x6c);
#ifdef HX_NATIVE
        mTexMovie = texMovie;
#endif
        // Retail returns immediately on a repeat request for the same song --
        // it does NOT set mSameSongRequested here (verified: fn_827A5790's
        // beq on song==mSong jumps straight to the epilogue, no store to
        // 0x6d anywhere in the function). mSameSongRequested is set/consumed
        // elsewhere (Poll()); this early-out path never touches it.
        if (song == mSong) {
            return;
        }
        if (!song.Null()) {
            if (!mSongMgr.HasSong(song)) {
                return;
            }
            int songID = mSongMgr.GetSongIDFromShortName(song, true);
            const SongMetadata *data = mSongMgr.Data(songID);
            if (data && !data->IsVersionOK()) {
                song = gNullStr;
            }
            if (!mRegisteredWithCM) {
                TheContentMgr.RegisterCallback(this, false);
                mRegisteredWithCM = true;
            }
        }
        mSong = song;
        mRestart = true;
        // Retail RB3-360 has no preview_db member (see SongPreview.h);
        // the rb3-Wii oracle plays the music fader at 0 dB here.
#ifdef HX_NATIVE
        mMusicFader->SetVal(mPreviewDb);
#else
        mMusicFader->SetVal(0.0f);
#endif
        mCrowdSingFader->SetVal(kDbSilence);
        switch (mState) {
        case kIdle:
        case kMountingSong:
            RELEASE(mStream);
            mState = kIdle;
            break;
        case kPreparingSong:
            mState = kDeletingSong;
            break;
        case kPlayingSong:
            // Retail (fn_827A5790) is unconditional here -- no song.Null()
            // branch, no DoFade(kSilenceVal, 0)/kIdle alternative. Confirmed
            // against the rb3-Wii oracle's parallel single-arg Start, which
            // has the same unconditional DoFade(-48.0f, mFadeTime) +
            // kFadingOutSong in its kPlayingSong case.
            mFader->DoFade(kSilenceVal, mFadeTime);
            mState = kFadingOutSong;
            break;
        default:
            break;
        }
    }
}

void SongPreview::PreparePreview() {
    float previewstart = 0.0f;
    float previewend = 15000.0f;
    if (mStartPreviewMs != 0 || mEndPreviewMs != 0) {
        previewstart = mStartPreviewMs;
        previewend = mEndPreviewMs;
    } else {
        int songid = mSongMgr.GetSongIDFromShortName(mSong);
        const SongMetadata *data = mSongMgr.Data(songid);
        data->PreviewTimes(previewstart, previewend);
    }
    mStartMs = previewstart;
    mEndMs = previewend;
    PrepareSong(mSong);
    if (!mLoopForever) {
        mRestart = false;
    }
}

#ifdef HX_NATIVE
// Native-only 1-arg form so the shared call sites (OnStart, MusicLibrary) can
// spell Start(sym) in both builds. In the match build the 1-arg form above IS
// the real retail function, so no forwarding exists there.
void SongPreview::Start(Symbol song) { Start(song, nullptr); }
#endif

DataNode SongPreview::OnStart(DataArray *arr) {
    mSecurePreview = false;
    if (arr->Size() == 3) {
        mStartPreviewMs = 0;
        mEndPreviewMs = 0;
        MILO_LOG(
            "start called in upper OnStart here : sym='%s'\n", arr->ForceSym(2).Str()
        );
        Start(arr->ForceSym(2));
    } else {
        mStartPreviewMs = arr->Float(3);
        mEndPreviewMs = arr->Float(4);
        if (arr->Size() >= 6) {
            mSecurePreview = arr->Int(5);
        }
        mSong = gNullStr;
        MILO_LOG(
            "start called in lower OnStart here : sym='%s'\n", arr->ForceSym(2).Str()
        );
        Start(arr->ForceSym(2));
    }
    return 1;
}

void SongPreview::DetachFader(Fader *f) {
    if (mStream && f) {
        for (int i = 0; i < mNumChannels; i++) {
            mStream->ChannelFaders(i)->Remove(f);
        }
    }
}

void SongPreview::PrepareFaders(const SongInfo *info) {
    for (int i = 0; i < mNumChannels; i++) {
        FaderGroup *f = mStream->ChannelFaders(i);
        f->Add(mMusicFader);
    }
}

void SongPreview::PrepareSong(Symbol song) {
    mState = kPreparingSong;
    RELEASE(mStream);
    SongInfo *songInfo = mSongMgr.SongAudioData(song);
    const char *filename = songInfo->GetBaseFileName();
#ifdef HX_NATIVE
    if (mTexMovie) {
        String str(mSongMgr.SongFilePath(song, "_prev.bik", 10));
#ifdef __EMSCRIPTEN__
        // Web: .bik files aren't in MEMFS (only DTA/DTB are bundled).
        // Skip FileExists check — WebMovieImpl fetches from the asset
        // server via /api/file/ and handles 404s gracefully.
        mTexMovie->SetFile(str.c_str());
        mTexMovie->SetVolume(-mAttenuation);
        mTexMovie->AddFader(mFader);
        mTexMovie->AddFader(mMusicFader);
#else
        if (FileExists(str.c_str(), 0, nullptr)) {
            mTexMovie->SetFile(str.c_str());
            mTexMovie->SetVolume(-mAttenuation);
            mTexMovie->AddFader(mFader);
            mTexMovie->AddFader(mMusicFader);
            return;
        }
        mTexMovie->SetFile(gNullStr);
#endif
    }
#endif // HX_NATIVE
    mStream = TheSynth->NewStream(filename, mStartMs, 0, mSameSongRequested);
    const std::vector<float> &pans = songInfo->GetPans();
    const std::vector<float> &vols = songInfo->GetVols();
    mNumChannels = pans.size();
    for (int i = 0; i < mNumChannels; i++) {
        mStream->SetVolume(i, vols[i]);
        mStream->SetPan(i, pans[i]);
    }
    const TrackChannels *trackChannels = songInfo->FindTrackChannel(kAudioTypeMulti);
    if (trackChannels) {
        for (int i = 0; i < trackChannels->mChannels.size(); i++) {
            mStream->SetVolume(trackChannels->mChannels[i], kDbSilence);
        }
    }
    DetachFader(mMusicFader);
    DetachFader(mCrowdSingFader);
    PrepareFaders(songInfo);
    mStream->SetVolume(-mAttenuation);
    mStream->Faders()->Add(mFader);
}

void SongPreview::Poll() {
    switch (mState) {
    case kIdle: {
        if (!mSong.Null() && mRestart) {
            const char *name = mSongMgr.ContentName(mSong);
            if (name) {
                mSongContent = name;
                if (TheContentMgr.MountContent(mSongContent.Str())) {
                    mSongContent = 0;
                }
                mState = kMountingSong;
            } else {
                PreparePreview();
            }
        } else if (mSameSongRequested) {
            mState = kFadingOutSong;
            mFader->DoFade(kSilenceVal, mFadeTime);
            mSameSongRequested = false;
        }
        break;
    }
    case kMountingSong: {
        if (mSongContent.Null()) {
            PreparePreview();
        }
        break;
    }
    case kPreparingSong: {
        if (HasMovie()) {
            mFader->SetVal(0);
        } else {
            if (!mStream->IsReady()) {
                return;
            }
            mFader->SetVal(kSilenceVal);
            mFader->DoFade(0, mFadeTime);
            mStream->Play();
        }
        mState = kPlayingSong;
        break;
    }
    case kDeletingSong: {
        RELEASE(mStream);
        mState = kIdle;
        break;
    }
    case kPlayingSong: {
        if (HasMovie() || mStream && mStream->GetTime() < mEndMs)
            return;
        MILO_LOG("mSong in Poll is %s\n", mSong);
        mState = kFadingOutSong;
        mFader->DoFade(kSilenceVal, mFadeTime);
        break;
    }
    case kFadingOutSong: {
        if (!mFader->IsFading()) {
            RELEASE(mStream);
#ifdef HX_NATIVE
            if (HasMovie()) {
                mTexMovie->SetFile(gNullStr);
            }
#endif
            mState = kIdle;
        }
        break;
    }
    default:
        break;
    }
}
