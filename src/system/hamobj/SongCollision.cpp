#include "hamobj/SongCollision.h"
#include "hamobj/Difficulty.h"
#include "hamobj/HamCharacter.h"
#include "hamobj/HamDirector.h"
#include "hamobj/HamGameData.h"
#include "hamobj/MocapSkeletonIterator.h"
#include "hamobj/MoveDir.h"
#include "math/Mtx.h"
#include "math/Vec.h"
#include "obj/Data.h"
#include "obj/DataUtl.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Trans.h"
#include "utl/BinStream.h"
#include "utl/Std.h"
#include "utl/TimeConversion.h"
#include <float.h>

#ifdef HX_NATIVE
float SongCollision::sCollisionTolerance;
#endif

std::vector<const char *> sCollisionUsefulBoneNames;
float sCollisionToleranceValue = 0.0f;

void bones_min_max_x(
    float &minX,
    float &maxX,
    std::vector<RndTransformable *> &transes,
    const Transform &xfm
) {
    FOREACH (it, transes) {
        Vector3 v40;
        MultiplyTranspose((*it)->WorldXfm().v, xfm, v40);
        minX = Min(minX, v40.x);
        maxX = Max(maxX, v40.x);
    }
}

namespace {
    void SetSongCollisionOffset(SongCollisionOutput &out, int idx, const Vector3 &pos) {
        *reinterpret_cast<Vector3 *>(out._data + idx * 0x10) = pos;
    }

    void SetSongCollisionWorldPos(SongCollisionOutput &out, int idx, const Vector3 &pos) {
        *reinterpret_cast<Vector3 *>(out._data + 0x90 + idx * 0x40) = pos;
    }

    void SetSongCollisionColliding(SongCollisionOutput &out, bool colliding) {
        *reinterpret_cast<bool *>(out._data + 0xE0) = colliding;
    }
}

bool AreDancersColliding1D(
    std::vector<RndTransformable *> &bones0,
    std::vector<RndTransformable *> &bones1,
    const Vector3 &worldPos0,
    const Vector3 &worldPos1
) {
    if (bones0.empty() || bones1.empty())
        return false;

    Transform xfm;
    xfm.v = worldPos0;
    xfm.m.x.Set(worldPos1.x - worldPos0.x, worldPos1.y - worldPos0.y, 0.0f);
    Normalize(xfm.m.x, xfm.m.x);
    xfm.m.z.Set(0.0f, 0.0f, 1.0f);
    Cross(xfm.m.z, xfm.m.x, xfm.m.y);

    float min0 = FLT_MAX, min1 = FLT_MAX;
    float max1 = FLT_MIN, max0 = FLT_MIN;

    bones_min_max_x(min0, max0, bones0, xfm);
    bones_min_max_x(min1, max1, bones1, xfm);

    float overlap;
    if (min0 < min1) {
        overlap = max0 - min1;
    } else {
        overlap = max1 - min0;
    }

    return overlap > SongCollision::sCollisionTolerance;
}

#pragma region BeatCollisionData

void BeatCollisionData::Set(
    float minX, float maxX, const Transform &start_xfm, const Transform &end_xfm
) {
    using namespace Hmx;
    MILO_ASSERT(start_xfm.m == Matrix3::GetIdentity(), 0x62);
    MILO_ASSERT(end_xfm.m == Matrix3::GetIdentity(), 0x63);
    mMinX = minX;
    mMaxX = maxX;
    Subtract(start_xfm.v, end_xfm.v, mOffset);
}

BinStream &operator<<(BinStream &bs, const BeatCollisionData &bcd) {
    bs << bcd.mMinX << bcd.mMaxX;
    bs << bcd.mOffset;
    return bs;
}

BinStreamRev &operator>>(BinStreamRev &d, BeatCollisionData &bcd) {
    d >> bcd.mMinX;
    d >> bcd.mMaxX;
    if (d.rev > 1) {
        d >> bcd.mOffset;
    } else if (d.rev > 0) {
        Transform xfm;
        d >> xfm;
        bcd.mOffset = xfm.v;
    }
    return d;
}

#pragma endregion
#pragma region SongCollision

SongCollision::SongCollision() {}

