#include "hamobj/MoveMgr.h"
#include "SongUtl.h"

MoveMgr *TheMoveMgr;
#include "SuperEasyRemixer.h"
#include "char/CharClip.h"
#include "char/FileMerger.h"
#include "flow/PropertyEventProvider.h"
#include "hamobj/Difficulty.h"
#include "hamobj/HamDirector.h"
#include "hamobj/HamMove.h"
#include "hamobj/MoveGraph.h"
#include "hamobj/SongLayout.h"
#include "math/Rand.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/System.h"
#include "rndobj/PropAnim.h"
#include "rndobj/PropKeys.h"
#include <algorithm>
#include <climits>

MoveMgr::MoveMgr() : mCurrentSongLayout(0), mLoadingProgressCounter(0) {
    mMovesDir = nullptr;
    for (int i = 0; i < kNumDifficultiesDC2; i++) {
        unk54[i].clear();
        mMovePropKeys[i] = nullptr;
        mClipPropKeys[i] = nullptr;
    }
    mPracticePropKeys = nullptr;
    mRoutineLoaded = false;
    mCurrentSong = "";
    mSuperEasyRemixer = Hmx::Object::New<SuperEasyRemixer>();
    mSuperEasyRemixer->SetType("easeup_remixer");
    mMoveDataDir = nullptr;
    mDefaultSongLayout = dynamic_cast<SongLayout *>(Hmx::Object::NewObject("SongLayout"));
}

MoveMgr::~MoveMgr() {
    RELEASE(mSuperEasyRemixer);
    mGenres.clear();
    mEras.clear();
    mFilteredGenres.clear();
    mFilteredEras.clear();
    mLoadingProgressCounter = 0;
    mMovesDir = nullptr;
    for (int i = 0; i < kNumDifficultiesDC2; i++) {
        unk54[i].clear();
        mMovePropKeys[i] = nullptr;
        mClipPropKeys[i] = nullptr;
    }
    mPracticePropKeys = nullptr;
    mMoveDataDir = nullptr;
    RELEASE(mDefaultSongLayout);
}

BEGIN_HANDLERS(MoveMgr)
    HANDLE_EXPR(graph_size, (int)MoveParents().size())
    HANDLE_ACTION(register_song_layout, RegisterSongLayout(_msg->Obj<SongLayout>(2)))
    HANDLE_ACTION(init_song, InitSong())
    HANDLE_EXPR(get_song_layout, GetSongLayout())
    HANDLE_EXPR(get_move_difficulty, GetMoveDifficulty(_msg->ForceSym(2)))
    HANDLE_ACTION(next_moves, NextMovesToShow(_msg->Array(2), _msg->Int(3)))
    HANDLE_EXPR(get_char_clip, FindCharClip(_msg->ForceSym(2)))
    HANDLE_EXPR(get_ham_move, FindHamMove(_msg->ForceSym(2)))
    HANDLE_EXPR(move_from_move_name, FindHamMoveFromName(_msg->ForceSym(2)))
    HANDLE_EXPR(
        variant_name_from_move_name, FindVariantNameFromHamMoveName(_msg->ForceSym(2))
    )
    HANDLE_ACTION(fill_routine_from_verses, FillRoutineFromVerses(_msg->Int(2)))
    HANDLE_ACTION(fill_routine_from_replacer, FillRoutineFromReplacer(_msg->Int(2)))
    HANDLE_ACTION(prepare_next_choice_set, PrepareNextChoiceSet(_msg->Int(2)))
    HANDLE_ACTION(
        pick_random_move_set,
        PickRandomMoveSet(_msg->ForceSym(2), _msg->Int(3), _msg->Array(4), _msg->Array(5))
    )
    HANDLE_EXPR(pick_random_genre, PickRandomGenre())
    HANDLE_EXPR(get_genre_token_name, GetGenreTokenName(_msg->ForceSym(2)))
    HANDLE(find_variants, OnFindVariants)
    HANDLE_EXPR(get_remixer, mSuperEasyRemixer)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

Symbol MoveMgr::GetGenreTokenName(Symbol s) {
    Symbol ret = mGenres.front().mToken;
    FOREACH (it, mGenres) {
        if (it->mName == s) {
            ret = it->mToken;
            break;
        }
    }
    return ret;
}

Difficulty MoveMgr::GetMoveDifficulty(Symbol s) {
    const MoveVariant *mv = mMoveGraph.FindMoveByVariantName(s);
    if (mv) {
        return mv->GetDifficulty();
    } else {
        return DefaultDifficulty();
    }
}

