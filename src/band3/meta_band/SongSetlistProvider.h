#pragma once
#include "obj/Object.h"
#include "ui/UIListProvider.h"

class SetlistProvider : public UIListProvider, public Hmx::Object {
public:
    SetlistProvider() {}
    // No user-declared dtor: retail ~SetlistProvider (0x8253ab38, called only by
    // its ??_G 0x8253b348) stores no own vptr before ~Object -- the implicit-dtor
    // shape (W16-HE 4.2). The user-declared {} dtor matched ~StandInProvider
    // (0x826730f0) instead (W16-IA).
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual int NumData() const;
};