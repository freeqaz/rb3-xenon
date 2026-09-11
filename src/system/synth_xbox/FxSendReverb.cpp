#include "FxSendReverb.h"
#include "FxSend.h"

FxSendReverb360::FxSendReverb360() : FxSend360(this) {}

FxSendReverb360::~FxSendReverb360() {}

// XDK export (leapfxlib); Synth.cpp declares it the same way for the master
// chain.  Retail 0x82B67BD8: `addi r3,r1,0x50; bl fn_82BBF340; lwz r3,0x50(r1)`.
extern "C" HRESULT CreateAudioReverb(IUnknown **ppApo);

IUnknown *FxSendReverb360::CreateFx() {
    IUnknown *apo;
    CreateAudioReverb(&apo);
    return apo;
}
