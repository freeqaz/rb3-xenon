#pragma once
#include "game/GemTrainerPanel.h"

class ProTrainerPanel : public GemTrainerPanel {
public:
    // NO constructor declared: an implicit ctor stores retail's literal 0 to the
    // vtordisp slot (cf. GemTrainerLoopPanel).
    virtual DataNode Handle(DataArray *, bool);
    // NO destructor declared: retail ??1ProTrainerPanel (retail row fn_826AF*) has no derived
    // vptr-restore, which a user-declared `{}` dtor would emit (cf. GemTrainerLoopPanel).
    virtual void Enter();
    virtual void SetLessonComplete(int);
    virtual bool AllSectionsFinished() const;
    virtual void NewDifficulty(int, int);
    virtual float GetLessonCompleteSpeed(int) const;
    virtual void SetSongSectionComplete(BandProfile *, int, Difficulty, int) = 0;

    std::vector<float> mSpeedCompleted; // 0xf4
};