BEGIN_HANDLERS(SongCollision)
    HANDLE_ACTION(update, Update(_msg->Obj<MoveDir>(2)))
    HANDLE_ACTION(print, Print())
    HANDLE_EXPR(equals, Equals(_msg->Obj<SongCollision>(2)))
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

// SyncProperty is in FileMergerOrganizer.cpp (cross-unit)

BEGIN_SAVES(SongCollision)
    SAVE_REVS(2, 1)
    SAVE_SUPERCLASS(Hmx::Object)
    for (int i = 0; i < kNumDifficulties; i++) {
        bs << mData[i];
    }
END_SAVES

BEGIN_COPYS(SongCollision)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(SongCollision)
    BEGIN_COPYING_MEMBERS
        for (int i = 0; i < kNumDifficulties; i++) {
            COPY_MEMBER(mData[i])
        }
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(2, 1)

BEGIN_LOADS(SongCollision)
    LOAD_REVS(bs)
    ASSERT_REVS(2, 1)
    LOAD_SUPERCLASS(Hmx::Object)
    if (d.altRev < 1) {
        for (int i = 0; i < kNumDifficultiesDC2; i++) {
            d >> mData[i];
        }
        mData[kDifficultyBeginner] = mData[kDifficultyEasy];
    } else {
        for (int i = 0; i < kNumDifficulties; i++) {
            d >> mData[i];
        }
    }
END_LOADS

bool SongCollision::Equals(SongCollision *other) {
    if (!other)
        return false;
    for (int i = 0; i < kNumDifficulties; i++) {
        if (mData[i].size() != other->mData[i].size())
            return false;
        for (unsigned int j = 0; j < mData[i].size(); j++) {
            BeatCollisionData &a = mData[i][j];
            BeatCollisionData &b = other->mData[i][j];
            bool close = std::fabs(a.mMaxX - b.mMaxX) < 0.0001f;
            if (!close)
                return false;
            close = std::fabs(a.mMinX - b.mMinX) < 0.0001f;
            if (!close)
                return false;
            float dz = a.mOffset.z - b.mOffset.z;
            float dx = a.mOffset.x - b.mOffset.x;
            float dy = a.mOffset.y - b.mOffset.y;
            close = std::fabs(std::sqrt(dz * dz + dx * dx + dy * dy)) < 0.0001f;
            if (!close)
                return false;
        }
    }
    return true;
}

void SongCollision::Print() {
    int maxDatas = 0;
    for (int i = 0; i < kNumDifficulties; i++) {
        int sz = (int)mData[i].size();
        if (maxDatas < sz) {
            maxDatas = sz;
        }
    }
    String str;
    str = "Beat\tData";
    for (Difficulty d = EasiestDifficulty(); d < kNumDifficulties;
         d = DifficultyOneHarder(d)) {
        str += MakeString("\t%s", DifficultyToSym(d).Str());
    }
    TheDebug << MakeString("%s\n", str.c_str());
    for (int i = 0; i < maxDatas; i++) {
        BeatCollisionData allDatas[kNumDifficulties];
        BeatCollisionData *dataIt = allDatas;
        for (int j = 0; j < kNumDifficulties; j++, dataIt++) {
            if (i < mData[j].size()) {
                *dataIt = mData[j][i];
            } else {
                memset(dataIt, 0, sizeof(BeatCollisionData));
            }
        }
        str = MakeString("%d\tMin X", i);
        for (Difficulty d = EasiestDifficulty(); d < kNumDifficulties;
             d = DifficultyOneHarder(d)) {
            str += MakeString("\t%f", allDatas[d].mMinX);
        }
        TheDebug << MakeString("%s\n", str.c_str());
        str = "\tMax X";
        for (Difficulty d = EasiestDifficulty(); d < kNumDifficulties;
             d = DifficultyOneHarder(d)) {
            str += MakeString("\t%f", allDatas[d].mMaxX);
        }
        TheDebug << MakeString("%s\n", str.c_str());
        str = "\tOffset X";
        for (Difficulty d = EasiestDifficulty(); d < kNumDifficulties;
             d = DifficultyOneHarder(d)) {
            str += MakeString("\t%f", allDatas[d].mOffset.x);
        }
        TheDebug << MakeString("%s\n", str.c_str());
        str = "\tOffset Y";
        for (Difficulty d = EasiestDifficulty(); d < kNumDifficulties;
             d = DifficultyOneHarder(d)) {
            str += MakeString("\t%f", allDatas[d].mOffset.y);
        }
        TheDebug << MakeString("%s\n", str.c_str());
        str = "\tOffset Z";
        for (Difficulty d = EasiestDifficulty(); d < kNumDifficulties;
             d = DifficultyOneHarder(d)) {
            str += MakeString("\t%f", allDatas[d].mOffset.z);
        }
        TheDebug << MakeString("%s\n", str.c_str());
    }
}


