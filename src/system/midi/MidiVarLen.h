#pragma once
#include "utl/BinStream.h"

class MidiVarLenNumber {
private:
    int mValue;

public:
    MidiVarLenNumber(BinStream &);
    BinStream &Read(BinStream &);
    int Value() const { return mValue; }
};
