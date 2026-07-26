#pragma once
#include "obj/Data.h"
#include "rndobj/Group.h"
#include "ui/UILabel.h"
#include "ui/UIPanel.h"

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
    std::map<Symbol, int> mDetailCounts; // 0x40 (0x18)
    // Retail RB3-360 has 4 extra bytes between mDetailCounts and
    // mDetailsPageSize (2026-07-26: UpdateScrollArrows' target asm reads
    // mDetailsPageSize at 0x5c and mDetailsHeight[] at 0x68, while unk70[]
    // stays at 0x78 — so the +4 lives BEFORE mDetailsPageSize, not after
    // mDetailsHeight where it was previously parked).  Likely the retail
    // std::map<Symbol,int> node header being 0x1c; modelled as an explicit
    // pad so no STL layout is disturbed.
    int unk58_retailpad; // 0x58
    float mDetailsPageSize; // 0x5c
    float mDetailsFooterSize; // 0x60
    float mDetailsScrollStep; // 0x64
    float mDetailsHeight[4]; // 0x68
    int unk70[4]; // 0x78
    bool unk80[4];
    bool unk84[4];
    RndGroup *mScrollGroups[4]; // 0x88
    bool unk98; // 0x98
    std::vector<UILabel *> mDetailLabels; // 0x9c
};