void MoveMgr::Clear() {
    mLoadingProgressCounter = 0;
    mMovesDir = 0;
    for (int i = 0; i < kNumDifficultiesDC2; i++) {
        unk54[i].clear();
        mMovePropKeys[i] = nullptr;
        mClipPropKeys[i] = nullptr;
    }
    mPracticePropKeys = nullptr;
    mCurrentSongLayout = 0;
    mVariants.clear();
    for (int i = 0; i < 2; i++) {
        mRoutineMeasures[i].clear();
        mMoveParents[i].clear();
        mPreferredVariants[i].clear();
    }
    mRoutineLoaded = false;
    mChoiceSets.clear();
    mGenres.clear();
    mEras.clear();
    mFilteredGenres.clear();
    mFilteredEras.clear();
    mCurrentSong = "";
    mMoveGraph.Clear();
    mMoveDataDir = nullptr;
}

bool MoveVariantsWithHamMove(const MoveVariant *var, void *v) {
    int tag = var->HamMoveName();
    int *iv = (int *)v;
    return tag == *iv;
}

bool MoveMgr::HasRoutine() const {
    static Symbol gameplay_mode("gameplay_mode");
    static Symbol practice("practice");
    return mRoutineLoaded && TheHamProvider->Property(gameplay_mode, true)->Sym() != practice;
}

void MoveMgr::InsertMoveInSong(const MoveVariant *var, int measure, int player) {
    static Symbol clip("clip");
    static Symbol move("move");
    static Symbol practice("practice");
    if (var) {
        Symbol name = var->Name();
        float beat = measure * 4;
        float f4 = BeatToFrame(beat);
        float f6 = BeatToFrame(measure > 0 ? beat - 1 : 0);
        RndPropAnim *anim = nullptr;
#ifdef HX_NATIVE
        static Symbol merge_moves("merge_moves");
        if (TheHamDirector && TheHamProvider
            && TheHamProvider->Property(merge_moves, true)->Int()) {
            // HACK(native): populate the routine-builder anim directly. The
            // SongAnim() fallback for native/web clip playback can otherwise
            // redirect these initial remixer writes into the authored song.anim,
            // leaving player_song_anim() with no move timeline for HUD sync.
            WorldDir *world = TheHamDirector->GetWorld();
            if (world) {
                anim = world->Find<RndPropAnim>(
                    player == 0 ? "player_1_routine_builder.anim"
                                : "player_2_routine_builder.anim",
                    true
                );
            }
        }
#endif
        if (!anim) {
            anim = TheHamDirector->SongAnim(player);
        }
        if (!anim) return;
        DataArrayPtr ptr90(clip);
        DataArrayPtr ptr88(move);
        anim->SetKeyVal(TheHamDirector, ptr90, f6, name, true);
        anim->SetKeyVal(TheHamDirector, ptr88, f4, var->HamMoveName(), true);
    }
}

void MoveMgr::SaveRoutine(DataArray *a) const {
    a->Resize(mMoveParents[0].size());
    int i = 0;
    FOREACH (it, mMoveParents[0]) {
        if (*it) {
            a->Node(i) = (*it)->Name();
        } else {
            a->Node(i) = 0;
        }
        i++;
    }
}

void MoveMgr::GenerateMoveChoice(
    Symbol s1,
    std::vector<const MoveVariant *> &vec1,
    std::vector<const MoveVariant *> &vec2
) {
    vec1.clear();
    vec2.clear();
    MILO_LOG("MoveMgr::GenerateMoveChoice %s\n", s1.Str());
    int invalidNum = 0;
    FOREACH (it, MoveParents()) {
        MoveParent *curParent = it->second;
        const MoveVariant *randomVar = curParent->PickRandomVariant();
        MILO_LOG("move=%s genre=%s", randomVar->Name(), randomVar->Genre().Str());
        if (!randomVar->IsValidForMinigame()) {
            MILO_LOG(" not valid for mini games\n");
            invalidNum++;
        } else {
            if (curParent->HasCategory(s1)) {
                MILO_LOG(" is %s\n", s1.Str());
                vec1.push_back(randomVar);
            } else {
                MILO_LOG(" NOT %s\n", s1.Str());
                vec2.push_back(randomVar);
            }
        }
    }
    MILO_LOG(
        "invalid=%d, wrong genre=%d right genre=%d\n", invalidNum, vec2.size(), vec1.size()
    );
    RandomShuffle(vec1.begin(), vec1.end());
    RandomShuffle(vec2.begin(), vec2.end());
}

