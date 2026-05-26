#include "hamobj/DanceRemixer.h"
#include "meta_ham/MetagameStats.h"
#include "MoveMgr.h"
#include "hamobj/HamDirector.h"
#include "hamobj/HamMaster.h"
#include "hamobj/HamAudio.h"
#include "hamobj/HamGameData.h"
#include "hamobj/HamMove.h"
#include "hamobj/MoveDetector.h"
#include "hamobj/MoveDir.h"
#include "hamobj/MoveGraph.h"
#include "obj/Data.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "os/System.h"
#include "utl/TimeConversion.h"

MetagameStats::FavoriteStat::FavoriteStat() {}

DanceRemixer::DanceRemixer() {}
DanceRemixer::~DanceRemixer() { HandleType(Message("deinit")); }

Symbol OnMoveVariantFromHamMove(const DataArray *array) {
    MILO_ASSERT(array->Size() == 3, 0x21C);
    DanceRemixer *remixer = array->Obj<DanceRemixer>(0);
    HamMove *move = array->Obj<HamMove>(2);
    MILO_ASSERT(move, 0x21F);
    const MoveVariant *mv = remixer->MoveVariantFromHamMove(move);
    MILO_ASSERT(mv, 0x221);
    return mv ? mv->Name() : "";
}

BEGIN_HANDLERS(DanceRemixer)
    HANDLE_ACTION(reset, Reset())
    HANDLE_ACTION(post_move_finished, PostMoveFinished())
    HANDLE_ACTION(set_jump, SetJump(_msg->Int(2), _msg->Int(3)))
    HANDLE_ACTION(clear_jump, ClearJump())
    HANDLE_EXPR(jump_from_beat, mFromMeasure * 4)
    HANDLE_EXPR(jump_to_beat, mToMeasure * 4)
    HANDLE_EXPR(jump_from_measure, mFromMeasure + 1)
    HANDLE_EXPR(jump_to_measure, mToMeasure + 1)
    HANDLE_EXPR(jumped_beat, JumpedBeat(_msg->Float(2)))
    HANDLE_EXPR(jumped_measure, JumpedMoveIdx(_msg->Int(2) - 1) + 1)
    HANDLE_EXPR(jumped_measure_add, JumpedMeasureAdd(_msg->Int(2), _msg->Int(3)))
    HANDLE_EXPR(
        jumped_measure_steps_between,
        JumpedMeasureStepsBetween(_msg->Int(2), _msg->Int(3), _msg->Int(4))
    )
    HANDLE_EXPR(scored_measure, ScoredDanceMeasure(_msg->Int(2), _msg->Int(3)))
    HANDLE_ACTION(set_unscored_measure, SetUnscoredMeasure(_msg->Int(2), _msg->Int(3)))
    HANDLE_ACTION(
        set_unscored_measure_range,
        SetUnscoredMeasureRange(_msg->Int(2), _msg->Int(3), _msg->Int(4))
    )
    HANDLE_ACTION(clear_unscored_measure, ClearUnscoredMeasure(_msg->Int(2), _msg->Int(3)))
    HANDLE_ACTION(
        clear_unscored_measure_range,
        ClearUnscoredMeasureRange(_msg->Int(2), _msg->Int(3), _msg->Int(4))
    )
    HANDLE_ACTION(clear_unscored_measures, mUnscoredMeasures[_msg->Int(2)].clear())
    HANDLE_EXPR(move_variant_from_ham_move, OnMoveVariantFromHamMove(_msg))
    HANDLE_EXPR(measures_total, mTotalMeasures)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

