#pragma once
#include "MoveDir.h"
#include "hamobj/Difficulty.h"
#include "hamobj/HamCharacter.h"
#include "math/Mtx.h"
#include "math/Vec.h"
#include "obj/Object.h"
#include "rndobj/Trans.h"
#include "utl/MemMgr.h"

struct BeatCollisionData {
    void Set(float, float, const Transform &, const Transform &);

    float mMinX; // 0x0
    float mMaxX; // 0x4
    Vector3 mOffset; // 0x8
};

struct SongCollisionOutput {
    // Raw storage - 228 bytes (0xE4)
    // Layout:
    //   0x00-0x5F: offset arrays (6 entries at 16-byte stride)
    //   0x60-0x8F: player collision data (48 bytes)
    //   0x90-0x9B: world pos player 0
    //   0x9C-0xCF: padding (52 bytes)
    //   0xD0-0xDB: world pos player 1
    //   0xDC-0xDF: padding (4 bytes)
    //   0xE0: mColliding flag
    //   0xE1-0xE3: padding
    char _data[0xE4];

    // Get world position for player (0 or 1) at 64-byte stride from 0x90
    const Vector3 &WorldPos(int playerIdx) const {
        return *reinterpret_cast<const Vector3 *>(
            _data + 0x90 + playerIdx * 0x40);
    }

    // Get offset at index (0-5) at 16-byte stride
    const Vector3 &Offset(int idx) const {
        return *reinterpret_cast<const Vector3 *>(_data + idx * 0x10);
    }

    // Get collision flag
    bool Colliding() const { return *reinterpret_cast<const bool *>(_data + 0xE0); }
};
static_assert(sizeof(SongCollisionOutput) == 0xE4, "SongCollisionOutput size mismatch");

/** "Contains data for handling potential character collisions" */
class SongCollision : public Hmx::Object {
public:
    // Hmx::Object
    OBJ_CLASSNAME(SongCollision);
    OBJ_SET_TYPE(SongCollision);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void Print();

    OBJ_MEM_OVERLOAD(0x2D)
    NEW_OBJ(SongCollision)

    void Update(MoveDir *);
    bool Equals(SongCollision *);
    bool IsCollision(
        int,
        int,
        const Difficulty *const,
        const Transform *const,
        std::vector<SongCollisionOutput> *
    ) const;

    static void Init();
    static void GatherUsefulBones(std::vector<RndTransformable *> &, HamCharacter *);

private:
    static float sCollisionTolerance;

    friend bool AreDancersColliding1D(
        std::vector<RndTransformable *> &,
        std::vector<RndTransformable *> &,
        const Vector3 &,
        const Vector3 &
    );

    const BeatCollisionData *BeatData(int, Difficulty) const;
    void CheckCollision(
        int, const Difficulty *const, const Transform *const, SongCollisionOutput &
    ) const;

protected:
    SongCollision();

    // indexed by difficulty, then beat
    // so mData[kDifficultyExpert][4] is
    // the BeatCollisionData for expert difficulty at beat 4
    std::vector<BeatCollisionData> mData[kNumDifficulties]; // 0x2c
};