void MoveMgr::PickRandomMoveSet(Symbol s1, int count, DataArray *a3, DataArray *a4) {
    MILO_ASSERT(count > 1, 0x4BB);
    a3->Resize(1);
    a4->Resize(count - 1);
    std::vector<const MoveVariant *> rightMoves;
    std::vector<const MoveVariant *> wrongMoves;
    std::set<const MoveVariant *> moveSet;
    FOREACH (it, MoveParents()) {
        const MoveVariant *randomVar = it->second->PickRandomVariant();
        if (randomVar->IsValidForMinigame()) {
            if (it->second->HasCategory(s1)) {
                rightMoves.push_back(randomVar);
            } else {
                wrongMoves.push_back(randomVar);
            }
        }
    }
    RandomShuffle(rightMoves.begin(), rightMoves.end());
    RandomShuffle(wrongMoves.begin(), wrongMoves.end());
    MILO_ASSERT(rightMoves.size() > 0, 0x4D9);
    MILO_ASSERT(wrongMoves.size() > count - 1, 0x4DA);
    const MoveVariant *firstRightMove = rightMoves[0];
    moveSet.insert(firstRightMove);
    a3->Node(0) = firstRightMove->Name();
    for (int i = 0; i < count - 1; i++) {
        const MoveVariant *curWrongMove = wrongMoves[i];
        moveSet.insert(curWrongMove);
        a4->Node(i) = curWrongMove->Name();
    }
    TheHamDirector->LoadRoutineBuilderData(moveSet, true);
}

void MoveMgr::LoadCategoryData(const char *filename) {
    mGenres.clear();
    mEras.clear();
    static Symbol genres("genres");
    static Symbol eras("eras");
    DataArray *file = DataReadFile(filename, false);
    if (file) {
        DataArray *genreArr = file->FindArray(genres, true);
        if (genreArr) {
            for (int i = 1; i < genreArr->Size(); i++) {
                CategoryData data;
                data.mName = genreArr->Array(i)->Sym(0);
                data.mToken = genreArr->Array(i)->Sym(1);
                mGenres.push_back(data);
            }
        }
        DataArray *eraArr = file->FindArray(eras, true);
        if (eraArr) {
            for (int i = 1; i < eraArr->Size(); i++) {
                CategoryData data;
                data.mName = eraArr->Array(i)->Sym(0);
                data.mToken = eraArr->Array(i)->Sym(1);
                mEras.push_back(data);
            }
        }
        file->Release();
    }
}

void MoveMgr::ImportMoveData(const char *filename, bool clear) {
    if (clear) {
        mMoveGraph.Clear();
    }
    DataArray *moveData = DataReadFile(filename, true);
    if (moveData) {
        mMoveGraph.ImportMoveData(moveData);
        moveData->Release();
    }
    MILO_LOG(
        "MoveMgr::ImportMoveData filename=%s move nodes=%d\n",
        filename,
        MoveParents().size()
    );
    LoadSubCategoryData();
}

void MoveMgr::LoadMoveData(ObjectDir *dir) {
    if (mMoveDataDir != dir) {
        mMoveDataDir = dir;
        if (dir) {
            MoveGraph *dirGraph = dir->Find<MoveGraph>("move_graph", true);
#ifdef HX_NATIVE
            if (!dirGraph) {
                MILO_LOG("MoveMgr::LoadMoveData - move_graph not found in dir '%s'\n",
                    dir->Name());
                return;
            }
#endif
            mMoveGraph.Copy(dirGraph, kCopyDeep);
            LoadSubCategoryData();
        } else {
            MILO_LOG("MoveMgr::LoadMoveData - NULL dir passed in. Doing nothing...\n");
        }
    } else {
        MILO_LOG("MoveMgr::LoadMoveData - Dir is already loaded. Doing nothing...\n");
    }
}

void MoveMgr::Init(const char *filename) {
    TheMoveMgr = new MoveMgr();
    if (ObjectDir::Main()) {
        TheMoveMgr->SetName("movemgr", ObjectDir::Main());
    }
    TheMoveMgr->LoadCategoryData("modular_song_data/category.dta");
    if (filename) {
        TheMoveMgr->ImportMoveData(filename, false);
    }
}

const MoveVariant *MoveMgr::GetRoutinePreferredVariant(int i1, int i2) const {
    if (i2 < mPreferredVariants[i1].size()) {
        const MoveVariant *var = mPreferredVariants[i1][i2];
        if (var && var->Parent() == mMoveParents[i1].at(i2)) {
            return var;
        }
    }
    return nullptr;
}

void MoveMgr::LoadSongData() { ImportMoveData("../meta/move_data.dta", true); }

