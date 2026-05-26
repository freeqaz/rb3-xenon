#pragma once
#include "obj/Data.h"
#include "obj/Object.h"
#include "rndobj/Overlay.h"
#include <vector>

class Watcher : public Hmx::Object {
public:
    virtual DataNode Handle(DataArray *, bool);

    void Init();
    void Update();
    void Add(DataArray *);
    void Remove(int);
    void SaveWatches();
    void LoadWatches();
    void SetOverlay(RndOverlay *o) { mOverlay = o; }

private:
    DataNode OnAdd(const DataArray *);
    DataNode OnRemove(const DataArray *);

    RndOverlay *mOverlay; // 0x2c
    std::vector<std::pair<DataArray *, DataNode> > mWatches; // 0x30
};
