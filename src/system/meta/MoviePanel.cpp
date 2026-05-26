#include "meta/MoviePanel.h"
#include "macros.h"
#include "math/Easing.h"
#include "math/Rand.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/System.h"
#include "rndobj/Anim.h"
#include "rndobj/Draw.h"
#include "rndobj/Rnd.h"
#include "ui/PanelDir.h"
#include "ui/UI.h"
#include "ui/UILabel.h"
#include "ui/UIPanel.h"
#include "utl/FilePath.h"
#include "utl/Loader.h"
#include "utl/MakeString.h"
#include "utl/Symbol.h"
#include <cstdio>
#ifdef __EMSCRIPTEN__
#include "platform/WebMovieImpl.h"
#include "platform/TexGpu.h"
#include "rndobj/Mat.h"
#include "rndobj/Tex.h"
extern void FlushPostProcessingForOverlay();
#endif

bool MoviePanel::sUseSubtitles;

#pragma region Hmx::Object

MoviePanel::MoviePanel()
    : mMovies(), mRecent(), mMovie(), mSubtitlesLoader(0), mSubtitles(0),
      mCurrentSubtitleIndex(0), mSubtitleCleared(1), mSubtitleLabel(0), mPauseHintAnim(0),
      mShowHint(0), mTimeShowHintStarted(0.0f), mShowMenu(0) {}

BEGIN_HANDLERS(MoviePanel)
    HANDLE_ACTION(set_paused, SetPaused(_msg->Int(2)))
    HANDLE_ACTION(set_menu_shown, ShowMenu(_msg->Int(2)))
    HANDLE_EXPR(is_menu_shown, mShowMenu)
    HANDLE_ACTION(show_hint, ShowHint())
    HANDLE_SUPERCLASS(UIPanel)
END_HANDLERS

BEGIN_PROPSYNCS(MoviePanel)
    SYNC_PROP(show_menu, mShowMenu)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

void MoviePanel::SetTypeDef(DataArray *d) {
    UIPanel::SetTypeDef(d);
    d->FindData("preload", mPreload, true);
    d->FindData("audio", mAudio, true);
    d->FindData("loop", mLoop, true);
}

#pragma endregion
#pragma region UIPanel

void MoviePanel::Load() {
    UIPanel::Load();
    mMovies.clear();
#ifdef HX_NATIVE
    const DataNode *videosProp = Property("videos", false);
    if (!videosProp) {
        printf("MoviePanel::Load: no 'videos' property, skipping\n");
        return;
    }
    DataArray *config = SystemConfig("videos", videosProp->Str());
#else
    DataArray *config = SystemConfig("videos", Property("videos", true)->Str());
#endif
    DataArray *files = config->FindArray("files");
    for (int i = 1; i < files->Size(); i++) {
        mMovies.push_back(files->Str(i));
    }
    mFillWidth = false;
    config->FindData("fill_width", mFillWidth, false);
    bool localize = false;
    config->FindData("localize", localize, false);
    if (localize) {
        mLanguage = Movie::LocalizationTrack();
    } else {
        mLanguage = 0;
    }
    if (sUseSubtitles && mMovies.size() == 1) {
        char pathBuffer[256];
        sprintf(pathBuffer, "ui/subtitles/eng/%s_keep.dta", FileGetBase(mMovies[0]));
        const char *subtitlesPath;
        bool local = FileIsLocal(pathBuffer);
        bool cd = UsingCD();
        if (!cd || local) {
            subtitlesPath = pathBuffer;
        } else {
            subtitlesPath = MakeString(
                "%s/gen/%s.dtb", FileGetPath(pathBuffer), FileGetBase(pathBuffer)
            );
        }

        if (FileExists(subtitlesPath, 0, nullptr)) {
            // bug? pathBuffer should probably be subtitlesPath
            mSubtitlesLoader = new DataLoader(pathBuffer, kLoadFront, true);
        }
    }
#ifdef HX_NATIVE
    if (!mMovies.empty())
#endif
    ChooseMovie();
}

