#pragma once
#include "MovieImpl.h"
#include "MovieSys.h"
#include "synth/Faders.h"
#include "utl/BinStream.h"
#include "utl/Loader.h"

class Movie {
public:
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
    void Draw();
    bool Poll();
    void SetWidthHeight(int, int);
    MovieImpl *GetImpl() const { return mImpl; }

protected:
    // RB3 retail Movie is a single Impl pointer (4 bytes); the FaderGroup-based
    // volume fader is a newer dc3-engine addition not present in RB3. Confirmed
    // against rb3-Wii Movie (mImpl only) and the embedded-Movie offsets in
    // MoviePanel/TexMovie target asm (mMovie 4 bytes: mSubtitlesLoader lands at
    // 0x60 not 0x64). mImpl@0x0.
    MovieImpl *mImpl; // 0x0
};
