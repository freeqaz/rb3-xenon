#pragma once
#include "obj/Object.h"
#include "synth/FxSend.h"
#include "synth/PlayableSample.h"
#include "synth/Stream.h"
#include "synth/SynthSample.h"

// Retail RB3-360 does NOT derive PlayableSample. Binary evidence from the
// pinned default/SampleInst span (all three instruments agree):
//   1. ctor fn_8272A940 stores exactly ONE vfptr (`stw r9, 0x0(r30)`) and then
//      inits floats at 0x28/0x2c/0x30/0x34/0x38/0x3c before `addi r3,r30,0x40`
//      for the sole ObjPtr ctor -> Object (0x28) is followed immediately by
//      mVolume; there is no second base subobject and no mSample.
//   2. dtor fn_8272A9F8 destroys exactly ONE ObjPtr, at 0x40 (mSend).
//   3. the setters pin the vtable prefix: SetVolume (fn_8272AAB8) dispatches
//      slot 0x74, SetBankPan (fn_8272A8D8) slot 0x78, SetSpeed/SetBankSpeed
//      (fn_8272A8F8/fn_8272A918) slot 0x7c -> SetVolumeImpl/SetPanImpl/
//      SetSpeedImpl are slots 29/30/31, i.e. exactly TWO virtuals (Pause,
//      SetADSR) sit between IsPlaying and SetFXCore.
// Both facts are reproduced by rb3-Wii's single-inheritance SampleInst with a
// uniform +0xc shift (Wii Hmx::Object is 0x1c, 360's is 0x28). DC3 (newer)
// refactored the class onto a PlayableSample/SynthPollable MI base and added an
// ObjPtr<SynthSample> mSample, which together insert 0x18 before mVolume and
// push Pause/SetADSR into a secondary vtable. Gate the DC3 form behind
// HX_NATIVE (same treatment as MidiInstrument.h) so the native runtime keeps
// its poll loop while the matching build reproduces retail's layout+vtable.
// `virtual` in the native build only. Retail RB3-360's SampleInst vtable is 35
// slots and ENDS at slot 34 (SetReverbEnableImpl) -- the retail prefix pinned
// in the comment above -- so nothing declared after it occupies a retail slot.
// Ours was 40. The five over-length entries were Play/Stop/DonePlaying/
// EndLoopImpl/ElapsedTime, and four of those exist solely to satisfy
// PlayableSample's pure virtuals, a base SampleInst only has under HX_NATIVE.
// Devirtualizing them in the matching build is behaviour-preserving, not just
// slot-correct: SampleInst360 is the ONLY class deriving from SampleInst and it
// overrides NONE of the five, so virtual and non-virtual dispatch resolve to the
// same function. This is the same finding, and the same treatment, that the
// non-virtual setters below already carry.
#ifdef HX_NATIVE
#define SAMPLEINST_NATIVE_VIRTUAL virtual
#else
#define SAMPLEINST_NATIVE_VIRTUAL
#endif

class SampleInst : public Hmx::Object
#ifdef HX_NATIVE
                   ,
                   public PlayableSample
#endif
{
public:
    SampleInst(SynthSample *);
    virtual ~SampleInst();
    virtual bool IsPlaying() const = 0; // slot 21
    virtual void Pause(bool) = 0; // slot 22
    virtual void SetADSR(const ADSRImpl &) = 0; // slot 23
    virtual void SetFXCore(FXCore) = 0; // slot 24
    virtual float GetProgress() { return 0; } // slot 25
    virtual void SetStartProgress(float) {} // slot 26
    virtual void StartImpl() = 0; // slot 27
    virtual void StopImpl(bool) = 0; // slot 28
    virtual void SetVolumeImpl(float) = 0; // slot 29 -> 0x74
    virtual void SetPanImpl(float) = 0; // slot 30 -> 0x78
    virtual void SetSpeedImpl(float) = 0; // slot 31 -> 0x7c
    virtual void SetSendImpl(FxSend *) {} // slot 32
    virtual void SetReverbMixDbImpl(float) {} // slot 33
    virtual void SetReverbEnableImpl(bool) {} // slot 34
    // --- DC3-only tail: kept so the newer engine sources still compile, but
    // declared AFTER the retail prefix so they cannot perturb slots 21..34.
#ifdef HX_NATIVE
    virtual void SynthPoll();
#endif
    SAMPLEINST_NATIVE_VIRTUAL void Play(float);
    SAMPLEINST_NATIVE_VIRTUAL void Stop(bool);
    SAMPLEINST_NATIVE_VIRTUAL bool DonePlaying();
    SAMPLEINST_NATIVE_VIRTUAL void EndLoopImpl() {
        MILO_NOTIFY("EndLoop not implemented on this platform\n");
    }
    // Non-virtual setters: retail RB3-360 calls these directly (bl) from
    // SfxInst's SampleInst* loops (Sfx.cpp), not through a vtable.
    void SetVolume(float);
    void SetPan(float);
    void SetSpeed(float);
    void SetReverbMixDb(float);
    void SetReverbEnable(bool);
    void SetSend(FxSend *);
#ifdef HX_NATIVE
    virtual void SetEventReceiver(Hmx::Object *);
    virtual Hmx::Object *GetEventReceiver() { return mEventReceiver; }
    virtual void EndLoop();
#endif
    SAMPLEINST_NATIVE_VIRTUAL float ElapsedTime() { return 0; }

    void SetBankVolume(float);
    void SetBankPan(float);
    void SetBankSpeed(float);

protected:
    void UpdateVolume();

    // Retail offsets (Hmx::Object is 0x28 on 360; rb3-Wii's layout +0xc).
    float mVolume; // 0x28
    float mBankVolume; // 0x2c
    float mPan; // 0x30
    float mBankPan; // 0x34
    float mSpeed; // 0x38
    float mBankSpeed; // 0x3c
    ObjPtr<FxSend> mSend; // 0x40
    float mReverbMixDb; // 0x4c
    bool mReverbEnabled; // 0x50
    // unka0/unka1 are free: they land in the padding at 0x51/0x52 that retail
    // leaves after mReverbEnabled, so sizeof stays 0x54 either way.
    bool unka0; // 0x51
    bool unka1; // 0x52
    // --- DC3-only tail, gated OUT of the matching build. Retail's ctor inits
    // nothing past mBankSpeed and its dtor destroys only mSend, so none of these
    // exist in RB3-360. They must not merely be parked at the end either:
    // sizeof(SampleInst) is load-bearing because SampleInst360::mVoice is the
    // first derived member, and retail dereferences it at 0x54
    // (SampleInst360::SetADSR: `lwz r11, 0x54(r31)`) == sizeof(SampleInst).
#ifdef HX_NATIVE
    ObjPtr<Hmx::Object> mEventReceiver;
    float unk98;
    ObjPtr<SynthSample> mSample;
#endif
};

#undef SAMPLEINST_NATIVE_VIRTUAL
