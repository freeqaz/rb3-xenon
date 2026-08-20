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

// Retail 0x827461A0 (116 B). NOTE: unlike the rndobj Replace family this one
// compares the held pointer DIRECTLY (lwz r11,-0x1c(r3); cmplw r11,r4) with
// no vbtable adjust, so RefIs() is already correct here -- do not "fix" it to
// a static_cast. The two real defects were SetObj (which resolves the type
// itself) where retail emits an explicit dynamic_cast + SetObjConcrete, and
// the Hmx::Object::Replace fallback, which retail does not have.
void TexMovie::Replace(ObjRef *a, Hmx::Object *b) {
    // Operand order matters: retail is cmplw (held, ref); RefIs() spells it
    // (ref, held), which costs exactly one instruction here.
    if (reinterpret_cast<void *>(mTex.Ptr()) == reinterpret_cast<void *>(a)) {
        mMovie.End();
        mTex.SetOwnerObj(dynamic_cast<RndTex *>(b));
    }
}

BEGIN_PROPSYNCS(TexMovie)
    SYNC_PROP_MODIFY(output_texture, mTex, DoBeginMovieFromFile(nullptr))
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
#ifdef HX_NATIVE
    // DC3-era additions; RB3-360 retail's TexMovie chain ends at `loop`.
    // Arbitrated on RETAIL BYTES (lane CQ-3): the 580 B retail body enumerates
    // exactly output_texture / bink_movie_file / loop -- three literals, ours
    // emitted five.  Native-only.
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
#endif
    SYNC_SUPERCLASS(RndDrawable)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain does not include this superclass;
    // DC3's newer engine added it. Native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
    SYNC_SUPERCLASS(RndPollable)
END_PROPSYNCS

// Retail writes the save revision by LOADING A GLOBAL, not a folded immediate
// (same pattern as ui/UIFontImporter.cpp's gSaveRev): target emits lis/lwz
// from a data address instead of the constant-folded `li r11, 8` that
// SAVE_REVS(8, 0)'s inline packRevs() produces.
static int gSaveRev = (0 << 16) | 8; // packRevs(alt=0, rev=8)

BEGIN_SAVES(TexMovie)
    bs << gSaveRev;
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndDrawable)
    SAVE_SUPERCLASS(RndPollable)
    bs << mTex << mLoop << sRoot;
#ifdef HX_NATIVE
    // DC3-era addition (see the is_localized/is_empty gate above);
    // RB3-360 retail's Save doesn't write this field.
    bs << mIsLocalized;
#endif
    mMovie.Save(&bs);
END_SAVES

// The RAW incoming BinStream is forwarded to every read and to each superclass
// Load: DC3's Object.h BinStreamRev stack decorator additionally emits
// ??0BinStream, a ??_7BinStreamRev@@6B@ vtable store and a ??1BinStream
// destructor that retail has none of, and dispatches each read on `&d`.
//
// NOTE(laneGLM3): this TU used to also carry the two-halfword `gRevs_TexMovie`
// aggregate (packed rev split into altRev/rev stored four bytes apart on one
// internal-linkage align(4) base).  RETAIL DOES NOT DO THAT HERE -- it keeps
// the packed value in a plain local `int` and compares it whole.  This is a
// third rev dialect, which is exactly why the dialect has to be read off the
// target bytes per TU rather than applied as a rule.  Four witnesses, any one
// of which is sufficient:
//   * retail contains no store to any such global -- the lis/addi of the base
//     and both `sth` are base-only in the aligned diff;
//   * every rev test re-loads `lwz r11, 0x54(r31)`, and 0x54(r31) is precisely
//     the stack slot that `ReadEndian(&slot, 4)` filled at function entry;
//   * retail compares with `cmpwi` (SIGNED, full word); a zero-extended
//     halfword load produced our `cmplwi`;
//   * the `rev > 4` argument to DoBeginMovieFromFile is built with the SIGNED
//     compare-to-mask idiom, extracting both sign bits
//     (srwi r9,r10,31 / srwi r11,r11,31 / subfc / subfe r11,r11,r9), where the
//     halfword form collapsed to the unsigned subfic / subfe r11,r11,r11.
//     A value known non-negative cannot generate the signed sequence.
BEGIN_LOADS(TexMovie)
    int rev;
    bs >> rev;
    Hmx::Object::Load(bs);
    RndDrawable::Load(bs);
    RndPollable::Load(bs);
    bs >> mTex >> mLoop;
    if (rev < 4) {
        bool dummy;
        bs >> dummy;
    }
    bs >> sRoot;
#ifdef HX_NATIVE
    // DC3-era additions, consistent with the is_localized/is_empty and Save
    // gates above: RB3-360 retail's Load reads neither.  Both blocks are
    // base-only in the aligned diff (13 consecutive base-only instructions
    // between `bs >> sRoot` and the change_file static).
    if (rev > 5) {
        bs >> mIsLocalized;
    }
    if (rev == 7) {
        bool dummy;
        bs >> dummy;
    }
#endif
    static Message msg("change_file");
    DataNode handled = HandleType(msg);
    if (handled.Type() == kDataString) {
        const char *str = handled.Str(nullptr);
        sRoot.Set(FilePath::Root().c_str(), str);
    }
    if (rev > 1 && rev < 3) {
        bool dummy;
        bs >> dummy;
    }
    FilePathTracker tracker(".");
    DoBeginMovieFromFile(rev > 4 ? &bs : nullptr);
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
    if (mTex) {
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
    DoBeginMovieFromFile(nullptr);
}

void TexMovie::DoBeginMovieFromFile(BinStream *stream) {
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
            stream
        );
    }
}

DataNode TexMovie::OnPlayMovie(DataArray *d) {
    if (d->Int(2) != 0) {
        if (!mMovie.IsLoading() && !mMovie.IsOpen())
            DoBeginMovieFromFile(nullptr);
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
