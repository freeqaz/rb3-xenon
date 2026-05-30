#include "movie/TexMovie.h"
#if defined(HX_NATIVE) && !defined(__EMSCRIPTEN__)
#include "platform/FFmpegMovieImpl.h"
#include "platform/TexGpu.h"
#elif defined(__EMSCRIPTEN__)
#include "platform/WebMovieImpl.h"
#include "platform/TexGpu.h"
#endif
#include "macros.h"
#include "obj/Data.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "obj/PropSync.h"
#include "os/Debug.h"
#include "os/File.h"
#include "rndobj/Draw.h"
#include "rndobj/Poll.h"
#include "rndobj/Rnd.h"
#include "rndobj/Utl.h"
#include "utl/BinStream.h"
#include "utl/FilePath.h"
#include "utl/Loader.h"
#include <cstddef>

TexMovie::TexMovie()
    : mTex(this), mLoop(1), mEntered(0), mIsLocalized(0), mPaused(0), sRoot(), mMovie() {}

TexMovie::~TexMovie() { mMovie.End(); }

BEGIN_COPYS(TexMovie)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndDrawable)
    COPY_SUPERCLASS(RndPollable)
    CREATE_COPY(TexMovie)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mTex)
        COPY_MEMBER(sRoot)
        COPY_MEMBER(mIsLocalized)
    END_COPYING_MEMBERS
END_COPYS

bool TexMovie::Replace(ObjRef *a, Hmx::Object *b) {
    bool check;
    if (RefIs(a, mTex)) {
        mMovie.End();
        mTex.SetObj(b);
        check = true;
    } else {
        check = Hmx::Object::Replace(a, b);
    }
    return check;
}

BEGIN_PROPSYNCS(TexMovie)
    SYNC_PROP_MODIFY(output_texture, mTex, DoBeginMovieFromFile(nullptr, kLoadFront))
    {
        _NEW_STATIC_SYMBOL(bink_movie_file)
        if (sym == _s) {
            if (_op == kPropSet) {
                FilePath fp(_val.Str(nullptr));
                SetFile(fp);
            } else {
                // kPropUnknown0x40 (aka kPropHandle) not supported for this property
                if (_op == kPropUnknown0x40)
                    return false;
                // kPropGet - return the relative path
                _val = FileRelativePath(FilePath::Root().c_str(), sRoot.c_str());
            }
            return true;
        }
    }
    SYNC_PROP(loop, mLoop)
    SYNC_PROP(is_localized, mIsLocalized)
    {
        _NEW_STATIC_SYMBOL(is_empty)
        if (sym == _s) {
            // Read-only property - only supports kPropGet
            if (_op != kPropSet) {
                if (_op == kPropUnknown0x40) {
                    return false;
                }
                _val = sRoot.empty();
            }
            return true;
        }
    }
    SYNC_SUPERCLASS(RndDrawable)
    SYNC_SUPERCLASS(Hmx::Object)
    SYNC_SUPERCLASS(RndPollable)
END_PROPSYNCS

BEGIN_SAVES(TexMovie)
    SAVE_REVS(8, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndDrawable)
    SAVE_SUPERCLASS(RndPollable)
    bs << mTex << mLoop << sRoot << mIsLocalized;
    mMovie.Save(&bs);
END_SAVES

INIT_REVS(8, 0)

