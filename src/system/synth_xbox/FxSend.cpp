#include "FxSend.h"
#include "Synth.h"
#include "os/Debug.h"
#include "synth/FxSend.h"
#include "utl/Std.h"

FxSend360::FxSend360(FxSend *fx) : unk4(0), mThis(fx), unk30(true) {
    TheXboxSynth->AddFxSend(this);
    MILO_ASSERT(mThis, 0x19);
}

// Retail 0x82B699E8 (372 B) cleans the chain inline: a local vector filled by
// FxSend::BuildChainVector (vtable +0x60), walked from the back, each entry
// dynamic_cast to FxSend360 and Cleanup()'d (0x82B69868). There is no
// out-of-line CleanChain; Synth360::Terminate calls Cleanup directly.
FxSend360::~FxSend360() {
    if (TheXboxSynth)
        TheXboxSynth->RemoveFxSend(this);
    std::vector<FxSend *> sends;
    mThis->BuildChainVector(sends);
    for (int i = sends.size() - 1; i >= 0; i--) {
        FxSend360 *send360 = dynamic_cast<FxSend360 *>(sends[i]);
        send360->Cleanup();
    }
}

// Declared virtual in FxSend.h and called from Voice.cpp, but never defined in
// this tree — our synth_xbox/FxSend.cpp is a truncated copy of DC3's.  Retail
// fn_82B697C8 (104 B) is this body: it scans mOwnerVoices (begin @0x34,
// end @0x38) WITHOUT an early exit, keeping the LAST match, then calls
// vector<Voice*>::_M_erase(pos, __false_type()) — the zeroed stack byte passed
// in r5 is that __false_type temp.  A std::find would have branched out of the
// loop on the first hit, so the no-break scan is load-bearing, not incidental.
void FxSend360::RemoveOwnerVoice(Voice *v) {
    std::vector<Voice *>::iterator itFind = mOwnerVoices.end();
    FOREACH (it, mOwnerVoices) {
        if (*it == v) {
            itFind = it;
        }
    }
    MILO_ASSERT(itFind != mOwnerVoices.end(), 0x265);
    mOwnerVoices.erase(itFind);
}

void FxSend360::AddOwnerVoice(Voice *v) { mOwnerVoices.push_back(v); }

