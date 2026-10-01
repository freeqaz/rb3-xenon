#pragma once
#include "meta/MemcardAction.h"

class BandProfile;

// The two profile memcard actions SaveLoadManager queues. Retail RTTI
// `.?AVSaveMemcardAction@@` (vtable 0x820B44B4) and `.?AVLoadMemcardAction@@`
// (vtable 0x820B44C4): three slots each (deleting dtor, PreAction, PostAction),
// so neither class adds a virtual. Both are 0x14 bytes (sizeof MemcardAction):
// SaveLoadManager allocates them with `li r3,0x14`. Neither declares a dtor:
// their shared deleting dtor (0x825D7940) stores only MemcardAction's vtable.
class SaveMemcardAction : public MemcardAction {
public:
    SaveMemcardAction(BandProfile *);
    virtual void PreAction();
    virtual void PostAction();
};

class LoadMemcardAction : public MemcardAction {
public:
    LoadMemcardAction(BandProfile *);
    virtual void PreAction();
    virtual void PostAction();
};
