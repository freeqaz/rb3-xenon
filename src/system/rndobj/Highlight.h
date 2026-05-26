#pragma once
#include "obj/Object.h"

class RndHighlightable : public virtual Hmx::Object {
public:
    RndHighlightable() {}
    virtual ~RndHighlightable() {}
    virtual void Highlight() = 0;
};
