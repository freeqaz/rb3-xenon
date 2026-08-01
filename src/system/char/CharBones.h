#pragma once
#include "obj/Object.h"
#include "stl/_vector.h"
#include "utl/MemMgr.h"
#include "utl/Symbol.h"
#include <vector>
#include <list>

class CharClip;


class ShortQuat {
public:
    void Set(const Hmx::Quat &);
    void ToQuat(Hmx::Quat &) const;

    short x;
    short y;
    short z;
    short w;
};

class ByteQuat {
public:
    void Set(const Hmx::Quat &);
    void ToQuat(Hmx::Quat &) const;
    char x;
    char y;
    char z;
    char w;
};

class CharBones {
public:
    enum Type {
        TYPE_POS = 0,
        TYPE_SCALE = 1,
        TYPE_QUAT = 2,
        TYPE_ROTX = 3,
        TYPE_ROTY = 4,
        TYPE_ROTZ = 5,
        TYPE_END = 6,
        NUM_TYPES = 7,
    };

    enum CompressionType {
        kCompressNone,
        kCompressRots,
        kCompressVects,
        kCompressQuats,
        kCompressAll
    };

    struct Bone {
        Bone() : name(), weight(1.0f) {}
        Bone(Symbol s, float w) : name(s), weight(w) {}
        Bone(const Bone &bone) : name(bone.name), weight(bone.weight) {}

        /** "Bone to blend into" */
        Symbol name;
        /** "Weight to blend with" */
        float weight;
    };

    CharBones();
    virtual ~CharBones() {}
    virtual void ScaleAdd(CharClip *, float, float, float);
    virtual void Print();

    void Zero();
    int TypeSize(int) const;
    void SetCompression(CompressionType);
    int FindOffset(Symbol) const;
    void *FindPtr(Symbol) const;
    const char *StringVal(Symbol);
    void SetWeights(float);
    void ListBones(std::list<Bone> &) const;
    void ClearBones();
    void AddBones(const std::vector<Bone> &);
    void AddBones(const std::list<Bone> &);
    void ScaleAdd(CharBones &, float) const;
    void ScaleDown(CharBones &, float) const;
    void RotateBy(CharBones &) const;
    void RotateTo(CharBones &, float) const;
    void Enter() {
        Zero();
        SetWeights(0);
    }
    void ScaleAddIdentity();
    void Blend(CharBones &) const;
    CompressionType GetCompression() const { return mCompression; }
    int TotalSize() const { return mTotalSize; }
    std::vector<Bone> GetBones() { return mBones; }
    Bone GetBonesAt(int index) { return mBones[index]; }
    char *GetStart() const { return mStart; }
    int GetOffset(Type type) const { return mOffsets[type]; }
    // rb3-Wii-style inline offset accessors. Retail RB3 addresses the bone
    // buffer through `&mBones` (the inlined accessor's `this`), not through the
    // owning object's `this` — see CharMirror::Poll.
    char *Start() const { return mStart; }
    char *ScaleOffset() const { return mStart + mOffsets[TYPE_SCALE]; }
    char *QuatOffset() const { return mStart + mOffsets[TYPE_QUAT]; }
    char *RotXOffset() const { return mStart + mOffsets[TYPE_ROTX]; }
    char *RotYOffset() const { return mStart + mOffsets[TYPE_ROTY]; }
    char *RotZOffset() const { return mStart + mOffsets[TYPE_ROTZ]; }
    char *EndOffset() const { return mStart + mOffsets[TYPE_END]; }
    int GetCount(int idx) const { return mCounts[idx]; }

    static Type TypeOf(Symbol);
    static const char *SuffixOf(Type);
    static Symbol ChannelName(const char *, Type);
    static void SetWeights(float, std::vector<Bone> &);

protected:
    virtual void ReallocateInternal() {}

    void AddBoneInternal(const Bone &);
    void RecomputeSizes();

    CompressionType mCompression; // 0x4
    /** "Bones that are animated" */
    std::vector<Bone> mBones; // 0x8
    char *mStart; // 0x14
    int mCounts[NUM_TYPES]; // 0x18 - 0x30
    int mOffsets[NUM_TYPES]; // 0x34 - 0x4c
    int mTotalSize; // 0x50
};

/** "Holds state for a set of bones" */
class CharBonesObject : public CharBones, public virtual Hmx::Object {
public:
    OBJ_CLASSNAME(CharBonesObject);
    OBJ_SET_TYPE(CharBonesObject);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);

    NEW_OBJ(CharBonesObject)
    static void Init() { REGISTER_OBJ_FACTORY(CharBonesObject) }
};

/** "Holds state for a set of bones, and allocates own space" */
class CharBonesAlloc : public CharBonesObject {
public:
    virtual ~CharBonesAlloc();

    MEM_OVERLOAD(CharBonesAlloc, 0x172);

    friend class CharMirror;

protected:
    virtual void ReallocateInternal();
};

BinStream &operator<<(BinStream &, const CharBones::Bone &);
BinStream &operator>>(BinStream &, CharBones::Bone &);

bool PropSync(CharBones::Bone &, DataNode &, DataArray *, int, PropOp);

inline short MakeShortAng(float f) {
    f = f * 1638.4f + 0.5f;
    MILO_ASSERT(f < 32768 && f > -32767, 0x60);
    f = floor(f);
    return f;
}