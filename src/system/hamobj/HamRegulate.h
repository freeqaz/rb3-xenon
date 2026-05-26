#pragma once
#include "char/CharIKFoot.h"
#include "char/CharPollable.h"
#include "char/Character.h"
#include "char/Waypoint.h"
#include "rndobj/Highlight.h"
#include "utl/MemMgr.h"

/** "Class to do regulation on a HamCharacter.  Has two modes of operation" */
class HamRegulate : public RndHighlightable, public CharPollable {
public:
    // Hmx::Object
    virtual ~HamRegulate();
    OBJ_CLASSNAME(HamRegulate);
    OBJ_SET_TYPE(HamRegulate);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // RndHighlightable
    virtual void Highlight() {}
    // CharPollable
    virtual void Poll();
    virtual void Enter();
    virtual void PollDeps(std::list<Hmx::Object *> &, std::list<Hmx::Object *> &);

    OBJ_MEM_OVERLOAD(0x18)
    NEW_OBJ(HamRegulate)

    void RegulateWay(Waypoint *, float);
    void SetWaypoint(Waypoint *w) { mWaypoint = w; }

protected:
    HamRegulate();

    virtual void SetName(const char *, ObjectDir *);

    void Regulate(Vector3 &, float &);

    Character *mCharacter; // 0x10
    ObjPtr<Waypoint> mWaypoint; // 0x14
    int mRegulateMode; // 0x28
    float mArriveRadius; // 0x2c
    Vector3 mPosDelta; // 0x30
    Vector3 mAccumVelocity; // 0x40
    int mFootState; // 0x50
    float mMaxSpeed; // 0x54
    ObjPtr<CharIKFoot> mLeftFoot; // 0x58
    ObjPtr<CharIKFoot> mRightFoot; // 0x6c
};
