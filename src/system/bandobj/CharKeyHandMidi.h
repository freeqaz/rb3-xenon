#pragma once
#include "char/CharWeightable.h"
#include "char/CharPollable.h"
#include "char/CharIKFingers.h"

class CharKeyHandMidi : public RndHighlightable,
                        public CharWeightable,
                        public CharPollable {
public:
    enum KeyboardKey {
        kNoKey = 0x2f,
        kKeyC4 = 0x48
    };

    CharKeyHandMidi();
    virtual ~CharKeyHandMidi();
    virtual void Highlight();
    OBJ_CLASSNAME(CharKeyHandMidi);
    OBJ_SET_TYPE(CharKeyHandMidi);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void Poll();
    virtual void PollDeps(std::list<Hmx::Object *> &, std::list<Hmx::Object *> &);
    virtual void Enter();
    virtual void SetName(const char *, ObjectDir *);

    void RunTest();
    void EndTest();
    bool KeyFinger(CharIKFingers::FingerNum, KeyboardKey);
    void UnkeyFinger(CharIKFingers::FingerNum);
    CharIKFingers::FingerNum DefaultSelectFinger(KeyboardKey);
    CharIKFingers::FingerNum FindPreferredFinger(KeyboardKey, KeyboardKey, CharIKFingers::FingerNum);
    bool IsBlackKey(KeyboardKey);

    DataNode OnFingersUp(DataArray *);
    DataNode OnFingersDown(DataArray *);

    static unsigned short gRev;
    static unsigned short gAltRev;
    NEW_OVERLOAD;
    DELETE_OVERLOAD;
    NEW_OBJ(CharKeyHandMidi)
    static void Init() { Register(); }
    static void Register() { REGISTER_OBJ_FACTORY(CharKeyHandMidi) }

    ObjPtr<CharIKFingers> mIKObject; // 0x28
    ObjPtr<RndTransformable> mFirstSpot; // 0x34
    ObjPtr<RndTransformable> mSecondSpot; // 0x40
    std::vector<Vector3> unk4c; // 0x4c
    std::vector<Vector3> unk54; // 0x58
    std::vector<KeyboardKey> unk5c; // 0x64
    int unk64; // 0x70
    int unk68; // 0x74
    std::vector<int> unk6c; // 0x78
    int unk74; // 0x84
    bool unk78; // 0x88
    ObjPtr<Character> unk7c; // 0x8c
    float unk88; // 0x98
    bool mIsRightHand; // 0x9c
};