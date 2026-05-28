#pragma once
// Stub for DC3 (meta_ham/) MemcardAction subclasses. Included by
// system/meta/FixedSizeSaveable.h solely so LoadMemcardAction can be
// declared as a friend; the class body is not needed for compilation.
#include "meta/MemcardAction.h"

class LoadMemcardAction : public MemcardAction {
public:
    LoadMemcardAction(Profile *p) : MemcardAction(p) {}
    virtual void PreAction() {}
    virtual void PostAction() {}
};

class SaveMemcardAction : public MemcardAction {
public:
    SaveMemcardAction(Profile *p) : MemcardAction(p) {}
    virtual void PreAction() {}
    virtual void PostAction() {}
};
