#pragma once
#include "ui/UILabel.h"
#include "ui/UIPanel.h"

class TourChallengeResultsPanel : public UIPanel {
public:
    TourChallengeResultsPanel();
    OBJ_CLASSNAME(TourChallengeResultsPanel);
    OBJ_SET_TYPE(TourChallengeResultsPanel);
    NEW_OBJ(TourChallengeResultsPanel);
    virtual DataNode Handle(DataArray *, bool);
    // NO destructor declared -- retail's ??_GTourChallengeResultsPanel has NO derived vptr-restore
    // (lane W16-X 2026-09-14). A user-declared dtor, even `{}`, makes MSVC emit
    // that prologue in ??1TourChallengeResultsPanel, bloating ??_DTourChallengeResultsPanel past the ??_G inline threshold:
    // 80 B via ??_D instead of retail's direct ??1UIPanel + ??1Hmx::Object.
    virtual void Enter();

    int GetPreGigTotalStars() const;
    int GetTotalTourStars() const;
    int GetGigTotalStars() const;
    int GetGigMaxStars() const;
    Symbol GetChallengeName() const;
    void UpdateSetlistLabel(UILabel *);
    void UpdateSongName(int, UILabel *);
    int GetSongTotalStars(int);
    int GetSongStars(int);
    int GetChallengeStars(int);
    int GetSongCount();
};