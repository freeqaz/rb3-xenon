#pragma once
#include "MovieImpl.h"
#include "MovieSys.h"
#include "synth/Faders.h"
#include "utl/BinStream.h"
#include "utl/Loader.h"

class Movie {
public:
#ifndef HX_NATIVE
    // RB3's Xbox player: one Bink wrapper class, defined in Movie.cpp. Every
    // Movie member below is a two-instruction forward to it
    // (`lwz r3,0(r3); b Impl::X`), except LockThread/UnlockThread/
    // SetWidthHeight, which are inlined into the Movie wrapper.
    class Impl;
#endif

    Movie();
    ~Movie();
    static void Init();
    static void Terminate();
    static void Validate();
    void Save(BinStream *);
    void End();
    bool IsOpen() const;
    bool IsLoading() const;
    bool CheckOpen(bool);
    bool Ready() const;
    void SetPaused(bool);
    void UnlockThread();
    void LockThread();
    int GetFrame() const;
    float MsPerFrame() const;
    int NumFrames() const;
    void SetVolume(float);
    static int LocalizationTrack();
    bool BeginFromFile(
        char const *, float, bool, bool, bool, bool, int, BinStream *, LoaderPos
    );
    // RB3 retail's Movie::BeginFromFile takes no LoaderPos (the LoaderPos
    // parameter is a newer dc3-engine addition). MoviePanel::PlayMovie calls
    // this 8-parameter form -- target passes one fewer stack argument.
    bool BeginFromFile(char const *, float, bool, bool, bool, bool, int, BinStream *);
    void Draw();
    bool Poll();
    void SetWidthHeight(int, int);
    float (*SetTimeCallback(float (*)()))();
#ifdef HX_NATIVE
    MovieImpl *GetImpl() const { return mImpl; }
#endif

protected:
    // RB3 retail Movie is a single Impl pointer (4 bytes); the FaderGroup-based
    // volume fader is a newer dc3-engine addition not present in RB3. Confirmed
    // against the embedded-Movie offsets in
    // MoviePanel/TexMovie target asm (mMovie 4 bytes: mSubtitlesLoader lands at
    // 0x60 not 0x64). mImpl@0x0.
#ifdef HX_NATIVE
    MovieImpl *mImpl; // 0x0
#else
    Impl *mImpl; // 0x0
#endif
};

float TaskMgrDeltaSeconds();