void MoviePanel::Draw() {
    if (GetState() != kUnloaded && mFinalDrawPassFlag == GetFinalDrawPass()) {
#ifdef __EMSCRIPTEN__
        // Web: upload decoded video frame to GPU texture, then draw fullscreen.
        // On Xbox, BinkDraw renders directly to the backbuffer; we replicate
        // that by drawing a fullscreen rect over everything.
        WebMovieImpl* impl = dynamic_cast<WebMovieImpl*>(mMovie.GetImpl());
        if (impl && impl->HasDecodedFrame()) {
            int vw = impl->GetDecodedWidth();
            int vh = impl->GetDecodedHeight();
            if (!mVideoTex) {
                mVideoTex = Hmx::Object::New<RndTex>();
                mVideoTex->SetBitmap(vw, vh, 32, RndTex::kRendered, false, nullptr);
                mVideoMat = Hmx::Object::New<RndMat>();
                mVideoMat->SetDiffuseTex(mVideoTex);
            }
            UploadRGBAToRndTex(mVideoTex, impl->GetRGBABuffer(), vw, vh);
        }
        mMovie.Draw(); // mark frame consumed
        // Flush post-processing (bloom etc.) before drawing the video rect.
        // The video should appear clean, not bloomed — same as Xbox where
        // BinkDraw writes directly to the backbuffer after post-proc.
        FlushPostProcessingForOverlay();
        // Always draw: video frame if available, black fallback otherwise.
        Hmx::Rect r(0, 0, 1, 1);
        if (mVideoTex) {
            TheRnd.DrawRectScreen(r, Hmx::Color(1, 1, 1, 1), mVideoMat, nullptr, nullptr);
        } else {
            TheRnd.DrawRectScreen(r, Hmx::Color(0, 0, 0, 1), nullptr, nullptr, nullptr);
        }
        // Skip UIPanel::Draw() — its meshes are untextured white rectangles
        // that would render on top of the video. The movie_overlay_panel
        // handles any needed overlay UI separately.
        return;
#else
        mMovie.Draw();
#endif
    }
    UIPanel::Draw();
}

void MoviePanel::Enter() { UIPanel::Enter(); }

void MoviePanel::Exit() {
    UIPanel::Exit();
    if (!mPreload) {
        mMovie.End();
    }
    mShowMenu = false;
#ifdef __EMSCRIPTEN__
    if (mVideoMat) { delete mVideoMat; mVideoMat = nullptr; }
    if (mVideoTex) { delete mVideoTex; mVideoTex = nullptr; }
#endif
}

void MoviePanel::Poll() {
    UIPanel::Poll();
    if (GetState() == kUnloaded)
        return;
#ifdef HX_NATIVE
    // If no movies were loaded, skip movie polling entirely
    if (mMovies.empty())
        return;
    // Stub MovieImpl never opens successfully — avoid infinite movie_done loop
    if (!mMovie.IsOpen())
        return;
#endif
    if (!mMovie.Poll() && !TheUI->InTransition()) {
        static Message movie_done("movie_done");
        DataNode handled = HandleType(movie_done);
        if (handled.Equal(DATA_UNHANDLED, nullptr, true)) {
            mMovie.End();
            PlayMovie();
        }
    } else {
        if (mSubtitles && mSubtitleLabel) {
            int frame = mMovie.GetFrame();
            DataArray *arr = mSubtitles->Array(mCurrentSubtitleIndex);
            if (mSubtitleCleared && arr->Int(0) <= frame) {
                mSubtitleLabel->SetSubtitle(arr);
                mSubtitleCleared = false;
            }
            if (arr->Int(1) < frame) {
                if (mSubtitles->Size() > mCurrentSubtitleIndex + 1) {
                    DataArray *a2 = mSubtitles->Array(mCurrentSubtitleIndex + 1);
                    if (a2) {
                        if (a2->Int(0) <= frame) {
                            mSubtitleLabel->SetSubtitle(a2);
                            mSubtitleCleared = false;
                            mCurrentSubtitleIndex++;
                            goto lol;
                        }
                    }
                }
                if (!mSubtitleCleared) {
                    mSubtitleLabel->SetTextToken(gNullStr);
                    mSubtitleCleared = true;
                }
            }
        }
    }
lol:
    if (mShowHint) {
        float secs = TheTaskMgr.UISeconds();
        if (secs < mTimeShowHintStarted) {
            mTimeShowHintStarted = secs;
        }
        if (secs - mTimeShowHintStarted >= 3.0f) {
            HideHint();
        }
    }
}

