#pragma once
#include "char/CharDriver.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "utl/BinStream.h"
#include "utl/MemMgr.h"

class CharDriverMidi : public CharDriver {
public:
    virtual ~CharDriverMidi();
    OBJ_CLASSNAME(CharDriverMidi)
    OBJ_SET_TYPE(CharDriverMidi)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);

    virtual void Poll();
    virtual void PollDeps(std::list<Hmx::Object *> &, std::list<Hmx::Object *> &);
    virtual void Enter();
    virtual void Exit();

    OBJ_MEM_OVERLOAD(0x14)
    NEW_OBJ(CharDriverMidi);

    bool mActive; // 0x8c - set true in Enter(), controls clip lookup vs default clip
    Symbol mParser; // 0x90
    Symbol mFlagParser; // 0x94
    int mClipFlags; // 0x98
    float mBlendOverridePct; // 0x9c

protected:
    CharDriverMidi();

    DataNode OnMidiParser(DataArray *);
    DataNode OnMidiParserFlags(DataArray *);
    DataNode OnMidiParserGroup(DataArray *);
};
