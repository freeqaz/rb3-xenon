#pragma once
#include "bandobj/BandCharacter.h"
#include "rndobj/Highlight.h"
#include "char/CharWeightable.h"
#include "char/CharPollable.h"
#include "rndobj/Trans.h"

class BandIKEffector : public RndHighlightable,
                       public CharWeightable,
                       public CharPollable {
public:
    class Constraint {
    public:
        Constraint(Hmx::Object *);
        Constraint(const Constraint &);
        Constraint &operator=(const Constraint &);

        ObjPtr<RndTransformable> mTarget; // 0x0
        ObjPtr<RndTransformable> mFinger; // 0xc
        float mWeight; // 0x18
    };

    BandIKEffector();
    virtual ~BandIKEffector();
    virtual void Highlight();
    virtual void Poll();
    virtual void PollDeps(std::list<Hmx::Object *> &, std::list<Hmx::Object *> &);
    OBJ_CLASSNAME(BandIKEffector);
    OBJ_SET_TYPE(BandIKEffector);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void SetName(const char *, ObjectDir *);

    // DECLARE_REVS expanded inline to avoid pulling obj/ObjMacros.h into this
    // header (which would flip other TUs' INIT_REVS macro arity — see BandWardrobe.h).
    static unsigned short gRev;
    static unsigned short gAltRev;
    OBJ_MEM_OVERLOAD(0x12)
    NEW_OBJ(BandIKEffector)
    static void Init() { REGISTER_OBJ_FACTORY(BandIKEffector); }

    static CharClip *sDeformClip;
    static void SetDeformClip(Hmx::Object *);
    static void NeutralLocalXfm(RndTransformable *, Transform &);
    static void NeutralWorldXfm(RndTransformable *, Transform &);
    static void NeutralLocalPos(RndTransformable *, Vector3 &);

    int GetType();
    float GetGroundHeight(RndTransformable *);
    int MeasureLengths(
        RndTransformable *&handBone,
        RndTransformable *&elbowBone,
        float &inv2ab,
        float &aaPlusbb,
        float &aPlusb
    );
    float ApplyPosConstraints(Vector3 &dst, const Vector3 &src, BandIKEffector *root);
    float ApplyConstraints(QuatXfm &q, const Transform &tf, BandIKEffector *root);
    void ComputeHandPullAndQuat(
        QuatXfm &outQuat,
        Transform &outElbowXfm,
        const Transform &shoulderXfm,
        const Vector3 &handTarget,
        float inv2ab,
        float aaPlusbb,
        float aPlusb
    );
    void ComputeElbowPullAndQuat(
        QuatXfm &outQuat, const Transform &shoulderXfm, const Vector3 &elbowTarget
    );
    void DoFancyElbow(QuatXfm &hand, float handWeight);
    void IKElbow(const Vector3 &);

    ObjPtr<RndTransformable> mEffector; // 0x28
    ObjPtr<RndTransformable> mGround; // 0x34
    ObjPtr<BandIKEffector> mMore; // 0x40
    ObjPtr<BandIKEffector> mElbow; // 0x4c
    ObjVector<Constraint> mConstraints; // 0x58
    ObjPtr<BandCharacter> unk64; // 0x64
};