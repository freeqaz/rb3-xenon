#pragma once
#include "obj/Object.h"

class PropertyEventProvider : public virtual Hmx::Object {
public:
    // Hmx::Object
    virtual ~PropertyEventProvider() {}
    OBJ_CLASSNAME(PropertyEventProvider)
    OBJ_SET_TYPE(PropertyEventProvider)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, CopyType);
    virtual void Load(BinStream &);

    NEW_OVERLOAD;
    DELETE_OVERLOAD
    NEW_OBJ(PropertyEventProvider)
protected:
    PropertyEventProvider();

    std::map<Symbol, float> mProperties; // 0x4
};

extern PropertyEventProvider *TheHamProvider;