void DanceRemixer::SetJump(int from, int to) {
    ClearJump();
    auto& fromMeasure = mFromMeasure;
    fromMeasure = from - 1;
    mToMeasure = to - 1;
    int fromBeat = fromMeasure * 4;
    if (fromMeasure == mToMeasure) {
        TheMaster->GetAudio()->SetLoop((float)(mToMeasure * 4), (float)fromBeat);
    } else {
        float fromMs = BeatToMs((float)fromBeat);
        float toMs = BeatToMs((float)(mToMeasure * 4));
        float jumpOffset = SystemConfig("dance", "jump")->Node(1).Float(nullptr);
        float crossfadeMs = BeatToMs((float)(fromMeasure * 4) + jumpOffset);
        TheMaster->GetAudio()->SetCrossfadeJump(fromMs, toMs, crossfadeMs - fromMs);

        mJumpMap[mToMeasure] = fromMeasure;
        if (fromMeasure > 0 && mToMeasure > 0) {
            mJumpMap[fromMeasure - 1] = mToMeasure - 1;
        }

        float curBeat = TheTaskMgr.Beat();
        int idx = fromMeasure - 1;
        int count = (int)curBeat / 4 - idx + 5;
        if (count > 0 && 0 < (int)count) {
            do {
                MILO_ASSERT(ValidMoveIdx(idx), 0x16d);
                for (int p = 0; p < 2; p++) {
                    SelectMove(p, idx);
                }
                idx = JumpedMeasureAdd(idx + 1, 1) - 1;
                count--;
            } while (count != 0);
        }
    }
}

void DanceRemixer::ClearJump() {
    mFromMeasure = -1;
    mToMeasure = -1;
    mJumpMap.clear();
    if (TheMaster && TheMaster->GetAudio()) {
        TheMaster->GetAudio()->ClearLoop();
    }
}

void DanceRemixer::SetUnscoredMeasure(int x, int y) { mUnscoredMeasures[x].insert(y); }
void DanceRemixer::ClearUnscoredMeasure(int x, int y) { mUnscoredMeasures[x].erase(y); }

BEGIN_PROPSYNCS(DanceRemixer)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(DanceRemixer)
    SAVE_SUPERCLASS(Hmx::Object)
END_SAVES

BEGIN_COPYS(DanceRemixer)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(DanceRemixer)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mTotalMeasures)
        for (int i = 0; i < 2; i++) {
            COPY_MEMBER(mUnscoredMeasures[i])
        }
        COPY_MEMBER(mPendingVariants)
        COPY_MEMBER(mNeedsUpdate)
        COPY_MEMBER(mFromMeasure)
        COPY_MEMBER(mToMeasure)
        COPY_MEMBER(mJumpMap)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_LOADS(DanceRemixer)
    Hmx::Object::Load(bs);
END_LOADS

void DanceRemixer::Init(int x) {
    if (TheMoveMgr->MoveParents().size() == 0) {
        MILO_FAIL("Failed to load move graph for: %s\n", TheGameData->GetSong());
    }
    mTotalMeasures = x;
    for (int i = 0; i < 2; i++) {
        TheMoveMgr->mMoveParents[i].resize(mTotalMeasures);
        TheMoveMgr->mPreferredVariants[i].resize(mTotalMeasures);
        TheMoveMgr->mRoutineMeasures[i].resize(mTotalMeasures);
    }
    ClearJump();
    HandleType(Message("post_init"));
}

void DanceRemixer::Reset() {
    mPendingVariants.clear();
    mNeedsUpdate = false;
    ClearJump();
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < mTotalMeasures; j++) {
            TheMoveMgr->mMoveParents[i][j] = 0;
            TheMoveMgr->mPreferredVariants[i][j] = 0;
            TheMoveMgr->mRoutineMeasures[i][j] = std::make_pair<const MoveVariant *, const MoveVariant *>(0, 0);
        }
        mUnscoredMeasures[i].clear();
    }
    TheMoveMgr->mRoutineLoaded = 1;
    HandleType(Message("post_reset"));
}

DataNode DanceRemixer::OnMovePassed(DataArray *a) {
    int i2 = a->Int(2);
    HamMove *move = a->Obj<HamMove>(3);
    Symbol s4 = a->ForceSym(4);
    MovePassed(i2, move, s4);
    return 0;
}

