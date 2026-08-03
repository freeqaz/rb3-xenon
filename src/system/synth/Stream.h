#pragma once
#include "synth/ADSR.h"
#include "synth/Faders.h"
#include "synth/FxSend.h"

enum FXCore {
    kFXCore0 = 0,
    kFXCore1 = 1,
    kFXCoreNone = -1
};

struct Marker {
    // Retail's default ctor only default-constructs `name` -- it does NOT
    // zero position/posMS and does NOT build+copy a String() default argument.
    // StandardStream::LoadMarkerList's `Marker marker;` compiles to a single
    // `bl ??0String@@QAA@XZ` on the marker itself; the `= String()` default-arg
    // spelling added a temp String, a copy ctor, two zero stores and a dtor.
    Marker() {}
    Marker(const String &str) : name(str), position(0), posMS(0) {}
    Marker(const String &name, int position, float posMS)
        : name(name), position(position), posMS(posMS) {}
    String name; // 0x0
    int position; // 0xc  (String is 12 bytes -- compiler-verified via
    float posMS; // 0x10  /d1reportSingleClassLayoutMarker: sizeof(Marker) == 20)
};
// RB3 retail's JumpInstance is NOT dc3's four-float POD. Retail evidence
// (TU5 raw bytes):
//   * vector<JumpInstance>::~vector at 0x82703E08 (called on this+0x12c, which
//     GetJumpInstances at 0x82701D00 confirms is mJumpInstances) uses
//     `li r10,0x2c / divw / mulli r3,r11,0x2c` => sizeof == 44, and it
//     materializes a __false_type and makes a real _M_destroy_range call =>
//     the element is NOT trivially destructible.
//   * that destroy-range helper (0x82703830) walks backwards by 0x2c calling
//     the element dtor at 0x8251E650, whose whole body is
//     `r3 = this+0x14; bl 0x827BDF38` then `r3 = this; bl 0x827BDF38`
//     -- i.e. two Marker sub-objects at 0x0 and 0x14. 0x827BDF38 is confirmed
//     to be Marker::~Marker by the sibling vector<Marker> dtor at 0x82703D50
//     (identical code, `li r10,0x14`, same element dtor).
// 20 + 20 + 4 == 44 exactly. This is an RB3-specific divergence from dc3:
// RB3's practice-mode A/B looping (SetLoop(String&, String&) takes two marker
// *names*) needs from/to Markers where dc3's stream only needed floats.
struct JumpInstance {
    Marker mFrom; // 0x0
    Marker mTo; // 0x14
    float mTotal; // 0x28
}; // 0x2c

class Stream {
public:
    virtual ~Stream() { delete mFaders; }
    virtual bool Fail() { return false; }
    virtual bool IsReady() const = 0;
    virtual bool IsFinished() const = 0;
    virtual int GetNumChannels() const = 0;
    virtual int GetNumChanParams() const = 0;
    virtual void Play() = 0;
    virtual void Stop() = 0;
    virtual bool IsPlaying() const = 0;
    virtual bool IsPaused() const { return false; }
    virtual void Resync(float) = 0;
    virtual void Fill() = 0;
    virtual bool FillDone() const = 0;
    virtual void EnableReads(bool) = 0;
    virtual float GetTime() = 0;
    virtual float GetJumpBackTotalTime(float) const = 0;
    virtual float GetInSongTime() = 0;
    virtual std::vector<struct JumpInstance> *GetJumpInstances() = 0;
    virtual float GetFilePos() const = 0;
    virtual float GetFileLength() const = 0;
    virtual void SetVolume(int, float) = 0;
    virtual float GetVolume(int) const = 0;
    virtual void SetPan(int, float) = 0;
    virtual float GetPan(int) const = 0;
    virtual void SetFX(int, bool) = 0;
    virtual bool GetFX(int) const = 0;
    virtual void SetFXCore(int, FXCore) = 0;
    virtual FXCore GetFXCore(int) const = 0;
    virtual void SetFXSend(int, FxSend *) {}
    // Retail takes ADSRImpl, NOT the Hmx::Object-derived ADSR. Verified on
    // retail bytes: StandardStream::SetADSR is fn_82702C20 and it sits at the
    // INHERITED Stream slot 0x74 (mChanParams[chan], addi r3,r11,0xc for
    // &->mADSR, memcpy of 0x28 == sizeof(ADSRImpl), then
    // mChannels[chan]->SetADSR). With `const ADSR&` here the derived
    // ADSRImpl overload is a DIFFERENT signature, so MSVC gives it a NEW slot
    // and StandardStream's vtable runs one slot long from 0xcc onward --
    // pushing SetJumpSamples to 0xdc where retail has 0xd8 and GetSampleRate
    // to 0xe0 where retail has 0xdc.
    virtual void SetADSR(int, const ADSRImpl &) {}
    virtual void SetSpeed(float) = 0;
    virtual float GetSpeed() const = 0;
    virtual void LoadMarkerList(const char *) = 0;
    virtual void ClearMarkerList() {}
    // Retail passes Marker BY VALUE (matches the rb3-Wii oracle): the caller
    // copy-constructs the Marker into its own frame slot and passes that.
    virtual void AddMarker(Marker) {}
    virtual int MarkerListSize() const { return 0; }
    virtual bool MarkerAt(int, Marker &) const { return 0; }
    // rb3-Wii/retail names this SetLoop (NOT a SetJump overload). Keeping the
    // dc3 name "SetJump" collided with SetJump(float,...) below: MSVC groups
    // same-name virtuals at the first declaration's slot, collapsing the float
    // overload from retail's 0xa0 to 0x94 (-3 slots). Renaming to SetLoop breaks
    // the collision so SetJump(float,...) lands at 0xa0.
    virtual void SetLoop(String &, String &) = 0;
    virtual bool CurrentJumpPoints(Marker &, Marker &) { return 0; }
    // RB3 retail (matching rb3-Wii) has an extra AbandonLoop slot here that
    // dc3 (newer) dropped; without it Stream's vtable is one slot short and
    // ChannelFaders lands at 0xbc instead of retail's 0xc0.
    virtual void AbandonLoop() = 0;
    virtual void SetJump(float, float, const char *) = 0;
    virtual void ClearJump() = 0;
    virtual void EnableSlipStreaming(int) = 0;
    virtual void SetSlipOffset(int, float) = 0;
    virtual void SlipStop(int) = 0;
    virtual float GetSlipOffset(int) = 0;
    virtual void SetSlipSpeed(int, float) = 0;
    virtual void SetStereoPair(int, int) {}
    // rb3-Wii returns FaderGroup*, dc3 returns FaderGroup&. Use ptr for
    // header compatibility with MasterAudio.cpp.
    virtual FaderGroup *ChannelFaders(int) = 0;
    virtual void AddVirtualChannels(int) {}
    virtual void RemapChannel(int, int) {}

    void SetVolume(float);

    FaderGroup *Faders() const { return mFaders; }

    static const float kStreamEndMs;
    static const int kStreamEndSamples;

protected:
    Stream();

    FaderGroup *mFaders; // 0x4
};
