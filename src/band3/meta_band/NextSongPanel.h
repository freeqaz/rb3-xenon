#pragma once
#include "obj/Data.h"
#include "rndobj/Group.h"
#include "ui/UILabel.h"
#include "ui/UIPanel.h"
#include <hash_map>

// mDetailCounts is a hash_map (see the member comment below). hash<Symbol> is
// normally supplied by meta/FixedSizeSaveableStream.h, but this header does not
// include it, and the member declaration would otherwise instantiate the
// primary template before that explicit specialization is seen.
#ifndef RB3_HASH_SYMBOL_DEFINED
#define RB3_HASH_SYMBOL_DEFINED
#if HX_NATIVE
namespace std {
template <> struct hash<Symbol> {
    size_t operator()(const Symbol &s) const { return (size_t)s.Str(); }
};
}
#else
namespace stlpmtx_std {
_STLP_TEMPLATE_NULL struct hash<Symbol> {
    size_t operator()(const Symbol &s) const { return (size_t)s.Str(); }
};
}
#endif
#endif

class NextSongPanel : public UIPanel {
public:
    NextSongPanel() {}
    OBJ_CLASSNAME(NextSongPanel);
    OBJ_SET_TYPE(NextSongPanel);
    NEW_OBJ(NextSongPanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~NextSongPanel() {}
    virtual void Enter();
    virtual void Exit();
    virtual bool Exiting() const;
    virtual void Poll();
    virtual void FinishLoad();

    void DeterminePerformanceAwards(int);
    void HideAllDetailComponents(int);
    void FillExpandedDetails(int);
    void ScrollExpandedDetails(int, int, bool);
    int GetMaxScrollPage(int);
    bool CanChangeSongReview(int) const;
    void InitializeSongReviewDisplay(int);
    void IncrementSongReview(int);
    void SetReviewDisplayValue(int, int);
    void UpdateScrollArrows(int, bool);
    void SetScrollExpandedDetails(int, int);
    int CountOrCreateExpandedDetails(int, DataArrayPtr &, bool);
    void SetupDetailLine(DataArray *, int, const char *, float);
    Symbol GetPerformanceAward(int);

    float mEnterTime; // 0x3c
    // hash_map, not map. Retail's container at 0x40 spans 0x1c bytes, which a
    // previous pass modelled as an explicit +4 pad (unk58_retailpad) on the
    // guess that a std::map node header was 0x1c. It is not: STLport map is
    // 0x18 and hash_map is 0x1c, and NextSongPanel's .text span defines and
    // calls ZERO _Rb_tree<Symbol,...> symbols while calling
    // hashtable<pair<const Symbol,int>>::_M_find and the hash_map default
    // ctor. So the +4 IS the hash_map, and the pad is deleted with it --
    // every following member keeps its retail offset.
    std::hash_map<Symbol, int> mDetailCounts; // 0x40 (0x1c)
    float mDetailsPageSize; // 0x5c
    float mDetailsFooterSize; // 0x60
    float mDetailsScrollStep; // 0x64
    float mDetailsHeight[4]; // 0x68
    int unk70[4]; // 0x78
    bool unk80[4];
    bool unk84[4];
    RndGroup *mScrollGroups[4]; // 0x88
    bool unk98; // 0xa0
    std::vector<UILabel *> mDetailLabels; // 0x9c
};