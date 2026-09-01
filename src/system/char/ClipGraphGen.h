#pragma once
#include "char/CharClip.h"
#include "char/ClipDistMap.h"
#include "obj/Data.h"
#include "obj/Object.h"

class ClipGraphGenerator : public Hmx::Object {
public:
    // Hmx::Object
    virtual ~ClipGraphGenerator();
    virtual DataNode Handle(DataArray *, bool);

    ClipGraphGenerator();
    ClipDistMap *
    GeneratePair(CharClip *, CharClip *, ClipDistMap::Node *, ClipDistMap::Node *);

    NEW_OBJ(ClipGraphGenerator);

protected:
    const DataArray *mTypeData; // 0x28
    ClipDistMap *mDmap; // 0x2c
    CharClip *mClipA; // 0x30
    CharClip *mClipB; // 0x34

    DataNode OnGenerateTransitions(DataArray *);
};