void DanceRemixer::PostMoveFinished() {
    int moveIdx = (int)TheTaskMgr.Beat() / 4 + 1;
    UpdateHamDirector();
    MoveDir *moveDir = TheHamDirector ? TheHamDirector->GetMoveDir() : nullptr;
#ifdef HX_NATIVE
    if (!moveDir) return; // No move directory loaded (no Kinect)
#endif
    MoveAsyncDetector *detector = moveDir->GetAsyncDetector();
#ifdef HX_NATIVE
    if (!detector) return;
#endif
    for (int i = 0; i < 2; i++) {
        auto _tmp1 = JumpedMoveIdx(moveIdx - 1);
        auto scored = ScoredDanceMeasure(i, _tmp1 + 1);
        if (scored) {
            detector->DisableAllDetectors();
            break;
        }
    }
    for (int i = 0; i < 2; i++) {
        if (ScoredDanceMeasure(i, moveIdx)) {
            const MoveVariant *mv = TheMoveMgr->mRoutineMeasures[i][moveIdx].first;
            if (mv) {
                const char *hamMoveName = mv->HamMoveName().Str();
                HamMove *move = moveDir->Find<HamMove>(hamMoveName, false);
                if (move) {
                    detector->EnableDetector(move);
                    moveDir->SetCurrentMove(i, move);
                } else {
                    MILO_NOTIFY(
                        "Ham move %s missing, possibly not loaded yet. From move variant %s",
                        hamMoveName,
                        mv->Name()
                    );
                }
            }
        }
    }
}

bool DanceRemixer::ScoredDanceMeasure(int x, int y) const {
    return mUnscoredMeasures[x].find(y) == mUnscoredMeasures[x].end();
}

void DanceRemixer::UpdateHamDirector() {
    for (int i = 0; i < 2; i++) {
        const auto &mvs_list = TheMoveMgr->mRoutineMeasures[i];
        for (int j = 0; j < mvs_list.size(); j++) {
            if (j <= mFromMeasure || j >= mToMeasure) {
                std::pair<const MoveVariant *, const MoveVariant *> mvs = mvs_list[j];
                if (mvs.first) {
                    mNeedsUpdate |= mPendingVariants.insert(mvs.first).second;
                }
                if (mvs.second && mvs.second != mvs.first) {
                    mNeedsUpdate |= mPendingVariants.insert(mvs.second).second;
                }
            }
        }
    }
    if (mNeedsUpdate && TheHamDirector->IsMoveMergerFinished()) {
        TheHamDirector->LoadRoutineBuilderData(mPendingVariants, true);
        mNeedsUpdate = false;
    }
}

int DanceRemixer::JumpedMoveIdxAdd(int idx, int add) const {
    return JumpedMeasureAdd(idx + 1, add) - 1;
}

void DanceRemixer::SelectMove(int, int) {}

float DanceRemixer::JumpedBeat(float beat) const {
    int fromBeat = mFromMeasure * 4;
    int toBeat = mToMeasure * 4;
    if ((int)beat < fromBeat) {
        return beat;
    }
    if ((int)beat < toBeat) {
        int jumpSize = toBeat - mFromMeasure * 4;
        if ((int)beat < toBeat - (jumpSize >> 1)) {
            return (float)jumpSize + beat;
        }
        return beat - (float)jumpSize;
    }
    if (toBeat >= fromBeat) {
        return beat;
    }
    return (beat - (float)fromBeat) + (float)toBeat;
}

int DanceRemixer::JumpedMoveIdx(int idx) const { return Round(JumpedBeat(idx * 4)) / 4; }

int DanceRemixer::JumpedMeasureAdd(int measure, int count) const {
    int step = 1;
    if (!(count > 0)) {
        step = -1;
    }
    int absCount = (count ^ (count >> 31)) - (count >> 31);
    for (int i = 0; i < absCount; i++) {
        measure = JumpedMoveIdx(measure + step - 1) + 1;
    }
    return measure;
}

int DanceRemixer::JumpedMeasureStepsBetween(int from, int to, int step) const {
    MILO_ASSERT(step == 1 || step == -1, 0x1bd);
    int count = 0;
    while (from != to) {
        count += step;
        if (((count ^ ((unsigned int)count >> 31)) - ((unsigned int)count >> 31)) > mTotalMeasures * 2) {
            TheDebug.Fail(MakeString("JumpedMeasureStepsBetween from %d to %d", from, to), nullptr);
        }
        from = JumpedMeasureAdd(from, step);
    }
    return count;
}

