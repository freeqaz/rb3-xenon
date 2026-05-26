#pragma once
#include "utl/Symbol.h"

class GemListInterface {
public:
    GemListInterface() {}
    virtual ~GemListInterface() {}
    virtual void SetTrack(Symbol) = 0;
    virtual bool GetGem(int, int &, int &, int &) = 0;
};