bool MoviePanel::IsLoaded() const {
#ifdef HX_NATIVE
    // Native Movie stub's Ready() always returns false — skip the check
    // so screen transitions don't block forever on video playback.
    return UIPanel::IsLoaded();
#endif
    if (!mMovie.Ready()) {
        return false;
    }
    if (mSubtitlesLoader && !mSubtitlesLoader->IsLoaded()) {
        return false;
    }
    return UIPanel::IsLoaded();
}

void MoviePanel::Unload() {
    UIPanel::Unload();
    if (mPreload) {
        mMovie.End();
    }
    if (mSubtitles) {
        mSubtitles->Release();
        mSubtitles = nullptr;
    }
}

void MoviePanel::FinishLoad() {
    UIPanel::FinishLoad();
    if (mSubtitlesLoader) {
        mSubtitles = mSubtitlesLoader->Data();
        mSubtitles->AddRef();
        RELEASE(mSubtitlesLoader);
        if (mDir)
            mSubtitleLabel = mDir->Find<UILabel>("subtitles.lbl", false);
    } else {
        mSubtitles = nullptr;
    }
    if (mDir) {
        mPauseHintAnim = mDir->Find<RndAnimatable>("fade_pausehint.anim", true);
    }
}

#pragma endregion
#pragma region MoviePanel

void MoviePanel::SetPaused(bool b) { mMovie.SetPaused(b); }

void MoviePanel::HideHint() {
    mShowHint = false;
    mPauseHintAnim->Animate(
        mPauseHintAnim->GetFrame(),
        mPauseHintAnim->StartFrame(),
        mPauseHintAnim->Units(),
        0,
        0.0f,
        0,
        kEaseLinear,
        0.0f,
        0
    );
}

void MoviePanel::ShowHint() {
    if (mPauseHintAnim) {
        mShowHint = true;
        mTimeShowHintStarted = TheTaskMgr.UISeconds();
        mPauseHintAnim->Animate(
            mPauseHintAnim->GetFrame(),
            mPauseHintAnim->EndFrame(),
            mPauseHintAnim->Units(),
            0.0f,
            0.0f,
            0,
            kEaseLinear,
            0.0f,
            false
        );
    }
}

void MoviePanel::PlayMovie() {
    mSubtitleCleared = true;
    mCurrentSubtitleIndex = 0;
    mMovie.BeginFromFile(
        MakeString("videos/%s", mCurrentMovie),
        0.0f,
        !mAudio,
        mLoop,
        mPreload,
        mFillWidth,
        mLanguage,
        0,
        kLoadFront
    );
}

void MoviePanel::ChooseMovie() {
    MILO_ASSERT(!mMovies.empty(), 0xf2);
    do {
        int random = RandomInt(0, mMovies.size());
        mCurrentMovie = mMovies[random];
    } while (std::find(mRecent.begin(), mRecent.end(), mCurrentMovie) != mRecent.end());

    mRecent.push_back(mCurrentMovie);

    if (mRecent.size() == mMovies.size()) {
        while (mRecent.size() > (mMovies.size() / 2)) {
            mRecent.pop_front();
        }
    }
    PlayMovie();
}

void MoviePanel::ShowMenu(bool b) {
    mShowMenu = b;
    if (mShowMenu)
        HideHint();
}
