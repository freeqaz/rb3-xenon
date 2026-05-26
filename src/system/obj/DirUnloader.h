#pragma once
#include "obj/Object.h"
#include "utl/Loader.h"
#include "utl/MemMgr.h"

class DirUnloader : public Loader, public Hmx::Object {
public:
    DirUnloader(ObjectDir *);
    virtual ~DirUnloader();
    virtual const char *DebugText();
    virtual bool IsLoaded() const { return false; }

private:
    virtual void PollLoading();

    std::vector<ObjPtr<Hmx::Object> > mObjects; // 0x48
};
