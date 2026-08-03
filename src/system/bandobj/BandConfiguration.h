#pragma once
// ⚠ obj/ObjMacros.h FIRST, and it is not optional. obj/Object.h pulls the
// "dialect" macro set (obj/dialect_object_push.h), in which INIT_REVS takes
// (rev, alt) and DECLARE_REVS / REGISTER_OBJ_FACTORY_FUNC do not exist -- the
// class then silently stops parsing at DECLARE_REVS and mXfms vanishes. Every
// other ported bandobj header (e.g. UnisonIcon.h) leads with this include for
// the same reason.
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "math/Mtx.h"
#include "char/Waypoint.h"

class BandConfiguration : public Hmx::Object {
public:
    // size 0x34
    class TargTransform {
    public:
        Symbol targName; // 0x0
        Transform xfm; // 0x4
    };

    // size 0xa0
    class TargTransforms {
    public:
        TargTransform xfms[3];
        Waypoint *mWay;

        static int sNumPlayModes;
    };

    BandConfiguration();
    virtual ~BandConfiguration();
    OBJ_CLASSNAME(BandConfiguration);
    OBJ_SET_TYPE(BandConfiguration);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);

    void SyncPlayMode();
    int ConfigIndex();

    DataNode OnStoreConfiguration(DataArray *);
    DataNode OnReleaseConfiguration(DataArray *);

    DECLARE_REVS;
    NEW_OVERLOAD;
    DELETE_OVERLOAD;
    NEW_OBJ(BandConfiguration)
    // ⚠ Init() MUST stay an inline one-liner in this header. Band.cpp's
    // BandInit() sees it through the scatter-include into BandCharacter.cpp,
    // and /Ob2 inlines the whole StaticClassName+RegisterFactory pattern
    // directly into BandInit() -- exactly as retail does. An out-of-line
    // Init() would desync BandInit's instruction sequence. This is the same
    // constraint the factory-only shim in Band.cpp was written to satisfy;
    // this real header replaces that shim and must preserve the shape.
    static void Init() { Register(); }
    REGISTER_OBJ_FACTORY_FUNC(BandConfiguration)

    TargTransforms mXfms[4]; // 0x1c, 0xbc, 0x15c, 0x1fc
    // 0x29c
};
