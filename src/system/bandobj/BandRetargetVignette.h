#pragma once
#include "rndobj/Poll.h"
#include "rndobj/Trans.h"

class BandRetargetVignette : public RndPollable {
public:
    BandRetargetVignette();
    OBJ_CLASSNAME(BandRetargetVignette);
    OBJ_SET_TYPE_ENGINE(BandRetargetVignette);
    virtual DataNode Handle(DataArray *, bool);
    virtual void Poll();
    virtual void Enter();
    virtual void Exit();
    virtual void ListPollChildren(std::list<RndPollable *> &) const;
    virtual ~BandRetargetVignette();
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);

    void EnterDir() const;

    static const char *sIkfs[];
    static unsigned short gRev;
    static unsigned short gAltRev;
    NEW_OVERLOAD;
    DELETE_OVERLOAD;
    NEW_OBJ(BandRetargetVignette)
    static void Init() { Register(); }
    static void Register() { REGISTER_OBJ_FACTORY(BandRetargetVignette); }

    std::list<String> mEffectors; // 0x8
    Symbol mPlayer; // 0x10
    Symbol mBone; // 0x14
    ObjPtr<RndTransformable> mProp; // 0x18
};