void MoveMgr::ComputePotentialMoves(std::set<const MoveParent *> &moves, int i2) {
    if (mMoveParents[0].size() < i2 + 1) {
        mMoveParents[0].resize(i2 + 1);
    }
    if (mMoveParents[0][i2]) {
        moves.insert(mMoveParents[0][i2]);
    } else {
        if (mChoiceSets.size() < i2 + 1) {
            mChoiceSets.resize(i2 + 1);
        }
        MoveChoiceSet &curChoice = mChoiceSets[i2];
        if (curChoice.mChoices[0]) {
            for (int i = 0; i < kNumDifficulties; i++) {
                if (curChoice.mChoices[i]) {
                    moves.insert(curChoice.mChoices[i]);
                }
            }
        } else {
            if (i2 > 0) {
                const MoveParent *last = mMoveParents[i2].back();
                if (last) {
                    FOREACH (adj, last->NextAdjacents()) {
                        if ((*adj)->IsValidForMiniGame()) {
                            moves.insert(*adj);
                        }
                    }
                }
            }
            if (moves.size() < 1) {
                for (std::map<Symbol, MoveParent *>::const_iterator it =
                         MoveParents().begin();
                     it != MoveParents().end();
                     ++it) {
                    MoveParent *cur = it->second;
                    if (cur->IsValidForMiniGame()) {
                        moves.insert(cur);
                    }
                }
            }
        }
    }
}

void MoveMgr::AutoFillParents() {
    int i = 0;
    for (std::vector<const MoveParent *>::iterator it = mMoveParents[0].begin();
         it != mMoveParents[0].end();
         ++it, ++i) {
        if (!*it) {
            int set = ComputeRandomChoiceSet(i);
            if (set != 0) {
                set = RandomInt(0, set);
                *it = mChoiceSets[i].mChoices[set];
            }
        }
    }
}

SongLayout *MoveMgr::GetSongLayout() {
    if (!mCurrentSongLayout) {
        mCurrentSongLayout = mDefaultSongLayout;
#ifdef HX_NATIVE
        if (!mCurrentSongLayout) {
            mDefaultSongLayout = new SongLayout();
            mCurrentSongLayout = mDefaultSongLayout;
        }
#endif
        mDefaultSongLayout->SetDefaultPattern(0x40);
    }
    if (mCurrentSongLayout->NumReplacers() == 0) {
#ifdef HX_NATIVE
        // SetDefaultReplacer needs a loaded song.anim (SongAnimByDifficulty).
        // On native the song may not be fully loaded at this point.
        RndPropAnim *songAnim = TheHamDirector ?
            TheHamDirector->SongAnimByDifficulty((Difficulty)0) : nullptr;
        if (songAnim)
#endif
        mCurrentSongLayout->SetDefaultReplacer();
    }
    return mCurrentSongLayout;
}

Symbol MoveMgr::PickRandomGenre() {
    static Symbol genre_none("genre_none");
    int size = mFilteredGenres.size();
    return size != 0 ? mFilteredGenres[RandomInt(0, size)].mName : genre_none;
}

Symbol MoveMgr::PickRandomCategory() { return PickRandomGenre(); }

bool IsSuperEasyMove(Symbol move) {
    DataArray *superEasy = SystemConfig("super_easy_moves");
    MILO_ASSERT(superEasy, 0x514);
    for (int i = 0; i < superEasy->Size(); i++) {
        if (superEasy->Sym(i) == move) {
            return true;
        }
    }
    return false;
}

void MoveMgr::SongInit() {
    mLoadingProgressCounter = 0;
    MILO_ASSERT(TheHamDirector, 0x123);
    mMovesDir = TheHamDirector->GetWorld()->Find<MoveDir>("moves", false);
    MILO_ASSERT(mMovesDir, 0x126);
#ifdef HX_NATIVE
    if (!mMovesDir) return;
#endif
    static Symbol move("move");
    static Symbol clip("clip");
    static Symbol practice("practice");
    for (int i = 0; i < kNumDifficultiesDC2; i++) {
        unk54[i].clear();
        PropKeys *pMovePropKeys = TheHamDirector->GetPropKeys((Difficulty)i, move);
        PropKeys *pClipPropKeys = TheHamDirector->GetPropKeys((Difficulty)i, clip);
        MILO_ASSERT(pMovePropKeys, 0x132);
        MILO_ASSERT(pClipPropKeys, 0x133);
#ifdef HX_NATIVE
        if (!pMovePropKeys || !pClipPropKeys) {
            mMovePropKeys[i] = nullptr;
            mClipPropKeys[i] = nullptr;
            continue;
        }
#endif
        mMovePropKeys[i] = pMovePropKeys->AsSymbolKeys();
        mClipPropKeys[i] = pClipPropKeys->AsSymbolKeys();
    }
    PropKeys *practiceKeys = TheHamDirector->GetPropKeys(kDifficultyExpert, practice);
    if (practiceKeys) {
        mPracticePropKeys = practiceKeys->AsSymbolKeys();
    } else
        mPracticePropKeys = nullptr;
}

