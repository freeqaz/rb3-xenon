#pragma once
#include "MoveGraph.h"
#include "hamobj/MoveGraph.h"
#include "hamobj/SongUtl.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "utl/MemMgr.h"
#include <vector>

// RB3 retail SongPattern is 24 bytes (0x18): { Symbol; Range; vector<Symbol> }.
// The DC3 (newer, dance-move) engine ADDED mMoveParents/mNumMoves — retail RB3
// (music game, no dance moves) lacks them. Verified from retail TU5: every
// vector<SongPattern> STL op uses a 0x18 element stride, not 0x28.
class SongPattern {
public:
    SongPattern() { mElements.clear(); }
    ~SongPattern() {}

    /** "The name of this pattern" */
    Symbol mName; // 0x0
    /** "The measure range when this pattern first appears" */
    Range mInitialMeasureRange; // 0x4
    /** "Pattern elements" */
    std::vector<Symbol> mElements; // 0xc
};

class SongSection {
public:
    /** "Measure start and end" */
    Range mMeasureRange; // 0x0
    /** "Pattern start and end" */
    Range mPatternRange; // 0x8
    /** "The pattern this section is using" */
    Symbol mPattern; // 0x10
    SongPattern *mSongPattern; // 0x14
};

struct MoveReplacer {
    MoveReplacer() : mFrom(gNullStr), mTo(gNullStr), mMoveParent(nullptr) {}
    MoveReplacer(const MoveReplacer &o);
    Symbol mFrom; // 0x0
    Symbol mTo; // 0x4
    const MoveParent *mMoveParent; // 0x8
    std::vector<int> mMeasures; // 0xc
};

/** "Song layout" */
class SongLayout : public Hmx::Object {
    friend class MoveMgr;
public:
    // Hmx::Object
    virtual ~SongLayout();
    OBJ_CLASSNAME(SongLayout);
    OBJ_SET_TYPE(SongLayout);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);

    OBJ_MEM_OVERLOAD(0x47)
    NEW_OBJ(SongLayout)

    void SetDefaultPattern(int);
    void SetDefaultReplacer();
    void ClearChosenPatterns();
    void AddPatternMove(int, Symbol);
    void SetReplacerMove(int, Symbol);
    void ClearAllPatternMoves();
    int FirstUnfilledPattern() const;
    int FirstUnfilledReplacer() const;
    int ReplacerFirstMeasure(int) const;
    void DumpPatterns() const;
    bool MeasureInPattern(int, int) const;
    void ClearMoves(int);

    int NumReplacers() const { return mMoveReplacers.size(); }
    const std::vector<SongSection> &SongSections() const { return mSongSections; }

    DataNode GetPatternName(int) const;

protected:
    SongLayout();

    DataNode AddPattern(DataArray *);
    DataNode AddSection(DataArray *);

    /** "Patterns of a song layout section" */
    std::vector<SongPattern> mSongPatterns; // 0x2c
    /** "Sections of a song layout" */
    std::vector<SongSection> mSongSections; // 0x38
    std::vector<MoveReplacer> mMoveReplacers; // 0x44
};
