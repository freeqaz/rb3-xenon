#include "movie/Movie.h"
#include "MovieImpl.h"
#include "MovieSys.h"
#include "macros.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/System.h"
#include "os/Timer.h"
#include "synth/Faders.h"
#include "synth/Synth.h"
#include "ui/UIListState.h"
#include "utl/BinStream.h"
#include "utl/Symbol.h"

Movie::Movie() : mImpl(nullptr) {
    mImpl = TheMovieSys.CreateMovieImpl();
    MILO_ASSERT(mImpl, 0x98);
}

Movie::~Movie() { RELEASE(mImpl); }

void Movie::Init() { TheMovieSys.Init(); }

void Movie::Terminate() { TheMovieSys.Terminate(); }

void Movie::Validate() { TheMovieSys.Validate(); }

void Movie::Save(BinStream *stream) { mImpl->Save(stream); }

void Movie::End() { mImpl->End(); }

bool Movie::IsOpen() const { return mImpl->IsOpen(); }

bool Movie::IsLoading() const { return mImpl->IsLoading(); }

bool Movie::CheckOpen(bool b) { return mImpl->CheckOpen(b); }

bool Movie::Ready() const { return mImpl->Ready(); }

void Movie::SetPaused(bool b) { mImpl->SetPaused(b); }

void Movie::UnlockThread() { mImpl->UnlockThread(); }

void Movie::LockThread() { mImpl->LockThread(); }

int Movie::GetFrame() const { return mImpl->GetFrame(); }

float Movie::MsPerFrame() const { return mImpl->MsPerFrame(); }

int Movie::NumFrames() const { return mImpl->NumFrames(); }

void Movie::SetVolume(float f) { mImpl->SetVolume(f); }

int Movie::LocalizationTrack() {
    Symbol language = HongKongExceptionMet() ? Symbol("eng") : SystemLanguage();

    DataArray *langs = SupportedLanguages(false);
    int i;
    for (i = 0; i < langs->Size(); i++) {
        if (langs->Sym(i) == language)
            break;
    }
    return (i < langs->Size() ? i : 0) + 1;
}

bool Movie::BeginFromFile(
    char const *c,
    float f,
    bool b1,
    bool b2,
    bool b3,
    bool b4,
    int i,
    BinStream *stream,
    LoaderPos lp
) {
    MILO_ASSERT(TheMovieSys.IsInitialized(), 0xdc);
    return mImpl->BeginFromFile(c, f, b1, b2, b3, b4, i, stream, lp);
}

bool Movie::BeginFromFile(
    char const *c, float f, bool b1, bool b2, bool b3, bool b4, int i, BinStream *stream
) {
    return BeginFromFile(c, f, b1, b2, b3, b4, i, stream, kLoadFront);
}

void Movie::Draw() {
    START_AUTO_TIMER("movie");
    mImpl->Draw();
}

bool Movie::Poll() {
    START_AUTO_TIMER("movie");
    return mImpl->Poll();
}

void Movie::SetWidthHeight(int a, int b) { mImpl->SetWidthHeight(a, b); }