void SongCollision::Init() {
    REGISTER_OBJ_FACTORY(SongCollision);
    DataArray *tolerance = DataGetMacro("SONG_COLLISION_TOLERANCE");
    if (tolerance) {
        sCollisionTolerance = tolerance->Float(0);
        sCollisionToleranceValue = sCollisionTolerance;
    }
    sCollisionUsefulBoneNames.clear();
    DataArray *bones = DataGetMacro("SONG_COLLISION_BONES");
    if (bones) {
        for (int i = 0; i < bones->Size(); i++) {
            sCollisionUsefulBoneNames.push_back(bones->Str(i));
        }
    }
}

const BeatCollisionData *SongCollision::BeatData(int beat, Difficulty diff) const {
    MILO_ASSERT_RANGE(diff, 0, kNumDifficulties, 0xe8);
    const std::vector<BeatCollisionData> &diffData = mData[diff];
    MILO_ASSERT(beat >= 0, 0xeb);
    if (beat < diffData.size()) {
        return &diffData[beat];
    } else {
        return nullptr;
    }
}

void SongCollision::GatherUsefulBones(
    std::vector<RndTransformable *> &usefulBones, HamCharacter *dancer
) {
    MILO_ASSERT(dancer, 0x19);
    usefulBones.clear();
    for (ObjDirItr<RndTransformable> it(dancer, true); it != nullptr; ++it) {
        const char *curName = it->Name();
        for (int i = 0; i < sCollisionUsefulBoneNames.size(); i++) {
            if (strneq(
                    curName,
                    sCollisionUsefulBoneNames[i],
                    strlen(sCollisionUsefulBoneNames[i])
                )) {
                usefulBones.push_back(it);
                break;
            }
        }
    }
}

void SongCollision::Update(MoveDir *moveDir) {

    auto& data = mData;
    if (moveDir) {
        MILO_ASSERT(TheGameData, 0xFB);
        MILO_ASSERT(TheHamDirector, 0xFC);
        HamCharacter *dancer = TheHamDirector->GetCharacter(0);
        MILO_ASSERT(dancer, 0xFF);
        std::vector<RndTransformable *> usefulBones;
        GatherUsefulBones(usefulBones, dancer);
        Timer timer;
        for (int i = 0; i < kNumDifficulties; i++) {
            MILO_LOG("Processing collisions for %s\n", DifficultyToSym((Difficulty)i));
            timer.Restart();
            MILO_ASSERT(TheGameData, 0x10C);
            TheGameData->Player(0)->SetDifficulty((Difficulty)i);
            data[i].clear();
            MocapSkeletonIterator it(0, TheHamDirector->SongAnim(0)->EndFrame());
            int current_beat = -1;
            Transform startXfm;
            float minX, maxX;
            for (; it; ++it) {
                int beat = SecondsToBeat(it.CurrentFrame() * 0.03333333507180214f);
                MILO_ASSERT(beat >= 0, 0x11B);
                if (beat != current_beat) {
                    MILO_ASSERT(beat == current_beat + 1, 0x11F);
                    if (current_beat >= 0) {
                        BeatCollisionData bcd;
                        bcd.Set(minX, maxX, startXfm, dancer->WorldXfm());
                        data[i].push_back(bcd);
                    }
                    startXfm = dancer->WorldXfm();
                    maxX = 0;
                    minX = 0;
                    current_beat = beat;
                }
                bones_min_max_x(minX, maxX, usefulBones, startXfm);
            }
            if (current_beat != -1) {
                BeatCollisionData bcd;
                bcd.Set(minX, maxX, startXfm, dancer->WorldXfm());
                data[i].push_back(bcd);
            } else {
                MILO_NOTIFY(
                    "Could not process collision mocap for %s", TheGameData->GetSong()
                );
            }
            timer.Stop();
            MILO_LOG("Took %fms\n", timer.Ms());
        }
        int sizeKB = data[0].size() * 0x48; // where is this 0x48 coming from
        sizeKB /= 1024;
        MILO_LOG("Approx size = %ikB\n", sizeKB);
    }
}

