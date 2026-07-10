#pragma once
#include "Synth.h"
#include "stl/_vector.h"
#include "synth/FxSend.h"
#include "synth_xbox/Voice.h"
#include "xdk/xapilibi/xbase.h"
#include "xdk/xaudio2/xaudio2.h"

// Retail 360 FxSend factory allocation shape (NewObject@FxSendXxx360, Synth.obj
// @82B2B580..82B2D348): the OBJ_MEM_OVERLOAD operator new inherited from the
// portable base class is INLINED into NewObject with the base
// StaticClassName() evaluation kept (Symbol-interning side effect), followed by
// the 2-arg retail MemAlloc(size, 0). The global OBJ_MEM_OVERLOAD lever is
// noinline (CacheMgr-verified `bl fn_82709EE0` shape), so the 360 subclasses
// re-declare operator new locally with the inline form.
#define FXSEND360_NEW(baseClass)                                                         \
    static void *operator new(unsigned int s) {                                          \
        void *mem = (MemAlloc)(s, (baseClass::StaticClassName(), 0));                    \
        return mem;                                                                      \
    }                                                                                    \
    static void *operator new(unsigned int s, void *place) { return place; }

class FxSend360 {
public:
    virtual ~FxSend360();
    virtual void SyncEffectParams(IXAudio2SubmixVoice *) const = 0;
    virtual bool IsStandard() const { return true; }
    virtual void AddOwnerVoice(Voice *);
    virtual void RemoveOwnerVoice(Voice *);
    virtual IUnknown *CreateFx() = 0;

    FxSend360(FxSend *);
    void SyncEffectParams();
    void UpdateVolumes();
    void Cleanup();
    void CleanChain();
    void Refresh(std::vector<FxSend *> &);

    int unk4;
    std::vector<IXAudio2SubmixVoice *> unk8;
    std::vector<int> unk14;
    std::vector<IUnknown *> unk20;
    FxSend *mThis; // 0x2c
    bool unk30;
    std::vector<Voice *> mOwnerVoices;

protected:
    virtual void InitParams(IXAudio2SubmixVoice *, int) {}

private:
    struct IXAudio2Voice *OutputVoice();
    void UpdateVoiceMatrices();
    void CreateInputVoice();
};
