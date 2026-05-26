#include "synth_xbox/FxSendSynapse.h"

namespace DSP {

SynapseAPOParams::SynapseAPOParams() {
    for (int i = 0; i < 3; i++) {
        bands[i].freq = 220.0f;
        bands[i].gain = 0.0f;
        bands[i].enabled = 0;
        bands[i].q = 0.0f;
    }
    lowCutoffFreq = 20.0f;
    highCutoffFreq = 40.0f;
}

}  // namespace DSP