const MoveVariant *DanceRemixer::MoveVariantFromHamMove(const HamMove *move) const {
    MILO_ASSERT(move, 0x1f7);
    Symbol moveName = move->Name();

    for (int i = 0; i < 2; i++) {
        const auto &measures = TheMoveMgr->mRoutineMeasures[i];
        for (unsigned int j = 0; j < measures.size(); j++) {
            if (JumpedMoveIdx(j) == (int)j) {
                if (measures[j].first && measures[j].first->HamMoveName() == moveName) {
                    return measures[j].first;
                }
                if (measures[j].second && measures[j].second->HamMoveName() == moveName) {
                    return measures[j].second;
                }
            }
        }
    }

    // Fall back to searching the move graph's variant map
    const std::map<Symbol, MoveVariant *> &variants = TheMoveMgr->mMoveGraph.MoveVariants();
    for (std::map<Symbol, MoveVariant *>::const_iterator it = variants.begin();
         it != variants.end(); ++it) {
        if (it->second->HamMoveName() == moveName) {
            return it->second;
        }
    }

    return nullptr;
}

const MoveParent *DanceRemixer::GetMoveParent(int x, int y) {
    return TheMoveMgr->CurParents(x)[y];
}

void BuildSetOfPrevAdjacentMoveParents(
    std::set<const MoveParent *> &s1, const std::set<const MoveParent *> &s2
) {
    std::set<const MoveParent *>::const_iterator it = s2.begin();
    std::set<const MoveParent *>::const_iterator end = s2.end();
    while (it != end) {
        const MoveParent *moveParent = *it;
        const std::vector<const MoveParent *> &prevAdjs = moveParent->PrevAdjacents();
        for (unsigned int i = 0; i < prevAdjs.size(); i++) {
            const MoveParent *prevAdj = prevAdjs[i];
            if (!prevAdj->HasRestMoveVariant() && !prevAdj->HasFinalMoveVariant()) {
                s1.insert(prevAdj);
            }
        }
        ++it;
    }
}

void DanceRemixer::SetUnscoredMeasureRange(int x, int y, int z) {
    for (int i = y; i <= z; i++) {
        mUnscoredMeasures[x].insert(i);
    }
}

void DanceRemixer::ClearUnscoredMeasureRange(int x, int y, int z) {
    for (int i = y; i < z; i++) {
        mUnscoredMeasures[x].erase(i);
    }
}

// Adds a move to the routine at the given measure, then propagates it to all jump targets
// originating from that measure (if any are registered in mJumpMap)
void DanceRemixer::AddRoutineMove(
    int player, int measure, const MoveParent *moveParent, const MoveVariant *moveVariant
) {
    // Set the move at the initial measure
    TheMoveMgr->mMoveParents[player][measure] = moveParent;
    TheMoveMgr->mPreferredVariants[player][measure] = moveVariant;
    TheMoveMgr->FillInRoutineAt(player, measure);
    TheMoveMgr->InsertMoveInSong(TheMoveMgr->mRoutineMeasures[player][measure].first, measure, player);

    // Propagate the same move along the jump chain from this measure
    std::map<int, int>::iterator it = mJumpMap.find(measure);
    while (it != mJumpMap.end()) {
        int jumpTarget = it->second;
        if (jumpTarget >= mTotalMeasures) {
            MILO_NOTIFY(
                "Jump target to index %d is out of bounds of the song (0 to %d)!",
                jumpTarget,
                mTotalMeasures - 1
            );
            return;
        }
        // Apply the same move at the jump target measure
        measure = jumpTarget;
        TheMoveMgr->mMoveParents[player][jumpTarget] = moveParent;
        TheMoveMgr->mPreferredVariants[player][jumpTarget] = moveVariant;
        TheMoveMgr->FillInRoutineAt(player, jumpTarget);
        TheMoveMgr->InsertMoveInSong(TheMoveMgr->mRoutineMeasures[player][jumpTarget].first, jumpTarget, player);
        it = mJumpMap.find(measure);
    }
}