void MoveMgr::NextMovesToShow(DataArray *a, int measure) {
    MILO_LOG("MoveMgr: next moves to show for measure %d\n", measure);
    if (mChoiceSets.size() < measure + 1) {
        mChoiceSets.resize(measure + 1);
    }
    const MoveParent **choices = mChoiceSets[measure].mChoices;
    if (!choices[0]) {
        MILO_LOG("MoveMgr: oh no they are not ready yet!\n");
        PrepareNextChoiceSet(measure - 1);
    }
    int numChoices = 0;
    int i = 0;
    for (i = 0; i < 4; i++) {
        if (!choices[i])
            break;
        char *choiceName = (char *)choices[i]->Name().Str();
        MILO_LOG("\t%s\n", choiceName);
        numChoices++;
    }
    a->Resize(4);
    for (i = 0; i < 4; i++) {
        a->Node(i) = choices[i % numChoices]->Name();
    }
}

void MoveMgr::PrepareNextChoiceSet(int measure) {
    ComputeRandomChoiceSet(measure + 1);
    ComputeLoadedMoveSet();
    TheHamDirector->LoadRoutineBuilderData(mVariants, true);
}

void MoveMgr::FillRoutineFromParents(int x) {
    if (x < 0) {
        x = mMoveParents[0].size() - 1;
    }
    for (int i = 0; i <= x; i++) {
        FillInRoutineAt(0, i);
    }
    for (int i = 0; i <= x; i++) {
        const MoveVariant *var = mRoutineMeasures[0][i].first;
        for (int j = 0; j < 2; j++) {
            InsertMoveInSong(var, i, j);
        }
    }
    mRoutineLoaded = true;
}

void MoveMgr::ResetRemixer() {
    if (mSuperEasyRemixer)
        mSuperEasyRemixer->Reset();
}

void MoveMgr::RegisterSongLayout(SongLayout *sl) { mCurrentSongLayout = sl; }

void MoveMgr::UnRegisterSongLayout(SongLayout *sl) {
    if (mCurrentSongLayout == sl) {
        mCurrentSongLayout = nullptr;
    }
}

const std::pair<const MoveVariant *, const MoveVariant *> *
MoveMgr::GetRoutineMeasure(int x, int y) const {
    int idx = mRoutineMeasures[x].size() != 0 ? x : 0;
    if (mRoutineMeasures[idx].size() <= y) {
        return 0;
    }
    return &mRoutineMeasures[idx][y];
}

CategoryData MoveMgr::GetCategoryByName(Symbol name) {
    for (int i = 0; i < mGenres.size(); i++) {
        if (mGenres[i].mName == name) {
            return mGenres[i];
        }
    }
    for (int i = 0; i < mEras.size(); i++) {
        if (mEras[i].mName == name) {
            return mEras[i];
        }
    }
    CategoryData data;
    data.mName = name;
    data.mToken = "category_unknown";
    return data;
}

void MoveMgr::SaveRoutineVariants(DataArray *a) const {
    a->Resize(mRoutineMeasures[0].size());
    int idx = 0;
    FOREACH (it, mRoutineMeasures[0]) {
        if (it->first) {
            Symbol first_name = it->first->Name();
            a->Node(idx) =
                DataArrayPtr(first_name, it->second ? it->second->Name() : first_name);
        } else {
            a->Node(idx) = DataArrayPtr(0, 0);
        }
        idx++;
    }
}

void MoveMgr::LoadRoutineVariants(const DataArray *a) {
    int aSize = a->Size();
    int idx = 0;
    auto it = mRoutineMeasures[0].begin();
    for (; it != mRoutineMeasures[0].end() && idx < aSize; ++it, ++idx) {
        mMoveParents[0][idx] = nullptr;
        it->first = nullptr;
        it->second = nullptr;
        DataArray *curArr = a->Array(idx);
        if (curArr->Type(0) == kDataSymbol && curArr->Type(1) == kDataSymbol) {
            it->first = mMoveGraph.FindMoveByVariantName(curArr->Sym(0));
            it->second = mMoveGraph.FindMoveByVariantName(curArr->Sym(1));
            if (it->first) {
                mMoveParents[0][idx] = it->first->Parent();
            }
        }
    }
    for (; it != mRoutineMeasures[0].end(); ++it, ++idx) {
        mMoveParents[0][idx] = nullptr;
        it->first = nullptr;
        it->second = nullptr;
    }
    for (int i = 0; i < mRoutineMeasures[0].size(); i++) {
        const MoveVariant *mv = mRoutineMeasures[0][i].first;
        for (int j = 0; j < 2; j++) {
            InsertMoveInSong(mv, i, j);
        }
    }
    mRoutineLoaded = true;
}

HamMove *MoveMgr::FindHamMoveFromName(Symbol name) const {
    if (name == Symbol("")) {
        return nullptr;
    }
    MoveDir *moveDir = TheHamDirector->GetMoveDir();
#ifdef HX_NATIVE
    if (!moveDir) {
#else
    if ((unsigned int)moveDir <= 0) {
#endif
        return nullptr;
    }
    HamMove *move = moveDir->Find<HamMove>(name.Str(), false);
    if (!move) {
        move = TheHamDirector->MergerDir()->Find<HamMove>(name.Str(), false);
        if (!move) {
            MILO_NOTIFY(
                "MoveMgr::FindHamMoveFromName couldn't find a move for %s", name
            );
        }
    }
    return move;
}

CharClip *MoveMgr::FindCharClip(Symbol name) const {
    Symbol nameSym = name;
    FOREACH (it, mVariants) {
        if ((*it)->Parent()->Name() == nameSym) {
            nameSym = (*it)->Name();
            break;
        }
    }
    CharClip *clip = TheHamDirector->ClipDir()->Find<CharClip>(nameSym.Str(), false);
    if (!clip) {
        MILO_LOG("Error: could not find clip for %s\n", nameSym.Str());
    }
    return clip;
}

DataNode MoveMgr::OnFindVariants(DataArray *a) {
    static Symbol unknown("unknown");
    DataArrayPtr ptr(unknown, unknown);
    const MoveParent *p1 = mMoveGraph.GetMoveParent(a->ForceSym(2));
    const MoveParent *p2 = mMoveGraph.GetMoveParent(a->ForceSym(3));
    const MoveVariant *mv1 = nullptr;
    const MoveVariant *mv2 = nullptr;
    mMoveGraph.FindVariantPair(mv1, mv2, p1, p2, nullptr, nullptr, "", false);
    if (mv1) {
        ptr->Node(0) = mv1->Name();
    } else if (p1) {
        ptr->Node(0) = p1->Variants().front()->Name();
    } else {
        ptr->Node(0) = "<unknown>";
    }
    if (mv2) {
        ptr->Node(1) = mv2->Name();
    } else if (p2) {
        ptr->Node(1) = p2->Variants().front()->Name();
    } else {
        ptr->Node(1) = "<unknown>";
    }
    return ptr;
}

HamMove *MoveMgr::FindHamMove(Symbol name) const {
    if (*name.Str() == '\0') {
        return nullptr;
    } else {
        Symbol nameSym = name;
        FOREACH (it, mVariants) {
            if ((*it)->Parent()->Name() == nameSym) {
                nameSym = (*it)->Name();
                break;
            }
        }
        const MoveVariant *mv = mMoveGraph.FindMoveByVariantName(nameSym);
        int numGraphNodes = MoveParents().size();
        if (!mv) {
            MILO_LOG(
                "Error: could not find a move in graph (%d nodes) called %s\n",
                numGraphNodes,
                nameSym.Str()
            );
        } else {
            nameSym = mv->HamMoveName();
        }
        HamMove *move = TheHamDirector->GetMoveDir()->Find<HamMove>(nameSym.Str(), false);
        if (!move) {
            move = TheHamDirector->MergerDir()->Find<HamMove>(nameSym.Str(), false);
        }
        return move;
    }
}

Symbol MoveMgr::FindVariantNameFromHamMoveName(Symbol name) const {
    std::vector<const MoveVariant *> moveVariants;
    mMoveGraph.GatherVariants(&moveVariants, MoveVariantsWithHamMove, &name);
    if (moveVariants.size() != 0) {
        return moveVariants.front()->Name();
    } else {
        return "";
    }
}

void MoveMgr::InitSong() {
    SongInit();
    mChoiceSets.clear();
    int i12 = INT_MAX;
    int i13 = 0;
    mCurrentSongLayout = GetSongLayout();
    FOREACH (it, mCurrentSongLayout->SongSections()) {
        if (i12 >= it->mMeasureRange.start) {
            i12 = it->mMeasureRange.start;
        }
        if (i13 <= it->mMeasureRange.end) {
            i13 = it->mMeasureRange.end;
        }
    }
    mMoveParents[0].resize(i13 + 2);
    mRoutineMeasures[0].resize(i13 + 2);
    mChoiceSets.resize(i13 + 2);
    mVariants.clear();
    FOREACH (it, mMoveParents[0]) {
        *it = nullptr;
    }
    FOREACH (it, mRoutineMeasures[0]) {
        it->first = nullptr;
        it->second = nullptr;
    }
    FOREACH (it, mChoiceSets) {
        it->mChoices[0] = 0;
        it->mChoices[1] = 0;
        it->mChoices[2] = 0;
        it->mChoices[3] = 0;
    }
    ComputeRandomChoiceSet(0);
    ComputeLoadedMoveSet();
    TheHamDirector->CleanOriginalMoveData();
    if (mMovesDir) {
        LoadMoveData(mMovesDir->Find<ObjectDir>("move_data", false));
    }
}

void MoveMgr::LoadSubCategoryData() {
    mFilteredGenres.clear();
    mFilteredEras.clear();
    std::map<Symbol, int> map70;
    std::map<Symbol, int> map90;
    map70.clear();
    map90.clear();
    FOREACH (it, mMoveGraph.MoveParents()) {
        MoveParent *cur = it->second;
        if (IsSuperEasyMove(cur->Name())) {
            cur->SetSuperEasy(true);
        }
        for (int i = 0; i < it->second->mGenreFlags.size(); i++) {
            map70[it->second->mGenreFlags[i]]++;
        }
        for (int i = 0; i < it->second->mEraFlags.size(); i++) {
            map90[it->second->mEraFlags[i]]++;
        }
    }
    DataArray *superEasy = SystemConfig("super_easy_moves");
    MILO_ASSERT(superEasy, 0x540);
    FOREACH (it, map70) {
        mFilteredGenres.push_back(GetCategoryByName(it->first));
    }
    FOREACH (it, map90) {
        mFilteredEras.push_back(GetCategoryByName(it->first));
    }
}

int MoveMgr::ComputeRandomChoiceSet(int measure) {
    MILO_LOG("MoveMgr: compute moves for measure %d:\n", measure);
    if (mChoiceSets.size() < (unsigned int)(measure + 1)) {
        mChoiceSets.resize(measure + 1);
    }
    MoveChoiceSet *choiceSet = &mChoiceSets[measure];
    std::set<const MoveParent *> potentialMoves;
    ComputePotentialMoves(potentialMoves, measure);
    int numPotential = potentialMoves.size();
    if (numPotential == 0) {
        return 0;
    }
    int indices[4];
    int numToPlace = 4;
    if (numPotential <= 4) {
        numToPlace = numPotential;
        for (int i = 0; i < numPotential; i++) {
            indices[i] = i;
        }
    } else {
        for (int i = 0; i < 4; i++) {
            indices[i] = RandomInt(0, numPotential);
        }
        std::sort(indices, indices + 4);
    }
    int numPlaced = 0;
    int idx = 0;
    std::set<const MoveParent *>::iterator it = potentialMoves.begin();
    while (it != potentialMoves.end()) {
        if (numPlaced >= numToPlace) {
            break;
        }
        if (idx >= indices[numPlaced]) {
            MILO_LOG("\t%s\n", (*it)->Name().Str());
            choiceSet->mChoices[numPlaced] = *it;
            numPlaced++;
        }
        ++it;
        idx++;
    }
    return numPlaced;
}

void MoveMgr::ComputeLoadedMoveSet() {
    mVariants.clear();
    unsigned int choiceSize = mChoiceSets.size();
    unsigned int routineSize = mRoutineMeasures[0].size();
    unsigned int maxSize = routineSize;
    if (maxSize <= choiceSize) {
        maxSize = choiceSize;
    }
    std::pair<const MoveVariant *, const MoveVariant *> nullPair(nullptr, nullptr);
    mRoutineMeasures[0].resize(maxSize, nullPair);
    mChoiceSets.resize(maxSize);
    std::pair<const MoveVariant *, const MoveVariant *> *routineData = &mRoutineMeasures[0][0];
    MoveChoiceSet *choiceData = &mChoiceSets[0];
    int count = (int)maxSize;
    if (count > 0) {
        do {
            if (routineData->first) {
                mVariants.insert(routineData->first);
                if (routineData->second && routineData->second != routineData->first) {
                    mVariants.insert(routineData->second);
                }
            } else if (choiceData->mChoices[0]) {
                const MoveParent *const *choice = choiceData->mChoices;
                int j = 4;
                do {
                    if (*choice) {
                        const MoveVariant *var = (*choice)->Variants().front();
                        mVariants.insert(var);
                    }
                    j--;
                    choice++;
                } while (j != 0);
            }
            count--;
            routineData++;
            choiceData++;
        } while (count != 0);
    }
}

void MoveMgr::FillInRoutineAt(int player, int measure) {
    const MoveParent *curParent = mMoveParents[player][measure];
    if (!curParent) {
        return;
    }
    const MoveVariant *preferred = GetRoutinePreferredVariant(player, measure);
    std::pair<const MoveVariant *, const MoveVariant *> *routinePair =
        &mRoutineMeasures[player].at(measure);
    routinePair->first = nullptr;
    routinePair->second = nullptr;

    // Try to find variant pair with previous measure
    if (measure > 0) {
        std::pair<const MoveVariant *, const MoveVariant *> *prevPair = routinePair - 1;
        const MoveParent *prevParent = mMoveParents[player][measure - 1];
        const MoveVariant *prevPreferred = GetRoutinePreferredVariant(player, measure - 1);
        if (!prevPreferred) {
            prevPreferred = prevPair->first;
        }
        if (!mMoveGraph.FindVariantPair(
                prevPair->second,
                routinePair->first,
                prevParent,
                curParent,
                prevPreferred,
                preferred,
                mCurrentSong,
                false
            )) {
            if (!prevPreferred) {
                prevPreferred = prevPair->first;
            }
            prevPair->second = prevPreferred;
        }
    }

    // Try to find variant pair with next measure
    if ((unsigned int)measure < (unsigned int)(mMoveParents[player].size() - 1)) {
        std::pair<const MoveVariant *, const MoveVariant *> *nextPair = routinePair + 1;
        const MoveParent *nextParent = mMoveParents[player][measure + 1];
        const MoveVariant *nextPreferred = GetRoutinePreferredVariant(player, measure + 1);
        if (!mMoveGraph.FindVariantPair(
                routinePair->second,
                nextPair->first,
                curParent,
                nextParent,
                preferred,
                nextPreferred,
                mCurrentSong,
                false
            )) {
            if (!nextPreferred) {
                nextPreferred = nextPair->second;
            }
            nextPair->first = nextPreferred;
        }
    }

    // Fallback for .first
    if (!routinePair->first) {
        routinePair->first = preferred;
        if (!preferred) {
            routinePair->first = routinePair->second;
            if (!routinePair->second) {
                routinePair->first = curParent->Variants().front();
                if (!mCurrentSong.Null()) {
                    const std::vector<MoveVariant *> &variants = curParent->Variants();
                    for (int i = 0; i < (int)variants.size(); i++) {
                        if (variants[i]->Song() == mCurrentSong) {
                            routinePair->first = variants[i];
                            break;
                        }
                    }
                }
            }
        }
    }

    // Fallback for .second
    if (!routinePair->second) {
        routinePair->second = preferred;
        if (!preferred) {
            routinePair->second = routinePair->first;
        }
    }
}

void MoveMgr::FillRoutineFromVerses(int player) {
    FOREACH (it, mCurrentSongLayout->mSongSections) {
        int secLen = (it->mMeasureRange.end - it->mMeasureRange.start) + 1;
        int patLen = (it->mPatternRange.end - it->mPatternRange.start) + 1;
        MILO_ASSERT(patLen == secLen, 0x428);
        for (int i = 0; i < it->mSongPattern->mNumMoves; i++) {
            int measure = it->mMeasureRange.start + i;
            unsigned int patIdx = (it->mPatternRange.start + i) - 1;
            if (patIdx >= (unsigned int)(it->mSongPattern->mElements.size() - 1)) {
                patIdx = it->mSongPattern->mElements.size() - 1;
            }
            mMoveParents[0][measure] = it->mSongPattern->mMoveParents[patIdx];
        }
    }
    FillRoutineFromParents(player);
}

void MoveMgr::FillRoutineFromReplacer(int player) {
    FOREACH (it, mCurrentSongLayout->mMoveReplacers) {
        if (it->mMoveParent) {
            for (std::vector<int>::iterator mit = it->mMeasures.begin();
                 mit != it->mMeasures.end();
                 ++mit) {
                mMoveParents[0][*mit] = it->mMoveParent;
            }
        }
    }
    FillRoutineFromParents(player);
}
