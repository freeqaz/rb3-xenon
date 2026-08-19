#pragma once
#include "BandProfile.h"
#include "beatmatch/TrackType.h"
#include "game/Defines.h"
#include "obj/Object.h"
#include <hash_map>

// Retail keys these three tables with STLport hash_maps, not std::maps: the
// LessonMgr .text span defines and calls ZERO _Rb_tree<Symbol,...> symbols
// while calling hashtable<pair<const Symbol,_>>::_M_find and the hash_map
// default ctor, and these three members are the unit's only Symbol-keyed
// containers.
#ifndef RB3_HASH_SYMBOL_DEFINED
#define RB3_HASH_SYMBOL_DEFINED
namespace stlpmtx_std {
_STLP_TEMPLATE_NULL struct hash<Symbol> {
    size_t operator()(const Symbol &s) const { return (size_t)s.Str(); }
};
}
#endif

class Lesson {
public:
    Lesson(Symbol, Symbol, Symbol, Symbol, TrackType);
    ~Lesson();
    Difficulty GetDifficulty() const;

    Symbol mTrainer; // 0x0
    Symbol mCategory; // 0x4
    Symbol mName; // 0x8
    Symbol mSong; // 0xc
    TrackType mTrackType; // 0x10
};

class LessonMgr : public Hmx::Object {
public:
    LessonMgr();
    virtual ~LessonMgr();

    Lesson *GetLesson(Symbol) const;
    bool HasLesson(Symbol s) const { return GetLesson(s) != nullptr; }
    TrackType GetTrackTypeFromTrainer(Symbol);
    std::vector<Symbol> *GetLessonsFromCategory(Symbol) const;
    std::vector<Symbol> *GetCategoriesFromTrainer(Symbol) const;
    int GetCompletedCountFromTrainer(BandProfile *, Symbol);
    int GetTotalCountFromTrainer(Symbol);
    Difficulty GetDifficulty() const;
    int GetTotalCountFromCategory(Symbol);
    int GetCompletedCountFromCategory(BandProfile *, Symbol);
    const std::hash_map<Symbol, Lesson *> &LessonsMap() const { return mLessonsMap; }

    static void Init();
    static LessonMgr *GetLessonMgr();

    std::vector<Symbol> mTrainers; // 0x28
    std::hash_map<Symbol, std::vector<Symbol> *> mTrainerToCategoriesMap; // 0x24
    std::hash_map<Symbol, std::vector<Symbol> *> mCategoryToLessonsMap; // 0x3c
    std::hash_map<Symbol, Lesson *> mLessonsMap; // 0x54
};