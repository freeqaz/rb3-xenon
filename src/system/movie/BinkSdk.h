#pragma once

// The leading fields of the RAD Bink SDK's BINK structure, with the SDK's own
// field names (the vendor header is in ../dc3-decomp at src/binkxenon/bink.h).
// The SDK allocates and owns the struct; Milo code only reads these fields
// through the BINK * that BinkOpen returns, so nothing past NumTracks is
// declared and sizeof(BINK) here means nothing.
//
// One declaration for every TU that reads a BINK.  movie/Movie.cpp,
// synth/BinkReader.h, utl/BinkIntegration.cpp and moviebink/BinkMovieImpl.h
// each used to declare its own partial copy -- four layouts of one struct
// (tools/layout_odr.py), one of them with an invented virtual destructor.
struct BINK {
    unsigned int Width; // 0x00
    unsigned int Height; // 0x04
    unsigned int Frames; // 0x08
    unsigned int FrameNum; // 0x0c  frame to be displayed (1-based)
    unsigned int LastFrameNum; // 0x10
    unsigned int FrameRate; // 0x14
    unsigned int FrameRateDiv; // 0x18
    unsigned int ReadError; // 0x1c
    unsigned int OpenFlags; // 0x20
    unsigned int BinkType; // 0x24
    unsigned int Size; // 0x28
    unsigned int FrameSize; // 0x2c
    unsigned int SndSize; // 0x30
    unsigned int FrameChangePercent; // 0x34
    int NumTracks; // 0x38
};