BEGIN_LOADS(TexMovie)
    LOAD_REVS(bs)
    ASSERT_REVS(8, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(RndDrawable)
    LOAD_SUPERCLASS(RndPollable)
    bs >> mTex >> mLoop;
    if (d.rev < 4) {
        bool dummy;
        d >> dummy;
    }
    bs >> sRoot;
    if (d.rev > 5) {
        d >> mIsLocalized;
    }
    if (d.rev == 7) {
        bool dummy;
        d >> dummy;
    }
    static Message msg("change_file");
    DataNode handled = HandleType(msg);
    if (handled.Type() == kDataString) {
        const char *str = handled.Str(nullptr);
        sRoot.Set(FilePath::Root().c_str(), str);
    }
    if (d.rev > 1 && d.rev < 3) {
        bool dummy;
        d >> dummy;
    }
    FilePathTracker tracker(".");
    DoBeginMovieFromFile(d.rev > 4 ? &bs : nullptr, kLoadFront);
END_LOADS

void TexMovie::DrawPreClear() {
    if (mShowing)
        DrawToTexture();
}

void TexMovie::UpdatePreClearState() {
    if (!mEntered)
        return;
    TheRnd.PreClearDrawAddOrRemove(this, true, TheRnd.GetReleaseImmediate());
}

void TexMovie::Poll() {
    if (!mPaused) {
        if (mShowing) {
            mMovie.SetPaused(false);
            if (mTex && !mMovie.Poll()) {
                mMovie.End();
            }
        } else {
            mMovie.SetPaused(true);
        }
    }
}

void TexMovie::Enter() {
    mEntered = true;
    RndPollable::Enter();
    bool b = (mTex && mTex->Width() && mTex->Height());
    if (b) {
        mTex->MakeDrawTarget();
        Hmx::Rect r(0, 0, 1, 1);
        Hmx::Color c(0, 0, 0, 1);
        TheRnd.DrawRectScreen(r, c, nullptr, nullptr, nullptr);
        mTex->FinishDrawTarget();
        TheRnd.MakeDrawTarget();
    }
    mMovie.CheckOpen(false);
    UpdatePreClearState();
}

void TexMovie::Exit() {
    mEntered = false;
    RndPollable::Exit();
}

void TexMovie::SetPaused(bool b) {
    mPaused = b;
    if (b) {
        if (!mMovie.IsOpen())
            return;
        mMovie.SetPaused(true);
    } else {
        if (!mMovie.IsOpen())
            return;
        mMovie.SetPaused(false);
    }
}

void TexMovie::Reset() {
    mPaused = false;
    mMovie.End();
}

bool TexMovie::IsEmpty() const { return sRoot.empty(); }

void TexMovie::DrawToTexture() {
    bool b = (mTex != nullptr && mTex->Width() && mTex->Height());

    if (b) {
#if defined(HX_NATIVE) && !defined(__EMSCRIPTEN__)
        // Native: check for decoded frame BEFORE Draw() clears the flag,
        // then upload RGBA pixels directly to GPU texture
        {
            FFmpegMovieImpl* impl = dynamic_cast<FFmpegMovieImpl*>(mMovie.GetImpl());
            if (impl && impl->HasDecodedFrame()) {
                UploadRGBAToRndTex(mTex, impl->GetRGBABuffer(),
                                   impl->GetDecodedWidth(), impl->GetDecodedHeight());
            }
            mMovie.Draw(); // marks frame as consumed
        }
#elif defined(__EMSCRIPTEN__)
        // Web: check for decoded frame BEFORE Draw() clears the flag,
        // then upload RGBA pixels
        {
            WebMovieImpl* impl = dynamic_cast<WebMovieImpl*>(mMovie.GetImpl());
            if (impl && impl->HasDecodedFrame()) {
                UploadRGBAToRndTex(mTex, impl->GetRGBABuffer(),
                                   impl->GetDecodedWidth(), impl->GetDecodedHeight());
            }
            mMovie.Draw(); // marks frame as consumed
        }
#else
        mTex->MakeDrawTarget();
        mMovie.Draw();
        mTex->FinishDrawTarget();
        TheRnd.MakeDrawTarget();
#endif
    }
}

void TexMovie::SetFile(FilePath const &fp) {
    mMovie.End();
    sRoot = fp;
    DoBeginMovieFromFile(nullptr, kLoadBack);
}

void TexMovie::DoBeginMovieFromFile(BinStream *stream, LoaderPos lp) {
    mMovie.End();
    if (!sRoot.empty() && mTex) {
        MILO_ASSERT(mTex->IsRenderTarget(), 0x83);
        int i = 1;
        if (mIsLocalized) {
            i = mMovie.LocalizationTrack();
        }
        mMovie.SetWidthHeight(mTex->Width(), mTex->Height());
        mMovie.BeginFromFile(
            FileRelativePath(FileRoot(), sRoot.c_str()),
            0.0f,
            0,
            true,
            mLoop,
            false,
            i,
            stream,
            lp
        );
    }
}

DataNode TexMovie::OnPlayMovie(DataArray *d) {
    if (d->Int(2) != 0) {
        if (!mMovie.IsLoading() && !mMovie.IsOpen())
            DoBeginMovieFromFile(nullptr, kLoadFront);
    } else {
        mMovie.End();
    }
    return DataNode();
}

DataNode TexMovie::OnGetRenderTextures(DataArray *d) { return GetRenderTextures(Dir()); }

BEGIN_HANDLERS(TexMovie)
    HANDLE(get_render_textures, OnGetRenderTextures)
    HANDLE(play_movie, OnPlayMovie)
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(RndPollable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS
