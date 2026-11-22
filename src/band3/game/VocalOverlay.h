#pragma once
#include "utl/Str.h"

class VocalPart;
class Singer;

// VocalPlayer's dev text overlay. No method below has a body in any tree, and
// retail has none to port: across every retail .text block of VocalPlayer.cpp
// (27 blocks, 8,644 instructions of TU5 band.exe) the mVocalOverlay slot
// (VocalPlayer+0x3c8, per /d1reportSingleClassLayout) is written once, by the
// constructor's `stw r29, 0x3c8(r30)` at 0x826EA7DC, and never loaded. Nothing
// therefore calls a VocalOverlay method through it (controls: +0x3c4 mOverlay
// fires 3 times, +0x3cc 14). The Poll call sites are `#ifdef HX_NATIVE` in
// VocalPlayer.cpp, so the match build references none of these methods; the
// native targets link no-op stubs (native/src/*_stubs.cpp).
class VocalOverlay {
public:
    VocalOverlay();
    ~VocalOverlay();
    void Reset(int);
    void AppendSingerPitch(int, float);
    void AddPossiblePart(int, VocalPart *);
    void EqualizeSingerStrings();
    void AppendAssignedPart(const Singer *, const std::vector<VocalPart *> &);
    void AppendEnergy(int, float, float);
    void AppendTalkyData(int, bool, bool, float);
    void AppendDeploymentTime(int, float);
    void AppendDeploymentMarker(int);
    void AppendPartData(const std::vector<VocalPart *> &);
    void AppendPhraseMeter(float);
    void FinalizeDisplayString();

    String mDisplayedString; // 0x0
    std::vector<String> mSingerStrings; // 0xc
    String mFooterString; // 0x18
    float unk20; // 0x24
    float unk24; // 0x28
};