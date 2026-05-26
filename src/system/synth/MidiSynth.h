#pragma once
#include "synth/MidiChannel.h"

class MidiSynth {
public:
    MidiSynth();

protected:
    std::vector<MidiChannel> mChannels; // 0x0
};