void SongCollision::CheckCollision(
    int beat,
    const Difficulty *const diffs,
    const Transform *const transforms,
    SongCollisionOutput &out
) const {
    Vector3 dir;
    Subtract(transforms[1].v, transforms[0].v, dir);
    Vector3 normalDir;
    Normalize(dir, normalDir);

    int i = 0;
    do {
        memcpy(out._data + 0x60 + i * 0x40, &transforms[i], sizeof(Transform));

        const BeatCollisionData *bd = BeatData(beat, diffs[i]);
        Vector3 *minEdge = reinterpret_cast<Vector3 *>(out._data + i * 0x10);
        Vector3 *maxEdge = reinterpret_cast<Vector3 *>(out._data + (i + 2) * 0x10);
        Vector3 *push = reinterpret_cast<Vector3 *>(out._data + (i + 4) * 0x10);

        if (!bd) {
            minEdge->Zero();
            maxEdge->Zero();
            push->Zero();
        } else {
            Vector3 minVec(bd->mMinX, 0.0f, 0.0f);
            Multiply(minVec, transforms[i], *minEdge);

            Vector3 maxVec(bd->mMaxX, 0.0f, 0.0f);
            Multiply(maxVec, transforms[i], *maxEdge);

            // Pre-compute all differences (target interleaves min/max loads)
            float minDz = minEdge->z - transforms[i].v.z;
            float minDy = minEdge->y - transforms[i].v.y;
            float maxDy = maxEdge->y - transforms[i].v.y;
            float minDx = minEdge->x - transforms[i].v.x;
            float maxDx = maxEdge->x - transforms[i].v.x;
            float maxDz = maxEdge->z - transforms[i].v.z;

            float proj = normalDir.z * minDz + normalDir.y * minDy + normalDir.x * minDx;

            if ((proj <= 0.0f || i != 0) && (proj >= 0.0f || i != 1)) {
                proj = normalDir.z * maxDz + normalDir.y * maxDy + normalDir.x * maxDx;
            }

            Scale(normalDir, proj, *push);
        }
        i++;
    } while (i < 2);

    float distance = Length(dir);
    float totalExtent = 0.0f;
    for (int j = 0; j < 2; j++) {
        totalExtent += Length(*reinterpret_cast<Vector3 *>(out._data + 0x40 + j * 0x10));
    }

    SetSongCollisionColliding(out, totalExtent - sCollisionTolerance > distance);
}

bool SongCollision::IsCollision(
    int startBeat,
    int endBeat,
    const Difficulty *const diffs,
    const Transform *const transforms,
    std::vector<SongCollisionOutput> *outputs
) const {
    // Copy transforms locally so we can accumulate beat offsets
    Transform localXfms[2];
    __int64 *dst = (__int64 *)localXfms;
    const __int64 *src = (const __int64 *)transforms;
    for (int k = 0; k < 16; k++) {
        dst[k] = src[k];
    }

    bool anyCollision = false;
    int beat = startBeat;
    do {
        if (beat >= endBeat) {
            return anyCollision;
        }
        SongCollisionOutput out;
        CheckCollision(beat, diffs, localXfms, out);
        if (out.Colliding()) {
            if (!outputs) {
                return true;
            }
            anyCollision = true;
        }
        if (outputs) {
            outputs->push_back(out);
        }

        // Accumulate beat offsets into local transforms
        for (int i = 0; i < 2; i++) {
            const BeatCollisionData *bd = BeatData(beat, diffs[i]);
            if (bd) {
                localXfms[i].v += bd->mOffset;
            }
        }
        beat++;
    } while (true);
}
